/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity utilities */

/*
 * Copyright (C) 2001 Havoc Pennington
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

#ifndef META_UTIL_H
#define META_UTIL_H

#include <glib.h>
#include <glib/gi18n.h>
#include <X11/extensions/Xfixes.h>

G_BEGIN_DECLS

typedef enum
{
  META_DEBUG_FOCUS = 1 << 0,
  META_DEBUG_WORKAREA = 1 << 1,
  META_DEBUG_STACK = 1 << 2,
  META_DEBUG_SM = 1 << 3,
  META_DEBUG_EVENTS = 1 << 4,
  META_DEBUG_WINDOW_STATE = 1 << 5,
  META_DEBUG_WINDOW_OPS = 1 << 6,
  META_DEBUG_GEOMETRY = 1 << 7,
  META_DEBUG_PLACEMENT = 1 << 8,
  META_DEBUG_PING = 1 << 9,
  META_DEBUG_XINERAMA = 1 << 10,
  META_DEBUG_KEYBINDINGS = 1 << 11,
  META_DEBUG_SYNC = 1 << 12,
  META_DEBUG_STARTUP = 1 << 13,
  META_DEBUG_PREFS = 1 << 14,
  META_DEBUG_GROUPS = 1 << 15,
  META_DEBUG_RESIZING = 1 << 16,
  META_DEBUG_SHAPES = 1 << 17,
  META_DEBUG_EDGE_RESISTANCE = 1 << 18,
  META_DEBUG_VERBOSE = 1 << 19,
  META_DEBUG_VULKAN = 1 << 20,
  META_DEBUG_DAMAGE_REGION = 1 << 21
} MetaDebugFlags;

void meta_init_debug (void);
void meta_toggle_debug (void);

gboolean meta_check_debug_flags (MetaDebugFlags flags);

gboolean meta_is_debugging (void);
void     meta_set_debugging (gboolean setting);
gboolean meta_is_syncing (void);
void     meta_set_syncing (gboolean setting);
gboolean meta_get_replace_current_wm (void);
void     meta_set_replace_current_wm (gboolean setting);

void meta_verbose (const char *format,
                   ...) G_GNUC_PRINTF (1, 2);

void meta_topic (MetaDebugFlags  topic,
                 const char     *format,
                 ...) G_GNUC_PRINTF (2, 3);

void meta_push_no_msg_prefix (void);
void meta_pop_no_msg_prefix  (void);

gint  meta_unsigned_long_equal (gconstpointer v1,
                                gconstpointer v2);
guint meta_unsigned_long_hash  (gconstpointer v);

const char* meta_gravity_to_string (int gravity);

char* meta_g_utf8_strndup (const gchar *src, gsize n);

gboolean meta_xserver_region_equal (Display       *xdisplay,
                                    XserverRegion  region1,
                                    XserverRegion  region2);

G_END_DECLS

#endif
