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

#ifndef META_CSS_PROVIDER_PRIVATE_H
#define META_CSS_PROVIDER_PRIVATE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL
#define META_TYPE_CSS_PROVIDER meta_css_provider_get_type ()
G_DECLARE_FINAL_TYPE (MetaCssProvider, meta_css_provider,
                      META, CSS_PROVIDER, GtkCssProvider)

G_GNUC_INTERNAL
GtkCssProvider *meta_css_provider_new (const gchar *name,
                                       const gchar *variant);

G_END_DECLS

#endif
