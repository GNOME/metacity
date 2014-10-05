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

#ifndef SELECT_WORKSPACE_H
#define SELECT_WORKSPACE_H

#include <gtk/gtk.h>
#include "../core/workspace.h"

#define META_TYPE_SELECT_WORKSPACE         (meta_select_workspace_get_type ())
#define META_SELECT_WORKSPACE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), META_TYPE_SELECT_WORKSPACE, MetaSelectWorkspace))
#define META_SELECT_WORKSPACE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    META_TYPE_SELECT_WORKSPACE, MetaSelectWorkspaceClass))
#define META_IS_SELECT_WORKSPACE(o)        (G_TYPE_CHECK_INSTANCE_CAST ((o), META_TYPE_SELECT_WORKSPACE))
#define META_IS_SELECT_WORKSPACE_CLASS(c)  (G_TYPE_CHECK_CLASS_CAST ((c),    META_TYPE_SELECT_WORKSPACE))
#define META_SELECT_WORKSPACE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  META_TYPE_SELECT_WORKSPACE, MetaSelectWorkspaceClass))

typedef struct _MetaSelectWorkspace        MetaSelectWorkspace;
typedef struct _MetaSelectWorkspaceClass   MetaSelectWorkspaceClass;
typedef struct _MetaSelectWorkspacePrivate MetaSelectWorkspacePrivate;

struct _MetaSelectWorkspace
{
  GtkDrawingArea              parent;
  MetaSelectWorkspacePrivate *priv;
};

struct _MetaSelectWorkspaceClass
{
  GtkDrawingAreaClass parent_class;
};

GType      meta_select_workspace_get_type (void) G_GNUC_CONST;
GtkWidget *meta_select_workspace_new      (MetaWorkspace       *workspace);
void       meta_select_workspace_select   (MetaSelectWorkspace *image);
void       meta_select_workspace_unselect (MetaSelectWorkspace *image);

#endif
