/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2008 Iain Holmes
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef META_WINDOW_H
#define META_WINDOW_H

#include <cairo.h>
#include <glib-object.h>
#include <X11/Xlib.h>

#include "boxes.h"
#include "types.h"

G_BEGIN_DECLS

#define META_TYPE_WINDOW meta_window_get_type ()
G_DECLARE_FINAL_TYPE (MetaWindow, meta_window, META, WINDOW, GObject)

MetaFrame *meta_window_get_frame (MetaWindow *window);
gboolean meta_window_has_focus (MetaWindow *window);
gboolean meta_window_is_shaded (MetaWindow *window);
MetaScreen *meta_window_get_screen (MetaWindow *window);
MetaDisplay *meta_window_get_display (MetaWindow *window);
Window meta_window_get_xwindow (MetaWindow *window);
Window meta_window_get_toplevel_xwindow (MetaWindow *window);
Visual *meta_window_get_toplevel_xvisual (MetaWindow *window);
MetaWindow *meta_window_get_transient_for (MetaWindow *window);
gboolean meta_window_is_fullscreen (MetaWindow *window);
gboolean meta_window_is_maximized (MetaWindow *window);
gboolean meta_window_is_attached_dialog (MetaWindow *window);
gboolean meta_window_is_toplevel_mapped (MetaWindow *window);
gboolean meta_window_appears_focused (MetaWindow *window);
cairo_region_t *meta_window_get_frame_bounds (MetaWindow *window);
void meta_window_move_to_monitor (MetaWindow *window, int monitor);

G_END_DECLS

#endif
