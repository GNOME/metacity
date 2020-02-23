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
#include "meta-surface-private.h"

#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>

#include "display-private.h"
#include "errors.h"
#include "frame.h"
#include "meta-compositor-private.h"
#include "prefs.h"
#include "window-private.h"

typedef struct
{
  MetaCompositor  *compositor;
  MetaWindow      *window;

  MetaDisplay     *display;
  Display         *xdisplay;

  Damage           damage;
  gboolean         damage_received;

  Pixmap           pixmap;

  int              x;
  int              y;
  gboolean         position_changed;

  int              width;
  int              height;

  XserverRegion    shape_region;
  gboolean         shape_region_changed;

  XserverRegion    opaque_region;
  gboolean         opaque_region_changed;

  /* This is a copy of the original unshaded window so that we can still see
   * what the window looked like when it is needed for the _get_window_surface
   * function.
   */
  cairo_surface_t *shaded_surface;
} MetaSurfacePrivate;

enum
{
  PROP_0,

  PROP_COMPOSITOR,
  PROP_WINDOW,

  LAST_PROP
};

static GParamSpec *surface_properties[LAST_PROP] = { NULL };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (MetaSurface, meta_surface, G_TYPE_OBJECT)

static void
add_full_damage (MetaSurface *self)
{
  MetaSurfacePrivate *priv;
  XserverRegion full_damage;

  priv = meta_surface_get_instance_private (self);

  if (priv->shape_region == None)
    return;

  full_damage = XFixesCreateRegion (priv->xdisplay, NULL, 0);
  XFixesCopyRegion (priv->xdisplay, full_damage, priv->shape_region);

  XFixesTranslateRegion (priv->xdisplay, full_damage, priv->x, priv->y);

  meta_compositor_add_damage (priv->compositor, "add_full_damage", full_damage);
  XFixesDestroyRegion (priv->xdisplay, full_damage);
}

static XserverRegion
get_frame_region (MetaSurface *self,
                  XRectangle  *client_rect)
{
  MetaSurfacePrivate *priv;
  XserverRegion frame_region;
  XserverRegion client_region;

  priv = meta_surface_get_instance_private (self);

  frame_region = XFixesCreateRegion (priv->xdisplay, &(XRectangle) {
                                       .width = priv->width,
                                       .height = priv->height
                                     }, 1);

  client_region = XFixesCreateRegion (priv->xdisplay, client_rect, 1);

  XFixesSubtractRegion (priv->xdisplay, frame_region, frame_region, client_region);
  XFixesDestroyRegion (priv->xdisplay, client_region);

  return frame_region;
}

static void
clip_shape_region (Display       *xdisplay,
                   XserverRegion  shape_region,
                   XRectangle    *client_rect)
{
  XserverRegion client_region;

  client_region = XFixesCreateRegion (xdisplay, client_rect, 1);
  XFixesIntersectRegion (xdisplay, shape_region, shape_region, client_region);
  XFixesDestroyRegion (xdisplay, client_region);
}

static gboolean
update_shape_region (MetaSurface   *self,
                     XserverRegion  damage_region)
{
  MetaSurfacePrivate *priv;
  MetaFrameBorders borders;
  XRectangle client_rect;
  XserverRegion shape_region;

  priv = meta_surface_get_instance_private (self);

  if (!priv->shape_region_changed)
    return FALSE;

  g_assert (priv->shape_region == None);

  meta_frame_calc_borders (priv->window->frame, &borders);

  client_rect.x = borders.total.left;
  client_rect.y = borders.total.top;
  client_rect.width = priv->width - borders.total.left - borders.total.right;
  client_rect.height = priv->height - borders.total.top - borders.total.bottom;

  if (priv->window->frame != NULL && priv->window->shape_region != None)
    {
      shape_region = XFixesCreateRegion (priv->xdisplay, NULL, 0);
      XFixesCopyRegion (priv->xdisplay, shape_region, priv->window->shape_region);

      XFixesTranslateRegion (priv->xdisplay,
                             shape_region,
                             client_rect.x,
                             client_rect.y);

      clip_shape_region (priv->xdisplay, shape_region, &client_rect);
    }
  else if (priv->window->shape_region != None)
    {
      shape_region = XFixesCreateRegion (priv->xdisplay, NULL, 0);
      XFixesCopyRegion (priv->xdisplay, shape_region, priv->window->shape_region);

      clip_shape_region (priv->xdisplay, shape_region, &client_rect);
    }
  else
    {
      shape_region = XFixesCreateRegion (priv->xdisplay, &client_rect, 1);
    }

  g_assert (shape_region != None);

  if (priv->window->frame != NULL)
    {
      XserverRegion frame_region;

      frame_region = get_frame_region (self, &client_rect);
      XFixesUnionRegion (priv->xdisplay, shape_region, shape_region, frame_region);
      XFixesDestroyRegion (priv->xdisplay, frame_region);
    }

  XFixesUnionRegion (priv->xdisplay, damage_region, damage_region, shape_region);

  priv->shape_region = shape_region;
  priv->shape_region_changed = FALSE;

  return TRUE;
}

static gboolean
update_opaque_region (MetaSurface   *self,
                      XserverRegion  damage_region)
{
  MetaSurfacePrivate *priv;
  XserverRegion opaque_region;

  priv = meta_surface_get_instance_private (self);

  if (!priv->opaque_region_changed)
    return FALSE;

  g_assert (priv->opaque_region == None);

  if (priv->window->frame != NULL && priv->window->opaque_region != None)
    {
      MetaFrameBorders borders;

      meta_frame_calc_borders (priv->window->frame, &borders);

      opaque_region = XFixesCreateRegion (priv->xdisplay, NULL, 0);
      XFixesCopyRegion (priv->xdisplay,
                        opaque_region,
                        priv->window->opaque_region);

      XFixesTranslateRegion (priv->xdisplay,
                             opaque_region,
                             borders.total.left,
                             borders.total.top);
    }
  else if (priv->window->opaque_region != None)
    {
      opaque_region = XFixesCreateRegion (priv->xdisplay, NULL, 0);
      XFixesCopyRegion (priv->xdisplay,
                        opaque_region,
                        priv->window->opaque_region);
    }
  else
    {
      opaque_region = None;
    }

  if (opaque_region != None)
    {
      XFixesUnionRegion (priv->xdisplay,
                         damage_region,
                         damage_region,
                         opaque_region);
    }

  priv->opaque_region = opaque_region;
  priv->opaque_region_changed = FALSE;

  return TRUE;
}

static void
free_pixmap (MetaSurface *self)
{
  MetaSurfacePrivate *priv;

  priv = meta_surface_get_instance_private (self);

  if (priv->pixmap == None)
    return;

  META_SURFACE_GET_CLASS (self)->free_pixmap (self);

  meta_error_trap_push (priv->display);

  XFreePixmap (priv->xdisplay, priv->pixmap);
  priv->pixmap = None;

  meta_error_trap_pop (priv->display);
}

static void
ensure_pixmap (MetaSurface *self)
{
  MetaSurfacePrivate *priv;
  Window xwindow;

  priv = meta_surface_get_instance_private (self);

  if (priv->pixmap != None)
    return;

  meta_error_trap_push (priv->display);

  xwindow = meta_window_get_toplevel_xwindow (priv->window);
  priv->pixmap = XCompositeNameWindowPixmap (priv->xdisplay, xwindow);

  if (meta_error_trap_pop_with_return (priv->display) != 0)
    priv->pixmap = None;
}

static void
destroy_damage (MetaSurface *self)
{
  MetaSurfacePrivate *priv;

  priv = meta_surface_get_instance_private (self);

  if (priv->damage == None)
    return;

  meta_error_trap_push (priv->display);

  XDamageDestroy (priv->xdisplay, priv->damage);
  priv->damage = None;

  meta_error_trap_pop (priv->display);
}

static void
create_damage (MetaSurface *self)
{
  MetaSurfacePrivate *priv;

  priv = meta_surface_get_instance_private (self);

  meta_error_trap_push (priv->display);

  g_assert (priv->damage == None);
  priv->damage = XDamageCreate (priv->xdisplay,
                                meta_window_get_toplevel_xwindow (priv->window),
                                XDamageReportNonEmpty);

  meta_error_trap_pop (priv->display);
}

static void
notify_decorated_cb (MetaWindow  *window,
                     GParamSpec  *pspec,
                     MetaSurface *self)
{
  destroy_damage (self);
  free_pixmap (self);

  create_damage (self);
}

static void
notify_shaded_cb (MetaWindow  *window,
                  GParamSpec  *pspec,
                  MetaSurface *self)
{
  MetaSurfacePrivate *priv;

  priv = meta_surface_get_instance_private (self);

  if (priv->shaded_surface != NULL)
    {
      cairo_surface_destroy (priv->shaded_surface);
      priv->shaded_surface = NULL;
    }

  if (meta_window_is_shaded (priv->window))
    priv->shaded_surface = META_SURFACE_GET_CLASS (self)->get_image (self);
}

static void
meta_surface_constructed (GObject *object)
{
  MetaSurface *self;
  MetaSurfacePrivate *priv;

  self = META_SURFACE (object);
  priv = meta_surface_get_instance_private (self);

  G_OBJECT_CLASS (meta_surface_parent_class)->constructed (object);

  priv->display = meta_compositor_get_display (priv->compositor);
  priv->xdisplay = meta_display_get_xdisplay (priv->display);

  meta_surface_sync_geometry (self);
  create_damage (self);

  g_signal_connect_object (priv->window, "notify::decorated",
                           G_CALLBACK (notify_decorated_cb),
                           self, 0);

  g_signal_connect_object (priv->window, "notify::shaded",
                           G_CALLBACK (notify_shaded_cb),
                           self, 0);
}

static void
meta_surface_finalize (GObject *object)
{
  MetaSurface *self;
  MetaSurfacePrivate *priv;

  self = META_SURFACE (object);
  priv = meta_surface_get_instance_private (self);

  destroy_damage (self);
  free_pixmap (self);

  if (priv->shape_region != None)
    {
      XFixesTranslateRegion (priv->xdisplay,
                             priv->shape_region,
                             priv->x,
                             priv->y);

      meta_compositor_add_damage (priv->compositor,
                                  "meta_surface_finalize",
                                  priv->shape_region);

      XFixesDestroyRegion (priv->xdisplay, priv->shape_region);
      priv->shape_region = None;
    }

  if (priv->opaque_region != None)
    {
      XFixesDestroyRegion (priv->xdisplay, priv->opaque_region);
      priv->opaque_region = None;
    }

  if (priv->shaded_surface != NULL)
    {
      cairo_surface_destroy (priv->shaded_surface);
      priv->shaded_surface = NULL;
    }

  G_OBJECT_CLASS (meta_surface_parent_class)->finalize (object);
}

static void
meta_surface_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  MetaSurface *self;
  MetaSurfacePrivate *priv;

  self = META_SURFACE (object);
  priv = meta_surface_get_instance_private (self);

  switch (property_id)
    {
      case PROP_COMPOSITOR:
        g_value_set_object (value, priv->compositor);
        break;

      case PROP_WINDOW:
        g_value_set_object (value, priv->window);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
meta_surface_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  MetaSurface *self;
  MetaSurfacePrivate *priv;

  self = META_SURFACE (object);
  priv = meta_surface_get_instance_private (self);

  switch (property_id)
    {
      case PROP_COMPOSITOR:
        g_assert (priv->compositor == NULL);
        priv->compositor = g_value_get_object (value);
        break;

      case PROP_WINDOW:
        g_assert (priv->window == NULL);
        priv->window = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
meta_surface_install_properties (GObjectClass *object_class)
{
  surface_properties[PROP_COMPOSITOR] =
    g_param_spec_object ("compositor", "compositor", "compositor",
                         META_TYPE_COMPOSITOR,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  surface_properties[PROP_WINDOW] =
    g_param_spec_object ("window", "window", "window",
                         META_TYPE_WINDOW,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP,
                                     surface_properties);
}

static void
meta_surface_class_init (MetaSurfaceClass *self_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (self_class);

  object_class->constructed = meta_surface_constructed;
  object_class->finalize = meta_surface_finalize;
  object_class->get_property = meta_surface_get_property;
  object_class->set_property = meta_surface_set_property;

  meta_surface_install_properties (object_class);
}

static void
meta_surface_init (MetaSurface *self)
{
}

MetaCompositor *
meta_surface_get_compositor (MetaSurface *self)
{
  MetaSurfacePrivate *priv;

  priv = meta_surface_get_instance_private (self);

  return priv->compositor;
}

MetaWindow *
meta_surface_get_window (MetaSurface *self)
{
  MetaSurfacePrivate *priv;

  priv = meta_surface_get_instance_private (self);

  return priv->window;
}

Pixmap
meta_surface_get_pixmap (MetaSurface *self)
{
  MetaSurfacePrivate *priv;

  priv = meta_surface_get_instance_private (self);

  return priv->pixmap;
}

int
meta_surface_get_x (MetaSurface *self)
{
  MetaSurfacePrivate *priv;

  priv = meta_surface_get_instance_private (self);

  return priv->x;
}

int
meta_surface_get_y (MetaSurface *self)
{
  MetaSurfacePrivate *priv;

  priv = meta_surface_get_instance_private (self);

  return priv->y;
}

int
meta_surface_get_width (MetaSurface *self)
{
  MetaSurfacePrivate *priv;

  priv = meta_surface_get_instance_private (self);

  return priv->width;
}

int
meta_surface_get_height (MetaSurface *self)
{
  MetaSurfacePrivate *priv;

  priv = meta_surface_get_instance_private (self);

  return priv->height;
}

XserverRegion
meta_surface_get_opaque_region (MetaSurface *self)
{
  MetaSurfacePrivate *priv;

  priv = meta_surface_get_instance_private (self);

  return priv->opaque_region;
}

XserverRegion
meta_surface_get_shape_region (MetaSurface *self)
{
  MetaSurfacePrivate *priv;

  priv = meta_surface_get_instance_private (self);

  return priv->shape_region;
}

cairo_surface_t *
meta_surface_get_image (MetaSurface *self)
{
  MetaSurfacePrivate *priv;

  priv = meta_surface_get_instance_private (self);

  if (meta_window_is_shaded (priv->window))
    {
      if (priv->shaded_surface != NULL)
        return cairo_surface_reference (priv->shaded_surface);
      else
        return NULL;
    }

  return META_SURFACE_GET_CLASS (self)->get_image (self);
}

gboolean
meta_surface_has_shadow (MetaSurface *self)
{
  MetaSurfacePrivate *priv;

  priv = meta_surface_get_instance_private (self);

  /* Do not add shadows to fullscreen windows */
  if (meta_window_is_fullscreen (priv->window))
    return FALSE;

  /* Do not add shadows to maximized windows */
  if (meta_window_is_maximized (priv->window))
    return FALSE;

  /* Add shadows to windows with frame */
  if (meta_window_get_frame (priv->window) != NULL)
    {
      /* Do not add shadows if GTK+ theme is used */
      if (meta_prefs_get_theme_type () == META_THEME_TYPE_GTK)
        return FALSE;

      return TRUE;
    }

  /* Do not add shadows to non-opaque windows */
  if (!meta_surface_is_opaque (self))
    return FALSE;

  /* Do not add shadows to client side decorated windows */
  if (meta_window_is_client_decorated (priv->window))
    return FALSE;

  /* Never put a shadow around shaped windows */
  if (priv->window->shape_region != None)
    return FALSE;

  /* Don't put shadow around DND icon windows */
  if (priv->window->type == META_WINDOW_DND)
    return FALSE;

  /* Don't put shadow around desktop windows */
  if (priv->window->type == META_WINDOW_DESKTOP)
    return FALSE;

  /* Add shadows to all other windows */
  return TRUE;
}

gboolean
meta_surface_is_opaque (MetaSurface *self)
{
  MetaSurfacePrivate *priv;
  Visual *xvisual;
  XRenderPictFormat *format;
  XserverRegion region;
  int n_rects;
  XRectangle bounds;
  XRectangle *rects;

  priv = meta_surface_get_instance_private (self);

  if (priv->window->opacity != 0xffffffff)
    return FALSE;

  xvisual = meta_window_get_toplevel_xvisual (priv->window);
  format = XRenderFindVisualFormat (priv->xdisplay, xvisual);

  if (format->type != PictTypeDirect || !format->direct.alphaMask)
    return TRUE;

  if (priv->opaque_region == None)
    return FALSE;

  region = XFixesCreateRegion (priv->xdisplay, NULL, 0);

  XFixesSubtractRegion (priv->xdisplay,
                        region,
                        priv->shape_region,
                        priv->opaque_region);

  rects = XFixesFetchRegionAndBounds (priv->xdisplay, region, &n_rects, &bounds);
  XFixesDestroyRegion (priv->xdisplay, region);
  XFree (rects);

  return (n_rects == 0 || bounds.width == 0 || bounds.height == 0);
}

gboolean
meta_surface_is_visible (MetaSurface *self)
{
  MetaSurfacePrivate *priv;

  priv = meta_surface_get_instance_private (self);

  if (!meta_window_is_toplevel_mapped (priv->window) ||
      priv->pixmap == None)
    return FALSE;

  return META_SURFACE_GET_CLASS (self)->is_visible (self);
}

void
meta_surface_show (MetaSurface *self)
{
  /* The reason we free pixmap here is so that we will still have
   * a valid pixmap when the window is unmapped.
   */
  free_pixmap (self);

  META_SURFACE_GET_CLASS (self)->show (self);
}

void
meta_surface_hide (MetaSurface *self)
{
  META_SURFACE_GET_CLASS (self)->hide (self);
  add_full_damage (self);
}

void
meta_surface_process_damage (MetaSurface        *self,
                             XDamageNotifyEvent *event)
{
  MetaSurfacePrivate *priv;

  priv = meta_surface_get_instance_private (self);

  priv->damage_received = TRUE;

  meta_compositor_queue_redraw (priv->compositor);
}

void
meta_surface_opacity_changed (MetaSurface *self)
{
  META_SURFACE_GET_CLASS (self)->opacity_changed (self);
  add_full_damage (self);
}

void
meta_surface_opaque_region_changed (MetaSurface *self)
{
  MetaSurfacePrivate *priv;

  priv = meta_surface_get_instance_private (self);

  if (priv->opaque_region != None)
    {
      XFixesTranslateRegion (priv->xdisplay,
                             priv->opaque_region,
                             priv->x,
                             priv->y);

      meta_compositor_add_damage (priv->compositor,
                                  "meta_surface_opaque_region_changed",
                                  priv->opaque_region);

      XFixesDestroyRegion (priv->xdisplay, priv->opaque_region);
      priv->opaque_region = None;
    }
  else
    {
      meta_compositor_queue_redraw (priv->compositor);
    }

  priv->opaque_region_changed = TRUE;
}

void
meta_surface_shape_region_changed (MetaSurface *self)
{
  MetaSurfacePrivate *priv;

  priv = meta_surface_get_instance_private (self);

  meta_compositor_queue_redraw (priv->compositor);

  if (priv->shape_region != None)
    {
      XFixesTranslateRegion (priv->xdisplay,
                             priv->shape_region,
                             priv->x,
                             priv->y);

      meta_compositor_add_damage (priv->compositor,
                                  "meta_surface_shape_region_changed",
                                  priv->shape_region);

      XFixesDestroyRegion (priv->xdisplay, priv->shape_region);
      priv->shape_region = None;
    }

  priv->shape_region_changed = TRUE;
}

void
meta_surface_sync_geometry (MetaSurface *self)
{
  MetaSurfacePrivate *priv;
  MetaRectangle rect;
  MetaRectangle old_geometry;
  gboolean position_changed;
  gboolean size_changed;

  priv = meta_surface_get_instance_private (self);

  meta_window_get_input_rect (priv->window, &rect);

  old_geometry.x = priv->x;
  old_geometry.y = priv->y;
  old_geometry.width = priv->width;
  old_geometry.height = priv->height;

  position_changed = FALSE;
  size_changed = FALSE;

  if (priv->x != rect.x ||
      priv->y != rect.y)
    {
      add_full_damage (self);

      priv->x = rect.x;
      priv->y = rect.y;

      priv->position_changed = TRUE;
      position_changed = TRUE;
    }

  if (priv->width != rect.width ||
      priv->height != rect.height)
    {
      free_pixmap (self);

      meta_surface_opaque_region_changed (self);
      meta_surface_shape_region_changed (self);

      priv->width = rect.width;
      priv->height = rect.height;

      size_changed = TRUE;
    }

  META_SURFACE_GET_CLASS (self)->sync_geometry (self,
                                                old_geometry,
                                                position_changed,
                                                size_changed);
}

void
meta_surface_pre_paint (MetaSurface *self)
{
  MetaSurfacePrivate *priv;
  XserverRegion damage;
  gboolean has_damage;

  priv = meta_surface_get_instance_private (self);

  damage = XFixesCreateRegion (priv->xdisplay, NULL, 0);
  has_damage = FALSE;

  if (priv->damage_received)
    {
      meta_error_trap_push (priv->display);
      XDamageSubtract (priv->xdisplay, priv->damage, None, damage);
      meta_error_trap_pop (priv->display);

      priv->damage_received = FALSE;
      has_damage = TRUE;
    }

  ensure_pixmap (self);

  META_SURFACE_GET_CLASS (self)->pre_paint (self, damage);

  if (update_shape_region (self, damage))
    has_damage = TRUE;

  if (update_opaque_region (self, damage))
    has_damage = TRUE;

  if (priv->position_changed)
    {
      XFixesUnionRegion (priv->xdisplay, damage, damage, priv->shape_region);

      priv->position_changed = FALSE;
      has_damage = TRUE;
    }

  if (!has_damage)
    {
      XFixesDestroyRegion (priv->xdisplay, damage);
      return;
    }

  XFixesTranslateRegion (priv->xdisplay, damage, priv->x, priv->y);
  meta_compositor_add_damage (priv->compositor, "meta_surface_pre_paint", damage);
  XFixesDestroyRegion (priv->xdisplay, damage);
}
