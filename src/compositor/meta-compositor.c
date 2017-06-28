/*
 * Copyright (C) 2008 Iain Holmes
 * Copyright (C) 2017 Alberts Muktupāvels
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

#include <X11/extensions/shape.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>

#include "display-private.h"
#include "errors.h"
#include "meta-compositor-none.h"
#include "meta-compositor-xrender.h"
#include "meta-compositor-vulkan.h"
#include "screen-private.h"

typedef struct
{
  MetaDisplay *display;

  /* _NET_WM_CM_Sn */
  Atom         cm_atom;
  Window       cm_window;
  guint32      cm_timestamp;

  /* XCompositeGetOverlayWindow */
  Window       overlay_window;

  /* XCompositeRedirectSubwindows */
  gboolean     windows_redirected;

  /* meta_compositor_queue_redraw */
  guint        redraw_id;
} MetaCompositorPrivate;

enum
{
  PROP_0,

  PROP_DISPLAY,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

static void initable_iface_init (GInitableIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (MetaCompositor, meta_compositor, G_TYPE_OBJECT,
                                  G_ADD_PRIVATE (MetaCompositor)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                         initable_iface_init))

static gboolean
redraw_idle_cb (gpointer user_data)
{
  MetaCompositor *compositor;
  MetaCompositorPrivate *priv;

  compositor = META_COMPOSITOR (user_data);
  priv = meta_compositor_get_instance_private (compositor);

  META_COMPOSITOR_GET_CLASS (compositor)->redraw (compositor);

  priv->redraw_id = 0;

  return G_SOURCE_REMOVE;
}

static gboolean
check_common_extensions (MetaCompositor  *compositor,
                         GError         **error)
{
  MetaCompositorPrivate *priv;

  priv = meta_compositor_get_instance_private (compositor);

  if (!priv->display->have_composite)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Missing composite extension required for compositing");

      return FALSE;
    }

  if (!priv->display->have_damage)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Missing damage extension required for compositing");

      return FALSE;
    }

  if (!priv->display->have_xfixes)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Missing xfixes extension required for compositing");

      return FALSE;
    }

  return TRUE;
}

static gboolean
meta_compositor_initable_init (GInitable     *initable,
                               GCancellable  *cancellable,
                               GError       **error)
{
  MetaCompositor *compositor;
  MetaCompositorClass *compositor_class;

  compositor = META_COMPOSITOR (initable);
  compositor_class = META_COMPOSITOR_GET_CLASS (compositor);

  if (!META_IS_COMPOSITOR_NONE (compositor))
    {
      if (!check_common_extensions (compositor, error))
        return FALSE;
    }

  return compositor_class->manage (compositor, error);
}

static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = meta_compositor_initable_init;
}

static void
meta_compositor_finalize (GObject *object)
{
  MetaCompositor *compositor;
  MetaCompositorPrivate *priv;
  Display *xdisplay;

  compositor = META_COMPOSITOR (object);
  priv = meta_compositor_get_instance_private (compositor);
  xdisplay = priv->display->xdisplay;

  if (priv->redraw_id > 0)
    {
      g_source_remove (priv->redraw_id);
      priv->redraw_id = 0;
    }

  if (priv->windows_redirected)
    {
      Window xroot;

      xroot = DefaultRootWindow (xdisplay);
      XCompositeUnredirectSubwindows (xdisplay, xroot, CompositeRedirectManual);
      priv->windows_redirected = FALSE;
    }

  if (priv->overlay_window != None)
    {
      Window overlay;
      XserverRegion region;

      overlay = priv->overlay_window;

      region = XFixesCreateRegion (xdisplay, NULL, 0);
      XFixesSetWindowShapeRegion (xdisplay, overlay, ShapeBounding, 0, 0, region);
      XFixesDestroyRegion (xdisplay, region);

      XCompositeReleaseOverlayWindow (xdisplay, overlay);
      priv->overlay_window = None;
    }

  if (priv->cm_window != None)
    {
      XSetSelectionOwner (xdisplay, priv->cm_atom, None, priv->cm_timestamp);
      XDestroyWindow (xdisplay, priv->cm_window);
      priv->cm_window = None;
    }

  G_OBJECT_CLASS (meta_compositor_parent_class)->finalize (object);
}

static void
meta_compositor_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  MetaCompositor *compositor;
  MetaCompositorPrivate *priv;

  compositor = META_COMPOSITOR (object);
  priv = meta_compositor_get_instance_private (compositor);

  switch (property_id)
    {
      case PROP_DISPLAY:
        g_value_set_pointer (value, priv->display);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
meta_compositor_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  MetaCompositor *compositor;
  MetaCompositorPrivate *priv;

  compositor = META_COMPOSITOR (object);
  priv = meta_compositor_get_instance_private (compositor);

  switch (property_id)
    {
      case PROP_DISPLAY:
        priv->display = g_value_get_pointer (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
install_properties (GObjectClass *object_class)
{
  properties[PROP_DISPLAY] =
    g_param_spec_pointer ("display", "display", "display",
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
meta_compositor_class_init (MetaCompositorClass *compositor_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (compositor_class);

  object_class->finalize = meta_compositor_finalize;
  object_class->get_property = meta_compositor_get_property;
  object_class->set_property = meta_compositor_set_property;

  install_properties (object_class);
}

static void
meta_compositor_init (MetaCompositor *compositor)
{
}

MetaCompositor *
meta_compositor_new (MetaCompositorType  type,
                     MetaDisplay        *display)
{
  GType gtype;
  MetaCompositor *compositor;
  GError *error;

  switch (type)
    {
      case META_COMPOSITOR_TYPE_NONE:
        gtype = META_TYPE_COMPOSITOR_NONE;
        break;

      case META_COMPOSITOR_TYPE_XRENDER:
        gtype = META_TYPE_COMPOSITOR_XRENDER;
        break;

      case META_COMPOSITOR_TYPE_VULKAN:
        gtype = META_TYPE_COMPOSITOR_VULKAN;
        break;

      default:
        g_assert_not_reached ();
        break;
    }

  error = NULL;
  compositor = g_initable_new (gtype, NULL, &error, "display", display, NULL);

  if (compositor == NULL)
    {
      g_warning ("Failed to create %s: %s", g_type_name (gtype), error->message);
      g_error_free (error);

      if (type != META_COMPOSITOR_TYPE_NONE)
        compositor = meta_compositor_new (META_COMPOSITOR_TYPE_NONE, display);
    }

  g_assert (compositor != NULL);

  return compositor;
}

void
meta_compositor_add_window (MetaCompositor *compositor,
                            MetaWindow     *window)
{
  MetaCompositorClass *compositor_class;

  compositor_class = META_COMPOSITOR_GET_CLASS (compositor);

  compositor_class->add_window (compositor, window);
}

void
meta_compositor_remove_window (MetaCompositor *compositor,
                               MetaWindow     *window)
{
  MetaCompositorClass *compositor_class;

  compositor_class = META_COMPOSITOR_GET_CLASS (compositor);

  compositor_class->remove_window (compositor, window);
}

void
meta_compositor_show_window (MetaCompositor *compositor,
                             MetaWindow     *window,
                             MetaEffectType  effect)
{
  MetaCompositorClass *compositor_class;

  compositor_class = META_COMPOSITOR_GET_CLASS (compositor);

  compositor_class->show_window (compositor, window, effect);
}

void
meta_compositor_hide_window (MetaCompositor *compositor,
                             MetaWindow     *window,
                             MetaEffectType  effect)
{
  MetaCompositorClass *compositor_class;

  compositor_class = META_COMPOSITOR_GET_CLASS (compositor);

  compositor_class->hide_window (compositor, window, effect);
}

void
meta_compositor_window_opacity_changed (MetaCompositor *compositor,
                                        MetaWindow     *window)
{
  MetaCompositorClass *compositor_class;

  compositor_class = META_COMPOSITOR_GET_CLASS (compositor);

  compositor_class->window_opacity_changed (compositor, window);
}

void
meta_compositor_window_opaque_region_changed (MetaCompositor *compositor,
                                              MetaWindow     *window)
{
  MetaCompositorClass *compositor_class;

  compositor_class = META_COMPOSITOR_GET_CLASS (compositor);

  compositor_class->window_opaque_region_changed (compositor, window);
}

void
meta_compositor_window_shape_region_changed (MetaCompositor *compositor,
                                             MetaWindow     *window)
{
  MetaCompositorClass *compositor_class;

  compositor_class = META_COMPOSITOR_GET_CLASS (compositor);

  compositor_class->window_shape_region_changed (compositor, window);
}

void
meta_compositor_set_updates_frozen (MetaCompositor *compositor,
                                    MetaWindow     *window,
                                    gboolean        updates_frozen)
{
  MetaCompositorClass *compositor_class;

  compositor_class = META_COMPOSITOR_GET_CLASS (compositor);

  compositor_class->set_updates_frozen (compositor, window, updates_frozen);
}

void
meta_compositor_process_event (MetaCompositor *compositor,
                               XEvent         *event,
                               MetaWindow     *window)
{
  MetaCompositorClass *compositor_class;

  compositor_class = META_COMPOSITOR_GET_CLASS (compositor);

  compositor_class->process_event (compositor, event, window);
}

cairo_surface_t *
meta_compositor_get_window_surface (MetaCompositor *compositor,
                                    MetaWindow     *window)
{
  MetaCompositorClass *compositor_class;

  compositor_class = META_COMPOSITOR_GET_CLASS (compositor);

  return compositor_class->get_window_surface (compositor, window);
}

void
meta_compositor_maximize_window (MetaCompositor *compositor,
                                 MetaWindow     *window)
{
  MetaCompositorClass *compositor_class;

  compositor_class = META_COMPOSITOR_GET_CLASS (compositor);

  compositor_class->maximize_window (compositor, window);
}

void
meta_compositor_unmaximize_window (MetaCompositor *compositor,
                                   MetaWindow     *window)
{
  MetaCompositorClass *compositor_class;

  compositor_class = META_COMPOSITOR_GET_CLASS (compositor);

  compositor_class->unmaximize_window (compositor, window);
}

void
meta_compositor_sync_screen_size (MetaCompositor *compositor)
{
  MetaCompositorClass *compositor_class;

  compositor_class = META_COMPOSITOR_GET_CLASS (compositor);

  compositor_class->sync_screen_size (compositor);
}

void
meta_compositor_sync_stack (MetaCompositor *compositor,
                            GList          *stack)
{
  MetaCompositorClass *compositor_class;

  compositor_class = META_COMPOSITOR_GET_CLASS (compositor);

  compositor_class->sync_stack (compositor, stack);
}

void
meta_compositor_sync_window_geometry (MetaCompositor *compositor,
                                      MetaWindow     *window)
{
  MetaCompositorClass *compositor_class;

  compositor_class = META_COMPOSITOR_GET_CLASS (compositor);

  compositor_class->sync_window_geometry (compositor, window);
}

gboolean
meta_compositor_is_our_xwindow (MetaCompositor *compositor,
                                Window          xwindow)
{
  MetaCompositorPrivate *priv;

  priv = meta_compositor_get_instance_private (compositor);

  if (priv->cm_window == xwindow)
    return TRUE;

  if (priv->overlay_window == xwindow)
    return TRUE;

  return FALSE;
}

gboolean
meta_compositor_is_composited (MetaCompositor *compositor)
{
  return !META_IS_COMPOSITOR_NONE (compositor);
}

gboolean
meta_compositor_set_selection (MetaCompositor  *compositor,
                               GError         **error)
{
  MetaCompositorPrivate *priv;
  Display *xdisplay;
  gchar *atom_name;
  Window xroot;

  priv = meta_compositor_get_instance_private (compositor);
  xdisplay = priv->display->xdisplay;

  atom_name = g_strdup_printf ("_NET_WM_CM_S%d", DefaultScreen (xdisplay));
  priv->cm_atom = XInternAtom (xdisplay, atom_name, FALSE);
  g_free (atom_name);

  xroot = DefaultRootWindow (xdisplay);
  priv->cm_window = meta_create_offscreen_window (xdisplay, xroot, NoEventMask);
  priv->cm_timestamp = meta_display_get_current_time_roundtrip (priv->display);

  XSetSelectionOwner (xdisplay, priv->cm_atom, priv->cm_window, priv->cm_timestamp);

  if (XGetSelectionOwner (xdisplay, priv->cm_atom) != priv->cm_window)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Could not acquire selection: %s",
                   XGetAtomName (xdisplay, priv->cm_atom));

      return FALSE;
    }

  return TRUE;
}

Window
meta_compositor_get_overlay_window (MetaCompositor *compositor)
{
  MetaCompositorPrivate *priv;
  Display *xdisplay;
  Window xroot;
  Window overlay;
  XserverRegion region;

  priv = meta_compositor_get_instance_private (compositor);

  if (priv->overlay_window != None)
    return priv->overlay_window;

  xdisplay = priv->display->xdisplay;
  xroot = DefaultRootWindow (xdisplay);

  overlay = XCompositeGetOverlayWindow (xdisplay, xroot);

  /* Get Expose events about window regions that have lost contents. */
  XSelectInput (xdisplay, overlay, ExposureMask);

  /* Make sure there isn't any left-over output shape on the overlay
   * window by setting the whole screen to be an output region.
   */
  XFixesSetWindowShapeRegion (xdisplay, overlay, ShapeBounding, 0, 0, 0);

  /* Allow events to pass through the overlay */
  region = XFixesCreateRegion (xdisplay, NULL, 0);
  XFixesSetWindowShapeRegion (xdisplay, overlay, ShapeInput, 0, 0, region);
  XFixesDestroyRegion (xdisplay, region);

  priv->overlay_window = overlay;
  return overlay;
}

gboolean
meta_compositor_redirect_windows (MetaCompositor  *compositor,
                                  GError         **error)
{
  MetaCompositorPrivate *priv;
  Display *xdisplay;
  Window xroot;

  priv = meta_compositor_get_instance_private (compositor);

  xdisplay = priv->display->xdisplay;
  xroot = DefaultRootWindow (xdisplay);

  meta_error_trap_push (priv->display);
  XCompositeRedirectSubwindows (xdisplay, xroot, CompositeRedirectManual);
  XSync (xdisplay, FALSE);

  if (meta_error_trap_pop_with_return (priv->display) != Success)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Another compositing manager is running on screen %i",
                   DefaultScreen (xdisplay));

      return FALSE;
    }

  priv->windows_redirected = TRUE;
  return TRUE;
}

MetaDisplay *
meta_compositor_get_display (MetaCompositor *compositor)
{
  MetaCompositorPrivate *priv;

  priv = meta_compositor_get_instance_private (compositor);

  return priv->display;
}

void
meta_compositor_queue_redraw (MetaCompositor *compositor)
{
  MetaCompositorPrivate *priv;
  gint priority;

  priv = meta_compositor_get_instance_private (compositor);
  priority = META_PRIORITY_REDRAW;

  if (priv->redraw_id > 0)
    return;

  priv->redraw_id = g_idle_add_full (priority, redraw_idle_cb, compositor, NULL);
  g_source_set_name_by_id (priv->redraw_id, "[metacity] redraw_idle_cb");
}
