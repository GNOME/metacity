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

#include "config.h"

#include <glib/gi18n.h>
#include <stdlib.h>

#include "theme-viewer-window.h"

static gchar *theme_type = NULL;
static gchar *theme_name = NULL;

static GOptionEntry entries[] =
{
  {
    "theme-type", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &theme_type,
    N_("Theme type to use (\"gtk\" or \"metacity\")"), N_("TYPE")
  },
  {
    "theme-name", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &theme_name,
    N_("Theme name to use"), N_("NAME")
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

      g_clear_pointer (&theme_type, g_free);
      g_clear_pointer (&theme_name, g_free);

      return FALSE;
    }

  g_option_context_free (context);

  return TRUE;
}

static void
destroy_cb (GtkWidget *widget)
{
  gtk_main_quit ();
}

int
main (int argc, char **argv)
{
  GtkWidget *window;

  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
  textdomain(GETTEXT_PACKAGE);

  gtk_init (&argc, &argv);

  if (!parse_arguments (&argc, &argv))
    return EXIT_FAILURE;

  window = theme_viewer_window_new ();

  if (theme_type != NULL)
    {
      MetaThemeType type;

      type = META_THEME_TYPE_GTK;
      if (g_strcmp0 (theme_type, "metacity") == 0)
        type = META_THEME_TYPE_METACITY;

      theme_viewer_window_set_theme_type (THEME_VIEWER_WINDOW (window), type);
    }

  if (theme_name != NULL)
    {
      theme_viewer_window_set_theme_name (THEME_VIEWER_WINDOW (window),
                                          theme_name);
    }

  g_signal_connect (window, "destroy", G_CALLBACK (destroy_cb), NULL);
  gtk_window_present (GTK_WINDOW (window));

  gtk_main ();

  g_clear_pointer (&theme_type, g_free);
  g_clear_pointer (&theme_name, g_free);

  return 0;
}
