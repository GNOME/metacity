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
#include "meta-session-dialog.h"

struct _MetaSessionDialog
{
  GtkDialog     parent;

  GtkListStore *lame_clients;

  unsigned int  timeout_id;
};

G_DEFINE_TYPE (MetaSessionDialog, meta_session_dialog, GTK_TYPE_DIALOG)

static gboolean
timeout_cb (void *user_data)
{
  MetaSessionDialog *self;

  self = META_SESSION_DIALOG (user_data);

  self->timeout_id = 0;

  gtk_widget_destroy (GTK_WIDGET (self));

  return G_SOURCE_REMOVE;
}

static void
meta_session_dialog_finalize (GObject *object)
{
  MetaSessionDialog *self;

  self = META_SESSION_DIALOG (object);

  if (self->timeout_id != 0)
    {
      g_source_remove (self->timeout_id);
      self->timeout_id = 0;
    }

  G_OBJECT_CLASS (meta_session_dialog_parent_class)->finalize (object);
}

static void
meta_session_dialog_class_init (MetaSessionDialogClass *self_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  const char *resource;

  object_class = G_OBJECT_CLASS (self_class);
  widget_class = GTK_WIDGET_CLASS (self_class);

  object_class->finalize = meta_session_dialog_finalize;

  resource = "/org/gnome/metacity/ui/session-dialog.ui";
  gtk_widget_class_set_template_from_resource (widget_class, resource);

  gtk_widget_class_bind_template_child (widget_class,
                                        MetaSessionDialog,
                                        lame_clients);
}

static void
meta_session_dialog_init (MetaSessionDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->timeout_id = g_timeout_add_seconds (240, timeout_cb, self);
}

GtkWidget *
meta_session_dialog_new (void)
{
  return g_object_new (META_TYPE_SESSION_DIALOG, NULL);
}

void
meta_session_dialog_set_lame_clients (MetaSessionDialog  *self,
                                      char              **lame_clients)
{
  unsigned int i;

  for (i = 0; i < g_strv_length (lame_clients); i += 2)
    {
      GtkTreeIter iter;

      gtk_list_store_append (self->lame_clients, &iter);
      gtk_list_store_set (self->lame_clients, &iter,
                          0, lame_clients[i],
                          1, lame_clients[i + 1],
                          -1);
    }
}
