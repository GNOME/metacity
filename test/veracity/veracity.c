/* 
 * veracity - test metacity
 *
 * Copyright (C) 2008 Thomas Thurman
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */
#include <stdio.h>
#include <libguile.h>
#include <guile/gh.h>
#include <sys/types.h>
#include <signal.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Shell.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

#include <gconf/gconf.h>

int x_server = 0; /* PID of the tame X server */
int window_manager = 0; /* PID of the window manager */
int dbus_daemon = 0;
int gconf_daemon = 0;

Display *current_display = NULL;

gboolean verbose = FALSE;

SCM
make_window()
{
  Window newbie;
 
  newbie = XCreateWindow(current_display,
			 DefaultRootWindow(current_display),
			 0, 0, 300, 300, 0,
			 CopyFromParent,
			 InputOutput,
			 CopyFromParent,
			 0, 0);
  
  XMapWindow (current_display, newbie);

  XSync (current_display, True);

  if (verbose)
    printf ("Created window %x\n", (int) newbie);

  return scm_int2num(newbie);
}

SCM
parent(SCM raw_window)
{
  Window window = gh_scm2int (raw_window);
  Display *display;
  Window w;
  Window root_return;
  Window parent_return;
  Window *children_return;
  unsigned int nchildren_return;

  XQueryTree(current_display, window,
	     &root_return, &parent_return,
	     &children_return, &nchildren_return);

  XFree (children_return);

  if (verbose)
    printf ("Parent of %x is %x (root is %x)\n", (int) window, (int) parent_return, (int) root_return);

  if (root_return == parent_return)
    return SCM_BOOL(0);
  else
    return scm_int2num (parent_return);
}

static SCM
start_wm ()
{
  /* FIXME: Which WM ought to be configurable */

  if (!(window_manager = fork()))
    {
      execlp ("metacity", "metacity", NULL);
    }

  /* have to wait a moment here while metacity starts up */
  sleep(2);

}

static SCM
key_event ()
{
  /*
   * Based around some public domain code by Adam Pierce
   * http://www.doctort.org/adam/nerd-notes/x11-fake-keypress-event.html
   */
}

static void
gconf_test (char *test)
{
  GConfValue *val = gconf_value_new_from_string (GCONF_VALUE_STRING,
						 test, NULL);

  if (verbose)
    printf ("Setting the theme to %s\n", test);
  gconf_client_set (gconf_client_get_default (),
		    "/apps/metacity/general/theme",
		    val,
		    NULL);

  gconf_value_free (val);
}

static void
start_dbus_daemon ()
{
  FILE *dbus_input;
  int pipes[2];
  char text[10240];
  char *cursor = text;
  ssize_t size;

  pipe (pipes);

  if (!(dbus_daemon = fork()))
    {
      close(1);
      dup2(pipes[1], 1);
      execlp ("dbus-launch", "dbus-launch", NULL);
    }
  sleep(1);
    
  size = read (pipes[0], &text, sizeof (text)-1);
  text[size] = 0;

  /* okay, now we have FOO=bar\nBAZ=wombat\n... in text */

  while (*cursor) {
    char *newline = index (cursor, '\n');

    *newline = 0;
    putenv (cursor);
    
    cursor = newline+1;
  }

}

static void
start_gconf_daemon ()
{
  if (!(gconf_daemon = fork()))
    {
      execlp ("gconftool-2", "gconftool-2", "--spawn", NULL);
    }
  sleep(1);

  gconf_test ("Atlanta");
}

void
start_x_server ()
{
  /* FIXME: Pick a sane number automagically */
  char *displaynumber = ":23";

  if (!(x_server = fork()))
    {
      fclose(stderr); /* hide all the useless spew */

      /*
      execlp ("Xnest", "Xnest", displaynumber, "-ac", NULL);
      */
      /*
      execlp ("Xvfb", "Xvfb", displaynumber, "-ac", "+extension", "RANDR", NULL);
      */
      execlp ("Xephyr", "Xephyr", displaynumber, "-nolisten", "tcp", "-host-cursor",
	      "-screen", "800x480x16", "-dpi", "96", "-ac",
	      "+extension", "Composite",
	      "+extension", "XFIXES",
	      "+extension", "DAMAGE",
	      "+extension", "RENDER",
	      "+extension", "GLX",
	      NULL);
    }
  sleep (1);

  current_display = XOpenDisplay (displaynumber);
  setenv ("DISPLAY", displaynumber, 1);
}

static void
start_guile ()
{
  scm_init_guile ();
  
  scm_c_define_gsubr("make-window", 0, 0, 0, make_window);
  scm_c_define_gsubr("parent", 1, 0, 0, parent);
  scm_c_define_gsubr("start-wm", 0, 0, 0, start_wm);
  scm_c_define_gsubr("key-event", 2, 0, 0, key_event);
  scm_c_define_gsubr("gconf-set!", 2, 0, 0, gconf_set);
}

static void
start ()
{
  /* Normalise error messages */
  putenv ("LANG=C");

  start_dbus_daemon ();
  start_gconf_daemon ();
  start_x_server ();
  start_guile ();
}

static gboolean
run_test (char *name)
{
  printf("%20s: ", name);
  fflush(stdout);

  // Load the scheme function definitions
  scm_c_primitive_load (name);

  SCM result = scm_c_eval_string("(test)");

  if (scm_is_string (result))
    {
      int length;
      char *raw_text = gh_scm2newstr (result, &length);
      printf ("FAIL (%s)\n", raw_text);
      free (raw_text);
      return FALSE;
    }
  else
    {
      printf ("pass\n");
      return TRUE;
    }
}

static void
finish ()
{
  if (x_server) kill(x_server, SIGKILL);
  if (window_manager) kill(window_manager, SIGKILL);
  if (dbus_daemon) kill(dbus_daemon, SIGKILL);
  if (gconf_daemon) kill(gconf_daemon, SIGKILL);

  XCloseDisplay (current_display);
}

int
main (int argc, char *argv[])
{
  start ();

  run_test ("001-reparent.scm");

  finish ();

  /* I'm making a note here: HUGE SUCCESS. */
  exit(EXIT_SUCCESS);
}

