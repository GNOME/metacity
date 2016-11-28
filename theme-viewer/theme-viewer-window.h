/*
 * Copyright (C) 2016 Alberts Muktupāvels
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

#ifndef THEME_VIEWER_WINDOW_H
#define THEME_VIEWER_WINDOW_H

#include <gtk/gtk.h>
#include <libmetacity/meta-theme.h>

G_BEGIN_DECLS

#define THEME_VIEWER_TYPE_WINDOW theme_viewer_window_get_type ()
G_DECLARE_FINAL_TYPE (ThemeViewerWindow, theme_viewer_window,
                      THEME_VIEWER, WINDOW, GtkWindow)

GtkWidget *theme_viewer_window_new            (void);

void       theme_viewer_window_set_theme_type (ThemeViewerWindow *window,
                                               MetaThemeType      theme_type);

void       theme_viewer_window_set_theme_name (ThemeViewerWindow *window,
                                               const gchar       *theme_name);

G_END_DECLS

#endif
