/*
 * Copyright (C) 2019 Alberts Muktupāvels
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

#include "config.h"
#include "meta-surface-xrender.h"

#include <cairo/cairo-xlib-xrender.h>
#include <libmetacity/meta-frame-borders.h>

#include "display.h"
#include "errors.h"
#include "frame.h"
#include "window-private.h"

#define OPAQUE 0xffffffff

struct _MetaSurfaceXRender
{
  MetaSurface parent;

  Picture     picture;
};

G_DEFINE_TYPE (MetaSurfaceXRender, meta_surface_xrender, META_TYPE_SURFACE)

static void
free_picture (MetaSurfaceXRender *self)
{
  MetaWindow *window;
  MetaDisplay *display;
  Display *xdisplay;

  if (self->picture == None)
    return;

  window = meta_surface_get_window (META_SURFACE (self));

  display = meta_window_get_display (window);
  xdisplay = meta_display_get_xdisplay (display);

  XRenderFreePicture (xdisplay, self->picture);
  self->picture = None;
}

static Picture
get_window_picture (MetaSurfaceXRender *self)
{
  MetaWindow *window;
  MetaDisplay *display;
  Display *xdisplay;
  Window xwindow;
  Visual *xvisual;
  XRenderPictFormat *format;
  Pixmap pixmap;
  Drawable drawable;
  XRenderPictureAttributes pa;
  unsigned int pa_mask;
  Picture picture;

  window = meta_surface_get_window (META_SURFACE (self));

  display = meta_window_get_display (window);
  xdisplay = meta_display_get_xdisplay (display);

  xwindow = meta_window_get_toplevel_xwindow (window);
  xvisual = meta_window_get_toplevel_xvisual (window);

  format = XRenderFindVisualFormat (xdisplay, xvisual);

  if (format == NULL)
    {
      xvisual = DefaultVisual (xdisplay, DefaultScreen (xdisplay));
      format = XRenderFindVisualFormat (xdisplay, xvisual);
    }

  if (format == NULL)
    return None;

  pixmap = meta_surface_get_pixmap (META_SURFACE (self));
  drawable = pixmap != None ? pixmap : xwindow;

  pa.subwindow_mode = IncludeInferiors;
  pa_mask = CPSubwindowMode;

  meta_error_trap_push (display);
  picture = XRenderCreatePicture (xdisplay, drawable, format, pa_mask, &pa);
  meta_error_trap_pop (display);

  return picture;
}

static void
meta_surface_xrender_finalize (GObject *object)
{
  MetaSurfaceXRender *self;

  self = META_SURFACE_XRENDER (object);

  free_picture (self);

  G_OBJECT_CLASS (meta_surface_xrender_parent_class)->finalize (object);
}

static void
meta_surface_xrender_show (MetaSurface *surface)
{
}

static void
meta_surface_xrender_hide (MetaSurface *surface)
{
  MetaSurfaceXRender *self;

  self = META_SURFACE_XRENDER (surface);

  free_picture (self);
}

static void
meta_surface_xrender_opacity_changed (MetaSurface *surface)
{
}

static void
meta_surface_xrender_free_pixmap (MetaSurface *surface)
{
  MetaSurfaceXRender *self;

  self = META_SURFACE_XRENDER (surface);

  free_picture (self);
}

static void
meta_surface_xrender_pre_paint (MetaSurface *surface)
{
  MetaSurfaceXRender *self;
  MetaWindow *window;

  self = META_SURFACE_XRENDER (surface);

  window = meta_surface_get_window (surface);

  if (!meta_window_is_toplevel_mapped (window))
    return;

  if (self->picture == None)
    self->picture = get_window_picture (self);
}

static void
meta_surface_xrender_class_init (MetaSurfaceXRenderClass *self_class)
{
  GObjectClass *object_class;
  MetaSurfaceClass *surface_class;

  object_class = G_OBJECT_CLASS (self_class);
  surface_class = META_SURFACE_CLASS (self_class);

  object_class->finalize = meta_surface_xrender_finalize;

  surface_class->show = meta_surface_xrender_show;
  surface_class->hide = meta_surface_xrender_hide;
  surface_class->opacity_changed = meta_surface_xrender_opacity_changed;
  surface_class->free_pixmap = meta_surface_xrender_free_pixmap;
  surface_class->pre_paint = meta_surface_xrender_pre_paint;
}

static void
meta_surface_xrender_init (MetaSurfaceXRender *self)
{
}

Pixmap
meta_surface_xrender_create_mask_pixmap (MetaSurfaceXRender *self,
                                         gboolean            with_opacity)
{
  MetaWindow *window;
  MetaFrame *frame;
  MetaDisplay *display;
  Display *xdisplay;
  int width;
  int height;
  XRenderPictFormat *format;
  double opacity;
  cairo_surface_t *surface;
  cairo_t *cr;
  Pixmap pixmap;

  window = meta_surface_get_window (META_SURFACE (self));

  frame = meta_window_get_frame (window);
  if (frame == NULL && window->opacity == OPAQUE)
    return None;

  display = meta_window_get_display (window);
  xdisplay = meta_display_get_xdisplay (display);

  width = frame != NULL ? meta_surface_get_width (META_SURFACE (self)) : 1;
  height = frame != NULL ? meta_surface_get_height (META_SURFACE (self)) : 1;

  format = XRenderFindStandardFormat (xdisplay, PictStandardA8);

  meta_error_trap_push (display);
  pixmap = XCreatePixmap (xdisplay,
                          DefaultRootWindow (xdisplay),
                          width,
                          height,
                          format->depth);

  if (meta_error_trap_pop_with_return (display) != 0)
    return None;

  opacity = 1.0;
  if (with_opacity)
    opacity = (double) window->opacity / OPAQUE;

  surface = cairo_xlib_surface_create_with_xrender_format (xdisplay,
                                                           pixmap,
                                                           DefaultScreenOfDisplay (xdisplay),
                                                           format,
                                                           width,
                                                           height);

  cr = cairo_create (surface);

  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_set_source_rgba (cr, 0, 0, 0, 1);
  cairo_paint (cr);

  if (frame != NULL)
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

      cairo_rectangle (cr, rect.x, rect.y, rect.width, rect.height);
      cairo_clip (cr);

      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cairo_set_source_rgba (cr, 0, 0, 0, opacity);
      cairo_paint (cr);

      cairo_reset_clip (cr);
      gdk_cairo_region (cr, frame_paint_region);
      cairo_region_destroy (frame_paint_region);
      cairo_clip (cr);

      cairo_push_group (cr);

      cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
      meta_frame_get_mask (frame, cr);

      cairo_pop_group_to_source (cr);
      cairo_paint_with_alpha (cr, opacity);
    }
  else
    {
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cairo_set_source_rgba (cr, 0, 0, 0, opacity);
      cairo_paint (cr);
    }

  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  return pixmap;
}

Picture
meta_surface_xrender_get_picture (MetaSurfaceXRender *self)
{
  return self->picture;
}
