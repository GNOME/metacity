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

#include <cairo/cairo-xlib.h>
#include <cairo/cairo-xlib-xrender.h>
#include <libmetacity/meta-frame-borders.h>

#include "display.h"
#include "errors.h"
#include "frame.h"
#include "meta-compositor-xrender.h"
#include "meta-shadow-xrender.h"
#include "window-private.h"

#define OPAQUE 0xffffffff

struct _MetaSurfaceXRender
{
  MetaSurface        parent;

  MetaDisplay       *display;
  Display           *xdisplay;

  Picture            picture;
  Picture            alpha_pict;

  XserverRegion      border_clip;

  MetaShadowXRender *shadow;
  gboolean           shadow_changed;

  gboolean           is_argb;
};

G_DEFINE_TYPE (MetaSurfaceXRender, meta_surface_xrender, META_TYPE_SURFACE)

static void
shadow_changed (MetaSurfaceXRender *self)
{
  MetaSurface *surface;
  MetaCompositor *compositor;

  surface = META_SURFACE (self);

  compositor = meta_surface_get_compositor (surface);

  if (self->shadow != NULL)
    {
      int x;
      int y;
      XserverRegion shadow_region;

      x = meta_surface_get_x (surface);
      y = meta_surface_get_y (surface);

      shadow_region = meta_shadow_xrender_get_region (self->shadow);
      XFixesTranslateRegion (self->xdisplay, shadow_region, x, y);

      meta_compositor_add_damage (compositor, "shadow_changed", shadow_region);
      XFixesDestroyRegion (self->xdisplay, shadow_region);

      meta_shadow_xrender_free (self->shadow);
      self->shadow = NULL;
    }
  else
    {
      meta_compositor_queue_redraw (compositor);
    }

  self->shadow_changed = TRUE;
}

static void
paint_opaque_parts (MetaSurfaceXRender *self,
                    XserverRegion       paint_region,
                    Picture             paint_buffer)
{
  MetaSurface *surface;
  MetaWindow *window;
  XserverRegion shape_region;
  XserverRegion opaque_region;
  int x;
  int y;
  int width;
  int height;
  XserverRegion clip_region;

  surface = META_SURFACE (self);

  window = meta_surface_get_window (surface);
  shape_region = meta_surface_get_shape_region (surface);
  opaque_region = meta_surface_get_opaque_region (surface);

  if ((self->is_argb && opaque_region == None) ||
      window->opacity != OPAQUE)
    return;

  x = meta_surface_get_x (surface);
  y = meta_surface_get_y (surface);
  width = meta_surface_get_width (surface);
  height = meta_surface_get_height (surface);

  clip_region = XFixesCreateRegion (self->xdisplay, NULL, 0);
  XFixesCopyRegion (self->xdisplay, clip_region, shape_region);

  if (window->frame != NULL)
    {
      MetaFrameBorders borders;
      XRectangle client_rect;
      XserverRegion client_region;

      meta_frame_calc_borders (window->frame, &borders);

      client_rect = (XRectangle) {
        .x = borders.total.left,
        .y = borders.total.top,
        .width = width - borders.total.left - borders.total.right,
        .height = height - borders.total.top - borders.total.bottom
      };

      client_region = XFixesCreateRegion (self->xdisplay, &client_rect, 1);

      XFixesIntersectRegion (self->xdisplay, clip_region, clip_region, client_region);
      XFixesDestroyRegion (self->xdisplay, client_region);
    }

  if (opaque_region != None)
    XFixesIntersectRegion (self->xdisplay, clip_region, clip_region, opaque_region);

  XFixesTranslateRegion (self->xdisplay, clip_region, x, y);
  XFixesIntersectRegion (self->xdisplay, clip_region, clip_region, paint_region);

  XFixesSetPictureClipRegion (self->xdisplay, paint_buffer, 0, 0, clip_region);

  XRenderComposite (self->xdisplay, PictOpSrc,
                    self->picture, None, paint_buffer,
                    0, 0, 0, 0,
                    x, y, width, height);

  XFixesSubtractRegion (self->xdisplay, paint_region, paint_region, clip_region);
  XFixesDestroyRegion (self->xdisplay, clip_region);
}

static void
paint_argb_parts (MetaSurfaceXRender *self,
                  Picture             paint_buffer)
{
  MetaSurface *surface;
  int x;
  int y;
  int width;
  int height;
  XserverRegion border_clip;
  XserverRegion shape_region;
  XserverRegion clip_region;

  surface = META_SURFACE (self);

  x = meta_surface_get_x (surface);
  y = meta_surface_get_y (surface);
  width = meta_surface_get_width (surface);
  height = meta_surface_get_height (surface);

  border_clip = self->border_clip;
  shape_region = meta_surface_get_shape_region (surface);

  clip_region = XFixesCreateRegion (self->xdisplay, NULL, 0);
  XFixesCopyRegion (self->xdisplay, clip_region, shape_region);

  XFixesTranslateRegion (self->xdisplay, clip_region, x, y);
  XFixesIntersectRegion (self->xdisplay, border_clip, border_clip, clip_region);
  XFixesDestroyRegion (self->xdisplay, clip_region);

  XFixesSetPictureClipRegion (self->xdisplay, paint_buffer, 0, 0, border_clip);

  XRenderComposite (self->xdisplay, PictOpOver,
                    self->picture, self->alpha_pict, paint_buffer,
                    0, 0, 0, 0,
                    x, y, width, height);
}

static void
clip_to_shape_region (MetaSurfaceXRender *self,
                      cairo_t            *cr)
{
  XserverRegion shape_region;
  int n_rects;
  XRectangle *rects;
  int i;

  shape_region = meta_surface_get_shape_region (META_SURFACE (self));
  rects = XFixesFetchRegion (self->xdisplay, shape_region, &n_rects);

  if (rects == NULL)
    return;

  for (i = 0; i < n_rects; i++)
    {
      XRectangle *rect;

      rect = &rects[i];

      cairo_rectangle (cr, rect->x, rect->y, rect->width, rect->height);
    }

  cairo_clip (cr);
  XFree (rects);
}

static void
free_picture (MetaSurfaceXRender *self)
{
  if (self->picture == None)
    return;

  XRenderFreePicture (self->xdisplay, self->picture);
  self->picture = None;
}

static Picture
get_window_picture (MetaSurfaceXRender *self)
{
  MetaWindow *window;
  Visual *xvisual;
  XRenderPictFormat *format;
  Pixmap pixmap;
  XRenderPictureAttributes pa;
  unsigned int pa_mask;
  Picture picture;

  window = meta_surface_get_window (META_SURFACE (self));

  xvisual = meta_window_get_toplevel_xvisual (window);
  format = XRenderFindVisualFormat (self->xdisplay, xvisual);

  if (format == NULL)
    {
      xvisual = DefaultVisual (self->xdisplay, DefaultScreen (self->xdisplay));
      format = XRenderFindVisualFormat (self->xdisplay, xvisual);
    }

  if (format == NULL)
    return None;

  pixmap = meta_surface_get_pixmap (META_SURFACE (self));

  if (pixmap == None)
    return None;

  self->is_argb = format->type == PictTypeDirect && format->direct.alphaMask;

  pa.subwindow_mode = IncludeInferiors;
  pa_mask = CPSubwindowMode;

  meta_error_trap_push (self->display);
  picture = XRenderCreatePicture (self->xdisplay, pixmap, format, pa_mask, &pa);
  meta_error_trap_pop (self->display);

  return picture;
}

static Picture
create_alpha_picture (MetaSurfaceXRender *self,
                      guint               opacity)
{
  Window xroot;
  Pixmap pixmap;
  XRenderPictFormat *format;
  XRenderPictureAttributes pa;
  Picture picture;
  XRenderColor c;

  xroot = DefaultRootWindow (self->xdisplay);
  pixmap = XCreatePixmap (self->xdisplay, xroot, 1, 1, 32);

  if (pixmap == None)
    return None;

  format = XRenderFindStandardFormat (self->xdisplay, PictStandardARGB32);

  if (format == NULL)
    {
      XFreePixmap (self->xdisplay, pixmap);
      return None;
    }

  pa.repeat = True;
  picture = XRenderCreatePicture (self->xdisplay,
                                  pixmap,
                                  format,
                                  CPRepeat,
                                  &pa);

  if (picture == None)
    {
      XFreePixmap (self->xdisplay, pixmap);
      return None;
    }

  c.alpha = ((double) opacity / OPAQUE) * 0xffff;
  c.red = 0;
  c.green = 0;
  c.blue = 0;

  XRenderFillRectangle (self->xdisplay, PictOpSrc, picture, &c, 0, 0, 1, 1);
  XFreePixmap (self->xdisplay, pixmap);

  return picture;
}

static void
notify_appears_focused_cb (MetaWindow         *window,
                           GParamSpec         *pspec,
                           MetaSurfaceXRender *self)
{
  shadow_changed (self);
}

static void
notify_decorated_cb (MetaWindow         *window,
                     GParamSpec         *pspec,
                     MetaSurfaceXRender *self)
{
  shadow_changed (self);
}

static void
notify_client_decorated_cb (MetaWindow         *window,
                            GParamSpec         *pspec,
                            MetaSurfaceXRender *self)
{
  shadow_changed (self);
}

static void
notify_window_type_cb (MetaWindow         *window,
                       GParamSpec         *pspec,
                       MetaSurfaceXRender *self)
{
  shadow_changed (self);
}

static void
meta_surface_xrender_constructed (GObject *object)
{
  MetaSurfaceXRender *self;
  MetaWindow *window;

  self = META_SURFACE_XRENDER (object);

  G_OBJECT_CLASS (meta_surface_xrender_parent_class)->constructed (object);

  window = meta_surface_get_window (META_SURFACE (self));

  self->display = meta_window_get_display (window);
  self->xdisplay = meta_display_get_xdisplay (self->display);

  g_signal_connect_object (window, "notify::appears-focused",
                           G_CALLBACK (notify_appears_focused_cb),
                           self, 0);

  g_signal_connect_object (window, "notify::decorated",
                           G_CALLBACK (notify_decorated_cb),
                           self, 0);

  g_signal_connect_object (window,
                           "notify::client-decorated",
                           G_CALLBACK (notify_client_decorated_cb),
                           self,
                           0);

  g_signal_connect_object (window, "notify::window-type",
                           G_CALLBACK (notify_window_type_cb),
                           self, 0);
}

static void
meta_surface_xrender_finalize (GObject *object)
{
  MetaSurfaceXRender *self;

  self = META_SURFACE_XRENDER (object);

  free_picture (self);

  if (self->alpha_pict != None)
    {
      XRenderFreePicture (self->xdisplay, self->alpha_pict);
      self->alpha_pict = None;
    }

  if (self->border_clip != None)
    {
      XFixesDestroyRegion (self->xdisplay, self->border_clip);
      self->border_clip = None;
    }

  shadow_changed (self);

  G_OBJECT_CLASS (meta_surface_xrender_parent_class)->finalize (object);
}

static cairo_surface_t *
meta_surface_xrender_get_image (MetaSurface *surface)
{
  MetaSurfaceXRender *self;
  Pixmap back_pixmap;
  MetaWindow *window;
  Visual *visual;
  int width;
  int height;
  cairo_surface_t *back_surface;
  cairo_surface_t *image;
  cairo_t *cr;

  self = META_SURFACE_XRENDER (surface);

  back_pixmap = meta_surface_get_pixmap (surface);
  if (back_pixmap == None)
    return NULL;

  window = meta_surface_get_window (surface);

  visual = meta_window_get_toplevel_xvisual (window);
  width = meta_surface_get_width (surface);
  height = meta_surface_get_height (surface);

  back_surface = cairo_xlib_surface_create (self->xdisplay,
                                            back_pixmap,
                                            visual,
                                            width,
                                            height);

  image = cairo_surface_create_similar (back_surface,
                                        CAIRO_CONTENT_COLOR_ALPHA,
                                        width,
                                        height);

  cr = cairo_create (image);
  cairo_set_source_surface (cr, back_surface, 0, 0);
  cairo_surface_destroy (back_surface);

  clip_to_shape_region (self, cr);

  cairo_paint (cr);

  cairo_destroy (cr);

  return image;
}

static gboolean
meta_surface_xrender_is_visible (MetaSurface *surface)
{
  MetaSurfaceXRender *self;

  self = META_SURFACE_XRENDER (surface);

  return self->picture != None;
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

  shadow_changed (self);
}

static void
meta_surface_xrender_opacity_changed (MetaSurface *surface)
{
  MetaSurfaceXRender *self;

  self = META_SURFACE_XRENDER (surface);

  if (self->alpha_pict != None)
    {
      XRenderFreePicture (self->xdisplay, self->alpha_pict);
      self->alpha_pict = None;
    }

  shadow_changed (self);
}

static void
meta_surface_xrender_sync_geometry (MetaSurface   *surface,
                                    MetaRectangle  old_geometry,
                                    gboolean       position_changed,
                                    gboolean       size_changed)
{
  MetaSurfaceXRender *self;
  MetaCompositor *compositor;
  XserverRegion region;

  self = META_SURFACE_XRENDER (surface);

  if (self->shadow == NULL)
    return;

  compositor = meta_surface_get_compositor (surface);

  region = meta_shadow_xrender_get_region (self->shadow);
  XFixesTranslateRegion (self->xdisplay, region, old_geometry.x, old_geometry.y);

  meta_compositor_add_damage (compositor,
                              "meta_surface_xrender_sync_geometry",
                              region);

  XFixesDestroyRegion (self->xdisplay, region);

  if (size_changed)
    {
      meta_shadow_xrender_free (self->shadow);
      self->shadow = NULL;

      self->shadow_changed = TRUE;
    }
}

static void
meta_surface_xrender_free_pixmap (MetaSurface *surface)
{
  MetaSurfaceXRender *self;

  self = META_SURFACE_XRENDER (surface);

  free_picture (self);
}

static void
meta_surface_xrender_pre_paint (MetaSurface   *surface,
                                XserverRegion  damage)
{
  MetaSurfaceXRender *self;
  MetaWindow *window;

  self = META_SURFACE_XRENDER (surface);

  window = meta_surface_get_window (surface);

  if (!meta_window_is_toplevel_mapped (window))
    return;

  if (self->picture == None)
    self->picture = get_window_picture (self);

  if (window->opacity != OPAQUE && self->alpha_pict == None)
    self->alpha_pict = create_alpha_picture (self, window->opacity);

  if (self->shadow_changed)
    {
      MetaCompositor *compositor;
      MetaCompositorXRender *compositor_xrender;

      compositor = meta_surface_get_compositor (surface);
      compositor_xrender = META_COMPOSITOR_XRENDER (compositor);

      if (self->shadow == NULL &&
          meta_compositor_xrender_have_shadows (compositor_xrender) &&
          meta_surface_has_shadow (surface))
        {
          XserverRegion shadow_region;

          self->shadow = meta_compositor_xrender_create_shadow (compositor_xrender,
                                                                surface);

          shadow_region = meta_shadow_xrender_get_region (self->shadow);
          XFixesUnionRegion (self->xdisplay, damage, damage, shadow_region);
          XFixesDestroyRegion (self->xdisplay, shadow_region);
        }

      self->shadow_changed = FALSE;
    }
}

static void
meta_surface_xrender_class_init (MetaSurfaceXRenderClass *self_class)
{
  GObjectClass *object_class;
  MetaSurfaceClass *surface_class;

  object_class = G_OBJECT_CLASS (self_class);
  surface_class = META_SURFACE_CLASS (self_class);

  object_class->constructed = meta_surface_xrender_constructed;
  object_class->finalize = meta_surface_xrender_finalize;

  surface_class->get_image = meta_surface_xrender_get_image;
  surface_class->is_visible = meta_surface_xrender_is_visible;
  surface_class->show = meta_surface_xrender_show;
  surface_class->hide = meta_surface_xrender_hide;
  surface_class->opacity_changed = meta_surface_xrender_opacity_changed;
  surface_class->sync_geometry = meta_surface_xrender_sync_geometry;
  surface_class->free_pixmap = meta_surface_xrender_free_pixmap;
  surface_class->pre_paint = meta_surface_xrender_pre_paint;
}

static void
meta_surface_xrender_init (MetaSurfaceXRender *self)
{
  self->shadow_changed = TRUE;
}

void
meta_surface_xrender_update_shadow (MetaSurfaceXRender *self)
{
  shadow_changed (self);
}

void
meta_surface_xrender_paint_shadow (MetaSurfaceXRender *self,
                                   XserverRegion       paint_region,
                                   Picture             paint_buffer)
{
  MetaSurface *surface;
  XserverRegion shadow_clip;

  surface = META_SURFACE (self);

  if (self->shadow == NULL)
    return;

  shadow_clip = XFixesCreateRegion (self->xdisplay, NULL, 0);
  XFixesCopyRegion (self->xdisplay, shadow_clip, self->border_clip);

  if (paint_region != None)
    XFixesIntersectRegion (self->xdisplay, shadow_clip, shadow_clip, paint_region);

  meta_shadow_xrender_paint (self->shadow,
                             shadow_clip,
                             paint_buffer,
                             meta_surface_get_x (surface),
                             meta_surface_get_y (surface));

  XFixesDestroyRegion (self->xdisplay, shadow_clip);
}

void
meta_surface_xrender_paint (MetaSurfaceXRender *self,
                            XserverRegion       paint_region,
                            Picture             paint_buffer,
                            gboolean            opaque)
{
  if (opaque)
    {
      paint_opaque_parts (self, paint_region, paint_buffer);

      g_assert (self->border_clip == None);

      self->border_clip = XFixesCreateRegion (self->xdisplay, NULL, 0);
      XFixesCopyRegion (self->xdisplay, self->border_clip, paint_region);
    }
  else
    {
      paint_argb_parts (self, paint_buffer);

      if (self->border_clip != None)
        {
          XFixesDestroyRegion (self->xdisplay, self->border_clip);
          self->border_clip = None;
        }
    }
}
