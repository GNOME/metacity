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

#ifndef META_THEME_GTK_PRIVATE_H
#define META_THEME_GTK_PRIVATE_H

#include "meta-theme-impl-private.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL
#define META_TYPE_THEME_GTK meta_theme_gtk_get_type ()
G_DECLARE_FINAL_TYPE (MetaThemeGtk, meta_theme_gtk,
                      META, THEME_GTK, MetaThemeImpl)

G_END_DECLS

#endif
