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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdlib.h>

#include "meta-delete-dialog.h"
#include "meta-session-dialog.h"

static char *type = NULL;
static char *window_title = NULL;
static int transient_for = 0;
static char **lame_clients = NULL;

static GOptionEntry entries[] =
{
  {
    "type",
    0,
    G_OPTION_FLAG_NONE,
    G_OPTION_ARG_STRING,
    &type,
    NULL,
    NULL
  },
  {
    "window-title",
    0,
    G_OPTION_FLAG_NONE,
    G_OPTION_ARG_STRING,
    &window_title,
    NULL,
    NULL
  },
  {
    "transient-for",
    0,
    G_OPTION_FLAG_NONE,
    G_OPTION_ARG_INT,
    &transient_for,
    NULL,
    NULL
  },
  {
    G_OPTION_REMAINING,
    0,
    G_OPTION_FLAG_NONE,
    G_OPTION_ARG_STRING_ARRAY,
    &lame_clients,
    NULL,
    NULL
  },
  {
    NULL
  }
};

static gboolean
parse_arguments (gint    *argc,
                 gchar ***argv)
{
  GOptionContext *context;
  GOptionGroup *gtk_group;
  GError *error;

  context = g_option_context_new (NULL);
  gtk_group = gtk_get_option_group (FALSE);

  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_add_group (context, gtk_group);

  error = NULL;
  if (g_option_context_parse (context, argc, argv, &error) == FALSE)
    {
      g_warning ("Failed to parse command line arguments: %s", error->message);
      g_error_free (error);

      g_option_context_free (context);

      g_clear_pointer (&lame_clients, g_strfreev);
      g_clear_pointer (&window_title, g_free);
      g_clear_pointer (&type, g_free);

      return FALSE;
    }

  g_option_context_free (context);

  return TRUE;
}

static void
destroy_cb (GtkWidget *widget,
            void      *user_data)
{
  gtk_main_quit ();
}

static void
response_cb (GtkDialog *dialog,
             int        response_id,
             void      *user_data)
{
  if (response_id == GTK_RESPONSE_OK)
    *(int *) user_data = EXIT_SUCCESS;
}

int
main (int    argc,
      char **argv)
{
  GtkWidget *dialog;
  int exit_status;

  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
  textdomain(GETTEXT_PACKAGE);

  gtk_init (&argc, &argv);

  if (!parse_arguments (&argc, &argv))
    return EXIT_FAILURE;

  if (g_strcmp0 (type, "delete") == 0)
    {
      dialog = meta_delete_dialog_new ();

      g_assert (window_title != NULL);
      g_assert (transient_for != 0);

      meta_delete_dialog_set_window_title (META_DELETE_DIALOG (dialog),
                                           window_title);

      meta_delete_dialog_set_transient_for (META_DELETE_DIALOG (dialog),
                                            transient_for);
    }
  else if (g_strcmp0 (type, "session") == 0)
    {
      dialog = meta_session_dialog_new ();

      g_assert (lame_clients != NULL);
      g_assert (g_strv_length (lame_clients) % 2 == 0);

      meta_session_dialog_set_lame_clients (META_SESSION_DIALOG (dialog),
                                            lame_clients);
    }
  else
    {
      g_assert_not_reached ();
    }

  exit_status = EXIT_FAILURE;

  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (response_cb),
                    &exit_status);

  g_signal_connect (dialog,
                    "destroy",
                    G_CALLBACK (destroy_cb),
                    NULL);

  gtk_window_present (GTK_WINDOW (dialog));

  gtk_main ();

  g_clear_pointer (&lame_clients, g_strfreev);
  g_clear_pointer (&window_title, g_free);
  g_clear_pointer (&type, g_free);

  return exit_status;
}
