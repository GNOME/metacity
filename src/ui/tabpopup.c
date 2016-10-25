/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity popup window thing showing windows you can tab to */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002 Red Hat, Inc.
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

#include <config.h>

#include "util.h"
#include "core.h"
#include "tabpopup.h"
#include "select-image.h"
#include "select-workspace.h"
#include <gtk/gtk.h>
#include <math.h>

typedef struct _TabEntry TabEntry;

struct _TabEntry
{
  MetaTabEntryKey  key;
  char            *title;
  GdkPixbuf       *icon, *dimmed_icon;
  GtkWidget       *widget;
  GdkRectangle     rect;
  GdkRectangle     inner_rect;
  guint blank : 1;
};

struct _MetaTabPopup
{
  GtkWidget *window;
  GtkWidget *label;
  GList *current;
  GList *entries;
  TabEntry *current_selected_entry;
  GtkWidget *outline_window;
  gboolean outline;
};

static gboolean
outline_window_draw (GtkWidget *widget,
                     cairo_t   *cr,
                     gpointer   data)
{
  GdkRGBA black = { 0.0, 0.0, 0.0, 1.0 };
  MetaTabPopup *popup;
  TabEntry *te;

  popup = data;

  if (!popup->outline || popup->current_selected_entry == NULL)
    return FALSE;

  te = popup->current_selected_entry;

  gdk_cairo_set_source_rgba (cr, &black);
  cairo_paint (cr);

  cairo_set_line_width (cr, 1.0);
  cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);

  cairo_rectangle (cr,
                   0.5, 0.5,
                   te->rect.width - 1,
                   te->rect.height - 1);
  cairo_stroke (cr);

  cairo_rectangle (cr,
                   te->inner_rect.x - 0.5, te->inner_rect.y - 0.5,
                   te->inner_rect.width + 1,
                   te->inner_rect.height + 1);
  cairo_stroke (cr);

  return FALSE;
}

static GdkPixbuf*
dimm_icon (GdkPixbuf *pixbuf)
{
  int x, y, pixel_stride, row_stride;
  guchar *row, *pixels;
  int w, h;
  GdkPixbuf *dimmed_pixbuf;

  if (gdk_pixbuf_get_has_alpha (pixbuf))
    {
      dimmed_pixbuf = gdk_pixbuf_copy (pixbuf);
    }
  else
    {
      dimmed_pixbuf = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);
    }

  w = gdk_pixbuf_get_width (dimmed_pixbuf);
  h = gdk_pixbuf_get_height (dimmed_pixbuf);

  pixel_stride = 4;

  row = gdk_pixbuf_get_pixels (dimmed_pixbuf);
  row_stride = gdk_pixbuf_get_rowstride (dimmed_pixbuf);

  for (y = 0; y < h; y++)
    {
      pixels = row;
      for (x = 0; x < w; x++)
        {
          pixels[3] /= 2;
          pixels += pixel_stride;
        }
      row += row_stride;
    }
  return dimmed_pixbuf;
}

static TabEntry*
tab_entry_new (const MetaTabEntry *entry,
               gboolean            outline)
{
  TabEntry *te;

  te = g_new (TabEntry, 1);
  te->key = entry->key;
  te->title = NULL;
  if (entry->title)
    {
      gchar *str;
      gchar *tmp;

      str = meta_g_utf8_strndup (entry->title, 4096);
      tmp = g_markup_printf_escaped (entry->hidden ? "[%s]" : "%s", str);
      g_free (str);
      str = tmp;

      if (entry->demands_attention)
        {
          /* Escape the whole line of text then markup the text and
           * copy it back into the original buffer.
           */
          tmp = g_strdup_printf ("<b>%s</b>", str);
          g_free (str);
          str = tmp;
        }

        te->title=g_strdup(str);

      g_free (str);
    }
  te->widget = NULL;
  te->icon = entry->icon;
  te->blank = entry->blank;
  te->dimmed_icon = NULL;
  if (te->icon)
    {
      g_object_ref (G_OBJECT (te->icon));
      if (entry->hidden)
        te->dimmed_icon = dimm_icon (entry->icon);
    }

  if (outline)
    {
      te->rect.x = entry->rect.x;
      te->rect.y = entry->rect.y;
      te->rect.width = entry->rect.width;
      te->rect.height = entry->rect.height;

      te->inner_rect.x = entry->inner_rect.x;
      te->inner_rect.y = entry->inner_rect.y;
      te->inner_rect.width = entry->inner_rect.width;
      te->inner_rect.height = entry->inner_rect.height;
    }
  return te;
}

MetaTabPopup*
meta_ui_tab_popup_new (const MetaTabEntry *entries,
                       int                 entry_count,
                       int                 width,
                       gboolean            outline)
{
  MetaTabPopup *popup;
  int i, left, right, top, bottom;
  int height;
  GtkWidget *grid;
  GtkWidget *vbox;
  GList *tmp;
  GtkWidget *frame;
  int max_label_width; /* the actual max width of the labels we create */
  AtkObject *obj;
  GdkScreen *screen;
  GdkVisual *visual;
  GdkWindow *root;
  int screen_width;

  popup = g_new (MetaTabPopup, 1);

  screen = gdk_screen_get_default ();
  visual = gdk_screen_get_rgba_visual (screen);

  if (outline)
    {
      popup->outline_window = gtk_window_new (GTK_WINDOW_POPUP);

      if (visual)
        gtk_widget_set_visual (popup->outline_window, visual);

      gtk_window_set_screen (GTK_WINDOW (popup->outline_window),
                             screen);

      gtk_widget_set_app_paintable (popup->outline_window, TRUE);
      gtk_widget_realize (popup->outline_window);

      g_signal_connect (G_OBJECT (popup->outline_window), "draw",
                        G_CALLBACK (outline_window_draw), popup);

      gtk_widget_show (popup->outline_window);
    }
  else
    popup->outline_window = NULL;

  popup->window = gtk_window_new (GTK_WINDOW_POPUP);

  gtk_window_set_screen (GTK_WINDOW (popup->window),
                         screen);

  gtk_window_set_position (GTK_WINDOW (popup->window),
                           GTK_WIN_POS_CENTER_ALWAYS);
  /* enable resizing, to get never-shrink behavior */
  gtk_window_set_resizable (GTK_WINDOW (popup->window),
                            TRUE);
  popup->current = NULL;
  popup->entries = NULL;
  popup->current_selected_entry = NULL;
  popup->outline = outline;

  for (i = 0; i < entry_count; ++i)
    {
      TabEntry* new_entry = tab_entry_new (&entries[i], outline);
      popup->entries = g_list_prepend (popup->entries, new_entry);
    }

  popup->entries = g_list_reverse (popup->entries);

  g_assert (width > 0);
  height = i / width;
  if (i % width)
    height += 1;

  grid = gtk_grid_new ();
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
  gtk_container_set_border_width (GTK_CONTAINER (grid), 1);
  gtk_container_add (GTK_CONTAINER (popup->window),
                     frame);
  gtk_container_add (GTK_CONTAINER (frame),
                     vbox);

  gtk_box_pack_start (GTK_BOX (vbox), grid, TRUE, TRUE, 0);

  popup->label = gtk_label_new ("");

  /* Set the accessible role of the label to a status bar so it
   * will emit name changed events that can be used by screen
   * readers.
   */
  obj = gtk_widget_get_accessible (popup->label);
  atk_object_set_role (obj, ATK_ROLE_STATUSBAR);

  gtk_box_pack_end (GTK_BOX (vbox), popup->label, FALSE, FALSE, 3);

  max_label_width = 0;
  top = 0;
  bottom = 1;
  tmp = popup->entries;

  while (tmp && top < height)
    {
      left = 0;
      right = 1;

      while (tmp && left < width)
        {
          GtkWidget *image;
          GtkRequisition req;

          TabEntry *te;

          te = tmp->data;

          if (te->blank)
            {
              /* just stick a widget here to avoid special cases */
              image = gtk_label_new ("");
            }
          else if (outline)
            {
              if (te->dimmed_icon)
                {
                  image = meta_select_image_new (te->dimmed_icon);
                }
              else
                {
                  image = meta_select_image_new (te->icon);
                }

              gtk_widget_set_halign (image, GTK_ALIGN_CENTER);
              gtk_widget_set_valign (image, GTK_ALIGN_CENTER);
            }
          else
            {
              image = meta_select_workspace_new ((MetaWorkspace *) te->key);
            }

          te->widget = image;

          gtk_grid_attach (GTK_GRID (grid), te->widget, left, top, 1, 1);

          /* Efficiency rules! */
          gtk_label_set_markup (GTK_LABEL (popup->label),
                              te->title);
          gtk_widget_get_preferred_size (popup->label, &req, NULL);
          max_label_width = MAX (max_label_width, req.width);

          tmp = tmp->next;

          ++left;
          ++right;
        }

      ++top;
      ++bottom;
    }

  /* remove all the temporary text */
  gtk_label_set_text (GTK_LABEL (popup->label), "");
  /* Make it so that we ellipsize if the text is too long */
  gtk_label_set_ellipsize (GTK_LABEL (popup->label), PANGO_ELLIPSIZE_END);

  /* Limit the window size to no bigger than screen_width/4 */
  root = gdk_screen_get_root_window (screen);
  screen_width = gdk_window_get_width (root);
  if (max_label_width>(screen_width/4))
    {
      max_label_width = screen_width/4;
    }

  max_label_width += 20; /* add random padding */

  gtk_window_set_default_size (GTK_WINDOW (popup->window),
                               max_label_width,
                               -1);

  return popup;
}

static void
free_tab_entry (gpointer data, gpointer user_data)
{
  TabEntry *te;

  te = data;

  g_free (te->title);
  if (te->icon)
    g_object_unref (G_OBJECT (te->icon));
  if (te->dimmed_icon)
    g_object_unref (G_OBJECT (te->dimmed_icon));

  g_free (te);
}

void
meta_ui_tab_popup_free (MetaTabPopup *popup)
{
  meta_verbose ("Destroying tab popup window\n");

  if (popup->outline_window != NULL)
    gtk_widget_destroy (popup->outline_window);
  gtk_widget_destroy (popup->window);

  g_list_foreach (popup->entries, free_tab_entry, NULL);

  g_list_free (popup->entries);

  g_free (popup);
}

void
meta_ui_tab_popup_set_showing (MetaTabPopup *popup,
                               gboolean      showing)
{
  if (showing)
    {
      gtk_widget_show_all (popup->window);
    }
  else
    {
      if (gtk_widget_get_visible (popup->window))
        {
          meta_verbose ("Hiding tab popup window\n");
          gtk_widget_hide (popup->window);
          meta_core_increment_event_serial (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));
        }
    }
}

static void
display_entry (MetaTabPopup *popup,
               TabEntry     *te)
{
  if (popup->current_selected_entry)
  {
    if (popup->outline)
      meta_select_image_unselect (META_SELECT_IMAGE (popup->current_selected_entry->widget));
    else
      meta_select_workspace_unselect (META_SELECT_WORKSPACE (popup->current_selected_entry->widget));
  }

  gtk_label_set_markup (GTK_LABEL (popup->label), te->title);

  if (popup->outline)
    meta_select_image_select (META_SELECT_IMAGE (te->widget));
  else
    meta_select_workspace_select (META_SELECT_WORKSPACE (te->widget));

  if (popup->outline)
    {
      GdkRectangle rect;
      GdkWindow *window;
      cairo_region_t *region;

      window = gtk_widget_get_window (popup->outline_window);

      /* Do stuff behind gtk's back */
      gdk_window_hide (window);
      meta_core_increment_event_serial (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));

      rect = te->rect;
      rect.x = 0;
      rect.y = 0;

      gtk_window_move (GTK_WINDOW (popup->outline_window), te->rect.x, te->rect.y);
      gtk_window_resize (GTK_WINDOW (popup->outline_window), te->rect.width, te->rect.height);

      region = cairo_region_create_rectangle (&rect);
      cairo_region_subtract_rectangle (region, &te->inner_rect);

      gdk_window_shape_combine_region (gtk_widget_get_window (popup->outline_window),
                                       region,
                                       0, 0);

      cairo_region_destroy (region);

      gdk_window_show_unraised (window);
    }

  /* Must be before we handle an expose for the outline window */
  popup->current_selected_entry = te;
}

void
meta_ui_tab_popup_forward (MetaTabPopup *popup)
{
  if (popup->current != NULL)
    popup->current = popup->current->next;

  if (popup->current == NULL)
    popup->current = popup->entries;

  if (popup->current != NULL)
    {
      TabEntry *te;

      te = popup->current->data;

      display_entry (popup, te);
    }
}

void
meta_ui_tab_popup_backward (MetaTabPopup *popup)
{
  if (popup->current != NULL)
    popup->current = popup->current->prev;

  if (popup->current == NULL)
    popup->current = g_list_last (popup->entries);

  if (popup->current != NULL)
    {
      TabEntry *te;

      te = popup->current->data;

      display_entry (popup, te);
    }
}

MetaTabEntryKey
meta_ui_tab_popup_get_selected (MetaTabPopup *popup)
{
  if (popup->current)
    {
      TabEntry *te;

      te = popup->current->data;

      return te->key;
    }
  else
    return (MetaTabEntryKey)None;
}

void
meta_ui_tab_popup_select (MetaTabPopup *popup,
                          MetaTabEntryKey key)
{
  GList *tmp;

  /* Note, "key" may not be in the list of entries; other code assumes
   * it's OK to pass in a key that isn't.
   */

  tmp = popup->entries;
  while (tmp != NULL)
    {
      TabEntry *te;

      te = tmp->data;

      if (te->key == key)
        {
          popup->current = tmp;

          display_entry (popup, te);

          return;
        }

      tmp = tmp->next;
    }
}
