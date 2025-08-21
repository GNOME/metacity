/*
 * Copyright (C) 2023 Alberts Muktupāvels
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

#ifndef META_DELETE_DIALOG_H
#define META_DELETE_DIALOG_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define META_TYPE_DELETE_DIALOG (meta_delete_dialog_get_type ())
G_DECLARE_FINAL_TYPE (MetaDeleteDialog, meta_delete_dialog,
                      META, DELETE_DIALOG, GtkDialog)

GtkWidget *
meta_delete_dialog_new (void);

void
meta_delete_dialog_set_window_title (MetaDeleteDialog *self,
                                     const char       *window_title);

void
meta_delete_dialog_set_transient_for (MetaDeleteDialog *self,
                                      int               transient_for);

G_END_DECLS

#endif
