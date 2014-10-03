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
#include "select-image.h"

#define OUTSIDE_SELECT_RECT 2
#define INSIDE_SELECT_RECT 2

static void     meta_select_image_class_init   (MetaSelectImageClass *klass);
static gboolean meta_select_image_draw         (GtkWidget            *widget,
                                                cairo_t              *cr);

static GtkImageClass *parent_class;

GType
meta_select_image_get_type (void)
{
  static GType image_type = 0;

  if (!image_type)
    {
      static const GTypeInfo image_info =
      {
        sizeof (MetaSelectImageClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) meta_select_image_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (MetaSelectImage),
        16,             /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };

      image_type = g_type_register_static (GTK_TYPE_IMAGE, "MetaSelectImage", &image_info, 0);
    }

  return image_type;
}

static void
meta_select_image_class_init (MetaSelectImageClass *klass)
{
  GtkWidgetClass *widget_class;
  
  parent_class = g_type_class_peek (gtk_image_get_type ());

  widget_class = GTK_WIDGET_CLASS (klass);
  
  widget_class->draw= meta_select_image_draw;
}

static gboolean
meta_select_image_draw (GtkWidget *widget,
                        cairo_t   *cr)
{
  GtkAllocation allocation;

  gtk_widget_get_allocation (widget, &allocation);

  if (META_SELECT_IMAGE (widget)->selected)
    {
      GtkMisc *misc;
      GtkRequisition requisition;
      GtkStyleContext *context;
      GdkRGBA color;
      int x, y, w, h;
      gint xpad, ypad;
      gfloat xalign, yalign;

      misc = GTK_MISC (widget);
      
      gtk_widget_get_preferred_size (widget, &requisition, 0);
      gtk_misc_get_alignment (misc, &xalign, &yalign);
      gtk_misc_get_padding (misc, &xpad, &ypad);

      x = (allocation.width - (requisition.width - xpad * 2)) * xalign + 0.5;
      y = (allocation.height - (requisition.height - ypad * 2)) * yalign + 0.5;

      x -= INSIDE_SELECT_RECT + 1;
      y -= INSIDE_SELECT_RECT + 1;       
      
      w = requisition.width - OUTSIDE_SELECT_RECT * 2 - 1;
      h = requisition.height - OUTSIDE_SELECT_RECT * 2 - 1;

      context = gtk_widget_get_style_context (widget);

      gtk_style_context_set_state (context,
                                   gtk_widget_get_state_flags (widget));

      gtk_style_context_lookup_color (context, "color", &color);

      cairo_set_line_width (cr, 2.0);
      cairo_set_source_rgb (cr, color.red, color.green, color.blue);

      cairo_rectangle (cr, x, y, w + 1, h + 1);
      cairo_stroke (cr);

      cairo_set_line_width (cr, 1.0);
    }

  return GTK_WIDGET_CLASS (parent_class)->draw (widget, cr);
}
