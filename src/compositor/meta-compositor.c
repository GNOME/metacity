/*
 * Copyright (C) 2008 Iain Holmes
 * Copyright (C) 2017-2019 Alberts Muktupāvels
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
#include "meta-compositor-private.h"

#include <X11/extensions/shape.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>

#include "display-private.h"
#include "errors.h"
#include "frame.h"
#include "util.h"
#include "screen-private.h"

typedef struct
{
  MetaDisplay   *display;

  gboolean       composited;

  /* _NET_WM_CM_Sn */
  Atom           cm_atom;
  Window         cm_window;
  guint32        cm_timestamp;

  /* XCompositeGetOverlayWindow */
  Window         overlay_window;

  /* XCompositeRedirectSubwindows */
  gboolean       windows_redirected;

  XserverRegion  all_damage;

  GHashTable    *surfaces;
  GList         *stack;

  /* meta_compositor_queue_redraw */
  guint          redraw_id;
} MetaCompositorPrivate;

enum
{
  PROP_0,

  PROP_DISPLAY,

  PROP_COMPOSITED,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

static void initable_iface_init (GInitableIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (MetaCompositor, meta_compositor, G_TYPE_OBJECT,
                                  G_ADD_PRIVATE (MetaCompositor)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                         initable_iface_init))

static void
debug_damage_region (MetaCompositor *compositor,
                     const gchar    *name,
                     XserverRegion   damage)
{
  MetaCompositorPrivate *priv;
  Display *xdisplay;

  if (!meta_check_debug_flags (META_DEBUG_DAMAGE_REGION))
    return;

  priv = meta_compositor_get_instance_private (compositor);
  xdisplay = priv->display->xdisplay;

  if (damage != None)
    {
      XRectangle *rects;
      int nrects;
      XRectangle bounds;

      rects = XFixesFetchRegionAndBounds (xdisplay, damage, &nrects, &bounds);

      if (nrects > 0)
        {
          int i;

          meta_topic (META_DEBUG_DAMAGE_REGION, "%s: %d rects, bounds: %d,%d (%d,%d)\n",
                      name, nrects, bounds.x, bounds.y, bounds.width, bounds.height);

          meta_push_no_msg_prefix ();

          for (i = 0; i < nrects; i++)
            {
              meta_topic (META_DEBUG_DAMAGE_REGION, "\t%d,%d (%d,%d)\n", rects[i].x,
                          rects[i].y, rects[i].width, rects[i].height);
            }

          meta_pop_no_msg_prefix ();
        }
      else
        {
          meta_topic (META_DEBUG_DAMAGE_REGION, "%s: empty\n", name);
        }

      XFree (rects);
    }
  else
    {
      meta_topic (META_DEBUG_DAMAGE_REGION, "%s: none\n", name);
    }
}

static MetaSurface *
find_surface_by_xwindow (MetaCompositor *compositor,
                         Window          xwindow)
{
  MetaCompositorPrivate *priv;
  MetaSurface *surface;
  GHashTableIter iter;

  priv = meta_compositor_get_instance_private (compositor);
  surface = NULL;

  g_hash_table_iter_init (&iter, priv->surfaces);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer) &surface))
    {
      MetaWindow *window;
      MetaFrame *frame;

      window = meta_surface_get_window (surface);
      frame = meta_window_get_frame (window);

      if (frame != NULL)
        {
          if (meta_frame_get_xwindow (frame) == xwindow)
            break;
        }
      else
        {
          if (meta_window_get_xwindow (window) == xwindow)
            break;
        }

      surface = NULL;
    }

  return surface;
}

static gboolean
redraw_idle_cb (gpointer user_data)
{
  MetaCompositor *compositor;
  MetaCompositorPrivate *priv;

  compositor = META_COMPOSITOR (user_data);
  priv = meta_compositor_get_instance_private (compositor);

  if (!META_COMPOSITOR_GET_CLASS (compositor)->ready_to_redraw (compositor))
    {
      priv->redraw_id = 0;

      return G_SOURCE_REMOVE;
    }

  META_COMPOSITOR_GET_CLASS (compositor)->pre_paint (compositor);

  if (priv->all_damage != None)
    {
      debug_damage_region (compositor, "paint_all", priv->all_damage);

      META_COMPOSITOR_GET_CLASS (compositor)->redraw (compositor, priv->all_damage);
      XFixesDestroyRegion (priv->display->xdisplay, priv->all_damage);
      priv->all_damage = None;
    }

  priv->redraw_id = 0;

  return G_SOURCE_REMOVE;
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

  return compositor_class->manage (compositor, error);
}

static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = meta_compositor_initable_init;
}

static void
meta_compositor_dispose (GObject *object)
{
  MetaCompositor *compositor;
  MetaCompositorPrivate *priv;

  compositor = META_COMPOSITOR (object);
  priv = meta_compositor_get_instance_private (compositor);

  g_clear_pointer (&priv->surfaces, g_hash_table_destroy);
  g_clear_pointer (&priv->stack, g_list_free);

  G_OBJECT_CLASS (meta_compositor_parent_class)->dispose (object);
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

  if (priv->all_damage != None)
    {
      XFixesDestroyRegion (xdisplay, priv->all_damage);
      priv->all_damage = None;
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

      case PROP_COMPOSITED:
        g_value_set_boolean (value, priv->composited);
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

      case PROP_COMPOSITED:
        g_assert_not_reached ();
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static gboolean
meta_compositor_ready_to_redraw (MetaCompositor *compositor)
{
  return TRUE;
}

static void
meta_compositor_pre_paint (MetaCompositor *compositor)
{
  MetaCompositorPrivate *priv;
  GHashTableIter iter;
  MetaSurface *surface;

  priv = meta_compositor_get_instance_private (compositor);

  g_hash_table_iter_init (&iter, priv->surfaces);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer) &surface))
    meta_surface_pre_paint (surface);
}

static void
install_properties (GObjectClass *object_class)
{
  properties[PROP_DISPLAY] =
    g_param_spec_pointer ("display", "display", "display",
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_COMPOSITED] =
    g_param_spec_boolean ("composited", "composited", "composited",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
meta_compositor_class_init (MetaCompositorClass *compositor_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (compositor_class);

  object_class->dispose = meta_compositor_dispose;
  object_class->finalize = meta_compositor_finalize;
  object_class->get_property = meta_compositor_get_property;
  object_class->set_property = meta_compositor_set_property;

  compositor_class->ready_to_redraw = meta_compositor_ready_to_redraw;
  compositor_class->pre_paint = meta_compositor_pre_paint;

  install_properties (object_class);
}

static void
meta_compositor_init (MetaCompositor *compositor)
{
  MetaCompositorPrivate *priv;

  priv = meta_compositor_get_instance_private (compositor);

  priv->surfaces = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                          NULL, g_object_unref);
}

void
meta_compositor_add_window (MetaCompositor *compositor,
                            MetaWindow     *window)
{
  MetaCompositorPrivate *priv;
  MetaCompositorClass *compositor_class;
  MetaSurface *surface;

  g_assert (window != NULL);

  priv = meta_compositor_get_instance_private (compositor);

  /* If already added, ignore */
  if (g_hash_table_lookup (priv->surfaces, window) != NULL)
    return;

  compositor_class = META_COMPOSITOR_GET_CLASS (compositor);
  surface = compositor_class->add_window (compositor, window);

  if (surface == NULL)
    return;

  g_hash_table_insert (priv->surfaces, window, surface);
  priv->stack = g_list_prepend (priv->stack, surface);
}

void
meta_compositor_remove_window (MetaCompositor *compositor,
                               MetaWindow     *window)
{
  MetaCompositorPrivate *priv;
  MetaSurface *surface;

  priv = meta_compositor_get_instance_private (compositor);

  surface = g_hash_table_lookup (priv->surfaces, window);
  if (surface == NULL)
    return;

  priv->stack = g_list_remove (priv->stack, surface);
  g_hash_table_remove (priv->surfaces, window);
}

void
meta_compositor_show_window (MetaCompositor *compositor,
                             MetaWindow     *window,
                             MetaEffectType  effect)
{
  MetaCompositorPrivate *priv;
  MetaSurface *surface;

  priv = meta_compositor_get_instance_private (compositor);

  surface = g_hash_table_lookup (priv->surfaces, window);
  if (surface == NULL)
    return;

  meta_surface_show (surface);
}

void
meta_compositor_hide_window (MetaCompositor *compositor,
                             MetaWindow     *window,
                             MetaEffectType  effect)
{
  MetaCompositorPrivate *priv;
  MetaSurface *surface;

  priv = meta_compositor_get_instance_private (compositor);

  surface = g_hash_table_lookup (priv->surfaces, window);
  if (surface == NULL)
    return;

  meta_surface_hide (surface);
}

void
meta_compositor_window_opacity_changed (MetaCompositor *compositor,
                                        MetaWindow     *window)
{
  MetaCompositorPrivate *priv;
  MetaSurface *surface;

  priv = meta_compositor_get_instance_private (compositor);

  surface = g_hash_table_lookup (priv->surfaces, window);
  if (surface == NULL)
    return;

  meta_surface_opacity_changed (surface);
}

void
meta_compositor_window_opaque_region_changed (MetaCompositor *compositor,
                                              MetaWindow     *window)
{
  MetaCompositorPrivate *priv;
  MetaSurface *surface;

  priv = meta_compositor_get_instance_private (compositor);

  surface = g_hash_table_lookup (priv->surfaces, window);
  if (surface == NULL)
    return;

  meta_surface_opaque_region_changed (surface);
}

void
meta_compositor_window_shape_region_changed (MetaCompositor *compositor,
                                             MetaWindow     *window)
{
  MetaCompositorPrivate *priv;
  MetaSurface *surface;

  priv = meta_compositor_get_instance_private (compositor);

  surface = g_hash_table_lookup (priv->surfaces, window);
  if (surface == NULL)
    return;

  meta_surface_shape_region_changed (surface);
}

void
meta_compositor_set_updates_frozen (MetaCompositor *compositor,
                                    MetaWindow     *window,
                                    gboolean        updates_frozen)
{
}

void
meta_compositor_process_event (MetaCompositor *compositor,
                               XEvent         *event,
                               MetaWindow     *window)
{
  MetaCompositorPrivate *priv;
  MetaCompositorClass *compositor_class;
  int damage_event_base;

  priv = meta_compositor_get_instance_private (compositor);

  compositor_class = META_COMPOSITOR_GET_CLASS (compositor);
  compositor_class->process_event (compositor, event, window);

  damage_event_base = meta_display_get_damage_event_base (priv->display);

  if (event->type == Expose)
    {
      XExposeEvent *expose_event;
      MetaSurface *surface;
      XRectangle rect;
      XserverRegion region;

      expose_event = (XExposeEvent *) event;

      if (window != NULL)
        surface = g_hash_table_lookup (priv->surfaces, window);
      else
        surface = find_surface_by_xwindow (compositor, expose_event->window);

      rect.x = expose_event->x;
      rect.y = expose_event->y;
      rect.width = expose_event->width;
      rect.height = expose_event->height;

      if (surface != NULL)
        {
          rect.x += meta_surface_get_x (surface);
          rect.y += meta_surface_get_y (surface);
        }

      region = XFixesCreateRegion (priv->display->xdisplay, &rect, 1);
      meta_compositor_add_damage (compositor, "XExposeEvent", region);
      XFixesDestroyRegion (priv->display->xdisplay, region);
    }
  else if (event->type == damage_event_base + XDamageNotify)
    {
      XDamageNotifyEvent *damage_event;
      MetaSurface *surface;

      damage_event = (XDamageNotifyEvent *) event;

      if (window != NULL)
        surface = g_hash_table_lookup (priv->surfaces, window);
      else
        surface = find_surface_by_xwindow (compositor, damage_event->drawable);

      if (surface != NULL)
        meta_surface_process_damage (surface, damage_event);
    }
}

cairo_surface_t *
meta_compositor_get_window_surface (MetaCompositor *compositor,
                                    MetaWindow     *window)
{
  MetaCompositorPrivate *priv;
  MetaSurface *surface;

  priv = meta_compositor_get_instance_private (compositor);

  surface = g_hash_table_lookup (priv->surfaces, window);
  if (surface == NULL)
    return NULL;

  return meta_surface_get_image (surface);
}

void
meta_compositor_maximize_window (MetaCompositor *compositor,
                                 MetaWindow     *window)
{
}

void
meta_compositor_unmaximize_window (MetaCompositor *compositor,
                                   MetaWindow     *window)
{
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
  MetaCompositorPrivate *priv;
  gboolean changed;
  GList *l1;
  GList *l2;

  priv = meta_compositor_get_instance_private (compositor);

  if (priv->stack == NULL)
    return;

  changed = FALSE;
  for (l1 = stack, l2 = priv->stack;
       l1 != NULL && l2 != NULL;
       l1 = l1->next, l2 = l2->next)
    {
      MetaWindow *window;
      MetaSurface *surface;

      window = META_WINDOW (l1->data);
      surface = g_hash_table_lookup (priv->surfaces, window);

      if (surface != META_SURFACE (l2->data))
        {
          changed = TRUE;
          break;
        }
    }

  if (!changed)
    return;

  for (l1 = stack; l1 != NULL; l1 = l1->next)
    {
      MetaWindow *window;
      MetaSurface *surface;

      window = META_WINDOW (l1->data);
      surface = g_hash_table_lookup (priv->surfaces, window);

      if (surface == NULL)
        {
          g_warning ("Failed to find MetaSurface for MetaWindow %p", window);
          continue;
        }

      priv->stack = g_list_remove (priv->stack, surface);
      priv->stack = g_list_prepend (priv->stack, surface);
    }

  priv->stack = g_list_reverse (priv->stack);
  meta_compositor_damage_screen (compositor);
}

void
meta_compositor_sync_window_geometry (MetaCompositor *compositor,
                                      MetaWindow     *window)
{
  MetaCompositorPrivate *priv;
  MetaSurface *surface;

  priv = meta_compositor_get_instance_private (compositor);

  surface = g_hash_table_lookup (priv->surfaces, window);
  if (surface == NULL)
    return;

  meta_surface_sync_geometry (surface);
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
  MetaCompositorPrivate *priv;

  priv = meta_compositor_get_instance_private (compositor);

  return priv->composited;
}

void
meta_compositor_set_composited (MetaCompositor *compositor,
                                gboolean        composited)
{
  MetaCompositorPrivate *priv;

  priv = meta_compositor_get_instance_private (compositor);

  if (priv->composited == composited)
    return;

  priv->composited = composited;

  g_object_notify_by_pspec (G_OBJECT (compositor), properties[PROP_COMPOSITED]);
}

gboolean
meta_compositor_check_common_extensions (MetaCompositor  *compositor,
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

/**
 * meta_compositor_get_stack:
 * @compositor: a #MetaCompositor
 *
 * Returns the the list of surfaces in stacking order.
 *
 * Returns: (transfer none) (element-type MetaSurface): the list of surfaces
 */
GList *
meta_compositor_get_stack (MetaCompositor *compositor)
{
  MetaCompositorPrivate *priv;

  priv = meta_compositor_get_instance_private (compositor);

  return priv->stack;
}

/**
 * meta_compositor_add_damage:
 * @compositor: a #MetaCompositor
 * @name: the name of damage region
 * @damage: the damage region
 *
 * Adds damage region and queues a redraw.
 */
void
meta_compositor_add_damage (MetaCompositor *compositor,
                            const gchar    *name,
                            XserverRegion   damage)
{
  MetaCompositorPrivate *priv;
  Display *xdisplay;

  priv = meta_compositor_get_instance_private (compositor);
  xdisplay = priv->display->xdisplay;

  debug_damage_region (compositor, name, damage);

  if (priv->all_damage != None)
    {
      XFixesUnionRegion (xdisplay, priv->all_damage, priv->all_damage, damage);
    }
  else
    {
      priv->all_damage = XFixesCreateRegion (xdisplay, NULL, 0);
      XFixesCopyRegion (xdisplay, priv->all_damage, damage);
    }

  meta_compositor_queue_redraw (compositor);
}

void
meta_compositor_damage_screen (MetaCompositor *compositor)
{
  MetaCompositorPrivate *priv;
  Display *xdisplay;
  int screen_width;
  int screen_height;
  XserverRegion screen_region;

  priv = meta_compositor_get_instance_private (compositor);
  xdisplay = priv->display->xdisplay;

  meta_screen_get_size (priv->display->screen, &screen_width, &screen_height);

  screen_region = XFixesCreateRegion (xdisplay, &(XRectangle) {
                                        .width = screen_width,
                                        .height = screen_height
                                      }, 1);

  meta_compositor_add_damage (compositor, "damage_screen", screen_region);
  XFixesDestroyRegion (xdisplay, screen_region);
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
