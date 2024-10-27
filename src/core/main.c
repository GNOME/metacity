/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity main() */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2006 Elijah Newren
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

/**
 * \file
 * Program startup.
 * Functions which parse the command-line arguments, create the display,
 * kick everything off and then close down Metacity when it's time to go.
 */

/**
 * \mainpage
 * Metacity - a boring window manager for the adult in you
 *
 * Many window managers are like Marshmallow Froot Loops; Metacity
 * is like Cheerios.
 *
 * The best way to get a handle on how the whole system fits together
 * is discussed in doc/code-overview.txt; if you're looking for functions
 * to investigate, read main(), meta_display_open(), and event_callback().
 */

#define _GNU_SOURCE
#define _XOPEN_SOURCE /* for putenv() and some signal-related functions */

#include <config.h>
#include "main.h"
#include "util.h"
#include "display-private.h"
#include "errors.h"
#include "meta-enum-types.h"
#include "ui.h"
#include "session.h"
#include "prefs.h"

#include <glib-object.h>
#include <glib-unix.h>
#include <glib/gprintf.h>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <time.h>
#include <unistd.h>

/**
 * Handle on the main loop, so that we have an easy way of shutting Metacity
 * down.
 */
static GMainLoop *meta_main_loop = NULL;

/**
 * If set, Metacity will spawn an identical copy of itself immediately
 * before quitting.
 */
static gboolean meta_restart_after_quit = FALSE;

static gboolean meta_shutdown_session = TRUE;

/**
 * Prints the version notice. This is shown when Metacity is called
 * with the --version switch.
 */
static void
version (void)
{
  const int latest_year = 2009;
  char yearbuffer[256];
  GDate date;

  /* this is all so the string to translate stays constant.
   * see how much we love the translators.
   */
  g_date_set_dmy (&date, 1, G_DATE_JANUARY, latest_year);
  if (g_date_strftime (yearbuffer, sizeof (yearbuffer), "%Y", &date)==0)
    /* didn't work?  fall back to decimal representation */
    g_sprintf (yearbuffer, "%d", latest_year);

  g_print (_("metacity %s\n"
             "Copyright (C) 2001-%s Havoc Pennington, Red Hat, Inc., and others\n"
             "This is free software; see the source for copying conditions.\n"
             "There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"),
           VERSION, yearbuffer);
  exit (0);
}

/**
 * Prints a list of which configure script options were used to
 * build this copy of Metacity. This is actually always called
 * on startup, but it's all no-op unless we're in verbose mode.
 */
static void
meta_print_compilation_info (void)
{
#ifdef HAVE_XINERAMA
  meta_topic (META_DEBUG_XINERAMA, "Compiled with Xinerama extension\n");
#else
  meta_topic (META_DEBUG_XINERAMA, "Compiled without Xinerama extension\n");
#endif
#ifdef HAVE_RANDR
  meta_verbose ("Compiled with randr extension\n");
#else
  meta_verbose ("Compiled without randr extension\n");
#endif
}

/**
 * Prints the version number, the current timestamp (not the
 * build date), the locale, the character encoding, and a list
 * of configure script options that were used to build this
 * copy of Metacity. This is actually always called
 * on startup, but it's all no-op unless we're in verbose mode.
 */
static void
meta_print_self_identity (void)
{
  char buf[256];
  GDate d;
  const char *charset;

  /* Version and current date. */
  g_date_clear (&d, 1);
  g_date_set_time_t (&d, time (NULL));
  g_date_strftime (buf, sizeof (buf), "%x", &d);
  meta_verbose ("Metacity version %s running on %s\n",
    VERSION, buf);

  /* Locale and encoding. */
  g_get_charset (&charset);
  meta_verbose ("Running in locale \"%s\" with encoding \"%s\"\n",
    setlocale (LC_ALL, NULL), charset);

  /* Compilation settings. */
  meta_print_compilation_info ();
}

/**
 * The set of possible options that can be set on Metacity's
 * command line. This type exists so that meta_parse_options() can
 * write to an instance of it.
 */
typedef struct
{
  gchar *save_file;
  gchar *display_name;
  gchar *client_id;
  gboolean replace_wm;
  gboolean disable_sm;
  gboolean print_version;
  gboolean sync;
  gboolean composite;
  gboolean no_composite;
  gboolean no_force_fullscreen;

  MetaCompositorType compositor;
  gboolean compositor_set;
} MetaArguments;

static gboolean
option_composite_cb (const char  *option_name,
                     const char  *value,
                     gpointer     data,
                     GError     **error)
{
  MetaArguments *args;

  args = data;
  args->composite = TRUE;

  g_warning (_("Option “%s” is deprecated, use the “--compositor” instead."),
             option_name);

  return TRUE;
}

static gboolean
option_no_composite_cb (const char  *option_name,
                        const char  *value,
                        gpointer     data,
                        GError     **error)
{
  MetaArguments *args;

  args = data;
  args->no_composite = TRUE;

  g_warning (_("Option “%s” is deprecated, use the “--compositor” instead."),
             option_name);

  return TRUE;
}

static gboolean
option_compositior_cb (const char  *option_name,
                       const char  *value,
                       gpointer     data,
                       GError     **error)
{
  MetaArguments *args;
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  args = data;

  enum_class = g_type_class_ref (META_TYPE_COMPOSITOR_TYPE);
  enum_value = g_enum_get_value_by_nick (enum_class, value);

  if (enum_value == NULL)
    {
      g_type_class_unref (enum_class);

      g_set_error (error,
                   G_OPTION_ERROR,
                   G_OPTION_ERROR_FAILED,
                   _("“%s” is not a valid compositor"),
                   value);

      return FALSE;
    }

  args->compositor = enum_value->value;
  args->compositor_set = TRUE;

  g_type_class_unref (enum_class);

  return TRUE;
}

/**
 * Parses argc and argv and returns the
 * arguments that Metacity understands in meta_args.
 *
 * The strange call signature has to be written like it is so
 * that g_option_context_parse() gets a chance to modify argc and
 * argv.
 *
 * \param argc  Pointer to the number of arguments Metacity was given
 * \param argv  Pointer to the array of arguments Metacity was given
 * \param meta_args  The result of parsing the arguments.
 **/
static void
meta_parse_options (int *argc, char ***argv,
                    MetaArguments *meta_args)
{
  MetaArguments my_args = { 0 };
  GOptionEntry options[] = {
    {
      "sm-disable", 0, 0, G_OPTION_ARG_NONE,
      &my_args.disable_sm,
      N_("Disable connection to session manager"),
      NULL
    },
    {
      "replace", 0, 0, G_OPTION_ARG_NONE,
      &my_args.replace_wm,
      N_("Replace the running window manager with Metacity"),
      NULL
    },
    {
      "sm-client-id", 0, 0, G_OPTION_ARG_STRING,
      &my_args.client_id,
      N_("Specify session management ID"),
      "ID"
    },
    {
      "display", 'd', 0, G_OPTION_ARG_STRING,
      &my_args.display_name, N_("X Display to use"),
      "DISPLAY"
    },
    {
      "sm-save-file", 0, 0, G_OPTION_ARG_FILENAME,
      &my_args.save_file,
      N_("Initialize session from savefile"),
      "FILE"
    },
    {
      "version", 0, 0, G_OPTION_ARG_NONE,
      &my_args.print_version,
      N_("Print version"),
      NULL
    },
    {
      "sync", 0, 0, G_OPTION_ARG_NONE,
      &my_args.sync,
      N_("Make X calls synchronous"),
      NULL
    },
    {
      "composite",
      'c',
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      option_composite_cb,
      N_("Turn compositing on"),
      NULL
    },
    {
      "no-composite",
      0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      option_no_composite_cb,
      N_("Turn compositing off"),
      NULL
    },
    {
      "compositor",
      0,
      G_OPTION_FLAG_NONE,
      G_OPTION_ARG_CALLBACK,
      option_compositior_cb,
      N_("Compositor to use"),
      "COMPOSITOR"
    },
    {
      "no-force-fullscreen", 0, G_OPTION_ARG_NONE, G_OPTION_ARG_NONE,
      &my_args.no_force_fullscreen,
      N_("Don't make fullscreen windows that are maximized and have no decorations"),
      NULL
    },
    {NULL}
  };
  GOptionContext *ctx;
  GOptionGroup *group;
  GError *error = NULL;

  ctx = g_option_context_new (NULL);
  group = g_option_group_new (NULL, NULL, NULL, &my_args, NULL);
  g_option_context_set_main_group (ctx, group);
  g_option_context_add_main_entries (ctx, options, "metacity");
  if (!g_option_context_parse (ctx, argc, argv, &error))
    {
      g_print ("metacity: %s\n", error->message);
      exit(1);
    }
  g_option_context_free (ctx);
  /* Return the parsed options through the meta_args param. */
  *meta_args = my_args;
}

/**
 * Selects which display Metacity should use. It first tries to use
 * display_name as the display. If display_name is NULL then
 * try to use the environment variable METACITY_DISPLAY. If that
 * also is NULL, use the default - :0.0
 */
static void
meta_select_display (gchar *display_name)
{
  const gchar *env_var;

  if (display_name)
    env_var = (const gchar *) display_name;
  else
    env_var = g_getenv ("METACITY_DISPLAY");

  if (env_var)
    {
      if (!g_setenv ("DISPLAY", env_var, TRUE))
        g_warning ("Couldn't set DISPLAY");
    }
}

static void
meta_finalize (void)
{
  MetaDisplay *display = meta_get_display();

  if (display)
    {
      guint32 timestamp;

      timestamp = meta_display_get_current_time_roundtrip (display);

      meta_display_close (display, timestamp);
    }

  if (meta_shutdown_session)
    meta_session_shutdown ();
}

static gboolean
sigterm_cb (gpointer user_data)
{
  meta_quit ();

  return G_SOURCE_REMOVE;
}

static gboolean
sigint_cb (gpointer user_data)
{
  meta_shutdown_session = FALSE;
  meta_quit ();

  return G_SOURCE_REMOVE;
}

/**
 * This is where the story begins. It parses commandline options and
 * environment variables, sets up the screen, hands control off to
 * GTK, and cleans up afterwards.
 *
 * \param argc Number of arguments (as usual)
 * \param argv Array of arguments (as usual)
 *
 * \bug It's a bit long. It would be good to split it out into separate
 * functions.
 */
int
main (int argc, char **argv)
{
  struct sigaction act;
  sigset_t empty_mask;
  MetaArguments meta_args;

  if (setlocale (LC_ALL, "") == NULL)
    g_warning ("Locale not understood by C library, internationalization "
               "will not work");

  sigemptyset (&empty_mask);
  act.sa_handler = SIG_IGN;
  act.sa_mask    = empty_mask;
  act.sa_flags   = 0;
  if (sigaction (SIGPIPE,  &act, NULL) < 0)
    g_printerr ("Failed to register SIGPIPE handler: %s\n",
                g_strerror (errno));
#ifdef SIGXFSZ
  if (sigaction (SIGXFSZ,  &act, NULL) < 0)
    g_printerr ("Failed to register SIGXFSZ handler: %s\n",
                g_strerror (errno));
#endif

  g_unix_signal_add (SIGTERM, sigterm_cb, NULL);
  g_unix_signal_add (SIGINT, sigint_cb, NULL);

  meta_init_debug ();

  if (g_getenv ("METACITY_DEBUG"))
    meta_set_debugging (TRUE);

  if (g_get_home_dir ())
    {
      if (chdir (g_get_home_dir ()) < 0)
        g_warning ("Could not change to home directory %s.", g_get_home_dir ());
    }

  meta_print_self_identity ();

  bindtextdomain (GETTEXT_PACKAGE, METACITY_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  /* Parse command line arguments.*/
  meta_parse_options (&argc, &argv, &meta_args);

  meta_set_syncing (meta_args.sync || (g_getenv ("METACITY_SYNC") != NULL));

  if (meta_args.print_version)
    version ();

  meta_select_display (meta_args.display_name);

  if (meta_args.replace_wm)
    meta_set_replace_current_wm (TRUE);

  if (meta_args.save_file && meta_args.client_id)
    {
      g_critical ("Can't specify both SM save file and SM client id");
      exit (EXIT_FAILURE);
    }

  meta_main_loop = g_main_loop_new (NULL, FALSE);

  meta_ui_init (&argc, &argv);

  /* Load prefs */
  meta_prefs_init ();

  /* Connect to SM as late as possible - but before managing display,
   * or we might try to manage a window before we have the session
   * info
   */
  if (!meta_args.disable_sm)
    {
      if (meta_args.client_id == NULL)
        {
          const gchar *desktop_autostart_id;

          desktop_autostart_id = g_getenv ("DESKTOP_AUTOSTART_ID");

          if (desktop_autostart_id != NULL)
            meta_args.client_id = g_strdup (desktop_autostart_id);
        }

      /* Unset DESKTOP_AUTOSTART_ID in order to avoid child processes to
       * use the same client id. */
      g_unsetenv ("DESKTOP_AUTOSTART_ID");

      meta_session_init (meta_args.client_id, meta_args.save_file);
    }
  /* Free memory possibly allocated by the argument parsing which are
   * no longer needed.
   */
  g_free (meta_args.save_file);
  g_free (meta_args.display_name);
  g_free (meta_args.client_id);

  if (meta_args.compositor_set)
    {
      meta_prefs_set_compositor (meta_args.compositor);
    }
  else if (meta_args.composite || meta_args.no_composite)
    {
      if (meta_args.composite)
        meta_prefs_set_compositor (META_COMPOSITOR_TYPE_XRENDER);
      else
        meta_prefs_set_compositor (META_COMPOSITOR_TYPE_NONE);
    }

  if (meta_args.no_force_fullscreen)
    meta_prefs_set_force_fullscreen (FALSE);

  if (!meta_display_open ())
    {
      exit (EXIT_FAILURE);
    }

  g_main_loop_run (meta_main_loop);

  meta_finalize ();

  if (meta_restart_after_quit)
    {
      GError *err;

      err = NULL;
      if (!g_spawn_async (NULL,
                          argv,
                          NULL,
                          G_SPAWN_SEARCH_PATH,
                          NULL,
                          NULL,
                          NULL,
                          &err))
        {
          g_critical ("Failed to restart: %s", err->message);
          g_error_free (err);

          return EXIT_FAILURE;
        }
    }

  return EXIT_SUCCESS;
}

/**
 * Stops Metacity. This tells the event loop to stop processing; it is rather
 * dangerous to use this rather than meta_restart() because this will leave
 * the user with no window manager. We generally do this only if, for example,
 * the session manager asks us to; we assume the session manager knows what
 * it's talking about.
 *
 * \param code The success or failure code to return to the calling process.
 */
void
meta_quit (void)
{
  if (g_main_loop_is_running (meta_main_loop))
    g_main_loop_quit (meta_main_loop);
}

/**
 * Restarts Metacity. In practice, this tells the event loop to stop
 * processing, having first set the meta_restart_after_quit flag which
 * tells Metacity to spawn an identical copy of itself before quitting.
 * This happens on receipt of a _METACITY_RESTART_MESSAGE client event.
 */
void
meta_restart (void)
{
  meta_restart_after_quit = TRUE;
  meta_quit ();
}
