/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

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
#include "select-image.h"

#define BORDER_WIDTH 2
#define PADDING 3

struct _MetaSelectImagePrivate
{
  gboolean selected;
};

G_DEFINE_TYPE_WITH_PRIVATE (MetaSelectImage, meta_select_image, GTK_TYPE_IMAGE);

static gboolean
meta_select_image_draw (GtkWidget *widget,
                        cairo_t   *cr)
{
  MetaSelectImage *image;

  image = META_SELECT_IMAGE (widget);

  if (image->priv->selected)
    {
      GtkRequisition requisition;
      GtkStyleContext *context;
      GdkRGBA color;
      int x, y, w, h;

      gtk_widget_get_preferred_size (widget, &requisition, 0);

      x = BORDER_WIDTH;
      y = BORDER_WIDTH;
      w = requisition.width - BORDER_WIDTH * 2;
      h = requisition.height - BORDER_WIDTH * 2;

      context = gtk_widget_get_style_context (widget);

      gtk_style_context_set_state (context, gtk_widget_get_state_flags (widget));
      gtk_style_context_lookup_color (context, "color", &color);

      cairo_set_line_width (cr, 2.0);
      cairo_set_source_rgb (cr, color.red, color.green, color.blue);

      cairo_rectangle (cr, x, y, w, h);
      cairo_stroke (cr);

      cairo_set_line_width (cr, 1.0);
    }

  return GTK_WIDGET_CLASS (meta_select_image_parent_class)->draw (widget, cr);
}

static void
meta_select_image_get_preferred_width (GtkWidget *widget,
                                       gint      *minimum_width,
                                       gint      *natural_width)
{
  GTK_WIDGET_CLASS (meta_select_image_parent_class)->get_preferred_width (widget,
                                                                          minimum_width,
                                                                          natural_width);

  *minimum_width += (BORDER_WIDTH + PADDING) * 2;
  *natural_width += (BORDER_WIDTH + PADDING) * 2;
}

static void
meta_select_image_get_preferred_height (GtkWidget *widget,
                                        gint      *minimum_height,
                                        gint      *natural_height)
{
  GTK_WIDGET_CLASS (meta_select_image_parent_class)->get_preferred_height (widget,
                                                                           minimum_height,
                                                                           natural_height);

  *minimum_height += (BORDER_WIDTH + PADDING) * 2;
  *natural_height += (BORDER_WIDTH + PADDING) * 2;
}

static void
meta_select_image_init (MetaSelectImage *image)
{
  image->priv = meta_select_image_get_instance_private (image);
}

static void
meta_select_image_class_init (MetaSelectImageClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = GTK_WIDGET_CLASS (class);

  widget_class->draw = meta_select_image_draw;
  widget_class->get_preferred_width = meta_select_image_get_preferred_width;
  widget_class->get_preferred_height = meta_select_image_get_preferred_height;
}

GtkWidget *
meta_select_image_new (GdkPixbuf *pixbuf)
{
  GtkWidget *widget;

  widget = g_object_new (META_TYPE_SELECT_IMAGE, NULL);
  gtk_image_set_from_pixbuf (GTK_IMAGE (widget), pixbuf);

  return widget;
}

void
meta_select_image_select (MetaSelectImage *image)
{
  image->priv->selected = TRUE;
  gtk_widget_queue_draw (GTK_WIDGET (image));
}

void
meta_select_image_unselect (MetaSelectImage *image)
{
  image->priv->selected = FALSE;
  gtk_widget_queue_draw (GTK_WIDGET (image));
}
