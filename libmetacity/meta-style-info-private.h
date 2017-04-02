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

#ifndef META_STYLE_INFO_PRIVATE_H
#define META_STYLE_INFO_PRIVATE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum
{
  META_STYLE_ELEMENT_WINDOW,
  META_STYLE_ELEMENT_DECORATION,
  META_STYLE_ELEMENT_TITLEBAR,
  META_STYLE_ELEMENT_TITLE,
  META_STYLE_ELEMENT_BUTTON,
  META_STYLE_ELEMENT_IMAGE,
  META_STYLE_ELEMENT_LAST
} MetaStyleElement;

G_GNUC_INTERNAL
#define META_TYPE_STYLE_INFO meta_style_info_get_type ()
G_DECLARE_FINAL_TYPE (MetaStyleInfo, meta_style_info, META, STYLE_INFO, GObject)

G_GNUC_INTERNAL
MetaStyleInfo   *meta_style_info_new            (const gchar      *gtk_theme_name,
                                                 const gchar      *gtk_theme_variant,
                                                 gboolean          composited,
                                                 gint              scale);

G_GNUC_INTERNAL
GtkStyleContext *meta_style_info_get_style      (MetaStyleInfo    *style_info,
                                                 MetaStyleElement  element);

G_GNUC_INTERNAL
void             meta_style_info_set_composited (MetaStyleInfo    *style_info,
                                                 gboolean          composited);

G_GNUC_INTERNAL
void             meta_style_info_set_scale      (MetaStyleInfo    *style_info,
                                                 gint              scale);

G_GNUC_INTERNAL
void             meta_style_info_set_flags      (MetaStyleInfo    *style_info,
                                                 MetaFrameFlags    flags);

G_END_DECLS

#endif
