/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity resizing-terminal-window feedback */

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
#include "resizepopup.h"
#include "meta-tooltip.h"
#include "util.h"
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

struct _MetaResizePopup
{
  GtkWidget *size_window;
  Display *display;

  int vertical_size;
  int horizontal_size;

  gboolean showing;

  MetaRectangle rect;
};

MetaResizePopup*
meta_ui_resize_popup_new (Display *display)
{
  MetaResizePopup *popup;

  popup = g_new0 (MetaResizePopup, 1);

  popup->display = display;

  return popup;
}

void
meta_ui_resize_popup_free (MetaResizePopup *popup)
{
  g_return_if_fail (popup != NULL);

  if (popup->size_window)
    gtk_widget_destroy (popup->size_window);

  g_free (popup);
}

static void
ensure_size_window (MetaResizePopup *popup)
{
  if (popup->size_window)
    return;

  popup->size_window = meta_tooltip_new ();
}

static void
update_size_window (MetaResizePopup *popup)
{
  char *str;
  int x, y;
  int width, height;

  g_return_if_fail (popup->size_window != NULL);

  /* Translators: This represents the size of a window.  The first number is
   * the width of the window and the second is the height.
   */
  str = g_strdup_printf (_("%d x %d"),
                         popup->horizontal_size,
                         popup->vertical_size);

  meta_tooltip_set_label_text (META_TOOLTIP (popup->size_window), str);
  g_free (str);

  gtk_window_get_size (GTK_WINDOW (popup->size_window), &width, &height);

  x = popup->rect.x + (popup->rect.width - width) / 2;
  y = popup->rect.y + (popup->rect.height - height) / 2;

  if (gtk_widget_get_realized (popup->size_window))
    {
      /* using move_resize to avoid jumpiness */
      gdk_window_move_resize (gtk_widget_get_window (popup->size_window),
                              x, y,
                              width, height);
    }
  else
    {
      gtk_window_move   (GTK_WINDOW (popup->size_window),
                         x, y);
    }
}

static void
sync_showing (MetaResizePopup *popup)
{
  if (popup->showing)
    {
      if (popup->size_window)
        gtk_widget_show (popup->size_window);

      if (popup->size_window && gtk_widget_get_realized (popup->size_window))
        gdk_window_raise (gtk_widget_get_window (popup->size_window));
    }
  else
    {
      if (popup->size_window)
        gtk_widget_hide (popup->size_window);
    }
}

void
meta_ui_resize_popup_set (MetaResizePopup *popup,
                          MetaRectangle    rect,
                          int              base_width,
                          int              base_height,
                          int              width_inc,
                          int              height_inc)
{
  gboolean need_update_size;
  int display_w, display_h;

  g_return_if_fail (popup != NULL);

  need_update_size = FALSE;

  display_w = rect.width - base_width;
  if (width_inc > 0)
    display_w /= width_inc;

  display_h = rect.height - base_height;
  if (height_inc > 0)
    display_h /= height_inc;

  if (!meta_rectangle_equal(&popup->rect, &rect) ||
      display_w != popup->horizontal_size ||
      display_h != popup->vertical_size)
    need_update_size = TRUE;

  popup->rect = rect;
  popup->vertical_size = display_h;
  popup->horizontal_size = display_w;

  if (need_update_size)
    {
      ensure_size_window (popup);
      update_size_window (popup);
    }

  sync_showing (popup);
}

void
meta_ui_resize_popup_set_showing  (MetaResizePopup *popup,
                                   gboolean         showing)
{
  g_return_if_fail (popup != NULL);

  if (showing == popup->showing)
    return;

  popup->showing = !!showing;

  if (popup->showing)
    {
      ensure_size_window (popup);
      update_size_window (popup);
    }

  sync_showing (popup);
}
