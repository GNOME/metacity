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

#ifndef META_STYLE_INFO_H
#define META_STYLE_INFO_H

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

typedef struct
{
  int refcount;

  GtkStyleContext *styles[META_STYLE_ELEMENT_LAST];
} MetaStyleInfo;

MetaStyleInfo *meta_style_info_new          (const gchar    *theme_name,
                                             const gchar    *variant,
                                             gboolean        composited);

MetaStyleInfo *meta_style_info_ref          (MetaStyleInfo  *style_info);

void           meta_style_info_unref        (MetaStyleInfo  *style_info);

void           meta_style_info_set_flags    (MetaStyleInfo  *style_info,
                                             MetaFrameFlags  flags);

G_END_DECLS

#endif
