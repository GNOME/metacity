/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity window frame manager widget */

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

#ifndef META_FRAMES_H
#define META_FRAMES_H

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <libmetacity/meta-theme.h>
#include "common.h"
#include "ui.h"

typedef enum
{
  META_FRAME_CONTROL_NONE,
  META_FRAME_CONTROL_TITLE,
  META_FRAME_CONTROL_DELETE,
  META_FRAME_CONTROL_MENU,
  META_FRAME_CONTROL_MINIMIZE,
  META_FRAME_CONTROL_MAXIMIZE,
  META_FRAME_CONTROL_UNMAXIMIZE,
  META_FRAME_CONTROL_RESIZE_SE,
  META_FRAME_CONTROL_RESIZE_S,
  META_FRAME_CONTROL_RESIZE_SW,
  META_FRAME_CONTROL_RESIZE_N,
  META_FRAME_CONTROL_RESIZE_NE,
  META_FRAME_CONTROL_RESIZE_NW,
  META_FRAME_CONTROL_RESIZE_W,
  META_FRAME_CONTROL_RESIZE_E,
  META_FRAME_CONTROL_CLIENT_AREA
} MetaFrameControl;

/* This is one widget that manages all the window frames
 * as subwindows.
 */

#define META_TYPE_FRAMES meta_frames_get_type ()
G_DECLARE_FINAL_TYPE (MetaFrames, meta_frames, META, FRAMES, GtkWindow)

typedef struct _MetaUIFrame         MetaUIFrame;

struct _MetaUIFrame
{
  Window xwindow;
  GdkWindow *window;
  gchar *theme_variant;
  gchar *title;
  guint expose_delayed : 1;
  guint shape_applied : 1;
  guint ignore_leave_notify : 1;

  /* FIXME get rid of this, it can just be in the MetaFrames struct */
  MetaFrameControl prelit_control;
  gint prelit_x;
  gint prelit_y;
};

MetaFrames *meta_frames_new (MetaUI *ui);

void meta_frames_manage_window (MetaFrames *frames,
                                Window      xwindow,
				GdkWindow  *window);
void meta_frames_unmanage_window (MetaFrames *frames,
                                  Window      xwindow);
void meta_frames_set_title (MetaFrames *frames,
                            Window      xwindow,
                            const char *title);

void meta_frames_update_frame_style (MetaFrames *frames,
                                     Window      xwindow);

void meta_frames_repaint_frame (MetaFrames *frames,
                                Window      xwindow);

void meta_frames_get_borders (MetaFrames *frames,
                              Window xwindow,
                              MetaFrameBorders *borders);

void meta_frames_apply_shapes (MetaFrames *frames,
                               Window      xwindow,
                               int         new_window_width,
                               int         new_window_height,
                               gboolean    window_has_shape);
cairo_region_t *meta_frames_get_frame_bounds (MetaFrames *frames,
                                              Window      xwindow,
                                              int         window_width,
                                              int         window_height);

void meta_frames_move_resize_frame (MetaFrames *frames,
				    Window      xwindow,
				    int         x,
				    int         y,
				    int         width,
				    int         height);
void meta_frames_queue_draw (MetaFrames *frames,
                             Window      xwindow);

void meta_frames_notify_menu_hide (MetaFrames *frames);

void meta_frames_composited_changed (MetaFrames *frames);

#endif
