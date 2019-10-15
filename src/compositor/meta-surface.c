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

#include "display-private.h"
#include "errors.h"
#include "frame.h"
#include "meta-compositor-private.h"
#include "window-private.h"

typedef struct
{
  MetaCompositor *compositor;
  MetaWindow     *window;

  MetaDisplay    *display;
  Display        *xdisplay;

  Damage          damage;
  Pixmap          pixmap;

  int             x;
  int             y;
  int             width;
  int             height;

  XserverRegion   opaque_region;
  gboolean        opaque_region_changed;
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
update_opaque_region (MetaSurface *self)
{
  MetaSurfacePrivate *priv;
  XserverRegion opaque_region;

  priv = meta_surface_get_instance_private (self);

  if (!priv->opaque_region_changed)
    return;

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
      XserverRegion copy;

      copy = XFixesCreateRegion (priv->xdisplay, NULL, 0);
      XFixesCopyRegion (priv->xdisplay, copy, opaque_region);
      XFixesTranslateRegion (priv->xdisplay, copy, priv->x, priv->y);

      meta_compositor_add_damage (priv->compositor, "update_opaque_region", copy);
      XFixesDestroyRegion (priv->xdisplay, copy);
    }

  priv->opaque_region = opaque_region;
  priv->opaque_region_changed = FALSE;
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

  if (priv->opaque_region != None)
    {
      XFixesDestroyRegion (priv->xdisplay, priv->opaque_region);
      priv->opaque_region = None;
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

cairo_surface_t *
meta_surface_get_image (MetaSurface *self)
{
  return META_SURFACE_GET_CLASS (self)->get_image (self);
}

gboolean
meta_surface_is_visible (MetaSurface *self)
{
  MetaSurfacePrivate *priv;

  priv = meta_surface_get_instance_private (self);

  if (!meta_window_is_toplevel_mapped (priv->window) ||
      priv->pixmap == None)
    return FALSE;

  return TRUE;
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
}

void
meta_surface_process_damage (MetaSurface        *self,
                             XDamageNotifyEvent *event)
{
  MetaSurfacePrivate *priv;

  priv = meta_surface_get_instance_private (self);

  meta_compositor_queue_redraw (priv->compositor);
}

void
meta_surface_opacity_changed (MetaSurface *self)
{
  META_SURFACE_GET_CLASS (self)->opacity_changed (self);
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
meta_surface_sync_geometry (MetaSurface *self)
{
  MetaSurfacePrivate *priv;
  MetaRectangle rect;

  priv = meta_surface_get_instance_private (self);

  meta_window_get_input_rect (priv->window, &rect);

  if (priv->x != rect.x ||
      priv->y != rect.y)
    {
      priv->x = rect.x;
      priv->y = rect.y;
    }

  if (priv->width != rect.width ||
      priv->height != rect.height)
    {
      free_pixmap (self);

      meta_surface_opaque_region_changed (self);

      priv->width = rect.width;
      priv->height = rect.height;
    }
}

void
meta_surface_pre_paint (MetaSurface *self)
{
  MetaSurfacePrivate *priv;
  XserverRegion parts;

  priv = meta_surface_get_instance_private (self);

  meta_error_trap_push (priv->display);

  parts = XFixesCreateRegion (priv->xdisplay, 0, 0);
  XDamageSubtract (priv->xdisplay, priv->damage, None, parts);
  XFixesTranslateRegion (priv->xdisplay, parts, priv->x, priv->y);

  meta_error_trap_pop (priv->display);

  meta_compositor_add_damage (priv->compositor, "meta_surface_pre_paint", parts);
  XFixesDestroyRegion (priv->xdisplay, parts);

  update_opaque_region (self);

  ensure_pixmap (self);

  META_SURFACE_GET_CLASS (self)->pre_paint (self);
}
