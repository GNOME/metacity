/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity window deletion */

/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2004 Elijah Newren
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
#define _XOPEN_SOURCE /* for gethostname() and kill() */

#include <config.h>
#include "util.h"
#include "window-private.h"
#include "errors.h"
#include "workspace.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

static void meta_window_present_delete_dialog (MetaWindow *window,
                                               guint32     timestamp);

static void
delete_ping_reply_func (MetaDisplay *display,
                        Window       xwindow,
                        guint32      timestamp,
                        void        *user_data)
{
  meta_topic (META_DEBUG_PING,
              "Got reply to delete ping for %s\n",
              ((MetaWindow*)user_data)->desc);

  /* we do nothing */
}

static void
dialog_exited (GPid     pid,
               int      status,
               gpointer user_data)
{
  MetaWindow *ours = (MetaWindow*) user_data;

  ours->dialog_pid = -1;

  /* exit status of 0 means the user pressed "Force Quit" */
  if (WIFEXITED (status) && WEXITSTATUS (status) == 0)
    meta_window_kill (ours);
}

static GPid
show_delete_dialog (const char *window_title,
                    const char *display,
                    const int   transient_for)
{
  GError *error = NULL;
  int i=0;
  GPid child_pid;
  const char **argvl;
  char *transient_for_s;

  argvl = g_malloc (sizeof (char *) * 12);
  transient_for_s = g_strdup_printf ("%d", transient_for);

  argvl[i++] = METACITY_LIBEXECDIR "/metacity-dialog";
  argvl[i++] = "--type";
  argvl[i++] = "delete";
  argvl[i++] = "--display";
  argvl[i++] = display;
  argvl[i++] = "--class";
  argvl[i++] = "metacity-dialog";
  argvl[i++] = "--window-title";
  argvl[i++] = window_title;
  argvl[i++] = "--transient-for";
  argvl[i++] = transient_for_s;

  argvl[i] = NULL;

  g_spawn_async ("/",
                 (char **) argvl,
                 NULL,
                 G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                 NULL,
                 NULL,
                 &child_pid,
                 &error);

  g_free (transient_for_s);
  g_free (argvl);

  if (error != NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  return child_pid;
}

static void
delete_ping_timeout_func (MetaDisplay *display,
                          Window       xwindow,
                          guint32      timestamp,
                          void        *user_data)
{
  MetaWindow *window = user_data;
  char *window_title;
  GPid dialog_pid;

  meta_topic (META_DEBUG_PING,
              "Got delete ping timeout for %s\n",
              window->desc);

  if (window->dialog_pid >= 0)
    {
      meta_window_present_delete_dialog (window, timestamp);
      return;
    }

  window_title = g_locale_from_utf8 (window->title, -1, NULL, NULL, NULL);

  dialog_pid = show_delete_dialog (window_title,
                                   window->screen->screen_name,
                                   window->xwindow);

  g_free (window_title);

  window->dialog_pid = dialog_pid;
  g_child_watch_add (dialog_pid, dialog_exited, window);
}

void
meta_window_delete (MetaWindow  *window,
                    guint32      timestamp)
{
  meta_error_trap_push (window->display);
  if (window->delete_window)
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Deleting %s with delete_window request\n",
                  window->desc);
      meta_window_send_icccm_message (window,
                                      window->display->atom_WM_DELETE_WINDOW,
                                      timestamp);
    }
  else
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Deleting %s with explicit kill\n",
                  window->desc);
      XKillClient (window->display->xdisplay, window->xwindow);
    }
  meta_error_trap_pop (window->display);

  meta_display_ping_window (window->display,
                            window,
                            timestamp,
                            delete_ping_reply_func,
                            delete_ping_timeout_func,
                            window);
}

void
meta_window_kill (MetaWindow *window)
{
  pid_t client_pid;
  char buf[257];

  meta_topic (META_DEBUG_WINDOW_OPS,
              "Killing %s brutally\n",
              window->desc);

  client_pid = meta_window_get_client_pid (window);

  if (window->wm_client_machine != NULL &&
      client_pid > 0)
    {
      if (gethostname (buf, sizeof(buf)-1) == 0)
        {
          if (strcmp (buf, window->wm_client_machine) == 0)
            {
              meta_topic (META_DEBUG_WINDOW_OPS,
                          "Killing %s with kill()\n",
                          window->desc);

              if (kill (client_pid, 9) < 0)
                meta_topic (META_DEBUG_WINDOW_OPS,
                            "Failed to signal %s: %s\n",
                            window->desc, strerror (errno));
            }
        }
      else
        {
          g_warning ("Failed to get hostname: %s", strerror (errno));
        }
    }

  meta_topic (META_DEBUG_WINDOW_OPS,
              "Disconnecting %s with XKillClient()\n",
              window->desc);
  meta_error_trap_push (window->display);
  XKillClient (window->display->xdisplay, window->xwindow);
  meta_error_trap_pop (window->display);
}

void
meta_window_free_delete_dialog (MetaWindow *window)
{
  if (window->dialog_pid >= 0)
    {
      kill (window->dialog_pid, 9);
      window->dialog_pid = -1;
    }
}

static void
meta_window_present_delete_dialog (MetaWindow *window, guint32 timestamp)
{
  meta_topic (META_DEBUG_PING,
              "Presenting existing ping dialog for %s\n",
              window->desc);

  if (window->dialog_pid >= 0)
    {
      GSList *windows;
      GSList *tmp;

      /* Activate transient for window that belongs to
       * metacity-dialog
       */

      windows = meta_display_list_windows (window->display, META_LIST_DEFAULT);
      tmp = windows;
      while (tmp != NULL)
        {
          MetaWindow *w = tmp->data;

          if (w->xtransient_for == window->xwindow &&
              w->res_class &&
              g_ascii_strcasecmp (w->res_class, "metacity-dialog") == 0)
            {
              meta_window_activate (w, timestamp);
              break;
            }

          tmp = tmp->next;
        }

      g_slist_free (windows);
    }
}
