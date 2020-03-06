/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity interface for talking to GTK+ UI module */

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

#ifndef META_UI_H
#define META_UI_H

/* Don't include gtk.h or gdk.h here */
#include "common.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libmetacity/meta-frame-borders.h>
#include <libmetacity/meta-frame-enums.h>

typedef struct _MetaUI MetaUI;

typedef gboolean (* MetaEventFunc) (XEvent *xevent, gpointer data);

typedef enum
{
  META_UI_DIRECTION_LTR,
  META_UI_DIRECTION_RTL
} MetaUIDirection;

void meta_ui_init (int *argc, char ***argv);

Display* meta_ui_get_display (void);

void meta_ui_add_event_func    (Display       *xdisplay,
                                MetaEventFunc  func,
                                gpointer       data);
void meta_ui_remove_event_func (Display       *xdisplay,
                                MetaEventFunc  func,
                                gpointer       data);

MetaUI* meta_ui_new (Display  *xdisplay,
                     gboolean  composited);
void    meta_ui_free (MetaUI *ui);

void meta_ui_set_composited (MetaUI   *ui,
                             gboolean  composited);

gint meta_ui_get_scale (MetaUI *ui);

void meta_ui_theme_get_frame_borders (MetaUI           *ui,
                                      MetaFrameType     type,
                                      MetaFrameFlags    flags,
                                      MetaFrameBorders *borders);
void meta_ui_get_frame_borders (MetaUI           *ui,
                                Window            frame_xwindow,
                                MetaFrameBorders *borders);

Window meta_ui_create_frame_window (MetaUI  *ui,
                                    Display *xdisplay,
                                    Visual  *xvisual,
                                    gint     x,
                                    gint     y,
                                    gint     width,
                                    gint     height,
                                    gulong  *create_serial);

void meta_ui_destroy_frame_window (MetaUI *ui,
				   Window  xwindow);
void meta_ui_move_resize_frame (MetaUI *ui,
				Window frame,
				int x,
				int y,
				int width,
				int height);

/* GDK insists on tracking map/unmap */
void meta_ui_map_frame   (MetaUI *ui,
                          Window  xwindow);
void meta_ui_unmap_frame (MetaUI *ui,
                          Window  xwindow);

void meta_ui_apply_frame_shape  (MetaUI  *ui,
                                 Window   xwindow,
                                 int      new_window_width,
                                 int      new_window_height,
                                 gboolean window_has_shape);

cairo_region_t *meta_ui_get_frame_bounds (MetaUI *ui,
                                          Window  xwindow,
                                          int     window_width,
                                          int     window_height);

void meta_ui_queue_frame_draw (MetaUI *ui,
                               Window xwindow);

void meta_ui_set_frame_title (MetaUI *ui,
                              Window xwindow,
                              const char *title);

void meta_ui_update_frame_style (MetaUI *ui,
                                 Window  window);

void meta_ui_repaint_frame (MetaUI *ui,
                            Window xwindow);

MetaWindowMenu* meta_ui_window_menu_new   (MetaUI             *ui,
                                           Window              client_xwindow,
                                           MetaMenuOp          ops,
                                           MetaMenuOp          insensitive,
                                           unsigned long       active_workspace,
                                           int                 n_workspaces,
                                           MetaWindowMenuFunc  func,
                                           gpointer            data);
void            meta_ui_window_menu_popup (MetaWindowMenu     *menu,
                                           const GdkRectangle *rect,
                                           guint32             timestamp);
void            meta_ui_window_menu_free  (MetaWindowMenu     *menu);


GdkPixbuf* meta_gdk_pixbuf_get_from_pixmap (Pixmap       xpixmap,
                                            int          src_x,
                                            int          src_y,
                                            int          width,
                                            int          height);

GdkPixbuf *meta_ui_get_default_window_icon (MetaUI *ui,
                                            int     ideal_size);

GdkPixbuf *meta_ui_get_default_mini_icon   (MetaUI *ui,
                                            int     ideal_size);

gboolean  meta_ui_window_should_not_cause_focus (Display *xdisplay,
                                                 Window   xwindow);

void meta_ui_reload_theme (MetaUI *ui);
void meta_ui_update_button_layout (MetaUI *ui);

/* Not a real key symbol but means "key above the tab key"; this is
 * used as the default keybinding for cycle_group.
 * 0x2xxxxxxx is a range not used by GDK or X. the remaining digits are
 * randomly chosen */
#define META_KEY_ABOVE_TAB 0x2f7259c9

gboolean meta_ui_parse_accelerator (const char          *accel,
                                    unsigned int        *keysym,
                                    unsigned int        *keycode,
                                    MetaVirtualModifier *mask);
gboolean meta_ui_parse_modifier    (const char          *accel,
                                    MetaVirtualModifier *mask);

gboolean meta_ui_window_is_widget (MetaUI *ui,
                                   Window  xwindow);

int      meta_ui_get_drag_threshold       (MetaUI *ui);

MetaUIDirection meta_ui_get_direction (void);

GdkPixbuf *meta_ui_get_pixbuf_from_surface (cairo_surface_t *surface);

#include "tabpopup.h"
#include "tile-preview.h"

#endif
