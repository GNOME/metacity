/*
 * Copyright (C) 2001 Havoc Pennington
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

#ifndef META_THEME_PRIVATE_H
#define META_THEME_PRIVATE_H

#include "theme.h"

G_BEGIN_DECLS

PangoFontDescription  *meta_style_info_create_font_desc        (MetaTheme                   *theme,
                                                                MetaStyleInfo               *style_info);

int                    meta_pango_font_desc_get_text_height    (const PangoFontDescription  *font_desc,
                                                                PangoContext                *context);

G_END_DECLS

#endif
