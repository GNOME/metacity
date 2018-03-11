/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001 Havoc Pennington, error trapping inspired by GDK
 * code copyrighted by the GTK team.
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

#include "config.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include "errors.h"

void
meta_error_trap_push (MetaDisplay *display)
{
  Display *xdisplay;
  GdkDisplay *gdk_display;

  xdisplay = meta_display_get_xdisplay (display);
  gdk_display = gdk_x11_lookup_xdisplay (xdisplay);
  g_assert (gdk_display != NULL);

  gdk_x11_display_error_trap_push (gdk_display);
}

void
meta_error_trap_pop (MetaDisplay *display)
{
  Display *xdisplay;
  GdkDisplay *gdk_display;

  xdisplay = meta_display_get_xdisplay (display);
  gdk_display = gdk_x11_lookup_xdisplay (xdisplay);
  g_assert (gdk_display != NULL);

  gdk_x11_display_error_trap_pop_ignored (gdk_display);
}

int
meta_error_trap_pop_with_return (MetaDisplay *display)
{
  Display *xdisplay;
  GdkDisplay *gdk_display;

  xdisplay = meta_display_get_xdisplay (display);
  gdk_display = gdk_x11_lookup_xdisplay (xdisplay);
  g_assert (gdk_display != NULL);

  return gdk_x11_display_error_trap_pop (gdk_display);
}
