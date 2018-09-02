/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity utilities */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2005 Elijah Newren
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

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L /* for fdopen() */

#include <config.h>
#include "util.h"
#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <X11/Xlib.h>   /* must explicitly be included for Solaris; #326746 */
#include <X11/Xutil.h>  /* Just for the definition of the various gravities */

static const GDebugKey debug_keys[] = {
  { "focus", META_DEBUG_FOCUS },
  { "workarea", META_DEBUG_WORKAREA },
  { "stack", META_DEBUG_STACK },
  { "sm", META_DEBUG_SM },
  { "events", META_DEBUG_EVENTS },
  { "window-state", META_DEBUG_WINDOW_STATE },
  { "window-ops", META_DEBUG_WINDOW_OPS },
  { "geometry", META_DEBUG_GEOMETRY },
  { "placement", META_DEBUG_PLACEMENT },
  { "ping", META_DEBUG_PING },
  { "xinerama", META_DEBUG_XINERAMA },
  { "keybindings", META_DEBUG_KEYBINDINGS },
  { "sync", META_DEBUG_SYNC },
  { "startup", META_DEBUG_STARTUP },
  { "prefs", META_DEBUG_PREFS },
  { "groups", META_DEBUG_GROUPS },
  { "resizing", META_DEBUG_RESIZING },
  { "shapes", META_DEBUG_SHAPES },
  { "edge-resistance", META_DEBUG_EDGE_RESISTANCE },
  { "verbose", META_DEBUG_VERBOSE },
  { "vulkan", META_DEBUG_VULKAN }
};

static guint debug_flags = 0;
static gboolean is_debugging = FALSE;
static gboolean replace_current = FALSE;
static int no_prefix = 0;
static FILE* logfile = NULL;

static void
ensure_logfile (void)
{
  if (logfile == NULL && g_getenv ("METACITY_USE_LOGFILE"))
    {
      char *filename = NULL;
      char *tmpl;
      int fd;
      GError *err;

      tmpl = g_strdup_printf ("metacity-%d-debug-log-XXXXXX",
                              (int) getpid ());

      err = NULL;
      fd = g_file_open_tmp (tmpl,
                            &filename,
                            &err);

      g_free (tmpl);

      if (err != NULL)
        {
          g_warning ("Failed to open debug log: %s", err->message);
          g_error_free (err);
          return;
        }

      logfile = fdopen (fd, "w");

      if (logfile == NULL)
        {
          g_warning ("Failed to fdopen() log file %s: %s",
                     filename, strerror (errno));
          close (fd);
        }
      else
        {
          g_printerr (_("Opened log file %s\n"), filename);
        }

      g_free (filename);
    }
}

void
meta_init_debug (void)
{
  debug_flags = g_parse_debug_string (g_getenv ("META_DEBUG"), debug_keys,
                                      G_N_ELEMENTS (debug_keys));

  if (debug_flags != 0)
    ensure_logfile ();
}

void
meta_toggle_debug (void)
{
  if (debug_flags == 0)
    {
      debug_flags = g_parse_debug_string ("all", debug_keys,
                                          G_N_ELEMENTS (debug_keys));

      ensure_logfile ();
    }
  else
    {
      debug_flags = 0;
    }
}

gboolean
meta_check_debug_flags (MetaDebugFlags flags)
{
  return (debug_flags & flags) != 0;
}

gboolean
meta_is_debugging (void)
{
  return is_debugging;
}

void
meta_set_debugging (gboolean setting)
{
  if (setting)
    ensure_logfile ();

  is_debugging = setting;
}

gboolean
meta_get_replace_current_wm (void)
{
  return replace_current;
}

void
meta_set_replace_current_wm (gboolean setting)
{
  replace_current = setting;
}

char *
meta_g_utf8_strndup (const gchar *src,
                     gsize        n)
{
  const gchar *s = src;
  while (n && *s)
    {
      s = g_utf8_next_char (s);
      n--;
    }

  return g_strndup (src, s - src);
}

static int
utf8_fputs (const char *str,
            FILE       *f)
{
  char *l;
  int retval;

  l = g_locale_from_utf8 (str, -1, NULL, NULL, NULL);

  if (l == NULL)
    retval = fputs (str, f); /* just print it anyway, better than nothing */
  else
    retval = fputs (l, f);

  g_free (l);

  return retval;
}

void
meta_verbose (const char *format, ...)
{
  va_list args;
  gchar *str;
  FILE *out;

  g_return_if_fail (format != NULL);

  if ((debug_flags & META_DEBUG_VERBOSE) == 0)
    return;

  va_start (args, format);
  str = g_strdup_vprintf (format, args);
  va_end (args);

  out = logfile ? logfile : stderr;

  if (no_prefix == 0)
    utf8_fputs ("Window manager: ", out);
  utf8_fputs (str, out);

  fflush (out);

  g_free (str);
}

static const char*
topic_name (MetaDebugFlags topic)
{
  switch (topic)
    {
    case META_DEBUG_FOCUS:
      return "FOCUS";
    case META_DEBUG_WORKAREA:
      return "WORKAREA";
    case META_DEBUG_STACK:
      return "STACK";
    case META_DEBUG_SM:
      return "SM";
    case META_DEBUG_EVENTS:
      return "EVENTS";
    case META_DEBUG_WINDOW_STATE:
      return "WINDOW_STATE";
    case META_DEBUG_WINDOW_OPS:
      return "WINDOW_OPS";
    case META_DEBUG_PLACEMENT:
      return "PLACEMENT";
    case META_DEBUG_GEOMETRY:
      return "GEOMETRY";
    case META_DEBUG_PING:
      return "PING";
    case META_DEBUG_XINERAMA:
      return "XINERAMA";
    case META_DEBUG_KEYBINDINGS:
      return "KEYBINDINGS";
    case META_DEBUG_SYNC:
      return "SYNC";
    case META_DEBUG_STARTUP:
      return "STARTUP";
    case META_DEBUG_PREFS:
      return "PREFS";
    case META_DEBUG_GROUPS:
      return "GROUPS";
    case META_DEBUG_RESIZING:
      return "RESIZING";
    case META_DEBUG_SHAPES:
      return "SHAPES";
    case META_DEBUG_EDGE_RESISTANCE:
      return "EDGE_RESISTANCE";
    case META_DEBUG_VERBOSE:
      return "VERBOSE";
    case META_DEBUG_VULKAN:
      return "VULKAN";
    default:
      break;
    }

  return "WM";
}

static int sync_count = 0;

void
meta_topic (MetaDebugFlags  topic,
            const char     *format,
            ...)
{
  va_list args;
  gchar *str;
  FILE *out;

  g_return_if_fail (format != NULL);

  if ((debug_flags & topic) == 0)
    return;

  va_start (args, format);
  str = g_strdup_vprintf (format, args);
  va_end (args);

  out = logfile ? logfile : stderr;

  if (no_prefix == 0)
    fprintf (out, "%s: ", topic_name (topic));

  if (topic == META_DEBUG_SYNC)
    {
      ++sync_count;
      fprintf (out, "%d: ", sync_count);
    }

  utf8_fputs (str, out);

  fflush (out);

  g_free (str);
}

void
meta_push_no_msg_prefix (void)
{
  ++no_prefix;
}

void
meta_pop_no_msg_prefix (void)
{
  g_return_if_fail (no_prefix > 0);

  --no_prefix;
}

gint
meta_unsigned_long_equal (gconstpointer v1,
                          gconstpointer v2)
{
  return *((const gulong*) v1) == *((const gulong*) v2);
}

guint
meta_unsigned_long_hash  (gconstpointer v)
{
  gulong val = * (const gulong *) v;

  /* I'm not sure this works so well. */
#if GLIB_SIZEOF_LONG > 4
  return (guint) (val ^ (val >> 32));
#else
  return val;
#endif
}

const char*
meta_gravity_to_string (int gravity)
{
  switch (gravity)
    {
    case NorthWestGravity:
      return "NorthWestGravity";
      break;
    case NorthGravity:
      return "NorthGravity";
      break;
    case NorthEastGravity:
      return "NorthEastGravity";
      break;
    case WestGravity:
      return "WestGravity";
      break;
    case CenterGravity:
      return "CenterGravity";
      break;
    case EastGravity:
      return "EastGravity";
      break;
    case SouthWestGravity:
      return "SouthWestGravity";
      break;
    case SouthGravity:
      return "SouthGravity";
      break;
    case SouthEastGravity:
      return "SouthEastGravity";
      break;
    case StaticGravity:
      return "StaticGravity";
      break;
    default:
      return "NorthWestGravity";
      break;
    }
}

GPid
meta_show_dialog (const char *type,
                  const char *message,
                  const char *timeout,
                  const char *display,
                  const char *ok_text,
                  const char *cancel_text,
                  const int transient_for,
                  GSList *columns,
                  GSList *entries)
{
  GError *error = NULL;
  GSList *tmp;
  int i=0;
  GPid child_pid;
  const char **argvl = g_malloc(sizeof (char*) *
                                (17 +
                                 g_slist_length (columns)*2 +
                                 g_slist_length (entries)));

  argvl[i++] = "zenity";
  argvl[i++] = type;
  argvl[i++] = "--display";
  argvl[i++] = display;
  argvl[i++] = "--class";
  argvl[i++] = "metacity-dialog";
  argvl[i++] = "--title";
  /* Translators: This is the title used on dialog boxes */
  argvl[i++] = _("Metacity");
  argvl[i++] = "--text";
  argvl[i++] = message;

  if (timeout)
    {
      argvl[i++] = "--timeout";
      argvl[i++] = timeout;
    }

  if (ok_text)
    {
      argvl[i++] = "--ok-label";
      argvl[i++] = ok_text;
     }

  if (cancel_text)
    {
      argvl[i++] = "--cancel-label";
      argvl[i++] = cancel_text;
    }

  tmp = columns;
  while (tmp)
    {
      argvl[i++] = "--column";
      argvl[i++] = tmp->data;
      tmp = tmp->next;
    }

  tmp = entries;
  while (tmp)
    {
      argvl[i++] = tmp->data;
      tmp = tmp->next;
    }

  argvl[i] = NULL;

  if (transient_for)
    {
      gchar *env = g_strdup_printf("%d", transient_for);
      setenv ("WINDOWID", env, 1);
      g_free (env);
    }

  g_spawn_async (
                 "/",
                 (gchar**) argvl, /* ugh */
                 NULL,
                 G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                 NULL, NULL,
                 &child_pid,
                 &error
                 );

  if (transient_for)
    unsetenv ("WINDOWID");

  g_free (argvl);

  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  return child_pid;
}
