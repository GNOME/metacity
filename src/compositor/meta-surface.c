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
#include "meta-compositor-private.h"
#include "window-private.h"

typedef struct
{
  MetaCompositor *compositor;
  MetaWindow     *window;

  Damage          damage;
  Pixmap          pixmap;

  int             width;
  int             height;
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
free_pixmap (MetaSurface *self)
{
  MetaSurfacePrivate *priv;
  MetaDisplay *display;
  Display *xdisplay;

  priv = meta_surface_get_instance_private (self);

  if (priv->pixmap == None)
    return;

  display = meta_compositor_get_display (priv->compositor);
  xdisplay = meta_display_get_xdisplay (display);

  META_SURFACE_GET_CLASS (self)->free_pixmap (self);

  meta_error_trap_push (display);

  XFreePixmap (xdisplay, priv->pixmap);
  priv->pixmap = None;

  meta_error_trap_pop (display);
}

static void
ensure_pixmap (MetaSurface *self)
{
  MetaSurfacePrivate *priv;
  MetaDisplay *display;
  Display *xdisplay;
  Window xwindow;

  priv = meta_surface_get_instance_private (self);
  display = meta_compositor_get_display (priv->compositor);
  xdisplay = meta_display_get_xdisplay (display);

  if (priv->pixmap != None)
    return;

  meta_error_trap_push (display);

  xwindow = meta_window_get_toplevel_xwindow (priv->window);
  priv->pixmap = XCompositeNameWindowPixmap (xdisplay, xwindow);

  if (meta_error_trap_pop_with_return (display) != 0)
    priv->pixmap = None;
}

static void
destroy_damage (MetaSurface *self)
{
  MetaSurfacePrivate *priv;
  MetaDisplay *display;
  Display *xdisplay;

  priv = meta_surface_get_instance_private (self);

  if (priv->damage == None)
    return;

  display = meta_compositor_get_display (priv->compositor);
  xdisplay = meta_display_get_xdisplay (display);

  meta_error_trap_push (display);

  XDamageDestroy (xdisplay, priv->damage);
  priv->damage = None;

  meta_error_trap_pop (display);
}

static void
create_damage (MetaSurface *self)
{
  MetaSurfacePrivate *priv;
  MetaDisplay *display;
  Display *xdisplay;

  priv = meta_surface_get_instance_private (self);
  display = meta_compositor_get_display (priv->compositor);
  xdisplay = meta_display_get_xdisplay (display);

  meta_error_trap_push (display);

  g_assert (priv->damage == None);
  priv->damage = XDamageCreate (xdisplay,
                                meta_window_get_toplevel_xwindow (priv->window),
                                XDamageReportNonEmpty);

  meta_error_trap_pop (display);
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

  create_damage (self);

  g_signal_connect_object (priv->window, "notify::decorated",
                           G_CALLBACK (notify_decorated_cb),
                           self, 0);
}

static void
meta_surface_finalize (GObject *object)
{
  MetaSurface *self;

  self = META_SURFACE (object);

  destroy_damage (self);
  free_pixmap (self);

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

void
meta_surface_show (MetaSurface *self)
{
  /* The reason we free pixmap here is so that we will still have
   * a valid pixmap when the window is unmapped.
   */
  free_pixmap (self);
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
meta_surface_pre_paint (MetaSurface *self)
{
  MetaSurfacePrivate *priv;
  MetaDisplay *display;
  Display *xdisplay;
  MetaRectangle rect;
  XserverRegion parts;

  priv = meta_surface_get_instance_private (self);
  display = meta_compositor_get_display (priv->compositor);
  xdisplay = meta_display_get_xdisplay (display);

  meta_window_get_input_rect (priv->window, &rect);

  meta_error_trap_push (display);

  parts = XFixesCreateRegion (xdisplay, 0, 0);
  XDamageSubtract (xdisplay, priv->damage, None, parts);
  XFixesTranslateRegion (xdisplay, parts, rect.x, rect.y);

  meta_error_trap_pop (display);

  meta_compositor_add_damage (priv->compositor, "meta_surface_pre_paint", parts);
  XFixesDestroyRegion (xdisplay, parts);

  ensure_pixmap (self);

  META_SURFACE_GET_CLASS (self)->pre_paint (self);
}

void
meta_surface_sync_geometry (MetaSurface *self)
{
  MetaSurfacePrivate *priv;
  MetaRectangle rect;

  priv = meta_surface_get_instance_private (self);

  meta_window_get_input_rect (priv->window, &rect);

  if (priv->width != rect.width ||
      priv->height != rect.height)
    {
      free_pixmap (self);

      priv->width = rect.width;
      priv->height = rect.height;
    }
}
