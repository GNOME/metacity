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

#ifndef SELECT_IMAGE_H
#define SELECT_IMAGE_H

#include <gtk/gtk.h>

#define META_TYPE_SELECT_IMAGE         (meta_select_image_get_type ())
#define META_SELECT_IMAGE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), META_TYPE_SELECT_IMAGE, MetaSelectImage))
#define META_SELECT_IMAGE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    META_TYPE_SELECT_IMAGE, MetaSelectImageClass))
#define META_IS_SELECT_IMAGE(o)        (G_TYPE_CHECK_INSTANCE_CAST ((o), META_TYPE_SELECT_IMAGE))
#define META_IS_SELECT_IMAGE_CLASS(c)  (G_TYPE_CHECK_CLASS_CAST ((c),    META_TYPE_SELECT_IMAGE))
#define META_SELECT_IMAGE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  META_TYPE_SELECT_IMAGE, MetaSelectImageClass))

typedef struct _MetaSelectImage        MetaSelectImage;
typedef struct _MetaSelectImageClass   MetaSelectImageClass;
typedef struct _MetaSelectImagePrivate MetaSelectImagePrivate;

struct _MetaSelectImage
{
  GtkImage                parent;
  MetaSelectImagePrivate *priv;
};

struct _MetaSelectImageClass
{
  GtkImageClass parent_class;
};

GType      meta_select_image_get_type (void) G_GNUC_CONST;
GtkWidget *meta_select_image_new      (GdkPixbuf       *pixbuf);
void       meta_select_image_select   (MetaSelectImage *image);
void       meta_select_image_unselect (MetaSelectImage *image);

#endif
