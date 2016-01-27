/*
 * Copyright (C) 2001 Havoc Pennington
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

#ifndef META_COLOR_H
#define META_COLOR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

void meta_color_shade                (const GdkRGBA   *source,
                                      const gdouble    factor,
                                      GdkRGBA         *destination);

void meta_color_get_background_color (GtkStyleContext *context,
                                      GtkStateFlags    state,
                                      GdkRGBA         *color);

void meta_color_get_light_color      (GtkStyleContext *context,
                                      GtkStateFlags    state,
                                      GdkRGBA         *color);

void meta_color_get_dark_color       (GtkStyleContext *context,
                                      GtkStateFlags    state,
                                      GdkRGBA         *color);

G_END_DECLS

#endif
