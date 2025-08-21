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

#include "config.h"
#include "meta-delete-dialog.h"

#include <gdk/gdkx.h>
#include <glib/gi18n.h>

struct _MetaDeleteDialog
{
  GtkDialog  parent;

  GtkLabel  *not_responding_label;
};

G_DEFINE_TYPE (MetaDeleteDialog, meta_delete_dialog, GTK_TYPE_DIALOG)

static void
meta_delete_dialog_class_init (MetaDeleteDialogClass *self_class)
{
  GtkWidgetClass *widget_class;
  const char *resource;

  widget_class = GTK_WIDGET_CLASS (self_class);

  resource = "/org/gnome/metacity/ui/delete-dialog.ui";
  gtk_widget_class_set_template_from_resource (widget_class, resource);

  gtk_widget_class_bind_template_child (widget_class,
                                        MetaDeleteDialog,
                                        not_responding_label);
}

static void
meta_delete_dialog_init (MetaDeleteDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
meta_delete_dialog_new (void)
{
  return g_object_new (META_TYPE_DELETE_DIALOG, NULL);
}

void
meta_delete_dialog_set_window_title (MetaDeleteDialog *self,
                                     const char       *window_title)
{
  char *tmp;
  char *markup;

  /* Translators: %s is a window title */
  tmp = g_strdup_printf (_("<tt>%s</tt> is not responding."), window_title);

  markup = g_strdup_printf ("<big><b>%s</b></big>", tmp);
  g_free (tmp);

  gtk_label_set_markup (self->not_responding_label, markup);
  g_free (markup);
}

void
meta_delete_dialog_set_transient_for (MetaDeleteDialog *self,
                                      int               transient_for)
{
  GdkDisplay *display;
  GdkWindow *window;

  if (!gtk_widget_get_realized (GTK_WIDGET (self)))
    gtk_widget_realize (GTK_WIDGET (self));

  display = gtk_widget_get_display (GTK_WIDGET (self));
  window = gtk_widget_get_window (GTK_WIDGET (self));

  gdk_x11_display_error_trap_push (display);

  XSetTransientForHint (gdk_x11_display_get_xdisplay (display),
                        gdk_x11_window_get_xid (window),
                        transient_for);

  gdk_x11_display_error_trap_pop_ignored (display);
}
