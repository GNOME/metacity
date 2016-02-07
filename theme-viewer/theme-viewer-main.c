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

#include "theme-viewer-window.h"

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

  window = theme_viewer_window_new ();
  gtk_window_present (GTK_WINDOW (window));

  g_signal_connect (window, "destroy", G_CALLBACK (destroy_cb), NULL);

  gtk_main ();

  return 0;
}
