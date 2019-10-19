/*
 * Copyright (C) 2007 Iain Holmes
 * Copyright (C) 2017-2019 Alberts Muktupāvels
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

#define WINDOW_SOLID 0
#define WINDOW_ARGB 1

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

typedef struct _MetaCompWindow
{
  MetaWindow *window;

  MetaRectangle rect;

  int mode;

  gboolean needs_shadow;
  MetaShadowType shadow_type;

  XserverRegion extents;

  Picture shadow;
  int shadow_dx;
  int shadow_dy;
  int shadow_width;
  int shadow_height;
} MetaCompWindow;

struct _MetaCompositorXRender
{
  MetaCompositor  parent;

  Display        *xdisplay;

  MetaScreen     *screen;
  GHashTable     *windows_by_xid;

  Window          overlay_window;

  gboolean        have_shadows;
  shadow         *shadows[LAST_SHADOW_TYPE];

  Picture         root_picture;
  Picture         root_buffer;
  Picture         black_picture;
  Picture         root_tile;

  gboolean        prefs_listener_added;

  guint           show_redraw : 1;
};

G_DEFINE_TYPE (MetaCompositorXRender, meta_compositor_xrender, META_TYPE_COMPOSITOR)

static Visual *
get_toplevel_xvisual (MetaWindow *window)
{
  if (window->frame != NULL)
    return meta_frame_get_xvisual (window->frame);

  return window->xvisual;
}

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
generate_shadows (MetaCompositorXRender *xrender)
{
  double radii[LAST_SHADOW_TYPE] = {SHADOW_MEDIUM_RADIUS,
                                    SHADOW_LARGE_RADIUS};
  int i;

  for (i = 0; i < LAST_SHADOW_TYPE; i++) {
    shadow *shad = g_new0 (shadow, 1);

    shad->gaussian_map = make_gaussian_map (radii[i]);
    presum_gaussian (shad);

    xrender->shadows[i] = shad;
  }
}

static XImage *
make_shadow (MetaCompositorXRender *xrender,
             MetaShadowType         shadow_type,
             double                 opacity,
             int                    width,
             int                    height)
{
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

  shad = xrender->shadows[shadow_type];
  msize = shad->gaussian_map->size;
  swidth = width + msize;
  sheight = height + msize;
  centre = msize / 2;

  data = g_malloc (swidth * sheight * sizeof (guchar));

  xdisplay = xrender->xdisplay;
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

static void
shadow_picture_clip (Display          *xdisplay,
                     Picture           shadow_picture,
                     MetaCompWindow   *cw,
                     MetaFrameBorders  borders,
                     int               width,
                     int               height)
{
  int shadow_dx;
  int shadow_dy;
  cairo_region_t *visible_region;
  XRectangle rect;
  XserverRegion region1;
  XserverRegion region2;

  visible_region = meta_window_get_frame_bounds (cw->window);

  if (!visible_region)
    return;

  shadow_dx = -1 * (int) shadow_offsets_x [cw->shadow_type] - borders.invisible.left;
  shadow_dy = -1 * (int) shadow_offsets_y [cw->shadow_type] - borders.invisible.top;

  rect.x = 0;
  rect.y = 0;
  rect.width = width;
  rect.height = height;

  region1 = XFixesCreateRegion (xdisplay, &rect, 1);
  region2 = cairo_region_to_xserver_region (xdisplay, visible_region);

  XFixesTranslateRegion (xdisplay, region2,
                         shadow_dx, shadow_dy);

  XFixesSubtractRegion (xdisplay, region1, region1, region2);
  XFixesSetPictureClipRegion (xdisplay, shadow_picture, 0, 0, region1);

  XFixesDestroyRegion (xdisplay, region1);
  XFixesDestroyRegion (xdisplay, region2);
}

static Picture
shadow_picture (MetaCompositorXRender *xrender,
                MetaCompWindow        *cw,
                double                 opacity,
                MetaFrameBorders       borders,
                int                    width,
                int                    height,
                int                   *wp,
                int                   *hp)
{
  Display *xdisplay;
  XImage *shadow_image;
  Pixmap shadow_pixmap;
  Picture shadow_picture;
  GC gc;

  shadow_image = make_shadow (xrender, cw->shadow_type, opacity, width, height);

  if (!shadow_image)
    return None;

  xdisplay = xrender->xdisplay;
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

  shadow_picture_clip (xdisplay, shadow_picture, cw, borders,
                       shadow_image->width, shadow_image->height);

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

static MetaCompWindow *
find_comp_window_by_xwindow (MetaCompositorXRender *xrender,
                             Window                 xwindow)
{
  GHashTableIter iter;
  MetaCompWindow *cw;

  g_hash_table_iter_init (&iter, xrender->windows_by_xid);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer) &cw))
    {
      MetaFrame *frame;

      frame = meta_window_get_frame (cw->window);

      if (frame)
        {
          if (meta_frame_get_xwindow (frame) == xwindow)
            return cw;
        }
      else
        {
          if (meta_window_get_xwindow (cw->window) == xwindow)
            return cw;
        }
    }

  return NULL;
}

static MetaCompWindow *
find_comp_window_by_window (MetaCompositorXRender *xrender,
                            MetaWindow            *window)
{
  Window xwindow;

  xwindow = meta_window_get_xwindow (window);

  return g_hash_table_lookup (xrender->windows_by_xid, (gpointer) xwindow);
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

static Picture
root_tile (MetaScreen *screen)
{
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  Picture picture;
  Pixmap pixmap;
  gboolean free_pixmap;
  gboolean fill;
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
              memcpy (&pixmap, prop, 4);
              XFree (prop);
              break;
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

  pa.repeat = TRUE;
  format = XRenderFindVisualFormat (xdisplay, DefaultVisual (xdisplay,
                                                             screen_number));
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

static Picture
create_root_buffer (MetaCompositorXRender *xrender)
{
  Display *xdisplay = xrender->xdisplay;
  Picture pict;
  XRenderPictFormat *format;
  Pixmap root_pixmap;
  Visual *visual;
  int depth, screen_width, screen_height, screen_number;

  meta_screen_get_size (xrender->screen, &screen_width, &screen_height);
  screen_number = meta_screen_get_screen_number (xrender->screen);
  visual = DefaultVisual (xdisplay, screen_number);
  depth = DefaultDepth (xdisplay, screen_number);

  format = XRenderFindVisualFormat (xdisplay, visual);
  g_return_val_if_fail (format != NULL, None);

  root_pixmap = XCreatePixmap (xdisplay, xrender->overlay_window,
                               screen_width, screen_height, depth);

  g_return_val_if_fail (root_pixmap != None, None);

  pict = XRenderCreatePicture (xdisplay, root_pixmap, format, 0, NULL);
  XFreePixmap (xdisplay, root_pixmap);

  return pict;
}

static void
paint_root (MetaCompositorXRender *xrender,
            Picture                root_buffer)
{
  int width, height;

  g_return_if_fail (root_buffer != None);

  if (xrender->root_tile == None)
    {
      xrender->root_tile = root_tile (xrender->screen);
      g_return_if_fail (xrender->root_tile != None);
    }

  meta_screen_get_size (xrender->screen, &width, &height);
  XRenderComposite (xrender->xdisplay, PictOpSrc,
                    xrender->root_tile, None, root_buffer,
                    0, 0, 0, 0, 0, 0, width, height);
}

static gboolean
window_has_shadow (MetaCompositorXRender *xrender,
                   MetaCompWindow        *cw)
{
  if (xrender->have_shadows == FALSE)
    return FALSE;

  /* Do not add shadows to client side decorated windows */
  if (meta_window_is_client_decorated (cw->window))
    {
      meta_verbose ("Window might have shadow because it is client side decorated\n");
      return FALSE;
    }

  /* Do not add shadows to fullscreen windows */
  if (meta_window_is_fullscreen (cw->window))
    {
      meta_verbose ("Window has no shadow because it is fullscreen\n");
      return FALSE;
    }

  /* Do not add shadows to maximized windows */
  if (meta_window_is_maximized (cw->window))
    {
      meta_verbose ("Window has no shadow because it is maximized\n");
      return FALSE;
    }

  /* Add shadows to windows with frame */
  if (meta_window_get_frame (cw->window))
    {
      /* Do not add shadows if GTK+ theme is used */
      if (meta_prefs_get_theme_type () == META_THEME_TYPE_GTK)
        {
          meta_verbose ("Window might have shadow from GTK+ theme\n");
          return FALSE;
        }

      meta_verbose ("Window has shadow because it has a frame\n");
      return TRUE;
    }

  /* Do not add shadows to ARGB windows */
  if (cw->mode == WINDOW_ARGB)
    {
      meta_verbose ("Window has no shadow as it is ARGB\n");
      return FALSE;
    }

  /* Never put a shadow around shaped windows */
  if (cw->window->shape_region != None)
    {
      meta_verbose ("Window has no shadow as it is shaped\n");
      return FALSE;
    }

  /* Don't put shadow around DND icon windows */
  if (cw->window->type == META_WINDOW_DND ||
      cw->window->type == META_WINDOW_DESKTOP) {
    meta_verbose ("Window has no shadow as it is DND or Desktop\n");
    return FALSE;
  }

  if (cw->mode != WINDOW_ARGB) {
    meta_verbose ("Window has shadow as it is not ARGB\n");
    return TRUE;
  }

  if (cw->window->type == META_WINDOW_MENU ||
      cw->window->type == META_WINDOW_DROPDOWN_MENU) {
    meta_verbose ("Window has shadow as it is a menu\n");
    return TRUE;
  }

  if (cw->window->type == META_WINDOW_TOOLTIP) {
    meta_verbose ("Window has shadow as it is a tooltip\n");
    return TRUE;
  }

  meta_verbose ("Window has no shadow as it fell through\n");
  return FALSE;
}

static XserverRegion
win_extents (MetaCompositorXRender *xrender,
             MetaCompWindow        *cw)
{
  XRectangle r;
  MetaFrame *frame;
  MetaFrameBorders borders;
  XRectangle sr;

  if (!cw->needs_shadow)
    return None;

  r.x = cw->rect.x;
  r.y = cw->rect.y;
  r.width = cw->rect.width;
  r.height = cw->rect.height;

  frame = meta_window_get_frame (cw->window);
  meta_frame_calc_borders (frame, &borders);

  cw->shadow_dx = (int) shadow_offsets_x [cw->shadow_type] + borders.invisible.left;
  cw->shadow_dy = (int) shadow_offsets_y [cw->shadow_type] + borders.invisible.top;

  if (!cw->shadow)
    {
      double opacity = SHADOW_OPACITY;
      int invisible_width = borders.invisible.left + borders.invisible.right;
      int invisible_height = borders.invisible.top + borders.invisible.bottom;

      if (cw->window->opacity != (guint) OPAQUE)
        opacity = opacity * ((double) cw->window->opacity) / ((double) OPAQUE);

      cw->shadow = shadow_picture (xrender, cw, opacity, borders,
                                   cw->rect.width - invisible_width,
                                   cw->rect.height - invisible_height,
                                   &cw->shadow_width, &cw->shadow_height);
    }

  sr.x = cw->rect.x + cw->shadow_dx;
  sr.y = cw->rect.y + cw->shadow_dy;
  sr.width = cw->shadow_width;
  sr.height = cw->shadow_height;

  if (sr.x < r.x)
    {
      r.width = (r.x + r.width) - sr.x;
      r.x = sr.x;
    }

  if (sr.y < r.y)
    {
      r.height = (r.y + r.height) - sr.y;
      r.y = sr.y;
    }

  if (sr.x + sr.width > r.x + r.width)
    r.width = sr.x + sr.width - r.x;

  if (sr.y + sr.height > r.y + r.height)
    r.height = sr.y + sr.height - r.y;

  return XFixesCreateRegion (xrender->xdisplay, &r, 1);
}

static void
paint_dock_shadows (MetaCompositorXRender *xrender,
                    GList                 *surfaces,
                    Picture                root_buffer,
                    XserverRegion          region)
{
  Display *xdisplay = xrender->xdisplay;
  GList *l;

  for (l = surfaces; l != NULL; l = l->next)
    {
      MetaSurfaceXRender *surface;
      MetaCompWindow *cw;
      XserverRegion shadow_clip;

      surface = META_SURFACE_XRENDER (l->data);
      cw = g_object_get_data (G_OBJECT (surface), "cw");

      if (cw->window->type == META_WINDOW_DOCK &&
          cw->needs_shadow && cw->shadow)
        {
          XserverRegion border_clip;

          shadow_clip = XFixesCreateRegion (xdisplay, NULL, 0);
          border_clip = meta_surface_xrender_get_border_clip (surface);

          XFixesIntersectRegion (xdisplay, shadow_clip,
                                 border_clip, region);

          XFixesSetPictureClipRegion (xdisplay, root_buffer, 0, 0, shadow_clip);

          XRenderComposite (xdisplay, PictOpOver, xrender->black_picture,
                            cw->shadow, root_buffer,
                            0, 0, 0, 0,
                            cw->rect.x + cw->shadow_dx,
                            cw->rect.y + cw->shadow_dy,
                            cw->shadow_width, cw->shadow_height);
          XFixesDestroyRegion (xdisplay, shadow_clip);
        }
    }
}

static void
paint_windows (MetaCompositorXRender *xrender,
               GList                 *surfaces,
               Picture                root_buffer,
               XserverRegion          region)
{
  MetaDisplay *display = meta_screen_get_display (xrender->screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  GList *index, *last;
  MetaCompWindow *cw;
  XserverRegion paint_region, desktop_region;

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
      MetaSurfaceXRender *surface;

      /* Store the last window we dealt with */
      last = index;

      surface = META_SURFACE_XRENDER (index->data);
      cw = g_object_get_data (G_OBJECT (surface), "cw");

      if (!meta_surface_is_visible (META_SURFACE (surface)))
        continue;

      meta_surface_xrender_paint (surface, paint_region, root_buffer, TRUE);

      if (cw->mode == WINDOW_SOLID)
        {
          if (cw->window->type == META_WINDOW_DESKTOP)
            {
              desktop_region = XFixesCreateRegion (xdisplay, 0, 0);
              XFixesCopyRegion (xdisplay, desktop_region, paint_region);
            }
        }
    }

  XFixesSetPictureClipRegion (xdisplay, root_buffer, 0, 0, paint_region);
  paint_root (xrender, root_buffer);

  paint_dock_shadows (xrender, surfaces, root_buffer,
                      desktop_region == None ? paint_region : desktop_region);

  if (desktop_region != None)
    XFixesDestroyRegion (xdisplay, desktop_region);

  /*
   * Painting from bottom to top, translucent windows and shadows are painted
   */
  for (index = last; index; index = index->prev)
    {
      MetaSurfaceXRender *surface;

      surface = META_SURFACE_XRENDER (index->data);
      cw = g_object_get_data (G_OBJECT (surface), "cw");

      if (meta_surface_xrender_get_picture (surface) != None)
        {
          int x, y;

          x = cw->rect.x;
          y = cw->rect.y;

          if (cw->shadow && cw->window->type != META_WINDOW_DOCK)
            {
              XserverRegion border_clip;

              border_clip = meta_surface_xrender_get_border_clip (surface);

              XFixesSetPictureClipRegion (xdisplay, root_buffer, 0, 0,
                                          border_clip);

              XRenderComposite (xdisplay, PictOpOver, xrender->black_picture,
                                cw->shadow, root_buffer, 0, 0, 0, 0,
                                x + cw->shadow_dx, y + cw->shadow_dy,
                                cw->shadow_width, cw->shadow_height);
            }

          meta_surface_xrender_paint (surface, paint_region, root_buffer, FALSE);
        }
    }

  XFixesDestroyRegion (xdisplay, paint_region);
}

static void
paint_all (MetaCompositorXRender *xrender,
           XserverRegion          region)
{
  MetaDisplay *display = meta_screen_get_display (xrender->screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  int screen_width, screen_height;
  GList *stack;

  /* Set clipping to the given region */
  XFixesSetPictureClipRegion (xdisplay, xrender->root_picture, 0, 0, region);

  meta_screen_get_size (xrender->screen, &screen_width, &screen_height);

  if (xrender->show_redraw)
    {
      Picture overlay;

      /* Make a random colour overlay */
      overlay = solid_picture (xdisplay, TRUE, 1, /* 0.3, alpha */
                               ((double) (rand () % 100)) / 100.0,
                               ((double) (rand () % 100)) / 100.0,
                               ((double) (rand () % 100)) / 100.0);

      XRenderComposite (xdisplay, PictOpOver, overlay, None, xrender->root_picture,
                        0, 0, 0, 0, 0, 0, screen_width, screen_height);
      XRenderFreePicture (xdisplay, overlay);
      XFlush (xdisplay);
      usleep (100 * 1000);
    }

  stack = meta_compositor_get_stack (META_COMPOSITOR (xrender));
  paint_windows (xrender, stack, xrender->root_buffer, region);

  XFixesSetPictureClipRegion (xdisplay, xrender->root_buffer, 0, 0, region);
  XRenderComposite (xdisplay, PictOpSrc, xrender->root_buffer, None,
                    xrender->root_picture, 0, 0, 0, 0, 0, 0,
                    screen_width, screen_height);
}

static void
cw_destroy_cb (gpointer data)
{
  MetaCompWindow *cw;
  MetaDisplay *display;
  Display *xdisplay;

  cw = (MetaCompWindow *) data;

  display = meta_window_get_display (cw->window);
  xdisplay = meta_display_get_xdisplay (display);

  if (cw->shadow)
    {
      XRenderFreePicture (xdisplay, cw->shadow);
      cw->shadow = None;
    }

  if (cw->extents)
    {
      XFixesDestroyRegion (xdisplay, cw->extents);
      cw->extents = None;
    }

  g_free (cw);
}

static void
determine_mode (MetaCompositorXRender *xrender,
                MetaCompWindow        *cw)
{
  XRenderPictFormat *format;
  MetaCompositor *compositor = META_COMPOSITOR (xrender);
  MetaDisplay *display = meta_compositor_get_display (compositor);
  Display *xdisplay = meta_display_get_xdisplay (display);

  format = XRenderFindVisualFormat (xdisplay, get_toplevel_xvisual (cw->window));

  if ((format && format->type == PictTypeDirect && format->direct.alphaMask)
      || cw->window->opacity != (guint) OPAQUE)
    cw->mode = WINDOW_ARGB;
  else
    cw->mode = WINDOW_SOLID;
}

static void
notify_appears_focused_cb (MetaWindow            *window,
                           GParamSpec            *pspec,
                           MetaCompositorXRender *xrender)
{
  MetaCompositor *compositor;
  MetaCompWindow *cw;
  Display *xdisplay;

  compositor = META_COMPOSITOR (xrender);
  cw = find_comp_window_by_window (xrender, window);

  if (cw == NULL)
    return;

  xdisplay = window->display->xdisplay;

  if (meta_window_appears_focused (window))
    cw->shadow_type = META_SHADOW_LARGE;
  else
    cw->shadow_type = META_SHADOW_MEDIUM;

  if (cw->shadow)
    {
      XRenderFreePicture (xdisplay, cw->shadow);
      cw->shadow = None;
    }

  if (cw->extents)
    {
      meta_compositor_add_damage (compositor,
                                  "notify_appears_focused_cb",
                                  cw->extents);

      XFixesDestroyRegion (xdisplay, cw->extents);
      cw->extents = None;
    }
}

static void
notify_decorated_cb (MetaWindow            *window,
                     GParamSpec            *pspec,
                     MetaCompositorXRender *xrender)
{
  MetaCompositor *compositor;
  MetaCompWindow *cw;

  compositor = META_COMPOSITOR (xrender);
  cw = find_comp_window_by_window (xrender, window);

  if (cw == NULL)
    return;

  if (cw->extents != None)
    {
      meta_compositor_add_damage (compositor, "notify_decorated_cb", cw->extents);
      XFixesDestroyRegion (xrender->xdisplay, cw->extents);
      cw->extents = None;
    }

  if (cw->shadow != None)
    {
      XRenderFreePicture (xrender->xdisplay, cw->shadow);
      cw->shadow = None;
    }

  determine_mode (xrender, cw);
  cw->needs_shadow = window_has_shadow (xrender, cw);

  meta_compositor_queue_redraw (compositor);
}

static void
notify_window_type_cb (MetaWindow            *window,
                       GParamSpec            *pspec,
                       MetaCompositorXRender *xrender)
{
  MetaCompositor *compositor;
  MetaCompWindow *cw;

  compositor = META_COMPOSITOR (xrender);
  cw = find_comp_window_by_window (xrender, window);

  if (cw == NULL)
    return;

  if (cw->extents != None)
    {
      meta_compositor_add_damage (compositor, "notify_window_type_cb", cw->extents);
      XFixesDestroyRegion (xrender->xdisplay, cw->extents);
      cw->extents = None;
    }

  if (cw->shadow != None)
    {
      XRenderFreePicture (xrender->xdisplay, cw->shadow);
      cw->shadow = None;
    }

  cw->needs_shadow = window_has_shadow (xrender, cw);

  meta_compositor_queue_redraw (compositor);
}

/* event processors must all be called with an error trap in place */
static void
process_property_notify (MetaCompositorXRender *xrender,
                         XPropertyEvent        *event)
{
  MetaCompositor *compositor = META_COMPOSITOR (xrender);
  MetaDisplay *display = meta_compositor_get_display (compositor);
  Display *xdisplay = meta_display_get_xdisplay (display);
  MetaScreen *screen;

  /* Check for the background property changing */
  if (event->atom == display->atom__XROOTPMAP_ID ||
      event->atom == display->atom__XSETROOT_ID)
    {
      screen = meta_display_get_screen (display);

      if (event->window == meta_screen_get_xroot (screen) &&
          xrender->root_tile != None)
        {
          XClearArea (xdisplay, event->window, 0, 0, 0, 0, TRUE);
          XRenderFreePicture (xdisplay, xrender->root_tile);
          xrender->root_tile = None;

          /* Damage the whole screen as we may need to redraw the
           * background ourselves
           */
          meta_compositor_damage_screen (compositor);
          return;
        }
    }
}

static void
expose_area (MetaCompositorXRender *xrender,
             XRectangle            *rects,
             int                    nrects)
{
  XserverRegion region;

  region = XFixesCreateRegion (xrender->xdisplay, rects, nrects);

  meta_compositor_add_damage (META_COMPOSITOR (xrender), "expose_area", region);
  XFixesDestroyRegion (xrender->xdisplay, region);
}

static void
process_expose (MetaCompositorXRender *xrender,
                XExposeEvent          *event)
{
  MetaCompWindow *cw = find_comp_window_by_xwindow (xrender, event->window);
  XRectangle rect[1];
  int origin_x = 0, origin_y = 0;

  if (cw != NULL)
    {
      origin_x = cw->rect.x;
      origin_y = cw->rect.y;
    }

  rect[0].x = event->x + origin_x;
  rect[0].y = event->y + origin_y;
  rect[0].width = event->width;
  rect[0].height = event->height;

  expose_area (xrender, rect, 1);
}

static int
timeout_debug (MetaCompositorXRender *compositor)
{
  compositor->show_redraw = (g_getenv ("METACITY_DEBUG_REDRAWS") != NULL);

  return FALSE;
}

static void
update_shadows (MetaPreference pref,
                gpointer       data)
{
  MetaCompositorXRender *xrender;
  GList *stack;
  GList *index;

  if (pref != META_PREF_THEME_TYPE)
    return;

  xrender = META_COMPOSITOR_XRENDER (data);
  stack = meta_compositor_get_stack (META_COMPOSITOR (data));

  for (index = stack; index; index = index->next)
    {
      MetaSurface *surface;
      MetaCompWindow *cw;

      surface = META_SURFACE (index->data);
      cw = g_object_get_data (G_OBJECT (surface), "cw");

      if (cw->shadow != None)
        {
          XRenderFreePicture (xrender->xdisplay, cw->shadow);
          cw->shadow = None;
        }

      cw->needs_shadow = window_has_shadow (xrender, cw);
    }
}

static void
meta_compositor_xrender_constructed (GObject *object)
{
  MetaCompositor *compositor;
  MetaCompositorXRender *xrender;
  MetaDisplay *display;

  G_OBJECT_CLASS (meta_compositor_xrender_parent_class)->constructed (object);

  compositor = META_COMPOSITOR (object);
  xrender = META_COMPOSITOR_XRENDER (object);
  display = meta_compositor_get_display (compositor);

  xrender->xdisplay = meta_display_get_xdisplay (display);
}

static void
meta_compositor_xrender_finalize (GObject *object)
{
  MetaCompositorXRender *xrender;
  MetaDisplay *display;
  Display *xdisplay;

  xrender = META_COMPOSITOR_XRENDER (object);
  display = meta_compositor_get_display (META_COMPOSITOR (xrender));
  xdisplay = meta_display_get_xdisplay (display);

  if (xrender->prefs_listener_added)
    {
      meta_prefs_remove_listener (update_shadows, xrender);
      xrender->prefs_listener_added = FALSE;
    }

  /* Destroy the windows */
  g_clear_pointer (&xrender->windows_by_xid, g_hash_table_destroy);

  if (xrender->root_picture)
    XRenderFreePicture (xdisplay, xrender->root_picture);

  if (xrender->black_picture)
    XRenderFreePicture (xdisplay, xrender->black_picture);

  if (xrender->have_shadows)
    {
      int i;

      for (i = 0; i < LAST_SHADOW_TYPE; i++)
        {
          g_clear_pointer (&xrender->shadows[i]->gaussian_map, g_free);
          g_clear_pointer (&xrender->shadows[i]->shadow_corner, g_free);
          g_clear_pointer (&xrender->shadows[i]->shadow_top, g_free);

          g_clear_pointer (&xrender->shadows[i], g_free);
        }
    }

  G_OBJECT_CLASS (meta_compositor_xrender_parent_class)->finalize (object);
}

static gboolean
meta_compositor_xrender_manage (MetaCompositor  *compositor,
                                GError         **error)
{
  MetaCompositorXRender *xrender = META_COMPOSITOR_XRENDER (compositor);
  MetaDisplay *display = meta_compositor_get_display (compositor);
  MetaScreen *screen = meta_display_get_screen (display);
  Display *xdisplay = meta_display_get_xdisplay (display);
  XRenderPictureAttributes pa;
  XRenderPictFormat *visual_format;
  int screen_number = meta_screen_get_screen_number (screen);

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

  xrender->screen = screen;

  visual_format = XRenderFindVisualFormat (xdisplay, DefaultVisual (xdisplay,
                                                                    screen_number));
  if (!visual_format)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Cannot find visual format on screen %i",
                   screen_number);

      return FALSE;
    }

  xrender->overlay_window = meta_compositor_get_overlay_window (compositor);

  pa.subwindow_mode = IncludeInferiors;
  xrender->root_picture = XRenderCreatePicture (xdisplay,
                                                xrender->overlay_window,
                                                visual_format,
                                                CPSubwindowMode, &pa);

  if (xrender->root_picture == None)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Cannot create root picture on screen %i",
                   screen_number);

      return FALSE;
    }

  xrender->root_buffer = None;
  xrender->black_picture = solid_picture (xdisplay, TRUE, 1, 0, 0, 0);

  xrender->root_tile = None;

  xrender->windows_by_xid = g_hash_table_new (g_direct_hash, g_direct_equal);

  xrender->have_shadows = (g_getenv("META_DEBUG_NO_SHADOW") == NULL);
  if (xrender->have_shadows)
    {
      meta_verbose ("Enabling shadows\n");
      generate_shadows (xrender);
    }
  else
    meta_verbose ("Disabling shadows\n");

  XClearArea (xdisplay, xrender->overlay_window, 0, 0, 0, 0, TRUE);

  meta_compositor_damage_screen (compositor);

  meta_prefs_add_listener (update_shadows, xrender);
  xrender->prefs_listener_added = TRUE;

  g_timeout_add (2000, (GSourceFunc) timeout_debug, compositor);

  return TRUE;
}

static MetaSurface *
meta_compositor_xrender_add_window (MetaCompositor *compositor,
                                    MetaWindow     *window)
{
  MetaCompositorXRender *xrender;
  MetaDisplay *display;
  MetaSurface *surface;
  MetaCompWindow *cw;
  Window xwindow;

  xrender = META_COMPOSITOR_XRENDER (compositor);
  display = meta_compositor_get_display (compositor);

  meta_error_trap_push (display);

  surface = g_object_new (META_TYPE_SURFACE_XRENDER,
                          "compositor", compositor,
                          "window", window,
                          NULL);

  cw = g_new0 (MetaCompWindow, 1);
  cw->window = window;

  g_object_set_data_full (G_OBJECT (surface), "cw", cw, cw_destroy_cb);

  meta_window_get_input_rect (window, &cw->rect);

  g_signal_connect_object (window, "notify::appears-focused",
                           G_CALLBACK (notify_appears_focused_cb),
                           xrender, 0);

  g_signal_connect_object (window, "notify::decorated",
                           G_CALLBACK (notify_decorated_cb),
                           xrender, 0);

  g_signal_connect_object (window, "notify::window-type",
                           G_CALLBACK (notify_window_type_cb),
                           xrender, 0);

  cw->extents = None;
  cw->shadow = None;
  cw->shadow_dx = 0;
  cw->shadow_dy = 0;
  cw->shadow_width = 0;
  cw->shadow_height = 0;

  if (meta_window_appears_focused (window))
    cw->shadow_type = META_SHADOW_LARGE;
  else
    cw->shadow_type = META_SHADOW_MEDIUM;

  determine_mode (xrender, cw);
  cw->needs_shadow = window_has_shadow (xrender, cw);

  xwindow = meta_window_get_xwindow (window);
  g_hash_table_insert (xrender->windows_by_xid, (gpointer) xwindow, cw);

  meta_error_trap_pop (display);

  return surface;
}

static void
meta_compositor_xrender_remove_window (MetaCompositor *compositor,
                                       MetaWindow     *window)
{
  MetaCompositorXRender *xrender;
  MetaCompWindow *cw;
  Window xwindow;

  xrender = META_COMPOSITOR_XRENDER (compositor);

  cw = find_comp_window_by_window (xrender, window);
  if (cw == NULL)
    return;

  if (cw->extents != None)
    {
      meta_compositor_add_damage (compositor, "remove_window", cw->extents);
      XFixesDestroyRegion (xrender->xdisplay, cw->extents);
      cw->extents = None;
    }

  xwindow = meta_window_get_xwindow (window);
  g_hash_table_remove (xrender->windows_by_xid, (gpointer) xwindow);
}

static void
meta_compositor_xrender_hide_window (MetaCompositor *compositor,
                                     MetaSurface    *surface,
                                     MetaEffectType  effect)
{
  MetaCompositorXRender *xrender;
  MetaCompWindow *cw;

  xrender = META_COMPOSITOR_XRENDER (compositor);

  cw = g_object_get_data (G_OBJECT (surface), "cw");

  if (cw->extents != None)
    {
      meta_compositor_add_damage (compositor, "hide_window", cw->extents);
      XFixesDestroyRegion (xrender->xdisplay, cw->extents);
      cw->extents = None;
    }

  if (cw->shadow)
    {
      XRenderFreePicture (xrender->xdisplay, cw->shadow);
      cw->shadow = None;
    }
}

static void
meta_compositor_xrender_window_opacity_changed (MetaCompositor *compositor,
                                                MetaSurface    *surface)
{
  MetaCompositorXRender *xrender;
  MetaCompWindow *cw;

  xrender = META_COMPOSITOR_XRENDER (compositor);

  cw = g_object_get_data (G_OBJECT (surface), "cw");

  determine_mode (xrender, cw);
  cw->needs_shadow = window_has_shadow (xrender, cw);

  if (cw->shadow)
    {
      XRenderFreePicture (xrender->xdisplay, cw->shadow);
      cw->shadow = None;
    }

  if (cw->extents)
    {
      meta_compositor_add_damage (compositor,
                                  "window_opacity_changed",
                                  cw->extents);

      XFixesDestroyRegion (xrender->xdisplay, cw->extents);
      cw->extents = None;
    }
}

static void
meta_compositor_xrender_process_event (MetaCompositor *compositor,
                                       XEvent         *event,
                                       MetaWindow     *window)
{
  MetaCompositorXRender *xrender;
  MetaDisplay *display;

  xrender = META_COMPOSITOR_XRENDER (compositor);
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
      process_property_notify (xrender, (XPropertyEvent *) event);
      break;

    case Expose:
      process_expose (xrender, (XExposeEvent *) event);
      break;

    default:
      break;
    }

  meta_error_trap_pop (display);
}

static void
meta_compositor_xrender_sync_screen_size (MetaCompositor *compositor)
{
  MetaCompositorXRender *xrender;

  xrender = META_COMPOSITOR_XRENDER (compositor);

  if (xrender->root_buffer)
    {
      XRenderFreePicture (xrender->xdisplay, xrender->root_buffer);
      xrender->root_buffer = None;
    }

  meta_compositor_damage_screen (compositor);
}

static void
meta_compositor_xrender_sync_window_geometry (MetaCompositor *compositor,
                                              MetaSurface    *surface)
{
  MetaCompositorXRender *xrender;
  MetaCompWindow *cw;
  MetaRectangle old_rect;

  xrender = META_COMPOSITOR_XRENDER (compositor);

  cw = g_object_get_data (G_OBJECT (surface), "cw");

  cw->needs_shadow = window_has_shadow (xrender, cw);

  meta_error_trap_push (cw->window->display);

  old_rect = cw->rect;
  meta_window_get_input_rect (cw->window, &cw->rect);

  if (cw->rect.width != old_rect.width || cw->rect.height != old_rect.height)
    {
      if (cw->shadow != None)
        {
          XRenderFreePicture (xrender->xdisplay, cw->shadow);
          cw->shadow = None;
        }
    }

  if (cw->extents != None)
    {
      meta_compositor_add_damage (compositor, "sync_window_geometry", cw->extents);
      XFixesDestroyRegion (xrender->xdisplay, cw->extents);
      cw->extents = None;
    }

  meta_error_trap_pop (cw->window->display);
}

static void
meta_compositor_xrender_pre_paint (MetaCompositor *compositor)
{
  MetaCompositorXRender *xrender;
  GList *stack;
  GList *l;

  xrender = META_COMPOSITOR_XRENDER (compositor);

  if (xrender->root_buffer == None)
    xrender->root_buffer = create_root_buffer (xrender);

  stack = meta_compositor_get_stack (compositor);

  for (l = stack; l != NULL; l = l->next)
    {
      MetaSurface *surface;
      MetaCompWindow *cw;

      surface = META_SURFACE (l->data);
      cw = g_object_get_data (G_OBJECT (surface), "cw");

      if (cw->extents == None)
        {
          cw->extents = win_extents (xrender, cw);

          if (cw->extents != None)
            {
              meta_compositor_add_damage (compositor,
                                          "meta_compositor_xrender_pre_paint",
                                          cw->extents);
            }
        }
    }
}

static void
meta_compositor_xrender_redraw (MetaCompositor *compositor,
                                XserverRegion   all_damage)
{
  MetaCompositorXRender *xrender;
  MetaDisplay *display;

  xrender = META_COMPOSITOR_XRENDER (compositor);
  display = meta_compositor_get_display (compositor);

  meta_error_trap_push (display);

  paint_all (xrender, all_damage);

  meta_error_trap_pop (display);
}

static void
meta_compositor_xrender_class_init (MetaCompositorXRenderClass *xrender_class)
{
  GObjectClass *object_class;
  MetaCompositorClass *compositor_class;

  object_class = G_OBJECT_CLASS (xrender_class);
  compositor_class = META_COMPOSITOR_CLASS (xrender_class);

  object_class->constructed = meta_compositor_xrender_constructed;
  object_class->finalize = meta_compositor_xrender_finalize;

  compositor_class->manage = meta_compositor_xrender_manage;
  compositor_class->add_window = meta_compositor_xrender_add_window;
  compositor_class->remove_window = meta_compositor_xrender_remove_window;
  compositor_class->hide_window = meta_compositor_xrender_hide_window;
  compositor_class->window_opacity_changed = meta_compositor_xrender_window_opacity_changed;
  compositor_class->process_event = meta_compositor_xrender_process_event;
  compositor_class->sync_screen_size = meta_compositor_xrender_sync_screen_size;
  compositor_class->sync_window_geometry = meta_compositor_xrender_sync_window_geometry;
  compositor_class->pre_paint = meta_compositor_xrender_pre_paint;
  compositor_class->redraw = meta_compositor_xrender_redraw;
}

static void
meta_compositor_xrender_init (MetaCompositorXRender *xrender)
{
  meta_compositor_set_composited (META_COMPOSITOR (xrender), TRUE);
}

MetaCompositor *
meta_compositor_xrender_new (MetaDisplay  *display,
                             GError      **error)
{
  return g_initable_new (META_TYPE_COMPOSITOR_XRENDER, NULL, error,
                         "display", display,
                         NULL);
}
