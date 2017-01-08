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
  META_COMPOSITOR_TYPE_XRENDER
} MetaCompositorType;

MetaCompositor  *meta_compositor_new                (MetaCompositorType  type,
                                                     MetaDisplay        *display);

void             meta_compositor_manage_screen      (MetaCompositor     *compositor,
                                                     MetaScreen         *screen);

void             meta_compositor_unmanage_screen    (MetaCompositor     *compositor,
                                                     MetaScreen         *screen);

void             meta_compositor_add_window         (MetaCompositor     *compositor,
                                                     MetaWindow         *window,
                                                     Window              xwindow,
                                                     XWindowAttributes  *attrs);

void             meta_compositor_remove_window      (MetaCompositor     *compositor,
                                                     Window              xwindow);

void             meta_compositor_set_updates        (MetaCompositor     *compositor,
                                                     MetaWindow         *window,
                                                     gboolean            updates);

void             meta_compositor_process_event      (MetaCompositor     *compositor,
                                                     XEvent             *event,
                                                     MetaWindow         *window);

cairo_surface_t *meta_compositor_get_window_surface (MetaCompositor     *compositor,
                                                     MetaWindow         *window);

void             meta_compositor_set_active_window  (MetaCompositor     *compositor,
                                                     MetaScreen         *screen,
                                                     MetaWindow         *window);

void             meta_compositor_begin_move         (MetaCompositor     *compositor,
                                                     MetaWindow         *window,
                                                     MetaRectangle      *initial,
                                                     gint                grab_x,
                                                     gint                grab_y);

void             meta_compositor_update_move        (MetaCompositor     *compositor,
                                                     MetaWindow         *window,
                                                     gint                x,
                                                     gint                y);

void             meta_compositor_end_move           (MetaCompositor     *compositor,
                                                     MetaWindow         *window);

void             meta_compositor_free_window        (MetaCompositor     *compositor,
                                                     MetaWindow         *window);

void             meta_compositor_maximize_window    (MetaCompositor     *compositor,
                                                     MetaWindow         *window);

void             meta_compositor_unmaximize_window  (MetaCompositor     *compositor,
                                                     MetaWindow         *window);

MetaDisplay     *meta_compositor_get_display        (MetaCompositor     *compositor);

G_END_DECLS

#endif
