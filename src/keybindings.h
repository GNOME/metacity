/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity Keybindings */

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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef META_KEYBINDINGS_H
#define META_KEYBINDINGS_H

#include "display.h"
#include "window.h"
#include "devices.h"

void     meta_display_init_keys             (MetaDisplay *display);
void     meta_display_shutdown_keys         (MetaDisplay *display);
void     meta_screen_grab_keys              (MetaScreen  *screen,
					     MetaDevInfo *dev);
void     meta_screen_ungrab_keys            (MetaScreen  *screen, 
					     MetaDevInfo *dev);
#ifdef MPX
gboolean meta_screen_grab_all_keys          (MetaScreen  *screen,
					     MetaDevInfo *dev,
                                             guint32      timestamp);
#else
gboolean meta_screen_grab_all_keys          (MetaScreen  *screen,
                                             guint32      timestamp);
#endif
void     meta_screen_ungrab_all_keys        (MetaScreen  *screen,
					     MetaDevInfo *dev,
                                             guint32      timestamp);
void     meta_window_grab_keys              (MetaWindow  *window,
					     MetaDevInfo *dev);
void     meta_window_ungrab_keys            (MetaWindow  *window,
					     MetaDevInfo *dev);
#ifdef MPX
gboolean meta_window_grab_all_keys          (MetaWindow  *window,
					     MetaDevInfo *dev,
                                             guint32      timestamp);
#else
gboolean meta_window_grab_all_keys          (MetaWindow  *window,
                                             guint32      timestamp);
#endif
void     meta_window_ungrab_all_keys        (MetaWindow  *window,
					     MetaDevInfo *dev,
                                             guint32      timestamp);
void     meta_display_process_key_event     (MetaDisplay     *display,
                                             MetaWindow      *window,
                                             XDeviceKeyEvent *xdke);
void     meta_set_keybindings_disabled      (gboolean     setting);
void     meta_display_process_mapping_event (MetaDisplay *display,
                                             XEvent      *event);

#endif




