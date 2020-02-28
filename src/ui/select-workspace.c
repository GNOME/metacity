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
#include <gtk/gtk.h>
#include <math.h>
#include "select-workspace.h"
#include "../core/frame-private.h"
#include "draw-workspace.h"

#define SELECT_OUTLINE_WIDTH 2
#define MINI_WORKSPACE_WIDTH 48

struct _MetaSelectWorkspacePrivate
{
  MetaWorkspace *workspace;
  gboolean       selected;
};

G_DEFINE_TYPE_WITH_PRIVATE (MetaSelectWorkspace, meta_select_workspace, GTK_TYPE_DRAWING_AREA);

/**
 * meta_convert_meta_to_wnck() converts a MetaWindow to a
 * WnckWindowDisplayInfo window that is used to build a thumbnail of a
 * workspace.
 **/
static WnckWindowDisplayInfo
meta_convert_meta_to_wnck (MetaWindow *window, MetaScreen *screen)
{
  WnckWindowDisplayInfo wnck_window;
  wnck_window.icon = window->icon;
  wnck_window.mini_icon = window->mini_icon;

  wnck_window.is_active = window->has_focus;

  if (window->frame)
    {
      wnck_window.x = window->frame->rect.x;
      wnck_window.y = window->frame->rect.y;
      wnck_window.width = window->frame->rect.width;
      wnck_window.height = window->frame->rect.height;
    }
  else
    {
      wnck_window.x = window->rect.x;
      wnck_window.y = window->rect.y;
      wnck_window.width = window->rect.width;
      wnck_window.height = window->rect.height;
    }
  return wnck_window;
}


static gboolean
meta_select_workspace_draw (GtkWidget *widget,
                            cairo_t   *cr)
{
  MetaSelectWorkspace *select;
  MetaWorkspace *workspace;
  WnckWindowDisplayInfo *windows;
  GtkAllocation allocation;
  int i, n_windows;
  GList *tmp, *list;

  select = META_SELECT_WORKSPACE (widget);
  workspace = select->priv->workspace;

  list = meta_stack_list_windows (workspace->screen->stack, workspace);
  n_windows = g_list_length (list);
  windows = g_new (WnckWindowDisplayInfo, n_windows);

  tmp = list;
  i = 0;
  while (tmp != NULL)
    {
      MetaWindow *window;
      gboolean ignoreable_sticky;

      window = tmp->data;

      ignoreable_sticky = window->on_all_workspaces &&
                          workspace != workspace->screen->active_workspace;

      if (window->skip_pager ||
          !meta_window_showing_on_its_workspace (window) ||
          window->unmaps_pending != NULL ||
          ignoreable_sticky)
        {
          --n_windows;
        }
      else
        {
          windows[i] = meta_convert_meta_to_wnck (window, workspace->screen);
          i++;
        }
      tmp = tmp->next;
    }

  g_list_free (list);

  gtk_widget_get_allocation (widget, &allocation);

  wnck_draw_workspace (widget,
                       cr,
                       SELECT_OUTLINE_WIDTH,
                       SELECT_OUTLINE_WIDTH,
                       allocation.width - SELECT_OUTLINE_WIDTH * 2,
                       allocation.height - SELECT_OUTLINE_WIDTH * 2,
                       workspace->screen->rect.width,
                       workspace->screen->rect.height,
                       NULL,
                       (workspace->screen->active_workspace == workspace),
                       windows,
                       n_windows);

  g_free (windows);

  if (select->priv->selected)
    {
      GtkStyleContext *context;
      GdkRGBA color;

      context = gtk_widget_get_style_context (widget);

      gtk_style_context_set_state (context,
                                   gtk_widget_get_state_flags (widget));

      gtk_style_context_lookup_color (context, "color", &color);

      cairo_set_line_width (cr, SELECT_OUTLINE_WIDTH);
      cairo_set_source_rgb (cr, color.red, color.green, color.blue);

      cairo_rectangle (cr,
                       SELECT_OUTLINE_WIDTH / 2.0, SELECT_OUTLINE_WIDTH / 2.0,
                       allocation.width - SELECT_OUTLINE_WIDTH,
                       allocation.height - SELECT_OUTLINE_WIDTH);
      cairo_stroke (cr);
    }

  return TRUE;
}

static void
meta_select_workspace_get_preferred_width (GtkWidget *widget,
                                           gint      *minimum_width,
                                           gint      *natural_width)
{
  GTK_WIDGET_CLASS (meta_select_workspace_parent_class)->get_preferred_width (widget,
                                                                              minimum_width,
                                                                              natural_width);

  *minimum_width += SELECT_OUTLINE_WIDTH * 2;
  *natural_width += SELECT_OUTLINE_WIDTH * 2;
}

static void
meta_select_workspace_get_preferred_height (GtkWidget *widget,
                                            gint      *minimum_height,
                                            gint      *natural_height)
{
  GTK_WIDGET_CLASS (meta_select_workspace_parent_class)->get_preferred_height (widget,
                                                                               minimum_height,
                                                                               natural_height);

  *minimum_height += SELECT_OUTLINE_WIDTH * 2;
  *natural_height += SELECT_OUTLINE_WIDTH * 2;
}

static void
meta_select_workspace_init (MetaSelectWorkspace *workspace)
{
  workspace->priv = meta_select_workspace_get_instance_private (workspace);
}

static void
meta_select_workspace_class_init (MetaSelectWorkspaceClass *class)
{
  GtkWidgetClass *widget_class;

  widget_class = GTK_WIDGET_CLASS (class);

  widget_class->draw = meta_select_workspace_draw;
  widget_class->get_preferred_width = meta_select_workspace_get_preferred_width;
  widget_class->get_preferred_height = meta_select_workspace_get_preferred_height;
}

GtkWidget *
meta_select_workspace_new (MetaWorkspace *workspace)
{
  GtkWidget *widget;
  MetaSelectWorkspace *select;
  double screen_aspect;

  widget = g_object_new (META_TYPE_SELECT_WORKSPACE, NULL);
  select = META_SELECT_WORKSPACE (widget);
  screen_aspect = (double) workspace->screen->rect.height /
                  (double) workspace->screen->rect.width;

  gtk_widget_set_size_request (widget,
                               MINI_WORKSPACE_WIDTH + SELECT_OUTLINE_WIDTH * 2,
                               MINI_WORKSPACE_WIDTH * screen_aspect + SELECT_OUTLINE_WIDTH * 2);

  select->priv->workspace = workspace;

  return widget;
}

void
meta_select_workspace_select (MetaSelectWorkspace *workspace)
{
  workspace->priv->selected = TRUE;
  gtk_widget_queue_draw (GTK_WIDGET (workspace));
}

void
meta_select_workspace_unselect (MetaSelectWorkspace *workspace)
{
  workspace->priv->selected = FALSE;
  gtk_widget_queue_draw (GTK_WIDGET (workspace));
}
