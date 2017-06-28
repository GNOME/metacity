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

#ifndef META_COMPOSITOR_PRIVATE_H
#define META_COMPOSITOR_PRIVATE_H

#include "meta-compositor.h"

G_BEGIN_DECLS

struct _MetaCompositorClass
{
  GObjectClass parent_class;

  gboolean          (* manage)                       (MetaCompositor     *compositor,
                                                      GError            **error);

  void              (* add_window)                   (MetaCompositor     *compositor,
                                                      MetaWindow         *window);

  void              (* remove_window)                (MetaCompositor     *compositor,
                                                      MetaWindow         *window);

  void              (* show_window)                  (MetaCompositor     *compositor,
                                                      MetaWindow         *window,
                                                      MetaEffectType      effect);

  void              (* hide_window)                  (MetaCompositor     *compositor,
                                                      MetaWindow         *window,
                                                      MetaEffectType      effect);

  void              (* window_opacity_changed)       (MetaCompositor     *compositor,
                                                      MetaWindow         *window);

  void              (* window_opaque_region_changed) (MetaCompositor     *compositor,
                                                      MetaWindow         *window);

  void              (* window_shape_region_changed)  (MetaCompositor     *compositor,
                                                      MetaWindow         *window);

  void              (* set_updates_frozen)           (MetaCompositor     *compositor,
                                                      MetaWindow         *window,
                                                      gboolean            updates_frozen);

  void              (* process_event)                (MetaCompositor     *compositor,
                                                      XEvent             *event,
                                                      MetaWindow         *window);

  cairo_surface_t * (* get_window_surface)           (MetaCompositor     *compositor,
                                                      MetaWindow         *window);

  void              (* maximize_window)              (MetaCompositor     *compositor,
                                                      MetaWindow         *window);

  void              (* unmaximize_window)            (MetaCompositor     *compositor,
                                                      MetaWindow         *window);

  void              (* sync_screen_size)             (MetaCompositor     *compositor);

  void              (* sync_stack)                   (MetaCompositor     *compositor,
                                                      GList              *stack);

  void              (* sync_window_geometry)         (MetaCompositor     *compositor,
                                                      MetaWindow         *window);

  void              (* redraw)                       (MetaCompositor     *compositor);
};

gboolean     meta_compositor_set_selection      (MetaCompositor  *compositor,
                                                 GError         **error);

Window       meta_compositor_get_overlay_window (MetaCompositor  *compositor);

gboolean     meta_compositor_redirect_windows   (MetaCompositor  *compositor,
                                                 GError         **error);

MetaDisplay *meta_compositor_get_display        (MetaCompositor  *compositor);

void         meta_compositor_queue_redraw       (MetaCompositor  *compositor);

G_END_DECLS

#endif
