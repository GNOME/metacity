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

#ifndef META_COLOR_SPEC_H
#define META_COLOR_SPEC_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum _MetaColorSpecType MetaColorSpecType;
typedef enum _MetaGtkColorComponent MetaGtkColorComponent;
typedef struct _MetaColorSpec MetaColorSpec;

MetaColorSpec *meta_color_spec_new             (MetaColorSpecType       type);

MetaColorSpec *meta_color_spec_new_from_string (const gchar            *str,
                                                GError                **error);

MetaColorSpec *meta_color_spec_new_gtk         (MetaGtkColorComponent   component,
                                                GtkStateFlags           state);

void           meta_color_spec_free            (MetaColorSpec          *spec);

void           meta_color_spec_render          (MetaColorSpec          *spec,
                                                GtkStyleContext        *context,
                                                GdkRGBA                *color);

GtkStateFlags  meta_gtk_state_from_string      (const gchar            *str);

G_END_DECLS

#endif
