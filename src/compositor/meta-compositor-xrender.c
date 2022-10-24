/*
 * Copyright (C) 2007 Iain Holmes
 * Copyright (C) 2017-2020 Alberts Muktupāvels
 *
 * Based on xcompmgr - (C) 2003 Keith Packard
 *          xfwm4    - (C) 2005-2007 Olivier Fourdan
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE
#define _XOPEN_SOURCE 600 /* for usleep() */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include <gdk/gdk.h>
#include <libmetacity/meta-frame-borders.h>

#include "display-private.h"
#include "screen.h"
#include "frame.h"
#include "errors.h"
#include "prefs.h"
#include "window-private.h"
#include "meta-compositor-xrender.h"
#include "meta-shadow-xrender.h"
#include "meta-surface-xrender.h"
#include "xprops.h"
#include "util.h"
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrender.h>

#define OPAQUE 0xffffffff

#define SHADOW_MEDIUM_RADIUS 6.0
#define SHADOW_LARGE_RADIUS 12.0

#define SHADOW_MEDIUM_OFFSET_X (SHADOW_MEDIUM_RADIUS * -3 / 2)
#define SHADOW_MEDIUM_OFFSET_Y (SHADOW_MEDIUM_RADIUS * -5 / 4)
#define SHADOW_LARGE_OFFSET_X -15
#define SHADOW_LARGE_OFFSET_Y -15

#define SHADOW_OPACITY 0.66

typedef enum _MetaShadowType
{
  META_SHADOW_MEDIUM,
  META_SHADOW_LARGE,
  LAST_SHADOW_TYPE
} MetaShadowType;

typedef struct _conv
{
  int size;
  double *data;
} conv;

typedef struct _shadow
{
  conv *gaussian_map;
  guchar *shadow_corner;
  guchar *shadow_top;
} shadow;

typedef struct
{
  Display    *xdisplay;

  MetaScreen *screen;

  Window      overlay_window;

  gboolean    have_shadows;
  shadow     *shadows[LAST_SHADOW_TYPE];

  Picture     root_picture;
  Picture     root_buffer;
  Picture     root_tile;

  gboolean    prefs_listener_added;

  guint       show_redraw : 1;
  GRand      *rand;
} MetaCompositorXRenderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaCompositorXRender,
                            meta_compositor_xrender,
                            META_TYPE_COMPOSITOR)

/* Gaussian stuff for creating the shadows */
static double
gaussian (double r,
          double x,
          double y)
{
  return ((1 / (sqrt (2 * G_PI * r))) *
          exp ((- (x * x + y * y)) / (2 * r * r)));
}

static conv *
make_gaussian_map (double r)
{
  conv *c;
  int size, centre;
  int x, y;
  double t, g;

  size = ((int) ceil ((r * 3)) + 1) & ~1;
  centre = size / 2;
  c = g_malloc (sizeof (conv) + size * size * sizeof (double));
  c->size = size;
  c->data = (double *) (c + 1);
  t = 0.0;

  for (y = 0; y < size; y++)
    {
      for (x = 0; x < size; x++)
        {
          g = gaussian (r, (double) (x - centre), (double) (y - centre));
          t += g;
          c->data[y * size + x] = g;
        }
    }

  for (y = 0; y < size; y++)
    {
      for (x = 0; x < size; x++)
        {
          c->data[y * size + x] /= t;
        }
    }

  return c;
}

/*
* A picture will help
*
*      -center   0                width  width+center
*  -center +-----+-------------------+-----+
*          |     |                   |     |
*          |     |                   |     |
*        0 +-----+-------------------+-----+
*          |     |                   |     |
*          |     |                   |     |
*          |     |                   |     |
*   height +-----+-------------------+-----+
*          |     |                   |     |
* height+  |     |                   |     |
*  center  +-----+-------------------+-----+
*/
static guchar
sum_gaussian (conv          *map,
              double         opacity,
              int            x,
              int            y,
              int            width,
              int            height)
{
  double *g_data, *g_line;
  double v;
  int fx, fy;
  int fx_start, fx_end;
  int fy_start, fy_end;
  int g_size, centre;

  g_line = map->data;
  g_size = map->size;
  centre = g_size / 2;
  fx_start = centre - x;
  if (fx_start < 0)
    fx_start = 0;

  fx_end = width + centre - x;
  if (fx_end > g_size)
    fx_end = g_size;

  fy_start = centre - y;
  if (fy_start < 0)
    fy_start = 0;

  fy_end = height + centre - y;
  if (fy_end > g_size)
    fy_end = g_size;

  g_line = g_line + fy_start * g_size + fx_start;

  v = 0.0;
  for (fy = fy_start; fy < fy_end; fy++)
    {
      g_data = g_line;
      g_line += g_size;

      for (fx = fx_start; fx < fx_end; fx++)
        v += *g_data++;
    }

  if (v > 1.0)
    v = 1.0;

  return ((guchar) (v * opacity * 255.0));
}

/* precompute shadow corners and sides to save time for large windows */
static void
presum_gaussian (shadow *shad)
{
  int centre;
  int opacity, x, y;
  int msize;
  conv *map;

  map = shad->gaussian_map;
  msize = map->size;
  centre = map->size / 2;

  if (shad->shadow_corner)
    g_free (shad->shadow_corner);
  if (shad->shadow_top)
    g_free (shad->shadow_top);

  shad->shadow_corner = (guchar *)(g_malloc ((msize + 1) * (msize + 1) * 26));
  shad->shadow_top = (guchar *) (g_malloc ((msize + 1) * 26));

  for (x = 0; x <= msize; x++)
    {

      shad->shadow_top[25 * (msize + 1) + x] =
        sum_gaussian (map, 1, x - centre, centre, msize * 2, msize * 2);
      for (opacity = 0; opacity < 25; opacity++)
        {
          shad->shadow_top[opacity * (msize + 1) + x] =
            shad->shadow_top[25 * (msize + 1) + x] * opacity / 25;
        }

      for (y = 0; y <= x; y++)
        {
          shad->shadow_corner[25 * (msize + 1) * (msize + 1)
                              + y * (msize + 1)
                              + x]
            = sum_gaussian (map, 1, x - centre, y - centre,
                            msize * 2, msize * 2);

          shad->shadow_corner[25 * (msize + 1) * (msize + 1)
                              + x * (msize + 1) + y] =
            shad->shadow_corner[25 * (msize + 1) * (msize + 1)
                                + y * (msize + 1) + x];

          for (opacity = 0; opacity < 25; opacity++)
            {
              shad->shadow_corner[opacity * (msize + 1) * (msize + 1)
                                  + y * (msize + 1) + x]
                = shad->shadow_corner[opacity * (msize + 1) * (msize + 1)
                                      + x * (msize + 1) + y]
                = shad->shadow_corner[25 * (msize + 1) * (msize + 1)
                                      + y * (msize + 1) + x] * opacity / 25;
            }
        }
    }
}

static void
generate_shadows (MetaCompositorXRender *self)
{
  MetaCompositorXRenderPrivate *priv;
  double radii[LAST_SHADOW_TYPE] = {SHADOW_MEDIUM_RADIUS,
                                    SHADOW_LARGE_RADIUS};
  int i;

  priv = meta_compositor_xrender_get_instance_private (self);

  for (i = 0; i < LAST_SHADOW_TYPE; i++) {
    shadow *shad = g_new0 (shadow, 1);

    shad->gaussian_map = make_gaussian_map (radii[i]);
    presum_gaussian (shad);

    priv->shadows[i] = shad;
  }
}

static XImage *
make_shadow (MetaCompositorXRender *self,
             MetaShadowType         shadow_type,
             double                 opacity,
             int                    width,
             int                    height)
{
  MetaCompositorXRenderPrivate *priv;
  Display *xdisplay;
  XImage *ximage;
  guchar *data;
  shadow *shad;
  int msize;
  int ylimit, xlimit;
  int swidth, sheight;
  int centre;
  int x, y;
  guchar d;
  int x_diff;
  int opacity_int = (int)(opacity * 25);
  int screen_number;

  priv = meta_compositor_xrender_get_instance_private (self);

  shad = priv->shadows[shadow_type];
  msize = shad->gaussian_map->size;
  swidth = width + msize;
  sheight = height + msize;
  centre = msize / 2;

  data = g_malloc (swidth * sheight * sizeof (guchar));

  xdisplay = priv->xdisplay;
  screen_number = DefaultScreen (xdisplay);
  ximage = XCreateImage (xdisplay, DefaultVisual (xdisplay, screen_number),
                         8, ZPixmap, 0, (char *) data,
                         swidth, sheight, 8, swidth * sizeof (guchar));
  if (!ximage)
    {
      g_free (data);
      return NULL;
    }

  /*
   * Build the gaussian in sections
   */

  /*
   * centre (fill the complete data array
   */
  if (msize > 0)
    d = shad->shadow_top[opacity_int * (msize + 1) + msize];
  else
    d = sum_gaussian (shad->gaussian_map, opacity, centre,
                      centre, width, height);
  memset (data, d, sheight * swidth);

  /*
   * corners
   */
  ylimit = msize;
  if (ylimit > sheight / 2)
    ylimit = (sheight + 1) / 2;

  xlimit = msize;
  if (xlimit > swidth / 2)
    xlimit = (swidth + 1) / 2;

  for (y = 0; y < ylimit; y++)
    {
      for (x = 0; x < xlimit; x++)
        {

          if (xlimit == msize && ylimit == msize)
            d = shad->shadow_corner[opacity_int * (msize + 1) * (msize + 1) + y * (msize + 1) + x];
          else
            d = sum_gaussian (shad->gaussian_map, opacity, x - centre,
                              y - centre, width, height);

          data[y * swidth + x] = d;
          data[(sheight - y - 1) * swidth + x] = d;
          data[(sheight - y - 1) * swidth + (swidth - x - 1)] = d;
          data[y * swidth + (swidth - x - 1)] = d;
        }
    }

  /* top/bottom */
  x_diff = swidth - (msize * 2);
  if (x_diff > 0 && ylimit > 0)
    {
      for (y = 0; y < ylimit; y++)
        {
          if (ylimit == msize)
            d = shad->shadow_top[opacity_int * (msize + 1) + y];
          else
            d = sum_gaussian (shad->gaussian_map, opacity, centre,
                              y - centre, width, height);

          memset (&data[y * swidth + msize], d, x_diff);
          memset (&data[(sheight - y - 1) * swidth + msize], d, x_diff);
        }
    }

  /*
   * sides
   */
  for (x = 0; x < xlimit; x++)
    {
      if (xlimit == msize)
        d = shad->shadow_top[opacity_int * (msize + 1) + x];
      else
        d = sum_gaussian (shad->gaussian_map, opacity, x - centre,
                          centre, width, height);

      for (y = msize; y < sheight - msize; y++)
        {
          data[y * swidth + x] = d;
          data[y * swidth + (swidth - x - 1)] = d;
        }
    }

  return ximage;
}

double shadow_offsets_x[LAST_SHADOW_TYPE] = {SHADOW_MEDIUM_OFFSET_X,
                                             SHADOW_LARGE_OFFSET_X};
double shadow_offsets_y[LAST_SHADOW_TYPE] = {SHADOW_MEDIUM_OFFSET_Y,
                                             SHADOW_LARGE_OFFSET_Y};

static XserverRegion
cairo_region_to_xserver_region (Display        *xdisplay,
                                cairo_region_t *region)
{
  int n_rects, i;
  XRectangle *rects;
  XserverRegion xregion;

  if (region == NULL)
    return None;

  n_rects = cairo_region_num_rectangles (region);
  rects = g_new (XRectangle, n_rects);

  for (i = 0; i < n_rects; i++)
    {
      cairo_rectangle_int_t rect;

      cairo_region_get_rectangle (region, i, &rect);

      rects[i].x = rect.x;
      rects[i].y = rect.y;
      rects[i].width = rect.width;
      rects[i].height = rect.height;
    }

  xregion = XFixesCreateRegion (xdisplay, rects, n_rects);
  g_free (rects);

  return xregion;
}

static Picture
shadow_picture (MetaCompositorXRender *self,
                MetaShadowType         shadow_type,
                double                 opacity,
                int                    width,
                int                    height,
                int                   *wp,
                int                   *hp)
{
  MetaCompositorXRenderPrivate *priv;
  Display *xdisplay;
  XImage *shadow_image;
  Pixmap shadow_pixmap;
  Picture shadow_picture;
  GC gc;

  priv = meta_compositor_xrender_get_instance_private (self);

  shadow_image = make_shadow (self, shadow_type, opacity, width, height);

  if (!shadow_image)
    return None;

  xdisplay = priv->xdisplay;
  shadow_pixmap = XCreatePixmap (xdisplay, DefaultRootWindow (xdisplay),
                                 shadow_image->width, shadow_image->height, 8);
  if (!shadow_pixmap)
    {
      XDestroyImage (shadow_image);
      return None;
    }

  shadow_picture = XRenderCreatePicture (xdisplay, shadow_pixmap,
                                         XRenderFindStandardFormat (xdisplay, PictStandardA8),
                                         0, 0);
  if (!shadow_picture)
    {
      XDestroyImage (shadow_image);
      XFreePixmap (xdisplay, shadow_pixmap);
      return None;
    }

  gc = XCreateGC (xdisplay, shadow_pixmap, 0, 0);
  if (!gc)
    {
      XDestroyImage (shadow_image);
      XFreePixmap (xdisplay, shadow_pixmap);
      XRenderFreePicture (xdisplay, shadow_picture);
      return None;
    }

  XPutImage (xdisplay, shadow_pixmap, gc, shadow_image, 0, 0, 0, 0,
             shadow_image->width, shadow_image->height);
  *wp = shadow_image->width;
  *hp = shadow_image->height;

  XFreeGC (xdisplay, gc);
  XDestroyImage (shadow_image);
  XFreePixmap (xdisplay, shadow_pixmap);

  return shadow_picture;
}

static Picture
solid_picture (Display  *xdisplay,
               gboolean  argb,
               double    a,
               double    r,
               double    g,
               double    b)
{
  Pixmap pixmap;
  Picture picture;
  XRenderPictureAttributes pa;
  XRenderPictFormat *render_format;
  XRenderColor c;
  Window xroot = DefaultRootWindow (xdisplay);

  render_format = XRenderFindStandardFormat (xdisplay,
                                             argb ? PictStandardARGB32 : PictStandardA8);

  pixmap = XCreatePixmap (xdisplay, xroot, 1, 1, argb ? 32 : 8);
  g_return_val_if_fail (pixmap != None, None);

  pa.repeat = TRUE;
  picture = XRenderCreatePicture (xdisplay, pixmap, render_format,
                                  CPRepeat, &pa);
  if (picture == None)
    {
      XFreePixmap (xdisplay, pixmap);
      g_warning ("(picture != None) failed");
      return None;
    }

  c.alpha = a * 0xffff;
  c.red = r * 0xffff;
  c.green = g * 0xffff;
  c.blue = b * 0xffff;

  XRenderFillRectangle (xdisplay, PictOpSrc, picture, &c, 0, 0, 1, 1);
  XFreePixmap (xdisplay, pixmap);

  return picture;
}

static gboolean
is_background_pixmap_valid (MetaDisplay  *display,
                            Pixmap        pixmap,
                            unsigned int *depth)
{
  Display *xdisplay;
  Window root_return;
  int x_return;
  int y_return;
  unsigned int width_return;
  unsigned int height_return;
  unsigned int border_width_return;
  unsigned int depth_return;
  Status status;

  xdisplay = meta_display_get_xdisplay (display);

  meta_error_trap_push (display);

  status = XGetGeometry (xdisplay,
                         pixmap,
                         &root_return,
                         &x_return,
                         &y_return,
                         &width_return,
                         &height_return,
                         &border_width_return,
                         &depth_return);

  if (meta_error_trap_pop_with_return (display) != 0 || status == 0)
    return FALSE;

  *depth = depth_return;

  return TRUE;
}

static Picture
root_tile (MetaScreen *screen)
{
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  Picture picture;
  Pixmap pixmap;
  gboolean free_pixmap;
  gboolean fill;
  Visual *xvisual;
  XRenderPictureAttributes pa;
  XRenderPictFormat *format;
  int p;
  Atom background_atoms[2];
  Atom pixmap_atom;
  int screen_number = meta_screen_get_screen_number (screen);
  Window xroot = meta_screen_get_xroot (screen);

  pixmap = None;
  free_pixmap = FALSE;
  fill = FALSE;
  xvisual = NULL;

  background_atoms[0] = display->atom__XROOTPMAP_ID;
  background_atoms[1] = display->atom__XSETROOT_ID;

  pixmap_atom = XInternAtom (xdisplay, "PIXMAP", False);
  for (p = 0; p < 2; p++)
    {
      Atom actual_type;
      int actual_format;
      gulong nitems, bytes_after;
      guchar *prop;

      if (XGetWindowProperty (xdisplay, xroot,
                              background_atoms[p],
                              0, 4, FALSE, AnyPropertyType,
                              &actual_type, &actual_format,
                              &nitems, &bytes_after, &prop) == Success)
        {
          if (actual_type == pixmap_atom &&
              actual_format == 32 &&
              nitems == 1)
            {
              unsigned int depth;

              memcpy (&pixmap, prop, 4);
              XFree (prop);

              if (is_background_pixmap_valid (display, pixmap, &depth))
                {
                  XVisualInfo visual_info;

                  if (XMatchVisualInfo (xdisplay,
                                        screen_number,
                                        depth,
                                        TrueColor,
                                        &visual_info) != 0)
                    {
                      xvisual = visual_info.visual;
                      break;
                    }
                }

              pixmap = None;
            }
        }
    }

  if (!pixmap)
    {
      int width;
      int height;

      meta_screen_get_size (screen, &width, &height);

      pixmap = XCreatePixmap (xdisplay, xroot, width, height,
                              DefaultDepth (xdisplay, screen_number));

      if (pixmap)
        {
          XGCValues gcv;
          GC gc;

          gcv.graphics_exposures = False;
          gcv.subwindow_mode = IncludeInferiors;

          gc = XCreateGC (xdisplay, xroot,
                          GCGraphicsExposures | GCSubwindowMode,
                          &gcv);

          XCopyArea (xdisplay, xroot, pixmap, gc, 0, 0, width, height, 0, 0);
          XSync (xdisplay, False);

          XFreeGC (xdisplay, gc);

          free_pixmap = TRUE;
        }
    }

  if (!pixmap)
    {
      pixmap = XCreatePixmap (xdisplay, xroot, 1, 1,
                              DefaultDepth (xdisplay, screen_number));
      g_return_val_if_fail (pixmap != None, None);

      free_pixmap = TRUE;
      fill = TRUE;
    }

  if (xvisual == NULL)
    xvisual = DefaultVisual (xdisplay, screen_number);

  pa.repeat = TRUE;
  format = XRenderFindVisualFormat (xdisplay, xvisual);
  g_return_val_if_fail (format != NULL, None);

  picture = XRenderCreatePicture (xdisplay, pixmap, format, CPRepeat, &pa);
  if ((picture != None) && (fill))
    {
      XRenderColor c;

      /* Background default to just plain ugly grey */
      c.red = 0x8080;
      c.green = 0x8080;
      c.blue = 0x8080;
      c.alpha = 0xffff;

      XRenderFillRectangle (xdisplay, PictOpSrc, picture, &c, 0, 0, 1, 1);
    }

  if (free_pixmap)
    XFreePixmap (xdisplay, pixmap);

  return picture;
}

static void
paint_root (MetaCompositorXRender *self,
            Picture                root_buffer)
{
  MetaCompositorXRenderPrivate *priv;
  int width, height;

  priv = meta_compositor_xrender_get_instance_private (self);

  g_return_if_fail (root_buffer != None);
  g_return_if_fail (priv->root_tile != None);

  meta_screen_get_size (priv->screen, &width, &height);
  XRenderComposite (priv->xdisplay, PictOpSrc,
                    priv->root_tile, None, root_buffer,
                    0, 0, 0, 0, 0, 0, width, height);
}

static void
paint_dock_shadows (GList         *surfaces,
                    Picture        root_buffer,
                    XserverRegion  region)
{
  GList *l;

  for (l = surfaces; l != NULL; l = l->next)
    {
      MetaSurface *surface;
      MetaWindow *window;

      surface = META_SURFACE (l->data);

      window = meta_surface_get_window (surface);

      if (window->type == META_WINDOW_DOCK)
        {
          meta_surface_xrender_paint_shadow (META_SURFACE_XRENDER (surface),
                                             region,
                                             root_buffer);
        }
    }
}

static void
paint_windows (MetaCompositorXRender *self,
               GList                 *surfaces,
               Picture                root_buffer,
               XserverRegion          region)
{
  MetaCompositorXRenderPrivate *priv;
  MetaDisplay *display;
  Display *xdisplay;
  GList *index, *last;
  XserverRegion paint_region, desktop_region;

  priv = meta_compositor_xrender_get_instance_private (self);

  display = meta_screen_get_display (priv->screen);
  xdisplay = meta_display_get_xdisplay (display);

  paint_region = XFixesCreateRegion (xdisplay, NULL, 0);
  XFixesCopyRegion (xdisplay, paint_region, region);

  desktop_region = None;

  /*
   * Painting from top to bottom, reducing the clipping area at
   * each iteration. Only the opaque windows are painted 1st.
   */
  last = NULL;
  for (index = surfaces; index; index = index->next)
    {
      MetaSurface *surface;
      MetaWindow *window;

      /* Store the last window we dealt with */
      last = index;

      surface = META_SURFACE (index->data);

      window = meta_surface_get_window (surface);

      if (window->type == META_WINDOW_DESKTOP &&
          meta_surface_is_opaque (surface))
        {
          if (desktop_region == None)
            {
              desktop_region = XFixesCreateRegion (xdisplay, NULL, 0);
              XFixesCopyRegion (xdisplay, desktop_region, paint_region);
            }
          else
            {
              XFixesUnionRegion (xdisplay,
                                 desktop_region,
                                 desktop_region,
                                 paint_region);
            }
        }

      meta_surface_xrender_paint (META_SURFACE_XRENDER (surface),
                                  paint_region,
                                  root_buffer,
                                  TRUE);
    }

  XFixesSetPictureClipRegion (xdisplay, root_buffer, 0, 0, paint_region);
  paint_root (self, root_buffer);

  paint_dock_shadows (surfaces,
                      root_buffer,
                      desktop_region == None ? paint_region : desktop_region);

  if (desktop_region != None)
    XFixesDestroyRegion (xdisplay, desktop_region);

  /*
   * Painting from bottom to top, translucent windows and shadows are painted
   */
  for (index = last; index; index = index->prev)
    {
      MetaSurface *surface;
      MetaSurfaceXRender *surface_xrender;
      MetaWindow *window;

      surface = META_SURFACE (index->data);
      surface_xrender = META_SURFACE_XRENDER (surface);

      window = meta_surface_get_window (surface);

      if (window->type != META_WINDOW_DOCK)
        meta_surface_xrender_paint_shadow (surface_xrender, None, root_buffer);

      meta_surface_xrender_paint (surface_xrender, None, root_buffer, FALSE);
    }

  XFixesDestroyRegion (xdisplay, paint_region);
}

/* event processors must all be called with an error trap in place */
static void
process_property_notify (MetaCompositorXRender *self,
                         XPropertyEvent        *event)
{
  MetaCompositorXRenderPrivate *priv;
  MetaCompositor *compositor;
  MetaDisplay *display;
  Display *xdisplay;
  MetaScreen *screen;

  priv = meta_compositor_xrender_get_instance_private (self);

  compositor = META_COMPOSITOR (self);
  display = meta_compositor_get_display (compositor);
  xdisplay = meta_display_get_xdisplay (display);

  /* Check for the background property changing */
  if (event->atom == display->atom__XROOTPMAP_ID ||
      event->atom == display->atom__XSETROOT_ID)
    {
      screen = meta_display_get_screen (display);

      if (event->window == meta_screen_get_xroot (screen) &&
          priv->root_tile != None)
        {
          XClearArea (xdisplay, event->window, 0, 0, 0, 0, TRUE);
          XRenderFreePicture (xdisplay, priv->root_tile);
          priv->root_tile = None;

          /* Damage the whole screen as we may need to redraw the
           * background ourselves
           */
          meta_compositor_damage_screen (compositor);
          return;
        }
    }
}

static int
timeout_debug (MetaCompositorXRender *self)
{
  MetaCompositorXRenderPrivate *priv;

  priv = meta_compositor_xrender_get_instance_private (self);

  priv->show_redraw = (g_getenv ("METACITY_DEBUG_REDRAWS") != NULL);

  if (priv->show_redraw)
    priv->rand = g_rand_new ();

  return FALSE;
}

static void
update_shadows (MetaPreference pref,
                gpointer       data)
{
  GList *stack;
  GList *index;

  if (pref != META_PREF_THEME_TYPE)
    return;

  stack = meta_compositor_get_stack (META_COMPOSITOR (data));

  for (index = stack; index; index = index->next)
    meta_surface_xrender_update_shadow (META_SURFACE_XRENDER (index->data));
}

static void
meta_compositor_xrender_constructed (GObject *object)
{
  MetaCompositorXRender *self;
  MetaCompositorXRenderPrivate *priv;
  MetaCompositor *compositor;
  MetaDisplay *display;

  G_OBJECT_CLASS (meta_compositor_xrender_parent_class)->constructed (object);

  self = META_COMPOSITOR_XRENDER (object);
  priv = meta_compositor_xrender_get_instance_private (self);

  compositor = META_COMPOSITOR (self);
  display = meta_compositor_get_display (compositor);

  priv->xdisplay = meta_display_get_xdisplay (display);
}

static void
meta_compositor_xrender_finalize (GObject *object)
{
  MetaCompositorXRender *self;
  MetaCompositorXRenderPrivate *priv;
  MetaDisplay *display;
  Display *xdisplay;

  self = META_COMPOSITOR_XRENDER (object);
  priv = meta_compositor_xrender_get_instance_private (self);

  display = meta_compositor_get_display (META_COMPOSITOR (self));
  xdisplay = meta_display_get_xdisplay (display);

  if (priv->prefs_listener_added)
    {
      meta_prefs_remove_listener (update_shadows, self);
      priv->prefs_listener_added = FALSE;
    }

  if (priv->root_picture)
    XRenderFreePicture (xdisplay, priv->root_picture);

  META_COMPOSITOR_XRENDER_GET_CLASS (self)->free_root_buffers (self);

  if (priv->root_tile)
    {
      XRenderFreePicture (xdisplay, priv->root_tile);
      priv->root_tile = None;
    }

  if (priv->have_shadows)
    {
      int i;

      for (i = 0; i < LAST_SHADOW_TYPE; i++)
        {
          g_clear_pointer (&priv->shadows[i]->gaussian_map, g_free);
          g_clear_pointer (&priv->shadows[i]->shadow_corner, g_free);
          g_clear_pointer (&priv->shadows[i]->shadow_top, g_free);

          g_clear_pointer (&priv->shadows[i], g_free);
        }
    }

  g_clear_pointer (&priv->rand, g_rand_free);

  G_OBJECT_CLASS (meta_compositor_xrender_parent_class)->finalize (object);
}

static gboolean
meta_compositor_xrender_manage (MetaCompositor  *compositor,
                                GError         **error)
{
  MetaCompositorXRender *self;
  MetaCompositorXRenderPrivate *priv;
  MetaDisplay *display;
  MetaScreen *screen;
  Display *xdisplay;
  XRenderPictureAttributes pa;
  XRenderPictFormat *visual_format;
  int screen_number;

  self = META_COMPOSITOR_XRENDER (compositor);
  priv = meta_compositor_xrender_get_instance_private (self);

  display = meta_compositor_get_display (compositor);
  screen = meta_display_get_screen (display);
  xdisplay = meta_display_get_xdisplay (display);

  screen_number = meta_screen_get_screen_number (screen);

  if (!meta_compositor_check_common_extensions (compositor, error))
    return FALSE;

  if (!display->have_render)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Missing render extension required for compositing");

      return FALSE;
    }

  if (!meta_compositor_set_selection (compositor, error))
    return FALSE;

  if (!meta_compositor_redirect_windows (compositor, error))
    return FALSE;

  priv->screen = screen;

  visual_format = XRenderFindVisualFormat (xdisplay, DefaultVisual (xdisplay,
                                                                    screen_number));
  if (!visual_format)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Cannot find visual format on screen %i",
                   screen_number);

      return FALSE;
    }

  priv->overlay_window = meta_compositor_get_overlay_window (compositor);

  pa.subwindow_mode = IncludeInferiors;
  priv->root_picture = XRenderCreatePicture (xdisplay,
                                             priv->overlay_window,
                                             visual_format,
                                             CPSubwindowMode,
                                             &pa);

  if (priv->root_picture == None)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Cannot create root picture on screen %i",
                   screen_number);

      return FALSE;
    }

  priv->root_buffer = None;

  priv->root_tile = None;

  priv->have_shadows = (g_getenv("META_DEBUG_NO_SHADOW") == NULL);
  if (priv->have_shadows)
    {
      meta_verbose ("Enabling shadows\n");
      generate_shadows (self);
    }
  else
    meta_verbose ("Disabling shadows\n");

  XClearArea (xdisplay, priv->overlay_window, 0, 0, 0, 0, TRUE);

  meta_compositor_damage_screen (compositor);

  meta_prefs_add_listener (update_shadows, self);
  priv->prefs_listener_added = TRUE;

  g_timeout_add (2000, (GSourceFunc) timeout_debug, compositor);

  return TRUE;
}

static MetaSurface *
meta_compositor_xrender_add_window (MetaCompositor *compositor,
                                    MetaWindow     *window)
{
  MetaSurface *surface;

  surface = g_object_new (META_TYPE_SURFACE_XRENDER,
                          "compositor", compositor,
                          "window", window,
                          NULL);

  return surface;
}

static void
meta_compositor_xrender_process_event (MetaCompositor *compositor,
                                       XEvent         *event,
                                       MetaWindow     *window)
{
  MetaCompositorXRender *self;
  MetaDisplay *display;

  self = META_COMPOSITOR_XRENDER (compositor);

  display = meta_compositor_get_display (compositor);

  /*
   * This trap is so that none of the compositor functions cause
   * X errors. This is really a hack, but I'm afraid I don't understand
   * enough about Metacity/X to know how else you are supposed to do it
   */
  meta_error_trap_push (display);

  switch (event->type)
    {
    case PropertyNotify:
      process_property_notify (self, (XPropertyEvent *) event);
      break;

    default:
      break;
    }

  meta_error_trap_pop (display);
}

static void
meta_compositor_xrender_sync_screen_size (MetaCompositor *compositor)
{
  MetaCompositorXRender *self;

  self = META_COMPOSITOR_XRENDER (compositor);

  META_COMPOSITOR_XRENDER_GET_CLASS (self)->free_root_buffers (self);
  meta_compositor_damage_screen (compositor);
}

static void
meta_compositor_xrender_pre_paint (MetaCompositor *compositor)
{
  MetaCompositorXRender *self;
  MetaCompositorXRenderPrivate *priv;

  self = META_COMPOSITOR_XRENDER (compositor);
  priv = meta_compositor_xrender_get_instance_private (self);

  META_COMPOSITOR_XRENDER_GET_CLASS (self)->ensure_root_buffers (self);

  if (priv->root_tile == None)
    priv->root_tile = root_tile (priv->screen);

  META_COMPOSITOR_CLASS (meta_compositor_xrender_parent_class)->pre_paint (compositor);
}

static void
meta_compositor_xrender_redraw (MetaCompositor *compositor,
                                XserverRegion   all_damage)
{
  MetaCompositorXRender *self;
  MetaCompositorXRenderPrivate *priv;
  MetaDisplay *display;
  Display *xdisplay;
  int screen_width;
  int screen_height;

  self = META_COMPOSITOR_XRENDER (compositor);
  priv = meta_compositor_xrender_get_instance_private (self);

  display = meta_compositor_get_display (compositor);
  xdisplay = meta_display_get_xdisplay (display);

  meta_screen_get_size (priv->screen, &screen_width, &screen_height);

  meta_compositor_xrender_draw (self, priv->root_buffer, all_damage);

  XFixesSetPictureClipRegion (xdisplay, priv->root_buffer, 0, 0, all_damage);
  XRenderComposite (xdisplay, PictOpSrc, priv->root_buffer, None,
                    priv->root_picture, 0, 0, 0, 0, 0, 0,
                    screen_width, screen_height);
}

static void
meta_compositor_xrender_ensure_root_buffers (MetaCompositorXRender *self)
{
  MetaCompositorXRenderPrivate *priv;

  priv = meta_compositor_xrender_get_instance_private (self);

  if (priv->root_buffer == None)
    {
      Pixmap root_pixmap;

      root_pixmap = None;
      meta_compositor_xrender_create_root_buffer (self,
                                                  &root_pixmap,
                                                  &priv->root_buffer);

      if (root_pixmap != None)
        XFreePixmap (priv->xdisplay, root_pixmap);
    }
}

static void
meta_compositor_xrender_free_root_buffers (MetaCompositorXRender *self)
{
  MetaCompositorXRenderPrivate *priv;

  priv = meta_compositor_xrender_get_instance_private (self);

  if (priv->root_buffer)
    {
      XRenderFreePicture (priv->xdisplay, priv->root_buffer);
      priv->root_buffer = None;
    }
}

static void
meta_compositor_xrender_class_init (MetaCompositorXRenderClass *self_class)
{
  GObjectClass *object_class;
  MetaCompositorClass *compositor_class;

  object_class = G_OBJECT_CLASS (self_class);
  compositor_class = META_COMPOSITOR_CLASS (self_class);

  object_class->constructed = meta_compositor_xrender_constructed;
  object_class->finalize = meta_compositor_xrender_finalize;

  compositor_class->manage = meta_compositor_xrender_manage;
  compositor_class->add_window = meta_compositor_xrender_add_window;
  compositor_class->process_event = meta_compositor_xrender_process_event;
  compositor_class->sync_screen_size = meta_compositor_xrender_sync_screen_size;
  compositor_class->pre_paint = meta_compositor_xrender_pre_paint;
  compositor_class->redraw = meta_compositor_xrender_redraw;

  self_class->ensure_root_buffers = meta_compositor_xrender_ensure_root_buffers;
  self_class->free_root_buffers = meta_compositor_xrender_free_root_buffers;
}

static void
meta_compositor_xrender_init (MetaCompositorXRender *self)
{
  meta_compositor_set_composited (META_COMPOSITOR (self), TRUE);
}

MetaCompositor *
meta_compositor_xrender_new (MetaDisplay  *display,
                             GError      **error)
{
  return g_initable_new (META_TYPE_COMPOSITOR_XRENDER, NULL, error,
                         "display", display,
                         NULL);
}

gboolean
meta_compositor_xrender_have_shadows (MetaCompositorXRender *self)
{
  MetaCompositorXRenderPrivate *priv;

  priv = meta_compositor_xrender_get_instance_private (self);

  return priv->have_shadows;
}

MetaShadowXRender *
meta_compositor_xrender_create_shadow (MetaCompositorXRender *self,
                                       MetaSurface           *surface)
{
  MetaCompositorXRenderPrivate *priv;
  MetaWindow *window;
  MetaShadowType shadow_type;
  MetaFrameBorders borders;
  GtkBorder *invisible;
  double opacity;
  int width;
  int height;
  MetaShadowXRender *ret;
  cairo_region_t *frame_bounds;

  priv = meta_compositor_xrender_get_instance_private (self);

  window = meta_surface_get_window (surface);

  if (meta_window_appears_focused (window))
    shadow_type = META_SHADOW_LARGE;
  else
    shadow_type = META_SHADOW_MEDIUM;

  meta_frame_calc_borders (window->frame, &borders);
  invisible = &borders.invisible;

  opacity = SHADOW_OPACITY;
  if (window->opacity != OPAQUE)
    opacity = opacity * ((double) window->opacity) / OPAQUE;

  width = meta_surface_get_width (surface);
  height = meta_surface_get_height (surface);

  ret = g_new0 (MetaShadowXRender, 1);
  ret->xdisplay = priv->xdisplay;

  ret->dx = shadow_offsets_x[shadow_type] + invisible->left;
  ret->dy = shadow_offsets_y[shadow_type] + invisible->top;

  ret->black = solid_picture (priv->xdisplay, TRUE, 1, 0, 0, 0);
  ret->shadow = shadow_picture (self,
                                shadow_type,
                                opacity,
                                width - invisible->left - invisible->right,
                                height - invisible->top - invisible->bottom,
                                &ret->width,
                                &ret->height);

  ret->region = XFixesCreateRegion (priv->xdisplay, &(XRectangle) {
                                      .x = ret->dx,
                                      .y = ret->dy,
                                      .width = ret->width,
                                      .height = ret->height
                                    }, 1);

  frame_bounds = meta_window_get_frame_bounds (window);

  if (frame_bounds != NULL)
    {
      XserverRegion bounds_region;

      bounds_region = cairo_region_to_xserver_region (priv->xdisplay,
                                                      frame_bounds);

      XFixesSubtractRegion (priv->xdisplay,
                            ret->region,
                            ret->region,
                            bounds_region);

      XFixesDestroyRegion (priv->xdisplay, bounds_region);
    }

  return ret;
}

void
meta_compositor_xrender_create_root_buffer (MetaCompositorXRender *self,
                                            Pixmap                *pixmap,
                                            Picture               *buffer)
{
  MetaCompositorXRenderPrivate *priv;
  int screen_width;
  int screen_height;
  int screen_number;
  Visual *visual;
  XRenderPictFormat *format;

  g_return_if_fail (pixmap == NULL || *pixmap == None);
  g_return_if_fail (buffer == NULL || *buffer == None);

  priv = meta_compositor_xrender_get_instance_private (self);

  meta_screen_get_size (priv->screen, &screen_width, &screen_height);

  screen_number = meta_screen_get_screen_number (priv->screen);
  visual = DefaultVisual (priv->xdisplay, screen_number);
  format = XRenderFindVisualFormat (priv->xdisplay, visual);
  g_return_if_fail (format != NULL);

  *pixmap = XCreatePixmap (priv->xdisplay,
                           priv->overlay_window,
                           screen_width,
                           screen_height,
                           DefaultDepth (priv->xdisplay, screen_number));

  g_return_if_fail (*pixmap != None);

  *buffer = XRenderCreatePicture (priv->xdisplay, *pixmap, format, 0, NULL);
}

void
meta_compositor_xrender_draw (MetaCompositorXRender *self,
                              Picture                buffer,
                              XserverRegion          region)
{
  MetaCompositorXRenderPrivate *priv;
  MetaDisplay *display;
  Display *xdisplay;
  int screen_width;
  int screen_height;
  GList *stack;
  GList *visible_stack;
  GList *l;

  priv = meta_compositor_xrender_get_instance_private (self);

  display = meta_compositor_get_display (META_COMPOSITOR (self));
  xdisplay = meta_display_get_xdisplay (display);

  meta_screen_get_size (priv->screen, &screen_width, &screen_height);

  /* Set clipping to the given region */
  XFixesSetPictureClipRegion (xdisplay, priv->root_picture, 0, 0, region);

  if (priv->show_redraw)
    {
      Picture overlay;

      /* Make a random colour overlay */
      overlay = solid_picture (xdisplay, TRUE, 1, /* 0.3, alpha */
                               g_rand_double (priv->rand),
                               g_rand_double (priv->rand),
                               g_rand_double (priv->rand));

      XRenderComposite (xdisplay, PictOpOver, overlay, None, priv->root_picture,
                        0, 0, 0, 0, 0, 0, screen_width, screen_height);
      XRenderFreePicture (xdisplay, overlay);
      XFlush (xdisplay);
      usleep (100 * 1000);
    }

  stack = meta_compositor_get_stack (META_COMPOSITOR (self));
  visible_stack = NULL;

  for (l = stack; l != NULL; l = l->next)
    {
      if (meta_surface_is_visible (META_SURFACE (l->data)))
        visible_stack = g_list_prepend (visible_stack, l->data);
    }

  visible_stack = g_list_reverse (visible_stack);
  paint_windows (self, visible_stack, buffer, region);
  g_list_free (visible_stack);
}
