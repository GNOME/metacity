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
#include "window.h"
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

typedef enum _MetaCompWindowType
{
  META_COMP_WINDOW_NORMAL,
  META_COMP_WINDOW_DND,
  META_COMP_WINDOW_DESKTOP,
  META_COMP_WINDOW_DOCK,
  META_COMP_WINDOW_MENU,
  META_COMP_WINDOW_DROP_DOWN_MENU,
  META_COMP_WINDOW_TOOLTIP,
} MetaCompWindowType;

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
  MetaScreen *screen;
  MetaWindow *window; /* May be NULL if this window isn't managed by Metacity */
  Window id;
  XWindowAttributes attrs;

  Pixmap back_pixmap;
  Pixmap mask_pixmap;

  int mode;

  gboolean damaged;
  gboolean shaped;

  XRectangle shape_bounds;

  MetaCompWindowType type;

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

  guint opacity;

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

  MetaWindow     *focus_window;

  Window          overlay_window;

  gboolean        have_shadows;
  shadow         *shadows[LAST_SHADOW_TYPE];

  Picture         root_picture;
  Picture         root_buffer;
  Picture         black_picture;
  Picture         root_tile;
  XserverRegion   all_damage;

  gboolean        clip_changed;

  GSList         *dock_windows;

  guint           repaint_id;

  guint           show_redraw : 1;
  guint           debug : 1;
};

G_DEFINE_TYPE (MetaCompositorXRender, meta_compositor_xrender, META_TYPE_COMPOSITOR)

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

  if (!cw->window)
    return;

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
find_window (MetaCompositorXRender *xrender,
             Window                 xwindow)
{
  return g_hash_table_lookup (xrender->windows_by_xid, (gpointer) xwindow);
}

static MetaCompWindow *
find_window_for_child_window (MetaCompositorXRender *xrender,
                              Window                 xwindow)
{
  Window ignored1, *ignored2;
  Window parent;
  guint ignored_children;

  XQueryTree (xrender->xdisplay, xwindow, &ignored1,
              &parent, &ignored2, &ignored_children);

  if (parent != None)
    return find_window (xrender, parent);

  return NULL;
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
create_root_buffer (MetaCompositorXRender *xrender,
                    MetaScreen            *screen)
{
  Display *xdisplay = xrender->xdisplay;
  Picture pict;
  XRenderPictFormat *format;
  Pixmap root_pixmap;
  Visual *visual;
  int depth, screen_width, screen_height, screen_number;

  meta_screen_get_size (screen, &screen_width, &screen_height);
  screen_number = meta_screen_get_screen_number (screen);
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
            MetaScreen            *screen,
            Picture                root_buffer)
{
  int width, height;

  g_return_if_fail (root_buffer != None);

  if (xrender->root_tile == None)
    {
      xrender->root_tile = root_tile (screen);
      g_return_if_fail (xrender->root_tile != None);
    }

  meta_screen_get_size (screen, &width, &height);
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

  /* Always put a shadow around windows with a frame. This should override
   * the restriction about not putting a shadow around shaped windows as the
   * frame might be the reason the window is shaped.
   */
  if (cw->window)
    {
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

      /* Do not add shadows if GTK+ theme is used */
      if (meta_prefs_get_theme_type () == META_THEME_TYPE_GTK)
        {
          meta_verbose ("Window has shadow from GTK+ theme\n");
          return FALSE;
        }

      if (meta_window_get_frame (cw->window))
        {
          meta_verbose ("Window has shadow because it has a frame\n");
          return TRUE;
        }
    }

  /* Do not add shadows to ARGB windows */
  if (cw->mode == WINDOW_ARGB)
    {
      meta_verbose ("Window has no shadow as it is ARGB\n");
      return FALSE;
    }

  /* Never put a shadow around shaped windows */
  if (cw->shaped) {
    meta_verbose ("Window has no shadow as it is shaped\n");
    return FALSE;
  }

  /* Don't put shadow around DND icon windows */
  if (cw->type == META_COMP_WINDOW_DND ||
      cw->type == META_COMP_WINDOW_DESKTOP) {
    meta_verbose ("Window has no shadow as it is DND or Desktop\n");
    return FALSE;
  }

  if (cw->mode != WINDOW_ARGB) {
    meta_verbose ("Window has shadow as it is not ARGB\n");
    return TRUE;
  }

  if (cw->type == META_COMP_WINDOW_MENU ||
      cw->type == META_COMP_WINDOW_DROP_DOWN_MENU) {
    meta_verbose ("Window has shadow as it is a menu\n");
    return TRUE;
  }

  if (cw->type == META_COMP_WINDOW_TOOLTIP) {
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

  r.x = cw->attrs.x;
  r.y = cw->attrs.y;
  r.width = cw->attrs.width + cw->attrs.border_width * 2;
  r.height = cw->attrs.height + cw->attrs.border_width * 2;

  if (cw->needs_shadow)
    {
      MetaFrameBorders borders;
      XRectangle sr;

      meta_frame_borders_clear (&borders);

      if (cw->window)
        {
          MetaFrame *frame = meta_window_get_frame (cw->window);

          if (frame)
            meta_frame_calc_borders (frame, &borders);
        }

      cw->shadow_dx = (int) shadow_offsets_x [cw->shadow_type] + borders.invisible.left;
      cw->shadow_dy = (int) shadow_offsets_y [cw->shadow_type] + borders.invisible.top;

      if (!cw->shadow)
        {
          double opacity = SHADOW_OPACITY;
          int invisible_width = borders.invisible.left + borders.invisible.right;
          int invisible_height = borders.invisible.top + borders.invisible.bottom;

          if (cw->opacity != (guint) OPAQUE)
            opacity = opacity * ((double) cw->opacity) / ((double) OPAQUE);

          cw->shadow = shadow_picture (xrender, cw, opacity, borders,
                                       cw->attrs.width - invisible_width + cw->attrs.border_width * 2,
                                       cw->attrs.height - invisible_height + cw->attrs.border_width * 2,
                                       &cw->shadow_width, &cw->shadow_height);
        }

      sr.x = cw->attrs.x + cw->shadow_dx;
      sr.y = cw->attrs.y + cw->shadow_dy;
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
get_window_region (MetaCompWindow *cw)
{
  MetaDisplay *display;
  Display *xdisplay;
  XserverRegion region;
  int x;
  int y;

  display = meta_screen_get_display (cw->screen);
  xdisplay = meta_display_get_xdisplay (display);

  meta_error_trap_push (display);
  region = XFixesCreateRegionFromWindow (xdisplay, cw->id, WindowRegionBounding);
  meta_error_trap_pop (display);

  if (region == None)
    return None;

  x = cw->attrs.x + cw->attrs.border_width;
  y = cw->attrs.y + cw->attrs.border_width;

  XFixesTranslateRegion (xdisplay, region, x, y);

  return region;
}

static XserverRegion
get_client_region (MetaCompWindow *cw)
{
  MetaDisplay *display;
  Display *xdisplay;
  XserverRegion region;
  MetaFrame *frame;

  display = meta_screen_get_display (cw->screen);
  xdisplay = meta_display_get_xdisplay (display);

  if (cw->window_region != None)
    {
      region = XFixesCreateRegion (xdisplay, NULL, 0);
      XFixesCopyRegion (xdisplay, region, cw->window_region);
    }
  else
    {
      region = get_window_region (cw);
      if (region == None)
        return None;
    }

  frame = cw->window ? meta_window_get_frame (cw->window) : NULL;

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

      x = cw->attrs.x;
      y = cw->attrs.y;
      width = cw->attrs.width + cw->attrs.border_width * 2;
      height = cw->attrs.height + cw->attrs.border_width * 2;

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
get_visible_region (MetaCompWindow *cw)
{
  MetaDisplay *display;
  Display *xdisplay;
  XserverRegion region;

  display = meta_screen_get_display (cw->screen);
  xdisplay = meta_display_get_xdisplay (display);

  if (cw->window_region != None)
    {
      region = XFixesCreateRegion (xdisplay, NULL, 0);
      XFixesCopyRegion (xdisplay, region, cw->window_region);
    }
  else
    {
      region = get_window_region (cw);
      if (region == None)
        return None;
    }

  if (cw->window)
    {
      cairo_region_t *visible;
      XserverRegion tmp;

      visible = meta_window_get_frame_bounds (cw->window);
      tmp = visible ? cairo_region_to_xserver_region (xdisplay, visible) : None;

      if (tmp != None)
        {
          int x;
          int y;

          x = cw->attrs.x + cw->attrs.border_width;
          y = cw->attrs.y + cw->attrs.border_width;

          XFixesTranslateRegion (xdisplay, tmp, x, y);
          XFixesIntersectRegion (xdisplay, region, region, tmp);
          XFixesDestroyRegion (xdisplay, tmp);
        }
    }

  return region;
}

static XRenderPictFormat *
get_window_format (Display        *xdisplay,
                   MetaCompWindow *cw)
{
  XRenderPictFormat *format;

  format = XRenderFindVisualFormat (xdisplay, cw->attrs.visual);

  if (!format)
    {
      Visual *visual;

      visual = DefaultVisual (xdisplay, DefaultScreen (xdisplay));
      format = XRenderFindVisualFormat (xdisplay, visual);
    }

  return format;
}

static Picture
get_window_picture (MetaCompWindow *cw)
{
  MetaScreen *screen = cw->screen;
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  XRenderPictureAttributes pa;
  XRenderPictFormat *format;
  Drawable draw;
  int error_code;

  draw = cw->id;

  meta_error_trap_push (display);

  if (cw->back_pixmap == None)
    cw->back_pixmap = XCompositeNameWindowPixmap (xdisplay, cw->id);

  error_code = meta_error_trap_pop_with_return (display);
  if (error_code != 0)
    cw->back_pixmap = None;

  if (cw->back_pixmap != None)
    draw = cw->back_pixmap;

  format = get_window_format (xdisplay, cw);
  if (format)
    {
      Picture pict;

      pa.subwindow_mode = IncludeInferiors;

      meta_error_trap_push (display);
      pict = XRenderCreatePicture (xdisplay, draw, format, CPSubwindowMode, &pa);
      meta_error_trap_pop (display);

      return pict;
    }

  return None;
}

static Picture
get_window_mask (MetaCompWindow *cw)
{
  MetaFrame *frame;
  MetaDisplay *display;
  Display *xdisplay;
  int width;
  int height;
  XRenderPictFormat *format;
  cairo_surface_t *surface;
  cairo_t *cr;
  Picture picture;

  if (cw->window == NULL)
    return None;

  frame = meta_window_get_frame (cw->window);
  if (frame == NULL)
    return None;

  display = meta_screen_get_display (cw->screen);
  xdisplay = meta_display_get_xdisplay (display);
  width = cw->attrs.width + cw->attrs.border_width * 2;
  height = cw->attrs.height + cw->attrs.border_width * 2;
  format = XRenderFindStandardFormat (xdisplay, PictStandardA8);

  if (cw->mask_pixmap == None)
    {
      meta_error_trap_push (display);
      cw->mask_pixmap = XCreatePixmap (xdisplay, cw->id, width, height,
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
  GSList *d;

  for (d = xrender->dock_windows; d; d = d->next)
    {
      MetaCompWindow *cw = d->data;
      XserverRegion shadow_clip;

      if (cw->shadow)
        {
          shadow_clip = XFixesCreateRegion (xdisplay, NULL, 0);
          XFixesIntersectRegion (xdisplay, shadow_clip,
                                 cw->border_clip, region);

          XFixesSetPictureClipRegion (xdisplay, root_buffer, 0, 0, shadow_clip);

          XRenderComposite (xdisplay, PictOpOver, xrender->black_picture,
                            cw->shadow, root_buffer,
                            0, 0, 0, 0,
                            cw->attrs.x + cw->shadow_dx,
                            cw->attrs.y + cw->shadow_dy,
                            cw->shadow_width, cw->shadow_height);
          XFixesDestroyRegion (xdisplay, shadow_clip);
        }
    }
}

static void
paint_windows (MetaCompositorXRender *xrender,
               MetaScreen            *screen,
               GList                 *windows,
               Picture                root_buffer,
               XserverRegion          region)
{
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  GList *index, *last;
  int screen_width, screen_height;
  MetaCompWindow *cw;
  XserverRegion paint_region, desktop_region;

  meta_screen_get_size (screen, &screen_width, &screen_height);

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

      if (cw->attrs.map_state != IsViewable)
        continue;

#if 0
      if ((cw->attrs.x + cw->attrs.width < 1) ||
          (cw->attrs.y + cw->attrs.height < 1) ||
          (cw->attrs.x >= screen_width) || (cw->attrs.y >= screen_height))
        {
          /* Off screen */
          continue;
        }
#endif

      if (cw->picture == None)
        cw->picture = get_window_picture (cw);

      if (cw->mask == None)
        cw->mask = get_window_mask (cw);

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

#if 0
          if (cw->extents)
            {
              XFixesDestroyRegion (xdisplay, cw->extents);
              cw->extents = None;
            }
#endif
        }

      if (cw->window_region == None)
        cw->window_region = get_window_region (cw);

      if (cw->visible_region == None)
        cw->visible_region = get_visible_region (cw);

      if (cw->client_region == None)
        cw->client_region = get_client_region (cw);

      if (cw->extents == None)
        cw->extents = win_extents (xrender, cw);

      if (cw->mode == WINDOW_SOLID)
        {
          int x, y, wid, hei;
          MetaFrame *frame;
          MetaFrameBorders borders;

          x = cw->attrs.x;
          y = cw->attrs.y;
          wid = cw->attrs.width + cw->attrs.border_width * 2;
          hei = cw->attrs.height + cw->attrs.border_width * 2;

          frame = cw->window ? meta_window_get_frame (cw->window) : NULL;
          meta_frame_calc_borders (frame, &borders);

          XFixesSetPictureClipRegion (xdisplay, root_buffer,
                                      0, 0, paint_region);
          XRenderComposite (xdisplay, PictOpSrc, cw->picture, None, root_buffer,
                            borders.total.left, borders.total.top, 0, 0,
                            x + borders.total.left, y + borders.total.top,
                            wid - borders.total.left - borders.total.right,
                            hei - borders.total.top - borders.total.bottom);

          if (cw->type == META_COMP_WINDOW_DESKTOP)
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
  paint_root (xrender, screen, root_buffer);

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

          x = cw->attrs.x;
          y = cw->attrs.y;
          wid = cw->attrs.width + cw->attrs.border_width * 2;
          hei = cw->attrs.height + cw->attrs.border_width * 2;

          if (cw->shadow && cw->type != META_COMP_WINDOW_DOCK)
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

          if ((cw->opacity != (guint) OPAQUE) && !(cw->alpha_pict))
            {
              cw->alpha_pict = solid_picture (xdisplay, FALSE,
                                              (double) cw->opacity / OPAQUE,
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
                                None, root_buffer, 0, 0, 0, 0,
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
                                None, root_buffer, 0, 0, 0, 0,
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
           MetaScreen            *screen,
           XserverRegion          region)
{
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  int screen_width, screen_height;

  /* Set clipping to the given region */
  XFixesSetPictureClipRegion (xdisplay, xrender->root_picture, 0, 0, region);

  meta_screen_get_size (screen, &screen_width, &screen_height);

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
    xrender->root_buffer = create_root_buffer (xrender, screen);

  paint_windows (xrender, screen, xrender->windows, xrender->root_buffer, region);

  XFixesSetPictureClipRegion (xdisplay, xrender->root_buffer, 0, 0, region);
  XRenderComposite (xdisplay, PictOpSrc, xrender->root_buffer, None,
                    xrender->root_picture, 0, 0, 0, 0, 0, 0,
                    screen_width, screen_height);
}

static void
repair_display (MetaCompositorXRender *xrender)
{
  MetaCompositor *compositor = META_COMPOSITOR (xrender);
  MetaDisplay *display = meta_compositor_get_display (compositor);
  Display *xdisplay = meta_display_get_xdisplay (display);
  MetaScreen *screen = meta_display_get_screen (display);

  if (xrender->all_damage != None)
    {
      meta_error_trap_push (display);

      paint_all (xrender, screen, xrender->all_damage);
      XFixesDestroyRegion (xdisplay, xrender->all_damage);
      xrender->all_damage = None;
      xrender->clip_changed = FALSE;

      meta_error_trap_pop (display);
    }
}

static gboolean
compositor_idle_cb (gpointer data)
{
  MetaCompositorXRender *xrender = META_COMPOSITOR_XRENDER (data);

  xrender->repaint_id = 0;
  repair_display (xrender);

  return FALSE;
}

static void
add_repair (MetaCompositorXRender *xrender)
{
  if (xrender->repaint_id > 0)
    return;

#if 1
  xrender->repaint_id = g_idle_add_full (META_PRIORITY_REDRAW,
                                         compositor_idle_cb, xrender,
                                         NULL);
#else
  /* Limit it to 50fps */
  xrender->repaint_id = g_timeout_add_full (G_PRIORITY_HIGH, 20,
                                            compositor_idle_cb, xrender,
                                            NULL);
#endif
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
damage_screen (MetaCompositorXRender *xrender,
               MetaScreen            *screen)
{
  MetaCompositor *compositor = META_COMPOSITOR (xrender);
  MetaDisplay *display = meta_compositor_get_display (compositor);
  Display *xdisplay = meta_display_get_xdisplay (display);
  XserverRegion region;
  int width, height;
  XRectangle r;

  r.x = 0;
  r.y = 0;
  meta_screen_get_size (screen, &width, &height);
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

  meta_error_trap_push (NULL);

  if (!cw->damaged)
    {
      parts = win_extents (xrender, cw);
      XDamageSubtract (xdisplay, cw->damage, None, None);
    }
  else
    {
      parts = XFixesCreateRegion (xdisplay, 0, 0);
      XDamageSubtract (xdisplay, cw->damage, None, parts);
      XFixesTranslateRegion (xdisplay, parts,
                             cw->attrs.x + cw->attrs.border_width,
                             cw->attrs.y + cw->attrs.border_width);
    }

  meta_error_trap_pop (NULL);

  dump_xserver_region (xrender, "repair_win", parts);
  add_damage (xrender, parts);
  cw->damaged = TRUE;
}

static void
free_win (MetaCompositorXRender *xrender,
          MetaCompWindow        *cw,
          gboolean               destroy)
{
  MetaDisplay *display = meta_screen_get_display (cw->screen);
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

      /* The window may not have been added to the list in this case,
         but we can check anyway */
      if (cw->type == META_COMP_WINDOW_DOCK)
        xrender->dock_windows = g_slist_remove (xrender->dock_windows, cw);

      g_free (cw);
    }

  meta_error_trap_pop (display);
}

static void
map_win (MetaCompositorXRender *xrender,
         Window                 id)
{
  MetaCompWindow *cw = find_window (xrender, id);
  Display *xdisplay = xrender->xdisplay;

  if (cw == NULL)
    return;

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

  cw->attrs.map_state = IsViewable;
  cw->damaged = FALSE;
}

static void
unmap_win (MetaCompositorXRender *xrender,
           Window                 id)
{
  MetaCompWindow *cw = find_window (xrender, id);

  if (cw == NULL)
    {
      return;
    }

  if (cw->window && cw->window == xrender->focus_window)
    xrender->focus_window = NULL;

  cw->attrs.map_state = IsUnmapped;
  cw->damaged = FALSE;

  if (cw->extents != None)
    {
      dump_xserver_region (xrender, "unmap_win", cw->extents);
      add_damage (xrender, cw->extents);
      cw->extents = None;
    }

  free_win (xrender, cw, FALSE);
  xrender->clip_changed = TRUE;
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

  if (cw->attrs.class == InputOnly)
    format = NULL;
  else
    format = XRenderFindVisualFormat (xdisplay, cw->attrs.visual);

  if ((format && format->type == PictTypeDirect && format->direct.alphaMask)
      || cw->opacity != (guint) OPAQUE)
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

static gboolean
is_shaped (MetaDisplay *display,
           Window       xwindow)
{
  Display *xdisplay = meta_display_get_xdisplay (display);
  int xws, yws, xbs, ybs;
  unsigned wws, hws, wbs, hbs;
  int bounding_shaped, clip_shaped;

  if (meta_display_has_shape (display))
    {
      XShapeQueryExtents (xdisplay, xwindow, &bounding_shaped,
                          &xws, &yws, &wws, &hws, &clip_shaped,
                          &xbs, &ybs, &wbs, &hbs);
      return (bounding_shaped != 0);
    }

  return FALSE;
}

static void
get_window_type (MetaDisplay    *display,
                 MetaCompWindow *cw)
{
  int n_atoms;
  Atom *atoms, type_atom;
  int i;

  type_atom = None;
  n_atoms = 0;
  atoms = NULL;

  meta_prop_get_atom_list (display, cw->id,
                           display->atom__NET_WM_WINDOW_TYPE,
                           &atoms, &n_atoms);

  for (i = 0; i < n_atoms; i++)
    {
      if (atoms[i] == display->atom__NET_WM_WINDOW_TYPE_DND ||
          atoms[i] == display->atom__NET_WM_WINDOW_TYPE_DESKTOP ||
          atoms[i] == display->atom__NET_WM_WINDOW_TYPE_DOCK ||
          atoms[i] == display->atom__NET_WM_WINDOW_TYPE_TOOLBAR ||
          atoms[i] == display->atom__NET_WM_WINDOW_TYPE_MENU ||
          atoms[i] == display->atom__NET_WM_WINDOW_TYPE_DIALOG ||
          atoms[i] == display->atom__NET_WM_WINDOW_TYPE_NORMAL ||
          atoms[i] == display->atom__NET_WM_WINDOW_TYPE_UTILITY ||
          atoms[i] == display->atom__NET_WM_WINDOW_TYPE_SPLASH ||
          atoms[i] == display->atom__NET_WM_WINDOW_TYPE_DROPDOWN_MENU ||
          atoms[i] == display->atom__NET_WM_WINDOW_TYPE_TOOLTIP)
        {
          type_atom = atoms[i];
          break;
        }
    }

  meta_XFree (atoms);

  if (type_atom == display->atom__NET_WM_WINDOW_TYPE_DND)
    cw->type = META_COMP_WINDOW_DND;
  else if (type_atom == display->atom__NET_WM_WINDOW_TYPE_DESKTOP)
    cw->type = META_COMP_WINDOW_DESKTOP;
  else if (type_atom == display->atom__NET_WM_WINDOW_TYPE_DOCK)
    cw->type = META_COMP_WINDOW_DOCK;
  else if (type_atom == display->atom__NET_WM_WINDOW_TYPE_MENU)
    cw->type = META_COMP_WINDOW_MENU;
  else if (type_atom == display->atom__NET_WM_WINDOW_TYPE_DROPDOWN_MENU)
    cw->type = META_COMP_WINDOW_DROP_DOWN_MENU;
  else if (type_atom == display->atom__NET_WM_WINDOW_TYPE_TOOLTIP)
    cw->type = META_COMP_WINDOW_TOOLTIP;
  else
    cw->type = META_COMP_WINDOW_NORMAL;

/*   meta_verbose ("Window is %d\n", cw->type); */
}

/* Must be called with an error trap in place */
static void
add_win (MetaCompositorXRender *xrender,
         MetaScreen            *screen,
         MetaWindow            *window,
         Window                 xwindow)
{
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  MetaCompWindow *cw;
  gulong event_mask;

  if (xwindow == xrender->overlay_window)
    return;

  /* If already added, ignore */
  if (find_window (xrender, xwindow) != NULL)
    return;

  cw = g_new0 (MetaCompWindow, 1);
  cw->screen = screen;
  cw->window = window;
  cw->id = xwindow;

  if (!XGetWindowAttributes (xdisplay, xwindow, &cw->attrs))
    {
      g_free (cw);
      return;
    }
  get_window_type (display, cw);

  /* If Metacity has decided not to manage this window then the input events
     won't have been set on the window */
  event_mask = cw->attrs.your_event_mask | PropertyChangeMask;

  XSelectInput (xdisplay, xwindow, event_mask);

  cw->back_pixmap = None;
  cw->mask_pixmap = None;

  cw->damaged = FALSE;
  cw->shaped = is_shaped (display, xwindow);

  cw->shape_bounds.x = cw->attrs.x;
  cw->shape_bounds.y = cw->attrs.y;
  cw->shape_bounds.width = cw->attrs.width;
  cw->shape_bounds.height = cw->attrs.height;

  if (cw->attrs.class == InputOnly)
    cw->damage = None;
  else
    cw->damage = XDamageCreate (xdisplay, xwindow, XDamageReportNonEmpty);

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

  if (window && meta_window_has_focus (window))
    cw->shadow_type = META_SHADOW_LARGE;
  else
    cw->shadow_type = META_SHADOW_MEDIUM;

  cw->opacity = OPAQUE;

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

  /* Only add the window to the list of docks if it needs a shadow */
  if (cw->type == META_COMP_WINDOW_DOCK && cw->needs_shadow)
    {
      meta_verbose ("Appending %p to dock windows\n", cw);
      xrender->dock_windows = g_slist_append (xrender->dock_windows, cw);
    }

  /* Add this to the list at the top of the stack
     before it is mapped so that map_win can find it again */
  xrender->windows = g_list_prepend (xrender->windows, cw);
  g_hash_table_insert (xrender->windows_by_xid, (gpointer) xwindow, cw);

  if (cw->attrs.map_state == IsViewable)
    map_win (xrender, xwindow);
}

static void
restack_win (MetaCompositorXRender *xrender,
             MetaCompWindow        *cw,
             Window                 above)
{
  Window previous_above;
  GList *sibling, *next;

  sibling = g_list_find (xrender->windows, (gconstpointer) cw);
  next = g_list_next (sibling);
  previous_above = None;

  if (next)
    {
      MetaCompWindow *ncw = (MetaCompWindow *) next->data;
      previous_above = ncw->id;
    }

  /* If above is set to None, the window whose state was changed is on
   * the bottom of the stack with respect to sibling.
   */
  if (above == None)
    {
      /* Insert at bottom of window stack */
      xrender->windows = g_list_delete_link (xrender->windows, sibling);
      xrender->windows = g_list_append (xrender->windows, cw);
    }
  else if (previous_above != above)
    {
      GList *index;

      for (index = xrender->windows; index; index = index->next) {
        MetaCompWindow *cw2 = (MetaCompWindow *) index->data;
        if (cw2->id == above)
          break;
      }

      if (index != NULL)
        {
          xrender->windows = g_list_delete_link (xrender->windows, sibling);
          xrender->windows = g_list_insert_before (xrender->windows, index, cw);
        }
    }
}

static void
resize_win (MetaCompositorXRender *xrender,
            MetaCompWindow        *cw,
            int                    x,
            int                    y,
            int                    width,
            int                    height,
            int                    border_width,
            gboolean               override_redirect)
{
  MetaScreen *screen = cw->screen;
  MetaDisplay *display = meta_screen_get_display (screen);
  Display *xdisplay = meta_display_get_xdisplay (display);
  XserverRegion damage;
  XserverRegion shape;

  if (cw->extents)
    {
      damage = XFixesCreateRegion (xdisplay, NULL, 0);
      XFixesCopyRegion (xdisplay, damage, cw->extents);
    }
  else
    {
      damage = None;
      if (xrender->debug)
        fprintf (stderr, "no extents to damage !\n");
    }

  /*  { // Damage whole screen each time ! ;-)
    XRectangle r;

    r.x = 0;
    r.y = 0;
    meta_screen_get_size (screen, &r.width, &r.height);
    fprintf (stderr, "Damage whole screen %d,%d (%d %d)\n",
             r.x, r.y, r.width, r.height);

    damage = XFixesCreateRegion (xdisplay, &r, 1);
    } */

  if (cw->attrs.width != width || cw->attrs.height != height)
    {
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

      if (cw->back_pixmap)
        {
          /* If the window is shaded, we store the old backing pixmap
           * so we can return a proper image of the window
           */
          if (cw->window && meta_window_is_shaded (cw->window))
            {
              cw->shaded.back_pixmap = cw->back_pixmap;
              cw->back_pixmap = None;
            }
          else
            {
              XFreePixmap (xdisplay, cw->back_pixmap);
              cw->back_pixmap = None;
            }
        }

      if (cw->mask_pixmap)
        {
          /* If the window is shaded, we store the old backing pixmap
           * so we can return a proper image of the window
           */
          if (cw->window && meta_window_is_shaded (cw->window))
            {
              cw->shaded.mask_pixmap = cw->mask_pixmap;
              cw->mask_pixmap = None;
            }
          else
            {
              XFreePixmap (xdisplay, cw->mask_pixmap);
              cw->mask_pixmap = None;
            }
        }

      if (cw->window && meta_window_is_shaded (cw->window))
        {
          cw->shaded.x = cw->attrs.x;
          cw->shaded.y = cw->attrs.y;
          cw->shaded.width = cw->attrs.width;
          cw->shaded.height = cw->attrs.height;

          if (cw->client_region != None)
            {
              cw->shaded.client_region = XFixesCreateRegion (xdisplay, NULL, 0);

              XFixesCopyRegion (xdisplay, cw->shaded.client_region,
                                cw->client_region);
            }
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
    }

  cw->attrs.x = x;
  cw->attrs.y = y;
  cw->attrs.width = width;
  cw->attrs.height = height;
  cw->attrs.border_width = border_width;
  cw->attrs.override_redirect = override_redirect;

  if (cw->extents)
    XFixesDestroyRegion (xdisplay, cw->extents);

  cw->extents = win_extents (xrender, cw);

  if (damage)
    {
      if (xrender->debug)
        fprintf (stderr, "Inexplicable intersection with new extents!\n");

      XFixesUnionRegion (xdisplay, damage, damage, cw->extents);
    }
  else
    {
      damage = XFixesCreateRegion (xdisplay, NULL, 0);
      XFixesCopyRegion (xdisplay, damage, cw->extents);
    }

  shape = XFixesCreateRegion (xdisplay, &cw->shape_bounds, 1);
  XFixesUnionRegion (xdisplay, damage, damage, shape);
  XFixesDestroyRegion (xdisplay, shape);

  dump_xserver_region (xrender, "resize_win", damage);
  add_damage (xrender, damage);

  xrender->clip_changed = TRUE;
}

/* event processors must all be called with an error trap in place */
static void
process_configure_notify (MetaCompositorXRender *xrender,
                          XConfigureEvent       *event)
{
  MetaCompositor *compositor = META_COMPOSITOR (xrender);
  MetaDisplay *display = meta_compositor_get_display (compositor);
  Display *xdisplay = meta_display_get_xdisplay (display);
  MetaCompWindow *cw = find_window (xrender, event->window);

  if (cw)
    {
      if (xrender->debug)
        {
          fprintf (stderr, "configure notify %d %d %d\n", cw->damaged,
                   cw->shaped, cw->needs_shadow);
          dump_xserver_region (xrender, "\textents", cw->extents);
          fprintf (stderr, "\txy (%d %d), wh (%d %d)\n",
                   event->x, event->y, event->width, event->height);
        }

      restack_win (xrender, cw, event->above);
      resize_win (xrender, cw, event->x, event->y, event->width, event->height,
                  event->border_width, event->override_redirect);
    }
  else
    {
      MetaScreen *screen;

      /* Might be the root window? */
      screen = meta_display_screen_for_root (display, event->window);
      if (screen == NULL)
        return;

      if (xrender->root_buffer)
        {
          XRenderFreePicture (xdisplay, xrender->root_buffer);
          xrender->root_buffer = None;
        }

      damage_screen (xrender, screen);
    }
}

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
      screen = meta_display_screen_for_root (display, event->window);
      if (screen)
        {
          Window xroot = meta_screen_get_xroot (screen);

          if (xrender->root_tile)
            {
              XClearArea (xdisplay, xroot, 0, 0, 0, 0, TRUE);
              XRenderFreePicture (xdisplay, xrender->root_tile);
              xrender->root_tile = None;

              /* Damage the whole screen as we may need to redraw the
                 background ourselves */
              damage_screen (xrender, screen);

              add_repair (xrender);

              return;
            }
        }
    }

  /* Check for the opacity changing */
  if (event->atom == display->atom__NET_WM_WINDOW_OPACITY)
    {
      MetaCompWindow *cw = find_window (xrender, event->window);
      gulong value;

      if (!cw)
        {
          /* Applications can set this for their toplevel windows, so
           * this must be propagated to the window managed by the compositor
           */
          cw = find_window_for_child_window (xrender, event->window);
        }

      if (!cw)
        return;

      if (meta_prop_get_cardinal (display, event->window,
                                  display->atom__NET_WM_WINDOW_OPACITY,
                                  &value) == FALSE)
        value = OPAQUE;

      cw->opacity = (guint)value;
      determine_mode (xrender, cw);
      cw->needs_shadow = window_has_shadow (xrender, cw);

      if (cw->shadow)
        {
          XRenderFreePicture (xdisplay, cw->shadow);
          cw->shadow = None;
        }

      if (cw->extents)
        XFixesDestroyRegion (xdisplay, cw->extents);
      cw->extents = win_extents (xrender, cw);

      cw->damaged = TRUE;

      add_repair (xrender);

      return;
    }

  if (event->atom == display->atom__NET_WM_WINDOW_TYPE) {
    MetaCompWindow *cw = find_window (xrender, event->window);

    if (!cw)
      return;

    get_window_type (display, cw);
    cw->needs_shadow = window_has_shadow (xrender, cw);
    return;
  }
}

static void
expose_area (MetaCompositorXRender *xrender,
             XRectangle            *rects,
             int                    nrects)
{
  MetaCompositor *compositor = META_COMPOSITOR (xrender);
  MetaDisplay *display = meta_compositor_get_display (compositor);
  Display *xdisplay = meta_display_get_xdisplay (display);
  XserverRegion region;

  region = XFixesCreateRegion (xdisplay, rects, nrects);

  dump_xserver_region (xrender, "expose_area", region);
  add_damage (xrender, region);
}

static void
process_expose (MetaCompositorXRender *xrender,
                XExposeEvent          *event)
{
  MetaCompWindow *cw = find_window (xrender, event->window);
  XRectangle rect[1];
  int origin_x = 0, origin_y = 0;

  if (cw != NULL)
    {
      origin_x = cw->attrs.x; /* + cw->attrs.border_width; ? */
      origin_y = cw->attrs.y; /* + cw->attrs.border_width; ? */
    }

  rect[0].x = event->x + origin_x;
  rect[0].y = event->y + origin_y;
  rect[0].width = event->width;
  rect[0].height = event->height;

  expose_area (xrender, rect, 1);
}

static void
process_unmap (MetaCompositorXRender *xrender,
               XUnmapEvent           *event)
{
  MetaCompWindow *cw;

  if (event->from_configure)
    {
      /* Ignore unmap caused by parent's resize */
      return;
    }

  cw = find_window (xrender, event->window);
  if (cw)
    unmap_win (xrender, event->window);
}

static void
process_map (MetaCompositorXRender *xrender,
             XMapEvent             *event)
{
  MetaCompWindow *cw = find_window (xrender, event->window);

  if (cw)
    map_win (xrender, event->window);
}

static void
process_damage (MetaCompositorXRender *xrender,
                XDamageNotifyEvent    *event)
{
  MetaCompWindow *cw = find_window (xrender, event->drawable);

  if (cw == NULL)
    return;

  repair_win (xrender, cw);

  if (event->more == FALSE)
    add_repair (xrender);
}

static void
process_shape (MetaCompositorXRender *xrender,
               XShapeEvent           *event)
{
  MetaCompWindow *cw = find_window (xrender, event->window);

  if (cw == NULL)
    return;

  if (event->kind == ShapeBounding)
    {
      if (!event->shaped && cw->shaped)
        cw->shaped = FALSE;

      resize_win (xrender, cw, cw->attrs.x, cw->attrs.y,
                  event->width + event->x, event->height + event->y,
                  cw->attrs.border_width, cw->attrs.override_redirect);

      if (event->shaped && !cw->shaped)
        cw->shaped = TRUE;

      if (event->shaped == True)
        {
          cw->shape_bounds.x = cw->attrs.x + event->x;
          cw->shape_bounds.y = cw->attrs.y + event->y;
          cw->shape_bounds.width = event->width;
          cw->shape_bounds.height = event->height;
        }
      else
        {
          cw->shape_bounds.x = cw->attrs.x;
          cw->shape_bounds.y = cw->attrs.y;
          cw->shape_bounds.width = cw->attrs.width;
          cw->shape_bounds.height = cw->attrs.height;
        }
    }
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

      if (cw->window && cw->shadow)
        {
          XRenderFreePicture (xrender->xdisplay, cw->shadow);
          cw->shadow = None;
        }

      cw->needs_shadow = window_has_shadow (xrender, cw);
    }
}

static void
show_overlay_window (MetaCompositorXRender *xrender,
                     MetaScreen            *screen,
                     Display               *xdisplay)
{
  XserverRegion region;

  region = XFixesCreateRegion (xdisplay, NULL, 0);

  XFixesSetWindowShapeRegion (xdisplay, xrender->overlay_window,
                              ShapeBounding, 0, 0, 0);

  XFixesSetWindowShapeRegion (xdisplay, xrender->overlay_window,
                              ShapeInput, 0, 0, region);

  XFixesDestroyRegion (xdisplay, region);

  damage_screen (xrender, screen);
}

static void
hide_overlay_window (MetaCompositorXRender *xrender,
                     Display               *xdisplay)
{
  XserverRegion region;

  region = XFixesCreateRegion (xdisplay, NULL, 0);

  XFixesSetWindowShapeRegion (xdisplay, xrender->overlay_window,
                              ShapeBounding, 0, 0, region);

  XFixesDestroyRegion (xdisplay, region);
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
  Window xroot = meta_screen_get_xroot (screen);

  gdk_error_trap_push ();
  XCompositeRedirectSubwindows (xdisplay, xroot, CompositeRedirectManual);
  XSync (xdisplay, FALSE);

  if (gdk_error_trap_pop ())
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Another compositing manager is running on screen %i",
                   screen_number);

      return FALSE;
    }

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

  xrender->overlay_window = XCompositeGetOverlayWindow (xdisplay, xroot);
  XSelectInput (xdisplay, xrender->overlay_window, ExposureMask);

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

  xrender->focus_window = meta_display_get_focus_window (display);

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

  meta_screen_set_cm_selection (screen);

  show_overlay_window (xrender, screen, xdisplay);

  meta_prefs_add_listener (update_shadows, xrender);

  g_timeout_add (2000, (GSourceFunc) timeout_debug, compositor);

  return TRUE;
}

static void
meta_compositor_xrender_unmanage (MetaCompositor *compositor)
{
  MetaCompositorXRender *xrender = META_COMPOSITOR_XRENDER (compositor);
  MetaDisplay *display = meta_compositor_get_display (compositor);
  MetaScreen *screen = meta_display_get_screen (display);
  Display *xdisplay = meta_display_get_xdisplay (display);
  Window xroot = meta_screen_get_xroot (screen);
  GList *index;

  meta_prefs_remove_listener (update_shadows, xrender);

  hide_overlay_window (xrender, xdisplay);

  /* Destroy the windows */
  for (index = xrender->windows; index; index = index->next)
    {
      MetaCompWindow *cw = (MetaCompWindow *) index->data;
      free_win (xrender, cw, TRUE);
    }
  g_list_free (xrender->windows);
  g_hash_table_destroy (xrender->windows_by_xid);

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

  XCompositeUnredirectSubwindows (xdisplay, xroot, CompositeRedirectManual);
  XCompositeReleaseOverlayWindow (xdisplay, xrender->overlay_window);

  meta_screen_unset_cm_selection (screen);
}

static void
meta_compositor_xrender_add_window (MetaCompositor *compositor,
                                    MetaWindow     *window)
{
  MetaCompositorXRender *xrender = META_COMPOSITOR_XRENDER (compositor);
  MetaDisplay *display = meta_compositor_get_display (compositor);
  MetaFrame *frame = meta_window_get_frame (window);
  Window xwindow;

  if (frame)
    xwindow = meta_frame_get_xwindow (frame);
  else
    xwindow = meta_window_get_xwindow (window);

  meta_error_trap_push (NULL);
  add_win (xrender, display->screen, window, xwindow);
  meta_error_trap_pop (NULL);
}

static void
meta_compositor_xrender_remove_window (MetaCompositor *compositor,
                                       MetaWindow     *window)
{
  MetaCompositorXRender *xrender;
  MetaFrame *frame;
  Window xwindow;
  MetaCompWindow *cw;

  xrender = META_COMPOSITOR_XRENDER (compositor);
  frame = meta_window_get_frame (window);

  if (frame)
    xwindow = meta_frame_get_xwindow (frame);
  else
    xwindow = meta_window_get_xwindow (window);

  cw = find_window (xrender, xwindow);
  if (cw == NULL)
    return;

  if (cw->extents != None)
    {
      dump_xserver_region (xrender, "remove_window", cw->extents);
      add_damage (xrender, cw->extents);
      cw->extents = None;
    }

  xrender->windows = g_list_remove (xrender->windows, (gconstpointer) cw);
  g_hash_table_remove (xrender->windows_by_xid, (gpointer) xwindow);

  free_win (xrender, cw, TRUE);
}

static void
meta_compositor_xrender_show_window (MetaCompositor *compositor,
                                     MetaWindow     *window,
                                     MetaEffectType  effect)
{
}

static void
meta_compositor_xrender_hide_window (MetaCompositor *compositor,
                                     MetaWindow     *window,
                                     MetaEffectType  effect)
{
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
  meta_error_trap_push (NULL);

  switch (event->type)
    {
    case ConfigureNotify:
      process_configure_notify (xrender, (XConfigureEvent *) event);
      break;

    case PropertyNotify:
      process_property_notify (xrender, (XPropertyEvent *) event);
      break;

    case Expose:
      process_expose (xrender, (XExposeEvent *) event);
      break;

    case UnmapNotify:
      process_unmap (xrender, (XUnmapEvent *) event);
      break;

    case MapNotify:
      process_map (xrender, (XMapEvent *) event);
      break;

    default:
      if (event->type == meta_display_get_damage_event_base (display) + XDamageNotify)
        process_damage (xrender, (XDamageNotifyEvent *) event);
      else if (event->type == meta_display_get_shape_event_base (display) + ShapeNotify)
        process_shape (xrender, (XShapeEvent *) event);
      break;
    }

  meta_error_trap_pop (NULL);
}

static cairo_surface_t *
meta_compositor_xrender_get_window_surface (MetaCompositor *compositor,
                                            MetaWindow     *window)
{
  MetaFrame *frame;
  Window xwindow;
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

  if (frame)
    xwindow = meta_frame_get_xwindow (frame);
  else
    xwindow = meta_window_get_xwindow (window);

  display = meta_compositor_get_display (compositor);
  cw = find_window (META_COMPOSITOR_XRENDER (compositor), xwindow);

  if (cw == NULL)
    return NULL;

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
                                 -cw->attrs.x, -cw->attrs.y);
        }
    }

  if (frame != NULL && xclient_region == None)
    return NULL;

  client_region = xserver_region_to_cairo_region (xdisplay, xclient_region);
  XFixesDestroyRegion (xdisplay, xclient_region);

  if (frame != NULL && client_region == NULL)
    return NULL;

  width = shaded ? cw->shaded.width : cw->attrs.width;
  height = shaded ? cw->shaded.height : cw->attrs.height;

  back_surface = cairo_xlib_surface_create (xdisplay, back_pixmap,
                                            cw->attrs.visual, width, height);

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
meta_compositor_xrender_set_active_window (MetaCompositor *compositor,
                                           MetaWindow     *window)
{
  MetaCompositorXRender *xrender;
  MetaDisplay *display;
  Display *xdisplay;
  MetaCompWindow *old_focus = NULL, *new_focus = NULL;
  MetaWindow *old_focus_win = NULL;

  xrender = META_COMPOSITOR_XRENDER (compositor);
  display = meta_compositor_get_display (compositor);
  xdisplay = meta_display_get_xdisplay (display);

  old_focus_win = xrender->focus_window;
  xrender->focus_window = window;

  if (old_focus_win)
    {
      MetaFrame *f = meta_window_get_frame (old_focus_win);

      old_focus = find_window (xrender,
                               f ? meta_frame_get_xwindow (f) :
                               meta_window_get_xwindow (old_focus_win));
    }

  if (window)
    {
      MetaFrame *f = meta_window_get_frame (window);
      new_focus = find_window (xrender,
                               f ? meta_frame_get_xwindow (f) :
                               meta_window_get_xwindow (window));
    }

  if (old_focus)
    {
      XserverRegion damage;

      /* Tear down old shadows */
      old_focus->shadow_type = META_SHADOW_MEDIUM;
      determine_mode (xrender, old_focus);
      old_focus->needs_shadow = window_has_shadow (xrender, old_focus);

      if (old_focus->attrs.map_state == IsViewable)
        {
          if (old_focus->mask)
            {
              XRenderFreePicture (xdisplay, old_focus->mask);
              old_focus->mask = None;
            }

          if (old_focus->shadow)
            {
              XRenderFreePicture (xdisplay, old_focus->shadow);
              old_focus->shadow = None;
            }

          if (old_focus->extents)
            {
              damage = XFixesCreateRegion (xdisplay, NULL, 0);
              XFixesCopyRegion (xdisplay, damage, old_focus->extents);
              XFixesDestroyRegion (xdisplay, old_focus->extents);
            }
          else
            damage = None;

          /* Build new extents */
          old_focus->extents = win_extents (xrender, old_focus);

          if (damage)
            XFixesUnionRegion (xdisplay, damage, damage, old_focus->extents);
          else
            {
              damage = XFixesCreateRegion (xdisplay, NULL, 0);
              XFixesCopyRegion (xdisplay, damage, old_focus->extents);
            }

          dump_xserver_region (xrender, "resize_win", damage);
          add_damage (xrender, damage);

          xrender->clip_changed = TRUE;
        }
    }

  if (new_focus)
    {
      XserverRegion damage;

      new_focus->shadow_type = META_SHADOW_LARGE;
      determine_mode (xrender, new_focus);
      new_focus->needs_shadow = window_has_shadow (xrender, new_focus);

      if (new_focus->mask)
        {
          XRenderFreePicture (xdisplay, new_focus->mask);
          new_focus->mask = None;
        }

      if (new_focus->shadow)
        {
          XRenderFreePicture (xdisplay, new_focus->shadow);
          new_focus->shadow = None;
        }

      if (new_focus->extents)
        {
          damage = XFixesCreateRegion (xdisplay, NULL, 0);
          XFixesCopyRegion (xdisplay, damage, new_focus->extents);
          XFixesDestroyRegion (xdisplay, new_focus->extents);
        }
      else
        damage = None;

      /* Build new extents */
      new_focus->extents = win_extents (xrender, new_focus);

      if (damage)
        XFixesUnionRegion (xdisplay, damage, damage, new_focus->extents);
      else
        {
          damage = XFixesCreateRegion (xdisplay, NULL, 0);
          XFixesCopyRegion (xdisplay, damage, new_focus->extents);
        }

      dump_xserver_region (xrender, "resize_win", damage);
      add_damage (xrender, damage);

      xrender->clip_changed = TRUE;
    }

  add_repair (xrender);
}

static void
meta_compositor_xrender_maximize_window (MetaCompositor *compositor,
                                         MetaWindow     *window)
{
  MetaCompositorXRender *xrender = META_COMPOSITOR_XRENDER (compositor);
  MetaFrame *frame = meta_window_get_frame (window);
  Window xid = frame ? meta_frame_get_xwindow (frame) : meta_window_get_xwindow (window);
  MetaCompWindow *cw = find_window (xrender, xid);

  if (!cw)
    return;

  cw->needs_shadow = window_has_shadow (xrender, cw);
}

static void
meta_compositor_xrender_unmaximize_window (MetaCompositor *compositor,
                                           MetaWindow     *window)
{
  MetaCompositorXRender *xrender = META_COMPOSITOR_XRENDER (compositor);
  MetaFrame *frame = meta_window_get_frame (window);
  Window xid = frame ? meta_frame_get_xwindow (frame) : meta_window_get_xwindow (window);
  MetaCompWindow *cw = find_window (xrender, xid);

  if (!cw)
    return;

  cw->needs_shadow = window_has_shadow (xrender, cw);
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
      MetaFrame *frame;
      Window xwindow;
      MetaCompWindow *cw;

      window = (MetaWindow *) tmp->data;
      frame = meta_window_get_frame (window);

      if (frame)
        xwindow = meta_frame_get_xwindow (frame);
      else
        xwindow = meta_window_get_xwindow (window);

      cw = find_window (xrender, xwindow);

      if (cw == NULL)
        {
          g_warning ("Failed to find MetaCompWindow for MetaWindow %p", window);
          continue;
        }

      xrender->windows = g_list_remove (xrender->windows, cw);
      xrender->windows = g_list_prepend (xrender->windows, cw);
    }

  xrender->windows = g_list_reverse (xrender->windows);
  xrender->clip_changed = TRUE;

  add_repair (xrender);
}

static gboolean
meta_compositor_xrender_is_our_xwindow (MetaCompositor *compositor,
                                        Window          xwindow)
{
  MetaCompositorXRender *xrender;

  xrender = META_COMPOSITOR_XRENDER (compositor);

  if (xrender->overlay_window == xwindow)
    return TRUE;

  return FALSE;
}

static void
meta_compositor_xrender_class_init (MetaCompositorXRenderClass *xrender_class)
{
  GObjectClass *object_class;
  MetaCompositorClass *compositor_class;

  object_class = G_OBJECT_CLASS (xrender_class);
  compositor_class = META_COMPOSITOR_CLASS (xrender_class);

  object_class->constructed = meta_compositor_xrender_constructed;

  compositor_class->manage = meta_compositor_xrender_manage;
  compositor_class->unmanage = meta_compositor_xrender_unmanage;
  compositor_class->add_window = meta_compositor_xrender_add_window;
  compositor_class->remove_window = meta_compositor_xrender_remove_window;
  compositor_class->show_window = meta_compositor_xrender_show_window;
  compositor_class->hide_window = meta_compositor_xrender_hide_window;
  compositor_class->set_updates_frozen = meta_compositor_xrender_set_updates_frozen;
  compositor_class->process_event = meta_compositor_xrender_process_event;
  compositor_class->get_window_surface = meta_compositor_xrender_get_window_surface;
  compositor_class->set_active_window = meta_compositor_xrender_set_active_window;
  compositor_class->maximize_window = meta_compositor_xrender_maximize_window;
  compositor_class->unmaximize_window = meta_compositor_xrender_unmaximize_window;
  compositor_class->sync_stack = meta_compositor_xrender_sync_stack;
  compositor_class->is_our_xwindow = meta_compositor_xrender_is_our_xwindow;
}

static void
meta_compositor_xrender_init (MetaCompositorXRender *xrender)
{
}
