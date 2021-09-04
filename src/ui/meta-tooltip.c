/*
 * Copyright (C) 2016 Alberts Muktupāvels
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "meta-tooltip.h"

#include "meta-request-csd.h"

struct _MetaTooltip
{
  GtkWindow  parent;

  GtkWidget *box;
  GtkWidget *label;
};

G_DEFINE_TYPE (MetaTooltip, meta_tooltip, GTK_TYPE_WINDOW)

static void
meta_tooltip_realize (GtkWidget *widget)
{
  GdkScreen *screen;
  GdkVisual *visual;
  gboolean has_rgba;
  GtkStyleContext *context;

  screen = gtk_widget_get_screen (widget);
  visual = gdk_screen_get_rgba_visual (screen);
  has_rgba = visual && gdk_screen_is_composited (screen);
  context = gtk_widget_get_style_context (widget);

  if (has_rgba)
    {
      gtk_widget_set_visual (widget, visual);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_CSD);
    }
  else
    {
      gtk_style_context_add_class (context, "solid-csd");
    }

  GTK_WIDGET_CLASS (meta_tooltip_parent_class)->realize (widget);
}

static void
meta_tooltip_class_init (MetaTooltipClass *tooltip_class)
{
  GtkWidgetClass *widget_class;

  widget_class = GTK_WIDGET_CLASS (tooltip_class);

  widget_class->realize = meta_tooltip_realize;

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_TOOL_TIP);
  gtk_widget_class_set_css_name (widget_class, "tooltip");
}

static void
meta_tooltip_init (MetaTooltip *tooltip)
{
  tooltip->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_container_add (GTK_CONTAINER (tooltip), tooltip->box);
  gtk_widget_show (tooltip->box);

  gtk_widget_set_margin_start (tooltip->box, 6);
  gtk_widget_set_margin_end (tooltip->box, 6);
  gtk_widget_set_margin_top (tooltip->box, 6);
  gtk_widget_set_margin_bottom (tooltip->box, 6);

  tooltip->label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (tooltip->box), tooltip->label, FALSE, FALSE, 0);

  gtk_label_set_line_wrap (GTK_LABEL (tooltip->label), TRUE);
  gtk_label_set_max_width_chars (GTK_LABEL (tooltip->label), 70);

  meta_request_csd (GTK_WINDOW (tooltip));
}

GtkWidget *
meta_tooltip_new (void)
{
  return g_object_new (META_TYPE_TOOLTIP,
                       "type", GTK_WINDOW_POPUP,
                       "type-hint", GDK_WINDOW_TYPE_HINT_TOOLTIP,
                       "resizable", FALSE,
                       NULL);
}

void
meta_tooltip_set_label_markup (MetaTooltip *tooltip,
                               const gchar *markup)
{
  if (markup != NULL)
    {
      gtk_label_set_markup (GTK_LABEL (tooltip->label), markup);
      gtk_widget_show (tooltip->label);
    }
  else
    {
      gtk_widget_hide (tooltip->label);
    }
}

void
meta_tooltip_set_label_text (MetaTooltip *tooltip,
                             const gchar *text)
{
  if (text != NULL)
    {
      gtk_label_set_text (GTK_LABEL (tooltip->label), text);
      gtk_widget_show (tooltip->label);
    }
  else
    {
      gtk_widget_hide (tooltip->label);
    }
}
