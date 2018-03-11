/*
 * Copyright (C) 2007 Iain Holmes
 * Copyright (C) 2017 Alberts Muktupāvels
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
#include <cairo/cairo-xlib.h>
#include <cairo/cairo-xlib-xrender.h>

#include "display-private.h"
#include "screen.h"
#include "frame.h"
#include "errors.h"
#include "prefs.h"
#include "window-private.h"
#include "meta-compositor-xrender.h"
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

#define SHADOW_SMALL_RADIUS 3.0
#define SHADOW_MEDIUM_RADIUS 6.0
#define SHADOW_LARGE_RADIUS 12.0

#define SHADOW_SMALL_OFFSET_X (SHADOW_SMALL_RADIUS * -3 / 2)
#define SHADOW_SMALL_OFFSET_Y (SHADOW_SMALL_RADIUS * -3 / 2)
#define SHADOW_MEDIUM_OFFSET_X (SHADOW_MEDIUM_RADIUS * -3 / 2)
#define SHADOW_MEDIUM_OFFSET_Y (SHADOW_MEDIUM_RADIUS * -5 / 4)
#define SHADOW_LARGE_OFFSET_X -15
#define SHADOW_LARGE_OFFSET_Y -15

#define SHADOW_OPACITY 0.66

typedef enum _MetaShadowType
{
  META_SHADOW_SMALL,
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

  Pixmap back_pixmap;
  Pixmap mask_pixmap;

  int mode;

  gboolean damaged;

  XserverRegion shape_region;

  Damage damage;
  Picture picture;
  Picture mask;
  Picture alpha_pict;

  gboolean needs_shadow;
  MetaShadowType shadow_type;

  XserverRegion window_region;
  XserverRegion visible_region;
  XserverRegion client_region;

  XserverRegion extents;

  Picture shadow;
  int shadow_dx;
  int shadow_dy;
  int shadow_width;
  int shadow_height;

  XserverRegion border_clip;

  /* When the window is shaded we will store few data of the original unshaded
   * window so we can still see what the window looked like when it is needed
   * for _get_window_surface function.
   */
  struct {
    Pixmap back_pixmap;
    Pixmap mask_pixmap;

    int x;
    int y;
    int width;
    int height;

    XserverRegion client_region;
  } shaded;
} MetaCompWindow;

struct _MetaCompositorXRender
{
  MetaCompositor  parent;

  Display        *xdisplay;

  MetaScreen     *screen;
  GList          *windows;
  GHashTable     *windows_by_xid;

  Window          overlay_window;

  gboolean        have_shadows;
  shadow         *shadows[LAST_SHADOW_TYPE];

  Picture         root_picture;
  Picture         root_buffer;
  Picture         black_picture;
  Picture         root_tile;
  XserverRegion   all_damage;

  gboolean        clip_changed;

  gboolean        prefs_listener_added;

  guint           show_redraw : 1;
  guint           debug : 1;
};

G_DEFINE_TYPE (MetaCompositorXRender, meta_compositor_xrender, META_TYPE_COMPOSITOR)

static Window
get_toplevel_xwindow (MetaWindow *window)
{
  MetaFrame *frame;

  frame = meta_window_get_frame (window);

  if (frame != NULL)
    return meta_frame_get_xwindow (frame);

  return meta_window_get_xwindow (window);
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

static void
dump_xserver_region (MetaCompositorXRender *xrender,
                     const gchar           *location,
                     XserverRegion          region)
{
  int nrects;
  XRectangle *rects;
  XRectangle bounds;

  if (!xrender->debug)
    return;

  if (region)
    {
      rects = XFixesFetchRegionAndBounds (xrender->xdisplay, region,
                                          &nrects, &bounds);

      if (nrects > 0)
        {
          int i;
          fprintf (stderr, "%s (XSR): %d rects, bounds: %d,%d (%d,%d)\n",
                   location, nrects, bounds.x, bounds.y, bounds.width, bounds.height);
          for (i = 1; i < nrects; i++)
            fprintf (stderr, "\t%d,%d (%d,%d)\n",
                     rects[i].x, rects[i].y, rects[i].width, rects[i].height);
        }
      else
        fprintf (stderr, "%s (XSR): empty\n", location);
      XFree (rects);
    }
  else
    fprintf (stderr, "%s (XSR): null\n", location);
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
  double radii[LAST_SHADOW_TYPE] = {SHADOW_SMALL_RADIUS,
                                    SHADOW_MEDIUM_RADIUS,
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

double shadow_offsets_x[LAST_SHADOW_TYPE] = {SHADOW_SMALL_OFFSET_X,
                                             SHADOW_MEDIUM_OFFSET_X,
                                             SHADOW_LARGE_OFFSET_X};
double shadow_offsets_y[LAST_SHADOW_TYPE] = {SHADOW_SMALL_OFFSET_Y,
                                             SHADOW_MEDIUM_OFFSET_Y,
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

static cairo_region_t *
xserver_region_to_cairo_region (Display       *xdisplay,
                                XserverRegion  xregion)
{
  XRectangle *xrects;
  int nrects;
  cairo_rectangle_int_t *rects;
  int i;
  cairo_region_t *region;

  if (xregion == None)
    return NULL;

  xrects = XFixesFetchRegion (xdisplay, xregion, &nrects);
  if (xrects == NULL)
    return NULL;

  if (nrects == 0)
    {
      XFree (xrects);
      return NULL;
    }

  rects = g_new (cairo_rectangle_int_t, nrects);

  for (i = 0; i < nrects; i++)
    {
      rects[i].x = xrects[i].x;
      rects[i].y = xrects[i].y;
      rects[i].width = xrects[i].width;
      rects[i].height = xrects[i].height;
    }

  XFree (xrects);

  region = cairo_region_create_rectangles (rects, nrects);
  g_free (rects);

  return region;
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
  if (cw->shape_region != None)
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

  r.x = cw->rect.x;
  r.y = cw->rect.y;
  r.width = cw->rect.width;
  r.height = cw->rect.height;

  if (cw->needs_shadow)
    {
      MetaFrame *frame;
      MetaFrameBorders borders;
      XRectangle sr;

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
    }

  return XFixesCreateRegion (xrender->xdisplay, &r, 1);
}

static XserverRegion
get_window_region (MetaDisplay    *display,
                   MetaCompWindow *cw)
{
  Display *xdisplay;
  Window xwindow;
  XserverRegion region;

  xdisplay = meta_display_get_xdisplay (display);
  xwindow = get_toplevel_xwindow (cw->window);

  meta_error_trap_push (display);
  region = XFixesCreateRegionFromWindow (xdisplay, xwindow, WindowRegionBounding);
  meta_error_trap_pop (display);

  if (region == None)
    return None;

  XFixesTranslateRegion (xdisplay, region, cw->rect.x, cw->rect.y);

  return region;
}

static XserverRegion
get_client_region (MetaDisplay    *display,
                   MetaCompWindow *cw)
{
  Display *xdisplay;
  XserverRegion region;
  MetaFrame *frame;

  xdisplay = meta_display_get_xdisplay (display);

  if (cw->window_region != None)
    {
      region = XFixesCreateRegion (xdisplay, NULL, 0);
      XFixesCopyRegion (xdisplay, region, cw->window_region);
    }
  else
    {
      region = get_window_region (display, cw);
      if (region == None)
        return None;
    }

  frame = meta_window_get_frame (cw->window);

  if (frame != NULL)
    {
      MetaFrameBorders borders;
      int x;
      int y;
      int width;
      int height;
      XRectangle rect;
      XserverRegion client;

      meta_frame_calc_borders (frame, &borders);

      x = cw->rect.x;
      y = cw->rect.y;
      width = cw->rect.width;
      height = cw->rect.height;

      rect.x = x + borders.total.left;
      rect.y = y + borders.total.top;
      rect.width = width - borders.total.left - borders.total.right;
      rect.height = height - borders.total.top - borders.total.bottom;

      client = XFixesCreateRegion (xdisplay, &rect, 1);

      XFixesIntersectRegion (xdisplay, region, region, client);
      XFixesDestroyRegion (xdisplay, client);
    }

  return region;
}

static XserverRegion
get_visible_region (MetaDisplay    *display,
                    MetaCompWindow *cw)
{
  Display *xdisplay;
  XserverRegion region;
  cairo_region_t *visible;
  XserverRegion tmp;

  xdisplay = meta_display_get_xdisplay (display);

  if (cw->window_region != None)
    {
      region = XFixesCreateRegion (xdisplay, NULL, 0);
      XFixesCopyRegion (xdisplay, region, cw->window_region);
    }
  else
    {
      region = get_window_region (display, cw);
      if (region == None)
        return None;
    }

  visible = meta_window_get_frame_bounds (cw->window);
  tmp = cairo_region_to_xserver_region (xdisplay, visible);

  if (tmp != None)
    {
      XFixesTranslateRegion (xdisplay, tmp, cw->rect.x, cw->rect.y);
      XFixesIntersectRegion (xdisplay, region, region, tmp);
      XFixesDestroyRegion (xdisplay, tmp);
    }

  return region;
}

static XRenderPictFormat *
get_window_format (Display        *xdisplay,
                   MetaCompWindow *cw)
{
  XRenderPictFormat *format;

  format = XRenderFindVisualFormat (xdisplay, cw->window->xvisual);

  if (!format)
    {
      Visual *visual;

      visual = DefaultVisual (xdisplay, DefaultScreen (xdisplay));
      format = XRenderFindVisualFormat (xdisplay, visual);
    }

  return format;
}

static Picture
get_window_picture (MetaDisplay    *display,
                    MetaCompWindow *cw)
{
  Display *xdisplay;
  Window xwindow;
  XRenderPictureAttributes pa;
  XRenderPictFormat *format;

  xdisplay = meta_display_get_xdisplay (display);
  xwindow = get_toplevel_xwindow (cw->window);

  if (cw->back_pixmap == None)
    {
      meta_error_trap_push (display);
      cw->back_pixmap = XCompositeNameWindowPixmap (xdisplay, xwindow);

      if (meta_error_trap_pop_with_return (display) != 0)
        cw->back_pixmap = None;
    }

  format = get_window_format (xdisplay, cw);
  if (format)
    {
      Drawable draw;
      Picture pict;

      draw = cw->back_pixmap != None ? cw->back_pixmap : xwindow;
      pa.subwindow_mode = IncludeInferiors;

      meta_error_trap_push (display);
      pict = XRenderCreatePicture (xdisplay, draw, format, CPSubwindowMode, &pa);
      meta_error_trap_pop (display);

      return pict;
    }

  return None;
}

static Picture
get_window_mask (MetaDisplay    *display,
                 MetaCompWindow *cw)
{
  MetaFrame *frame;
  Display *xdisplay;
  int width;
  int height;
  XRenderPictFormat *format;
  cairo_surface_t *surface;
  cairo_t *cr;
  Picture picture;

  frame = meta_window_get_frame (cw->window);
  if (frame == NULL)
    return None;

  xdisplay = meta_display_get_xdisplay (display);
  width = cw->rect.width;
  height = cw->rect.height;
  format = XRenderFindStandardFormat (xdisplay, PictStandardA8);

  if (cw->mask_pixmap == None)
    {
      Window xwindow;

      xwindow = get_toplevel_xwindow (cw->window);

      meta_error_trap_push (display);
      cw->mask_pixmap = XCreatePixmap (xdisplay, xwindow, width, height,
                                       format->depth);

      if (meta_error_trap_pop_with_return (display) != 0)
        return None;
    }

  surface = cairo_xlib_surface_create_with_xrender_format (xdisplay, cw->mask_pixmap,
                                                           DefaultScreenOfDisplay (xdisplay),
                                                           format, width, height);

  cr = cairo_create (surface);

  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_set_source_rgba (cr, 0, 0, 0, 1);
  cairo_paint (cr);

  {
    cairo_rectangle_int_t rect;
    cairo_region_t *frame_paint_region;
    MetaFrameBorders borders;

    rect.x = 0;
    rect.y = 0;
    rect.width = width;
    rect.height = height;

    frame_paint_region = cairo_region_create_rectangle (&rect);
    meta_frame_calc_borders (frame, &borders);

    rect.x += borders.total.left;
    rect.y += borders.total.top;
    rect.width -= borders.total.left + borders.total.right;
    rect.height -= borders.total.top + borders.total.bottom;

    cairo_region_subtract_rectangle (frame_paint_region, &rect);

    gdk_cairo_region (cr, frame_paint_region);
    cairo_clip (cr);

    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
    meta_frame_get_mask (frame, cr);

    cairo_surface_flush (surface);
    cairo_region_destroy (frame_paint_region);
  }

  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  meta_error_trap_push (display);
  picture = XRenderCreatePicture (xdisplay, cw->mask_pixmap, format, 0, NULL);
  meta_error_trap_pop (display);

  return picture;
}

static void
paint_dock_shadows (MetaCompositorXRender *xrender,
                    Picture                root_buffer,
                    XserverRegion          region)
{
  Display *xdisplay = xrender->xdisplay;
  GList *window;

  for (window = xrender->windows; window; window = window->next)
    {
      MetaCompWindow *cw = window->data;
      XserverRegion shadow_clip;

      if (cw->window->type == META_WINDOW_DOCK &&
          cw->needs_shadow && cw->shadow)
        {
          shadow_clip = XFixesCreateRegion (xdisplay, NULL, 0);
          XFixesIntersectRegion (xdisplay, shadow_clip,
                                 cw->border_clip, region);

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
               GList                 *windows,
               Picture                root_buffer,
               XserverRegion          region)
{
  MetaDisplay *display = meta_screen_get_display (xrender->screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  GList *index, *last;
  int screen_width, screen_height;
  MetaCompWindow *cw;
  XserverRegion paint_region, desktop_region;

  meta_screen_get_size (xrender->screen, &screen_width, &screen_height);

  if (region == None)
    {
      XRectangle r;
      r.x = 0;
      r.y = 0;
      r.width = screen_width;
      r.height = screen_height;
      paint_region = XFixesCreateRegion (xdisplay, &r, 1);
    }
  else
    {
      paint_region = XFixesCreateRegion (xdisplay, NULL, 0);
      XFixesCopyRegion (xdisplay, paint_region, region);
    }

  desktop_region = None;

  /*
   * Painting from top to bottom, reducing the clipping area at
   * each iteration. Only the opaque windows are painted 1st.
   */
  last = NULL;
  for (index = windows; index; index = index->next)
    {
      /* Store the last window we dealt with */
      last = index;

      cw = (MetaCompWindow *) index->data;
      if (!cw->damaged)
        {
          /* Not damaged */
          continue;
        }

      if (!cw->window->mapped)
        continue;

      if (cw->picture == None)
        cw->picture = get_window_picture (display, cw);

      if (cw->mask == None)
        cw->mask = get_window_mask (display, cw);

      /* If the clip region of the screen has been changed
         then we need to recreate the extents of the window */
      if (xrender->clip_changed)
        {
          if (cw->window_region)
            {
              XFixesDestroyRegion (xdisplay, cw->window_region);
              cw->window_region = None;
            }

          if (cw->visible_region)
            {
              XFixesDestroyRegion (xdisplay, cw->visible_region);
              cw->visible_region = None;
            }

          if (cw->client_region)
            {
              XFixesDestroyRegion (xdisplay, cw->client_region);
              cw->client_region = None;
            }
        }

      if (cw->window_region == None)
        cw->window_region = get_window_region (display, cw);

      if (cw->visible_region == None)
        cw->visible_region = get_visible_region (display, cw);

      if (cw->client_region == None)
        cw->client_region = get_client_region (display, cw);

      if (cw->extents == None)
        cw->extents = win_extents (xrender, cw);

      if (cw->mode == WINDOW_SOLID)
        {
          int x, y, wid, hei;
          MetaFrame *frame;
          MetaFrameBorders borders;

          x = cw->rect.x;
          y = cw->rect.y;
          wid = cw->rect.width;
          hei = cw->rect.height;

          frame = meta_window_get_frame (cw->window);
          meta_frame_calc_borders (frame, &borders);

          XFixesSetPictureClipRegion (xdisplay, root_buffer,
                                      0, 0, paint_region);
          XRenderComposite (xdisplay, PictOpSrc, cw->picture, None, root_buffer,
                            borders.total.left, borders.total.top, 0, 0,
                            x + borders.total.left, y + borders.total.top,
                            wid - borders.total.left - borders.total.right,
                            hei - borders.total.top - borders.total.bottom);

          if (cw->window->type == META_WINDOW_DESKTOP)
            {
              desktop_region = XFixesCreateRegion (xdisplay, 0, 0);
              XFixesCopyRegion (xdisplay, desktop_region, paint_region);
            }

          if (frame == NULL)
            {
              XFixesSubtractRegion (xdisplay, paint_region,
                                    paint_region, cw->window_region);
            }
          else
            {
              XFixesSubtractRegion (xdisplay, paint_region,
                                    paint_region, cw->client_region);
            }
        }

      if (!cw->border_clip)
        {
          cw->border_clip = XFixesCreateRegion (xdisplay, 0, 0);
          XFixesCopyRegion (xdisplay, cw->border_clip, paint_region);
        }
    }

  XFixesSetPictureClipRegion (xdisplay, root_buffer, 0, 0, paint_region);
  paint_root (xrender, root_buffer);

  paint_dock_shadows (xrender, root_buffer,
                      desktop_region == None ? paint_region : desktop_region);

  if (desktop_region != None)
    XFixesDestroyRegion (xdisplay, desktop_region);

  /*
   * Painting from bottom to top, translucent windows and shadows are painted
   */
  for (index = last; index; index = index->prev)
    {
      cw = (MetaCompWindow *) index->data;

      if (cw->picture)
        {
          int x, y, wid, hei;

          x = cw->rect.x;
          y = cw->rect.y;
          wid = cw->rect.width;
          hei = cw->rect.height;

          if (cw->shadow && cw->window->type != META_WINDOW_DOCK)
            {
              XserverRegion shadow_clip;

              shadow_clip = XFixesCreateRegion (xdisplay, NULL, 0);
              XFixesSubtractRegion (xdisplay, shadow_clip, cw->border_clip,
                                    cw->visible_region);
              XFixesSetPictureClipRegion (xdisplay, root_buffer, 0, 0,
                                          shadow_clip);

              XRenderComposite (xdisplay, PictOpOver, xrender->black_picture,
                                cw->shadow, root_buffer, 0, 0, 0, 0,
                                x + cw->shadow_dx, y + cw->shadow_dy,
                                cw->shadow_width, cw->shadow_height);

              if (shadow_clip)
                XFixesDestroyRegion (xdisplay, shadow_clip);
            }

          if ((cw->window->opacity != (guint) OPAQUE) && !(cw->alpha_pict))
            {
              cw->alpha_pict = solid_picture (xdisplay, FALSE,
                                              (double) cw->window->opacity / OPAQUE,
                                              0, 0, 0);
            }

          XFixesIntersectRegion (xdisplay, cw->border_clip, cw->border_clip,
                                 cw->window_region);
          XFixesSetPictureClipRegion (xdisplay, root_buffer, 0, 0,
                                      cw->border_clip);

          if (cw->mode == WINDOW_SOLID && cw->mask != None)
            {
              XRenderComposite (xdisplay, PictOpOver, cw->mask,
                                cw->alpha_pict, root_buffer, 0, 0, 0, 0,
                                x, y, wid, hei);

              XRenderComposite (xdisplay, PictOpAdd, cw->picture,
                                cw->alpha_pict, root_buffer, 0, 0, 0, 0,
                                x, y, wid, hei);
            }
          else if (cw->mode == WINDOW_ARGB && cw->mask != None)
            {
              XserverRegion clip;
              XserverRegion client;

              clip = XFixesCreateRegion (xdisplay, NULL, 0);
              client = cw->client_region;

              XFixesSubtractRegion (xdisplay, clip, cw->border_clip, client);
              XFixesSetPictureClipRegion (xdisplay, root_buffer, 0, 0, clip);

              XRenderComposite (xdisplay, PictOpOver, cw->mask,
                                cw->alpha_pict, root_buffer, 0, 0, 0, 0,
                                x, y, wid, hei);

              XRenderComposite (xdisplay, PictOpAdd, cw->picture,
                                cw->alpha_pict, root_buffer, 0, 0, 0, 0,
                                x, y, wid, hei);

              XFixesIntersectRegion (xdisplay, clip, cw->border_clip, client);
              XFixesSetPictureClipRegion (xdisplay, root_buffer, 0, 0, clip);

              XRenderComposite (xdisplay, PictOpOver, cw->picture,
                                cw->alpha_pict, root_buffer, 0, 0, 0, 0,
                                x, y, wid, hei);

              if (clip)
                XFixesDestroyRegion (xdisplay, clip);
            }
          else if (cw->mode == WINDOW_ARGB && cw->mask == None)
            {
              XRenderComposite (xdisplay, PictOpOver, cw->picture,
                                cw->alpha_pict, root_buffer, 0, 0, 0, 0,
                                x, y, wid, hei);
            }
        }

      if (cw->border_clip)
        {
          XFixesDestroyRegion (xdisplay, cw->border_clip);
          cw->border_clip = None;
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

  /* Set clipping to the given region */
  XFixesSetPictureClipRegion (xdisplay, xrender->root_picture, 0, 0, region);

  meta_screen_get_size (xrender->screen, &screen_width, &screen_height);

  if (xrender->show_redraw)
    {
      Picture overlay;

      dump_xserver_region (xrender, "paint_all", region);

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

  if (xrender->root_buffer == None)
    xrender->root_buffer = create_root_buffer (xrender);

  paint_windows (xrender, xrender->windows, xrender->root_buffer, region);

  XFixesSetPictureClipRegion (xdisplay, xrender->root_buffer, 0, 0, region);
  XRenderComposite (xdisplay, PictOpSrc, xrender->root_buffer, None,
                    xrender->root_picture, 0, 0, 0, 0, 0, 0,
                    screen_width, screen_height);
}

static void
add_repair (MetaCompositorXRender *xrender)
{
  meta_compositor_queue_redraw (META_COMPOSITOR (xrender));
}

static void
add_damage (MetaCompositorXRender *xrender,
            XserverRegion          damage)
{
  Display *xdisplay = xrender->xdisplay;

  /* dump_xserver_region (xrender, "add_damage", damage); */

  if (xrender->all_damage)
    {
      XFixesUnionRegion (xdisplay, xrender->all_damage, xrender->all_damage, damage);
      XFixesDestroyRegion (xdisplay, damage);
    }
  else
    xrender->all_damage = damage;

  add_repair (xrender);
}

static void
damage_screen (MetaCompositorXRender *xrender)
{
  MetaCompositor *compositor = META_COMPOSITOR (xrender);
  MetaDisplay *display = meta_compositor_get_display (compositor);
  Display *xdisplay = meta_display_get_xdisplay (display);
  XserverRegion region;
  int width, height;
  XRectangle r;

  r.x = 0;
  r.y = 0;
  meta_screen_get_size (xrender->screen, &width, &height);
  r.width = width;
  r.height = height;

  region = XFixesCreateRegion (xdisplay, &r, 1);
  dump_xserver_region (xrender, "damage_screen", region);
  add_damage (xrender, region);
}

static void
repair_win (MetaCompositorXRender *xrender,
            MetaCompWindow        *cw)
{
  MetaCompositor *compositor = META_COMPOSITOR (xrender);
  MetaDisplay *display = meta_compositor_get_display (compositor);
  Display *xdisplay = meta_display_get_xdisplay (display);
  XserverRegion parts;

  meta_error_trap_push (display);

  if (!cw->damaged)
    {
      parts = win_extents (xrender, cw);
      XDamageSubtract (xdisplay, cw->damage, None, None);
    }
  else
    {
      parts = XFixesCreateRegion (xdisplay, 0, 0);
      XDamageSubtract (xdisplay, cw->damage, None, parts);
      XFixesTranslateRegion (xdisplay, parts, cw->rect.x, cw->rect.y);
    }

  meta_error_trap_pop (display);

  dump_xserver_region (xrender, "repair_win", parts);
  add_damage (xrender, parts);
  cw->damaged = TRUE;
}

static void
free_win (MetaCompositorXRender *xrender,
          MetaCompWindow        *cw,
          gboolean               destroy)
{
  MetaDisplay *display = meta_screen_get_display (xrender->screen);
  Display *xdisplay = meta_display_get_xdisplay (display);

  meta_error_trap_push (display);

  /* See comment in map_win */
  if (cw->back_pixmap && destroy)
    {
      XFreePixmap (xdisplay, cw->back_pixmap);
      cw->back_pixmap = None;
    }

  if (cw->mask_pixmap && destroy)
    {
      XFreePixmap (xdisplay, cw->mask_pixmap);
      cw->mask_pixmap = None;
    }

  if (cw->shape_region)
    {
      XFixesDestroyRegion (xdisplay, cw->shape_region);
      cw->shape_region = None;
    }

  if (cw->picture)
    {
      XRenderFreePicture (xdisplay, cw->picture);
      cw->picture = None;
    }

  if (cw->mask)
    {
      XRenderFreePicture (xdisplay, cw->mask);
      cw->mask = None;
    }

  if (cw->shadow)
    {
      XRenderFreePicture (xdisplay, cw->shadow);
      cw->shadow = None;
    }

  if (cw->alpha_pict)
    {
      XRenderFreePicture (xdisplay, cw->alpha_pict);
      cw->alpha_pict = None;
    }

  if (cw->window_region)
    {
      XFixesDestroyRegion (xdisplay, cw->window_region);
      cw->window_region = None;
    }

  if (cw->visible_region)
    {
      XFixesDestroyRegion (xdisplay, cw->visible_region);
      cw->visible_region = None;
    }

  if (cw->client_region && destroy)
    {
      XFixesDestroyRegion (xdisplay, cw->client_region);
      cw->client_region = None;
    }

  if (cw->border_clip)
    {
      XFixesDestroyRegion (xdisplay, cw->border_clip);
      cw->border_clip = None;
    }

  if (cw->extents)
    {
      XFixesDestroyRegion (xdisplay, cw->extents);
      cw->extents = None;
    }

  if (cw->shaded.back_pixmap && destroy)
    {
      XFreePixmap (xdisplay, cw->shaded.back_pixmap);
      cw->shaded.back_pixmap = None;
    }

  if (cw->shaded.mask_pixmap && destroy)
    {
      XFreePixmap (xdisplay, cw->shaded.mask_pixmap);
      cw->shaded.mask_pixmap = None;
    }

  if (cw->shaded.client_region && destroy)
    {
      XFixesDestroyRegion (xdisplay, cw->shaded.client_region);
      cw->shaded.client_region = None;
    }

  if (destroy)
    {
      if (cw->damage != None)
        {
          XDamageDestroy (xdisplay, cw->damage);
          cw->damage = None;
        }

      g_free (cw);
    }

  meta_error_trap_pop (display);
}

static void
map_win (MetaCompositorXRender *xrender,
         MetaCompWindow        *cw)
{
  Display *xdisplay = xrender->xdisplay;

  /* The reason we deallocate this here and not in unmap
     is so that we will still have a valid pixmap for
     whenever the window is unmapped */
  if (cw->back_pixmap)
    {
      XFreePixmap (xdisplay, cw->back_pixmap);
      cw->back_pixmap = None;
    }

  if (cw->mask_pixmap)
    {
      XFreePixmap (xdisplay, cw->mask_pixmap);
      cw->mask_pixmap = None;
    }

  if (cw->client_region)
    {
      XFixesDestroyRegion (xdisplay, cw->client_region);
      cw->client_region = None;
    }

  if (cw->shaded.back_pixmap)
    {
      XFreePixmap (xdisplay, cw->shaded.back_pixmap);
      cw->shaded.back_pixmap = None;
    }

  if (cw->shaded.mask_pixmap)
    {
      XFreePixmap (xdisplay, cw->shaded.mask_pixmap);
      cw->shaded.mask_pixmap = None;
    }

  if (cw->shaded.client_region)
    {
      XFixesDestroyRegion (xdisplay, cw->shaded.client_region);
      cw->shaded.client_region = None;
    }
}

static void
determine_mode (MetaCompositorXRender *xrender,
                MetaCompWindow        *cw)
{
  XRenderPictFormat *format;
  MetaCompositor *compositor = META_COMPOSITOR (xrender);
  MetaDisplay *display = meta_compositor_get_display (compositor);
  Display *xdisplay = meta_display_get_xdisplay (display);

  if (cw->alpha_pict)
    {
      XRenderFreePicture (xdisplay, cw->alpha_pict);
      cw->alpha_pict = None;
    }

  format = XRenderFindVisualFormat (xdisplay, cw->window->xvisual);

  if ((format && format->type == PictTypeDirect && format->direct.alphaMask)
      || cw->window->opacity != (guint) OPAQUE)
    cw->mode = WINDOW_ARGB;
  else
    cw->mode = WINDOW_SOLID;

  if (cw->extents)
    {
      XserverRegion damage;
      damage = XFixesCreateRegion (xdisplay, NULL, 0);
      XFixesCopyRegion (xdisplay, damage, cw->extents);

      dump_xserver_region (xrender, "determine_mode", damage);
      add_damage (xrender, damage);
    }
}

static void
notify_appears_focused_cb (MetaWindow            *window,
                           GParamSpec            *pspec,
                           MetaCompositorXRender *xrender)
{
  MetaCompWindow *cw;
  Display *xdisplay;
  XserverRegion damage;

  cw = find_comp_window_by_window (xrender, window);

  if (cw == NULL)
    return;

  xdisplay = window->display->xdisplay;
  damage = None;

  if (meta_window_appears_focused (window))
    cw->shadow_type = META_SHADOW_LARGE;
  else
    cw->shadow_type = META_SHADOW_MEDIUM;

  determine_mode (xrender, cw);
  cw->needs_shadow = window_has_shadow (xrender, cw);

  if (cw->mask)
    {
      XRenderFreePicture (xdisplay, cw->mask);
      cw->mask = None;
    }

  if (cw->shadow)
    {
      XRenderFreePicture (xdisplay, cw->shadow);
      cw->shadow = None;
    }

  if (cw->extents)
    {
      damage = XFixesCreateRegion (xdisplay, NULL, 0);
      XFixesCopyRegion (xdisplay, damage, cw->extents);
      XFixesDestroyRegion (xdisplay, cw->extents);
    }

  cw->extents = win_extents (xrender, cw);

  if (damage)
    {
      XFixesUnionRegion (xdisplay, damage, damage, cw->extents);
    }
  else
    {
      damage = XFixesCreateRegion (xdisplay, NULL, 0);
      XFixesCopyRegion (xdisplay, damage, cw->extents);
    }

  dump_xserver_region (xrender, "notify_appears_focused_cb", damage);
  add_damage (xrender, damage);

  xrender->clip_changed = TRUE;
  add_repair (xrender);
}

static void
notify_decorated_cb (MetaWindow            *window,
                     GParamSpec            *pspec,
                     MetaCompositorXRender *xrender)
{
  MetaCompWindow *cw;
  XserverRegion damage;

  cw = find_comp_window_by_window (xrender, window);
  damage = None;

  if (cw == NULL)
    return;

  meta_error_trap_push (window->display);

  if (cw->back_pixmap != None)
    {
      XFreePixmap (xrender->xdisplay, cw->back_pixmap);
      cw->back_pixmap = None;
    }

  if (cw->mask_pixmap != None)
    {
      XFreePixmap (xrender->xdisplay, cw->mask_pixmap);
      cw->mask_pixmap = None;
    }

  if (cw->shape_region != None)
    {
      XFixesDestroyRegion (xrender->xdisplay, cw->shape_region);
      cw->shape_region = None;
    }

  if (cw->damage != None)
    {
      XDamageDestroy (xrender->xdisplay, cw->damage);
      cw->damage = None;
    }

  if (cw->picture != None)
    {
      XRenderFreePicture (xrender->xdisplay, cw->picture);
      cw->picture = None;
    }

  if (cw->mask != None)
    {
      XRenderFreePicture (xrender->xdisplay, cw->mask);
      cw->mask = None;
    }

  if (cw->alpha_pict != None)
    {
      XRenderFreePicture (xrender->xdisplay, cw->alpha_pict);
      cw->alpha_pict = None;
    }

  if (cw->window_region != None)
    {
      XFixesDestroyRegion (xrender->xdisplay, cw->window_region);
      cw->window_region = None;
    }

  if (cw->visible_region != None)
    {
      XFixesDestroyRegion (xrender->xdisplay, cw->visible_region);
      cw->visible_region = None;
    }

  if (cw->client_region != None)
    {
      XFixesDestroyRegion (xrender->xdisplay, cw->client_region);
      cw->client_region = None;
    }

  if (cw->extents != None)
    {
      damage = cw->extents;
      cw->extents = None;
    }

  if (cw->shadow != None)
    {
      XRenderFreePicture (xrender->xdisplay, cw->shadow);
      cw->shadow = None;
    }

  if (cw->border_clip != None)
    {
      XFixesDestroyRegion (xrender->xdisplay, cw->border_clip);
      cw->border_clip = None;
    }

  if (cw->shaded.back_pixmap != None)
    {
      XFreePixmap (xrender->xdisplay, cw->shaded.back_pixmap);
      cw->shaded.back_pixmap = None;
    }

  if (cw->shaded.mask_pixmap != None)
    {
      XFreePixmap (xrender->xdisplay, cw->shaded.mask_pixmap);
      cw->shaded.mask_pixmap = None;
    }

  if (cw->shaded.client_region != None)
    {
      XFixesDestroyRegion (xrender->xdisplay, cw->shaded.client_region);
      cw->shaded.client_region = None;
    }

  cw->damage = XDamageCreate (xrender->xdisplay,
                              get_toplevel_xwindow (window),
                              XDamageReportNonEmpty);

  determine_mode (xrender, cw);
  cw->needs_shadow = window_has_shadow (xrender, cw);

  meta_error_trap_pop (window->display);

  dump_xserver_region (xrender, "notify_decorated_cb", damage);
  add_damage (xrender, damage);
  cw->damaged = TRUE;

  xrender->clip_changed = TRUE;
  add_repair (xrender);
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
          damage_screen (xrender);

          add_repair (xrender);
          return;
        }
    }

  if (event->atom == display->atom__NET_WM_WINDOW_TYPE)
    {
      MetaCompWindow *cw = find_comp_window_by_xwindow (xrender, event->window);

      if (!cw)
        return;

      cw->needs_shadow = window_has_shadow (xrender, cw);
      return;
    }
}

static void
expose_area (MetaCompositorXRender *xrender,
             XRectangle            *rects,
             int                    nrects)
{
  XserverRegion region;

  region = XFixesCreateRegion (xrender->xdisplay, rects, nrects);

  dump_xserver_region (xrender, "expose_area", region);
  add_damage (xrender, region);
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

static void
process_damage (MetaCompositorXRender *xrender,
                XDamageNotifyEvent    *event)
{
  MetaCompWindow *cw = find_comp_window_by_xwindow (xrender, event->drawable);

  if (cw == NULL)
    return;

  repair_win (xrender, cw);

  if (event->more == FALSE)
    add_repair (xrender);
}

static int
timeout_debug (MetaCompositorXRender *compositor)
{
  compositor->show_redraw = (g_getenv ("METACITY_DEBUG_REDRAWS") != NULL);
  compositor->debug = (g_getenv ("METACITY_DEBUG_COMPOSITOR") != NULL);

  return FALSE;
}

static void
update_shadows (MetaPreference pref,
                gpointer       data)
{
  MetaCompositorXRender *xrender;
  GList *index;

  if (pref != META_PREF_THEME_TYPE)
    return;

  xrender = META_COMPOSITOR_XRENDER (data);

  for (index = xrender->windows; index; index = index->next)
    {
      MetaCompWindow *cw;

      cw = (MetaCompWindow *) index->data;

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
  GList *index;

  xrender = META_COMPOSITOR_XRENDER (object);
  display = meta_compositor_get_display (META_COMPOSITOR (xrender));
  xdisplay = meta_display_get_xdisplay (display);

  if (xrender->prefs_listener_added)
    {
      meta_prefs_remove_listener (update_shadows, xrender);
      xrender->prefs_listener_added = FALSE;
    }

  /* Destroy the windows */
  for (index = xrender->windows; index; index = index->next)
    {
      MetaCompWindow *cw = (MetaCompWindow *) index->data;
      free_win (xrender, cw, TRUE);
    }
  g_list_free (xrender->windows);
  g_clear_pointer (&xrender->windows_by_xid, g_hash_table_destroy);

  if (xrender->root_picture)
    XRenderFreePicture (xdisplay, xrender->root_picture);

  if (xrender->black_picture)
    XRenderFreePicture (xdisplay, xrender->black_picture);

  if (xrender->have_shadows)
    {
      int i;

      for (i = 0; i < LAST_SHADOW_TYPE; i++)
        g_free (xrender->shadows[i]->gaussian_map);
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
  xrender->all_damage = None;

  xrender->windows = NULL;
  xrender->windows_by_xid = g_hash_table_new (g_direct_hash, g_direct_equal);

  xrender->clip_changed = TRUE;

  xrender->have_shadows = (g_getenv("META_DEBUG_NO_SHADOW") == NULL);
  if (xrender->have_shadows)
    {
      meta_verbose ("Enabling shadows\n");
      generate_shadows (xrender);
    }
  else
    meta_verbose ("Disabling shadows\n");

  XClearArea (xdisplay, xrender->overlay_window, 0, 0, 0, 0, TRUE);

  damage_screen (xrender);

  meta_prefs_add_listener (update_shadows, xrender);
  xrender->prefs_listener_added = TRUE;

  g_timeout_add (2000, (GSourceFunc) timeout_debug, compositor);

  return TRUE;
}

static void
meta_compositor_xrender_add_window (MetaCompositor *compositor,
                                    MetaWindow     *window)
{
  MetaCompositorXRender *xrender;
  MetaDisplay *display;
  MetaCompWindow *cw;
  Window xwindow;

  g_assert (window != NULL);

  xrender = META_COMPOSITOR_XRENDER (compositor);
  display = meta_compositor_get_display (compositor);

  /* If already added, ignore */
  if (find_comp_window_by_window (xrender, window) != NULL)
    return;

  meta_error_trap_push (display);

  cw = g_new0 (MetaCompWindow, 1);
  cw->window = window;

  meta_window_get_input_rect (window, &cw->rect);

  g_signal_connect_object (window, "notify::appears-focused",
                           G_CALLBACK (notify_appears_focused_cb),
                           xrender, 0);

  g_signal_connect_object (window, "notify::decorated",
                           G_CALLBACK (notify_decorated_cb),
                           xrender, 0);

  cw->back_pixmap = None;
  cw->mask_pixmap = None;

  cw->damaged = FALSE;

  cw->shape_region = cairo_region_to_xserver_region (xrender->xdisplay,
                                                     window->shape_region);

  if (cw->shape_region != None)
    {
      XFixesTranslateRegion (xrender->xdisplay, cw->shape_region,
                             cw->rect.x, cw->rect.y);
    }

  xwindow = get_toplevel_xwindow (window);
  cw->damage = XDamageCreate (xrender->xdisplay, xwindow, XDamageReportNonEmpty);

  cw->alpha_pict = None;

  cw->window_region = None;
  cw->visible_region = None;
  cw->client_region = None;

  cw->extents = None;
  cw->shadow = None;
  cw->shadow_dx = 0;
  cw->shadow_dy = 0;
  cw->shadow_width = 0;
  cw->shadow_height = 0;

  if (meta_window_has_focus (window))
    cw->shadow_type = META_SHADOW_LARGE;
  else
    cw->shadow_type = META_SHADOW_MEDIUM;

  cw->border_clip = None;

  cw->shaded.back_pixmap = None;
  cw->shaded.mask_pixmap = None;
  cw->shaded.x = 0;
  cw->shaded.y = 0;
  cw->shaded.width = 0;
  cw->shaded.height = 0;
  cw->shaded.client_region = None;

  determine_mode (xrender, cw);
  cw->needs_shadow = window_has_shadow (xrender, cw);

  xwindow = meta_window_get_xwindow (window);
  xrender->windows = g_list_prepend (xrender->windows, cw);
  g_hash_table_insert (xrender->windows_by_xid, (gpointer) xwindow, cw);

  if (cw->window->mapped)
    map_win (xrender, cw);

  meta_error_trap_pop (display);
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
      dump_xserver_region (xrender, "remove_window", cw->extents);
      add_damage (xrender, cw->extents);
      cw->extents = None;
    }

  xwindow = meta_window_get_xwindow (window);
  xrender->windows = g_list_remove (xrender->windows, (gconstpointer) cw);
  g_hash_table_remove (xrender->windows_by_xid, (gpointer) xwindow);

  free_win (xrender, cw, TRUE);
}

static void
meta_compositor_xrender_show_window (MetaCompositor *compositor,
                                     MetaWindow     *window,
                                     MetaEffectType  effect)
{
  MetaCompositorXRender *xrender;
  MetaCompWindow *cw;

  xrender = META_COMPOSITOR_XRENDER (compositor);

  cw = find_comp_window_by_window (xrender, window);
  if (cw == NULL)
    return;

  cw->damaged = TRUE;

  map_win (xrender, cw);
}

static void
meta_compositor_xrender_hide_window (MetaCompositor *compositor,
                                     MetaWindow     *window,
                                     MetaEffectType  effect)
{
  MetaCompositorXRender *xrender;
  MetaCompWindow *cw;

  xrender = META_COMPOSITOR_XRENDER (compositor);

  cw = find_comp_window_by_window (xrender, window);
  if (cw == NULL)
    return;

  cw->damaged = FALSE;

  if (cw->extents != None)
    {
      dump_xserver_region (xrender, "hide_window", cw->extents);
      add_damage (xrender, cw->extents);
      cw->extents = None;
    }

  free_win (xrender, cw, FALSE);
  xrender->clip_changed = TRUE;
}

static void
meta_compositor_xrender_window_opacity_changed (MetaCompositor *compositor,
                                                MetaWindow     *window)
{
  MetaCompositorXRender *xrender;
  MetaCompWindow *cw;

  xrender = META_COMPOSITOR_XRENDER (compositor);

  cw = find_comp_window_by_window (xrender, window);
  if (cw == NULL)
    return;

  determine_mode (xrender, cw);
  cw->needs_shadow = window_has_shadow (xrender, cw);

  if (cw->shadow)
    {
      XRenderFreePicture (xrender->xdisplay, cw->shadow);
      cw->shadow = None;
    }

  if (cw->extents)
    XFixesDestroyRegion (xrender->xdisplay, cw->extents);
  cw->extents = win_extents (xrender, cw);

  cw->damaged = TRUE;

  add_repair (xrender);
}

static void
meta_compositor_xrender_window_opaque_region_changed (MetaCompositor *compositor,
                                                      MetaWindow     *window)
{
}

static void
meta_compositor_xrender_window_shape_region_changed (MetaCompositor *compositor,
                                                     MetaWindow     *window)
{
  MetaCompositorXRender *xrender;
  MetaCompWindow *cw;

  xrender = META_COMPOSITOR_XRENDER (compositor);

  cw = find_comp_window_by_window (xrender, window);
  if (cw == NULL)
    return;

  if (cw->shape_region != None)
    {
      dump_xserver_region (xrender, "shape_changed", cw->shape_region);
      add_damage (xrender, cw->shape_region);

      xrender->clip_changed = TRUE;
    }

  cw->shape_region = cairo_region_to_xserver_region (xrender->xdisplay,
                                                     window->shape_region);

  if (cw->shape_region != None)
    {
      XFixesTranslateRegion (xrender->xdisplay, cw->shape_region,
                             cw->rect.x, cw->rect.y);
    }
}

static void
meta_compositor_xrender_set_updates_frozen (MetaCompositor *compositor,
                                            MetaWindow     *window,
                                            gboolean        updates_frozen)
{
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
      if (event->type == meta_display_get_damage_event_base (display) + XDamageNotify)
        process_damage (xrender, (XDamageNotifyEvent *) event);
      break;
    }

  meta_error_trap_pop (display);
}

static cairo_surface_t *
meta_compositor_xrender_get_window_surface (MetaCompositor *compositor,
                                            MetaWindow     *window)
{
  MetaFrame *frame;
  MetaCompWindow *cw;
  MetaDisplay *display;
  Display *xdisplay;
  gboolean shaded;
  Pixmap back_pixmap;
  Pixmap mask_pixmap;
  int width;
  int height;
  XserverRegion xclient_region;
  cairo_region_t *client_region;
  cairo_surface_t *back_surface;
  cairo_surface_t *window_surface;
  cairo_t *cr;

  frame = meta_window_get_frame (window);
  cw = find_comp_window_by_window (META_COMPOSITOR_XRENDER (compositor), window);

  if (cw == NULL)
    return NULL;

  display = meta_compositor_get_display (compositor);
  xdisplay = meta_display_get_xdisplay (display);
  shaded = meta_window_is_shaded (window);

  back_pixmap = shaded ? cw->shaded.back_pixmap : cw->back_pixmap;
  if (back_pixmap == None)
    return NULL;

  mask_pixmap = shaded ? cw->shaded.mask_pixmap : cw->mask_pixmap;
  if (frame != NULL && mask_pixmap == None)
    return NULL;

  xclient_region = None;
  if (shaded)
    {
      if (cw->shaded.client_region != None)
        {
          xclient_region = XFixesCreateRegion (xdisplay, NULL, 0);
          XFixesCopyRegion (xdisplay, xclient_region, cw->shaded.client_region);
          XFixesTranslateRegion (xdisplay, xclient_region,
                                 -cw->shaded.x, -cw->shaded.y);
        }
    }
  else
    {
      if (cw->client_region != None)
        {
          xclient_region = XFixesCreateRegion (xdisplay, NULL, 0);
          XFixesCopyRegion (xdisplay, xclient_region, cw->client_region);
          XFixesTranslateRegion (xdisplay, xclient_region,
                                 -cw->rect.x, -cw->rect.y);
        }
    }

  if (frame != NULL && xclient_region == None)
    return NULL;

  client_region = xserver_region_to_cairo_region (xdisplay, xclient_region);
  XFixesDestroyRegion (xdisplay, xclient_region);

  if (frame != NULL && client_region == NULL)
    return NULL;

  width = shaded ? cw->shaded.width : cw->rect.width;
  height = shaded ? cw->shaded.height : cw->rect.height;

  back_surface = cairo_xlib_surface_create (xdisplay, back_pixmap,
                                            cw->window->xvisual,
                                            width, height);

  window_surface = cairo_surface_create_similar (back_surface,
                                                 CAIRO_CONTENT_COLOR_ALPHA,
                                                 width, height);

  cr = cairo_create (window_surface);
  cairo_set_source_surface (cr, back_surface, 0, 0);
  cairo_paint (cr);

  if (frame != NULL)
    {
      cairo_rectangle_int_t rect = { 0, 0, width, height};
      cairo_region_t *region;
      Screen *xscreen;
      XRenderPictFormat *format;
      cairo_surface_t *mask;

      region = cairo_region_create_rectangle (&rect);
      cairo_region_subtract (region, client_region);

      xscreen = DefaultScreenOfDisplay (xdisplay);
      format = XRenderFindStandardFormat (xdisplay, PictStandardA8);
      mask = cairo_xlib_surface_create_with_xrender_format (xdisplay, mask_pixmap,
                                                            xscreen, format,
                                                            width, height);

      gdk_cairo_region (cr, region);
      cairo_clip (cr);

      cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
      cairo_set_source_rgba (cr, 0, 0, 0, 0);
      cairo_paint (cr);

      cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
      cairo_set_source_surface (cr, back_surface, 0, 0);
      cairo_mask_surface (cr, mask, 0, 0);
      cairo_fill (cr);

      cairo_surface_destroy (mask);
      cairo_region_destroy (region);
    }

  cairo_destroy (cr);
  cairo_surface_destroy (back_surface);
  cairo_region_destroy (client_region);

  return window_surface;
}

static void
meta_compositor_xrender_maximize_window (MetaCompositor *compositor,
                                         MetaWindow     *window)
{
  MetaCompositorXRender *xrender = META_COMPOSITOR_XRENDER (compositor);
  MetaCompWindow *cw = find_comp_window_by_window (xrender, window);

  if (!cw)
    return;

  cw->needs_shadow = window_has_shadow (xrender, cw);
}

static void
meta_compositor_xrender_unmaximize_window (MetaCompositor *compositor,
                                           MetaWindow     *window)
{
  MetaCompositorXRender *xrender = META_COMPOSITOR_XRENDER (compositor);
  MetaCompWindow *cw = find_comp_window_by_window (xrender, window);

  if (!cw)
    return;

  cw->needs_shadow = window_has_shadow (xrender, cw);
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

  damage_screen (xrender);
}

static void
meta_compositor_xrender_sync_stack (MetaCompositor *compositor,
                                    GList          *stack)
{
  MetaCompositorXRender *xrender;
  GList *tmp;

  xrender = META_COMPOSITOR_XRENDER (compositor);

  for (tmp = stack; tmp != NULL; tmp = tmp->next)
    {
      MetaWindow *window;
      MetaCompWindow *cw;

      window = (MetaWindow *) tmp->data;
      cw = find_comp_window_by_window (xrender, window);

      if (cw == NULL)
        {
          g_warning ("Failed to find MetaCompWindow for MetaWindow %p", window);
          continue;
        }

      xrender->windows = g_list_remove (xrender->windows, cw);
      xrender->windows = g_list_prepend (xrender->windows, cw);
    }

  xrender->windows = g_list_reverse (xrender->windows);

  damage_screen (xrender);
  add_repair (xrender);
}

static void
meta_compositor_xrender_sync_window_geometry (MetaCompositor *compositor,
                                              MetaWindow     *window)
{
  MetaCompositorXRender *xrender;
  MetaCompWindow *cw;
  MetaRectangle old_rect;
  XserverRegion damage;

  xrender = META_COMPOSITOR_XRENDER (compositor);
  cw = find_comp_window_by_window (xrender, window);

  if (cw == NULL)
    return;

  meta_error_trap_push (window->display);

  old_rect = cw->rect;
  meta_window_get_input_rect (window, &cw->rect);

  if (xrender->debug)
    {
      fprintf (stderr, "configure notify %d %d %d\n", cw->damaged,
               cw->shape_region != None, cw->needs_shadow);
      dump_xserver_region (xrender, "\textents", cw->extents);
      fprintf (stderr, "\txy (%d %d), wh (%d %d)\n",
               cw->rect.x, cw->rect.y, cw->rect.width, cw->rect.height);
    }

  if (cw->extents)
    {
      damage = XFixesCreateRegion (xrender->xdisplay, NULL, 0);
      XFixesCopyRegion (xrender->xdisplay, damage, cw->extents);
    }
  else
    {
      damage = None;
      if (xrender->debug)
        fprintf (stderr, "no extents to damage !\n");
    }

  if (cw->rect.width != old_rect.width || cw->rect.height != old_rect.height)
    {
      if (cw->shaded.back_pixmap != None)
        {
          XFreePixmap (xrender->xdisplay, cw->shaded.back_pixmap);
          cw->shaded.back_pixmap = None;
        }

      if (cw->shaded.mask_pixmap != None)
        {
          XFreePixmap (xrender->xdisplay, cw->shaded.mask_pixmap);
          cw->shaded.mask_pixmap = None;
        }

      if (cw->shaded.client_region != None)
        {
          XFixesDestroyRegion (xrender->xdisplay, cw->shaded.client_region);
          cw->shaded.client_region = None;
        }

      if (cw->back_pixmap != None)
        {
          /* If the window is shaded, we store the old backing pixmap
           * so we can return a proper image of the window
           */
          if (meta_window_is_shaded (cw->window))
            {
              cw->shaded.back_pixmap = cw->back_pixmap;
              cw->back_pixmap = None;
            }
          else
            {
              XFreePixmap (xrender->xdisplay, cw->back_pixmap);
              cw->back_pixmap = None;
            }
        }

      if (cw->mask_pixmap != None)
        {
          /* If the window is shaded, we store the old backing pixmap
           * so we can return a proper image of the window
           */
          if (meta_window_is_shaded (cw->window))
            {
              cw->shaded.mask_pixmap = cw->mask_pixmap;
              cw->mask_pixmap = None;
            }
          else
            {
              XFreePixmap (xrender->xdisplay, cw->mask_pixmap);
              cw->mask_pixmap = None;
            }
        }

      if (meta_window_is_shaded (cw->window))
        {
          cw->shaded.x = old_rect.x;
          cw->shaded.y = old_rect.y;
          cw->shaded.width = old_rect.width;
          cw->shaded.height = old_rect.height;

          if (cw->client_region != None)
            {
              cw->shaded.client_region = XFixesCreateRegion (xrender->xdisplay,
                                                             NULL, 0);

              XFixesCopyRegion (xrender->xdisplay, cw->shaded.client_region,
                                cw->client_region);
            }
        }

      if (cw->picture != None)
        {
          XRenderFreePicture (xrender->xdisplay, cw->picture);
          cw->picture = None;
        }

      if (cw->mask != None)
        {
          XRenderFreePicture (xrender->xdisplay, cw->mask);
          cw->mask = None;
        }

      if (cw->shadow != None)
        {
          XRenderFreePicture (xrender->xdisplay, cw->shadow);
          cw->shadow = None;
        }
    }

  if (cw->extents)
    XFixesDestroyRegion (xrender->xdisplay, cw->extents);

  cw->extents = win_extents (xrender, cw);

  if (damage)
    {
      if (xrender->debug)
        fprintf (stderr, "Inexplicable intersection with new extents!\n");

      XFixesUnionRegion (xrender->xdisplay, damage, damage, cw->extents);
    }
  else
    {
      damage = XFixesCreateRegion (xrender->xdisplay, NULL, 0);
      XFixesCopyRegion (xrender->xdisplay, damage, cw->extents);
    }

  if (cw->shape_region != None)
    {
      gint dx;
      gint dy;

      dx = cw->rect.x - old_rect.x;
      dy = cw->rect.y - old_rect.y;

      XFixesUnionRegion (xrender->xdisplay, damage, damage, cw->shape_region);
      XFixesTranslateRegion (xrender->xdisplay, cw->shape_region, dx, dy);
    }

  dump_xserver_region (xrender, "sync_window_geometry", damage);
  add_damage (xrender, damage);

  xrender->clip_changed = TRUE;

  meta_error_trap_pop (window->display);
}

static void
meta_compositor_xrender_redraw (MetaCompositor *compositor)
{
  MetaCompositorXRender *xrender;
  MetaDisplay *display;

  xrender = META_COMPOSITOR_XRENDER (compositor);
  display = meta_compositor_get_display (compositor);

  if (xrender->all_damage == None)
    return;

  meta_error_trap_push (display);

  paint_all (xrender, xrender->all_damage);
  XFixesDestroyRegion (xrender->xdisplay, xrender->all_damage);
  xrender->all_damage = None;
  xrender->clip_changed = FALSE;

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
  compositor_class->show_window = meta_compositor_xrender_show_window;
  compositor_class->hide_window = meta_compositor_xrender_hide_window;
  compositor_class->window_opacity_changed = meta_compositor_xrender_window_opacity_changed;
  compositor_class->window_opaque_region_changed = meta_compositor_xrender_window_opaque_region_changed;
  compositor_class->window_shape_region_changed = meta_compositor_xrender_window_shape_region_changed;
  compositor_class->set_updates_frozen = meta_compositor_xrender_set_updates_frozen;
  compositor_class->process_event = meta_compositor_xrender_process_event;
  compositor_class->get_window_surface = meta_compositor_xrender_get_window_surface;
  compositor_class->maximize_window = meta_compositor_xrender_maximize_window;
  compositor_class->unmaximize_window = meta_compositor_xrender_unmaximize_window;
  compositor_class->sync_screen_size = meta_compositor_xrender_sync_screen_size;
  compositor_class->sync_stack = meta_compositor_xrender_sync_stack;
  compositor_class->sync_window_geometry = meta_compositor_xrender_sync_window_geometry;
  compositor_class->redraw = meta_compositor_xrender_redraw;
}

static void
meta_compositor_xrender_init (MetaCompositorXRender *xrender)
{
}
