/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity device structures */

/* 
 * Copyright (C) 2007 Paulo Ricardo Zanoni
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef META_DEVICES_H
#define META_DEVICES_H

#ifdef MPX

#include <X11/extensions/XInput.h>

#include "common.h"
#include "boxes.h"

/* By default, the MetaDevInfo lists have size 4. Almost no client has more
 * than 4 mice or keyboards... */
#define DEFAULT_INPUT_ARRAY_SIZE 4

typedef struct _MetaDevices MetaDevices;

/* typedef struct _MetaDevInfo MetaDevInfo; This guy was declared at common.h */

typedef struct _MetaGrabOpInfo MetaGrabOpInfo;

/* TODO: create MetaPtrInfo and MetaKbdInfo, so that you can differentiate it
 * and force correct type using in function prototypes */

#if 0 
--> To be used soon!! (next commit)
struct _MetaGrabOpInfo
{
  /* current window operation */
  MetaGrabOp       op;
  MetaScreen      *screen;
  MetaWindow      *window;
  Window           xwindow;
  int              button;
  int              anchor_root_x;
  int              anchor_root_y;
  MetaRectangle    anchor_window_pos;
  int              latest_motion_x;
  int              latest_motion_y;
  gulong           mask;
  guint            have_pointer : 1;
  guint            grabbed_pointer : 1;
  guint            have_keyboard : 1;
  guint            wireframe_active : 1;
  guint            was_cancelled : 1;    /* Only used in wireframe mode */
  guint            frame_action : 1;
  MetaRectangle    wireframe_rect;
  MetaRectangle    wireframe_last_xor_rect;
  MetaRectangle    initial_window_pos;
  int              initial_x, grab_initial_y;  /* These are only relevant for */
  gboolean         threshold_movement_reached; /* raise_on_click == FALSE.    */
  MetaResizePopup *resize_popup;
  GTimeVal         last_moveresize_time;
  guint32          motion_notify_time;
  int              wireframe_last_display_width;
  int              wireframe_last_display_height;
  GList*           old_window_stacking;
  MetaEdgeResistanceData *edge_resistance_data;
  unsigned int     last_user_action_was_snap;
};
#endif

struct _MetaDevInfo
{
  XDevice        *xdev;
  gchar          *name;
  MetaGrabOpInfo *grab_op;
};

struct _MetaDevices
{
  MetaDevInfo *mice;
  int          miceUsed;
  int          miceSize;

  MetaDevInfo *keyboards;
  int          keybsUsed; /* XXX :%s/keybsUsed/kbdsUsed/g or something else? */
  int          keybsSize; /* I don't like "keybs" */
  XID         *pairedPointers; /* indexed by the keyboards! */
  /* TODO: consider creating a structure to store the pairs!! */
};

MetaDevInfo* meta_devices_find_mouse_by_name    (MetaDisplay *display, 
                                                 gchar       *name);

MetaDevInfo* meta_devices_find_mouse_by_id      (MetaDisplay *display,
                                                 XID          id);

MetaDevInfo* meta_devices_find_keyboard_by_id   (MetaDisplay *display,
                                                 XID         id);

MetaDevInfo* meta_devices_find_paired_mouse     (MetaDisplay *display,
						 XID          id);

MetaDevInfo* meta_devices_find_paired_keyboard  (MetaDisplay *display,
						 XID          id);

#else
#error "This branch will ONLY compile if you enable --enable-mpx!"
#endif

#endif
