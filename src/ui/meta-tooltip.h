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

#ifndef META_TOOLTIP_H
#define META_TOOLTIP_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define META_TYPE_TOOLTIP meta_tooltip_get_type ()
G_DECLARE_FINAL_TYPE (MetaTooltip, meta_tooltip, META, TOOLTIP, GtkWindow)

GtkWidget *meta_tooltip_new              (void);

void       meta_tooltip_set_label_markup (MetaTooltip *tooltip,
                                          const gchar *markup);

void       meta_tooltip_set_label_text   (MetaTooltip *tooltip,
                                          const gchar *text);

G_END_DECLS

#endif
