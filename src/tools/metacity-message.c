/* Metacity send-magic-messages app */

/*
 * Copyright (C) 2002 Havoc Pennington
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

#include <config.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <stdlib.h>
#include <string.h>

static void
send_restart (void)
{
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
  xev.xclient.window = gdk_x11_get_default_root_xwindow ();
  xev.xclient.message_type = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                          "_METACITY_RESTART_MESSAGE",
                                          False);
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = 0;
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = 0;

  XSendEvent (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
              gdk_x11_get_default_root_xwindow (),
              False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);

  XFlush (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));
  XSync (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), False);
}

static void
send_reload_theme (void)
{
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
  xev.xclient.window = gdk_x11_get_default_root_xwindow ();
  xev.xclient.message_type = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                          "_METACITY_RELOAD_THEME_MESSAGE",
                                          False);
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = 0;
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = 0;

  XSendEvent (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
              gdk_x11_get_default_root_xwindow (),
              False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);

  XFlush (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));
  XSync (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), False);
}

static void
send_set_keybindings (gboolean enabled)
{
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
  xev.xclient.window = gdk_x11_get_default_root_xwindow ();
  xev.xclient.message_type = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                          "_METACITY_SET_KEYBINDINGS_MESSAGE",
                                          False);
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = enabled;
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = 0;

  XSendEvent (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
              gdk_x11_get_default_root_xwindow (),
              False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);

  XFlush (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));
  XSync (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), False);
}

static void
send_set_mousemods (gboolean enabled)
{
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
  xev.xclient.window = gdk_x11_get_default_root_xwindow ();
  xev.xclient.message_type = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                          "_METACITY_SET_MOUSEMODS_MESSAGE",
                                          False);
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = enabled;
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = 0;

  XSendEvent (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
              gdk_x11_get_default_root_xwindow (),
              False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);

  XFlush (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));
  XSync (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), False);
}

static void
send_toggle_verbose (void)
{
  XEvent xev;

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
  xev.xclient.window = gdk_x11_get_default_root_xwindow ();
  xev.xclient.message_type = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                          "_METACITY_TOGGLE_VERBOSE",
                                          False);
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = 0;
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = 0;

  XSendEvent (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
              gdk_x11_get_default_root_xwindow (),
              False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);

  XFlush (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));
  XSync (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), False);
}

static void
usage (void)
{
  g_printerr (_("Usage: %s\n"),
              "metacity-message (restart|reload-theme|enable-keybindings|disable-keybindings|enable-mouse-button-modifiers|disable-mouse-button-modifiers|toggle-verbose)");
  exit (1);
}

int
main (int argc, char **argv)
{
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  gtk_init (&argc, &argv);

  if (argc < 2)
    usage ();

  if (strcmp (argv[1], "restart") == 0)
    send_restart ();
  else if (strcmp (argv[1], "reload-theme") == 0)
    send_reload_theme ();
  else if (strcmp (argv[1], "enable-keybindings") == 0)
    send_set_keybindings (TRUE);
  else if (strcmp (argv[1], "disable-keybindings") == 0)
    send_set_keybindings (FALSE);
  else if (strcmp (argv[1], "enable-mouse-button-modifiers") == 0)
    send_set_mousemods (TRUE);
  else if (strcmp (argv[1], "disable-mouse-button-modifiers") == 0)
    send_set_mousemods (FALSE);
  else if (strcmp (argv[1], "toggle-verbose") == 0)
    send_toggle_verbose ();
  else
    usage ();

  return 0;
}

