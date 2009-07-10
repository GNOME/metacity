/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Window matching */

/*
 * Copyright (C) 2009 Thomas Thurman
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

#include "matching.h"
#include "window-private.h"

/**
 * We currently keep this information in a GKeyFile.
 * This is just for an example and may change.
 */
GKeyFile *matching_keyfile = NULL;

static void
load_matching_data (void)
{
  if (matching_keyfile)
    return;

  /* load it, or... (stub) */
  matching_keyfile = g_key_file_new ();
  /* FIXME: would be helpful to add a leading comment */
}

static gchar*
get_window_role (MetaWindow *window)
{
  if (window->role)
    return window->role;
  else if (window->title) /* hacky fallback */
    return window->title;
  else /* give up */
    return NULL;
}

void
meta_matching_load_from_role (MetaWindow *window)
{
  gint x, y, w, h;
  gchar *role = get_window_role (window);

  if (!role)
      return;

  load_matching_data ();

  /* FIXME error checking */
  x = g_key_file_get_integer (matching_keyfile, role, "x", NULL);
  y = g_key_file_get_integer (matching_keyfile, role, "y", NULL);
  w = g_key_file_get_integer (matching_keyfile, role, "w", NULL);
  h = g_key_file_get_integer (matching_keyfile, role, "h", NULL);

  
}

void
meta_matching_save_to_role (MetaWindow *window)
{
  gint x, y, w, h;
  gchar *role = get_window_role (window);

  if (!role)
      return;

  load_matching_data ();

  meta_window_get_geometry (window, &x, &y, &w, &h);

  g_key_file_set_integer (matching_keyfile, role, "x", x);
  g_key_file_set_integer (matching_keyfile, role, "y", y);
  g_key_file_set_integer (matching_keyfile, role, "w", w);
  g_key_file_set_integer (matching_keyfile, role, "h", h);

  meta_matching_save_all ();
}

void
meta_matching_save_all (void)
{
  char *data = NULL;

  load_matching_data ();

  data = g_key_file_to_data (matching_keyfile, NULL, NULL);

  g_file_set_contents ("/tmp/metacity-matching.conf",
                       data,
                       -1,
                       NULL);

  g_free (data);
}

/* eof matching.c */

