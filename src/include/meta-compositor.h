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

#ifndef META_COMPOSITOR_H
#define META_COMPOSITOR_H

#include <glib-object.h>
#include <X11/Xlib.h>

#include "types.h"
#include "boxes.h"

G_BEGIN_DECLS

#define META_TYPE_COMPOSITOR meta_compositor_get_type ()
G_DECLARE_DERIVABLE_TYPE (MetaCompositor, meta_compositor,
                          META, COMPOSITOR, GObject)

typedef enum
{
  META_COMPOSITOR_TYPE_NONE,
  META_COMPOSITOR_TYPE_XRENDER,
  META_COMPOSITOR_TYPE_XPRESENT,
  META_COMPOSITOR_TYPE_EXTERNAL, /*< skip >*/
  META_COMPOSITOR_TYPE_VULKAN /*< skip >*/
} MetaCompositorType;

typedef enum /*< skip >*/
{
  META_EFFECT_TYPE_NONE,
  META_EFFECT_TYPE_CREATE,
  META_EFFECT_TYPE_DESTROY,
  META_EFFECT_TYPE_MINIMIZE,
  META_EFFECT_TYPE_UNMINIMIZE,
} MetaEffectType;

void             meta_compositor_add_window                   (MetaCompositor     *compositor,
                                                               MetaWindow         *window);

void             meta_compositor_remove_window                (MetaCompositor     *compositor,
                                                               MetaWindow         *window);

void             meta_compositor_show_window                  (MetaCompositor     *compositor,
                                                               MetaWindow         *window,
                                                               MetaEffectType      effect);

void             meta_compositor_hide_window                  (MetaCompositor     *compositor,
                                                               MetaWindow         *window,
                                                               MetaEffectType      effect);

void             meta_compositor_window_opacity_changed       (MetaCompositor     *compositor,
                                                               MetaWindow         *window);

void             meta_compositor_window_opaque_region_changed (MetaCompositor     *compositor,
                                                               MetaWindow         *window);

void             meta_compositor_window_shape_region_changed  (MetaCompositor     *compositor,
                                                               MetaWindow         *window);

void             meta_compositor_set_updates_frozen           (MetaCompositor     *compositor,
                                                               MetaWindow         *window,
                                                               gboolean            updates_frozen);

void             meta_compositor_process_event                (MetaCompositor     *compositor,
                                                               XEvent             *event,
                                                               MetaWindow         *window);

cairo_surface_t *meta_compositor_get_window_surface           (MetaCompositor     *compositor,
                                                               MetaWindow         *window);

void             meta_compositor_maximize_window              (MetaCompositor     *compositor,
                                                               MetaWindow         *window);

void             meta_compositor_unmaximize_window            (MetaCompositor     *compositor,
                                                               MetaWindow         *window);

void             meta_compositor_sync_screen_size             (MetaCompositor     *compositor);

void             meta_compositor_sync_stack                   (MetaCompositor     *compositor,
                                                               GList              *stack);

void             meta_compositor_sync_window_geometry         (MetaCompositor     *compositor,
                                                               MetaWindow         *window);

gboolean         meta_compositor_is_our_xwindow               (MetaCompositor     *compositor,
                                                               Window              xwindow);

gboolean         meta_compositor_is_composited                (MetaCompositor     *compositor);

G_END_DECLS

#endif
