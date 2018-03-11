/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity fixed tooltip routine */

/*
 * Copyright (C) 2001 Havoc Pennington
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
#include "fixedtip.h"
#include "meta-tooltip.h"
#include "ui.h"

/**
 * The floating rectangle.  This is a GtkWindow, and it contains
 * the "label" widget, below.
 */
static GtkWidget *tip = NULL;

static void
get_monitor_geometry (gint          root_x,
                      gint          root_y,
                      GdkRectangle *geometry)
{
  GdkDisplay *display;
  GdkMonitor *monitor;

  display = gdk_display_get_default ();
  monitor = gdk_display_get_monitor_at_point (display, root_x, root_y);

  gdk_monitor_get_geometry (monitor, geometry);
}

void
meta_fixed_tip_show (int root_x, int root_y,
                     const char *markup_text)
{
  gint w;
  gint h;
  GdkRectangle rect;
  gint screen_right_edge;

  if (tip == NULL)
    {
      tip = meta_tooltip_new ();

      g_signal_connect (tip, "destroy",
			G_CALLBACK (gtk_widget_destroyed), &tip);
    }

  get_monitor_geometry (root_x, root_y, &rect);
  screen_right_edge = rect.x + rect.width;

  meta_tooltip_set_label_markup (META_TOOLTIP (tip), markup_text);

  gtk_window_get_size (GTK_WINDOW (tip), &w, &h);

  if (meta_ui_get_direction() == META_UI_DIRECTION_RTL)
      root_x = MAX(0, root_x - w);

  if ((root_x + w) > screen_right_edge)
    root_x -= (root_x + w) - screen_right_edge;

  gtk_window_move (GTK_WINDOW (tip), root_x, root_y);

  gtk_widget_show (tip);
}

void
meta_fixed_tip_hide (void)
{
  if (tip)
    {
      gtk_widget_destroy (tip);
      tip = NULL;
    }
}
