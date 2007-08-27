/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity Keybindings */ 
/* 
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002 Red Hat Inc.
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
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

#include <config.h>
#include "keybindings.h"
#include "workspace.h"
#include "errors.h"
#include "edge-resistance.h"
#include "ui.h"
#include "frame.h"
#include "place.h"
#include "prefs.h"
#include "effects.h"
#include "devices.h"

#include <X11/keysym.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif

static gboolean all_bindings_disabled = FALSE;

typedef void (* MetaKeyHandlerFunc) (MetaDisplay     *display,
                                     MetaScreen      *screen,
                                     MetaWindow      *window,
                                     XDeviceKeyEvent *xdke,
                                     MetaKeyBinding  *binding);

static void handle_activate_workspace (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_activate_menu      (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_tab_forward        (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_tab_backward       (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_cycle_forward      (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_cycle_backward     (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_toggle_fullscreen  (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_toggle_desktop     (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_panel_keybinding   (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_toggle_above       (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_toggle_maximize    (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_maximize           (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_unmaximize         (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_toggle_shade       (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_close_window       (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_minimize_window    (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_begin_move         (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_begin_resize       (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_toggle_sticky      (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_move_to_workspace  (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_move_to_workspace_flip  (MetaDisplay    *display,
                                            MetaScreen     *screen,
                                            MetaWindow     *window,
                                            //XEvent         *event,
                                            XDeviceKeyEvent *xdke,
                                            MetaKeyBinding *binding);
static void handle_workspace_switch   (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_raise_or_lower     (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_raise              (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_lower              (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_run_command        (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_maximize_vert      (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_maximize_horiz     (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_move_to_side_n     (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_move_to_side_w     (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_move_to_side_s     (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_move_to_side_e     (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_move_to_corner_nw  (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_move_to_corner_ne  (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_move_to_corner_sw  (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_move_to_corner_se  (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_spew_mark          (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);
static void handle_run_terminal       (MetaDisplay    *display,
                                       MetaScreen     *screen,
                                       MetaWindow     *window,
                                       //XEvent         *event,
                                       XDeviceKeyEvent *xdke,
                                       MetaKeyBinding *binding);

/* debug */
static gboolean process_mouse_move_resize_grab (MetaDisplay *display,
                                                MetaScreen  *screen,
                                                MetaWindow  *window,
                                                //XEvent      *event,
                                                XDeviceKeyEvent *xdke,
                                                KeySym       keysym);

static gboolean process_keyboard_move_grab (MetaDisplay *display,
                                            MetaScreen  *screen,
                                            MetaWindow  *window,
                                            //XEvent      *event,
                                            XDeviceKeyEvent *xdke,
                                            KeySym       keysym);

static gboolean process_keyboard_resize_grab (MetaDisplay *display,
                                              MetaScreen  *screen,
                                              MetaWindow  *window,
                                              //XEvent      *event,
                                              XDeviceKeyEvent *xdke,
                                              KeySym       keysym);

static gboolean process_tab_grab           (MetaDisplay *display,
                                            MetaScreen  *screen,
                                            //XEvent      *event,
                                            XDeviceKeyEvent *xdke,
                                            KeySym       keysym);

static gboolean process_workspace_switch_grab (MetaDisplay *display,
                                               MetaScreen  *screen,
                                               //XEvent      *event,
                                               XDeviceKeyEvent *xdke,
                                               KeySym       keysym);

static void regrab_screen_bindings         (MetaDisplay *display);
static void regrab_window_bindings         (MetaDisplay *display);

typedef struct
{
  const char *name;
  MetaKeyHandlerFunc func;
  void *data;
} MetaKeyHandler;

struct _MetaKeyBinding
{
  const char *name;
  KeySym keysym;
  KeyCode keycode;
  unsigned int mask;
  MetaVirtualModifier modifiers;
  const MetaKeyHandler *handler;
};

static const MetaKeyHandler screen_handlers[] = {
  { META_KEYBINDING_WORKSPACE_1, handle_activate_workspace,
    GINT_TO_POINTER (0) },
  { META_KEYBINDING_WORKSPACE_2, handle_activate_workspace,
    GINT_TO_POINTER (1) },
  { META_KEYBINDING_WORKSPACE_3, handle_activate_workspace,
    GINT_TO_POINTER (2) },
  { META_KEYBINDING_WORKSPACE_4, handle_activate_workspace,
    GINT_TO_POINTER (3) },
  { META_KEYBINDING_WORKSPACE_5, handle_activate_workspace,
    GINT_TO_POINTER (4) },
  { META_KEYBINDING_WORKSPACE_6, handle_activate_workspace,
    GINT_TO_POINTER (5) },
  { META_KEYBINDING_WORKSPACE_7, handle_activate_workspace,
    GINT_TO_POINTER (6) },
  { META_KEYBINDING_WORKSPACE_8, handle_activate_workspace,
    GINT_TO_POINTER (7) },
  { META_KEYBINDING_WORKSPACE_9, handle_activate_workspace,
    GINT_TO_POINTER (8) },
  { META_KEYBINDING_WORKSPACE_10, handle_activate_workspace,
    GINT_TO_POINTER (9) },
  { META_KEYBINDING_WORKSPACE_11, handle_activate_workspace,
    GINT_TO_POINTER (10) },
  { META_KEYBINDING_WORKSPACE_12, handle_activate_workspace,
    GINT_TO_POINTER (11) },
  { META_KEYBINDING_WORKSPACE_LEFT, handle_workspace_switch,
    GINT_TO_POINTER (META_MOTION_LEFT) },
  { META_KEYBINDING_WORKSPACE_RIGHT, handle_workspace_switch,
    GINT_TO_POINTER (META_MOTION_RIGHT) },
  { META_KEYBINDING_WORKSPACE_UP, handle_workspace_switch,
    GINT_TO_POINTER (META_MOTION_UP) },
  { META_KEYBINDING_WORKSPACE_DOWN, handle_workspace_switch,
    GINT_TO_POINTER (META_MOTION_DOWN) },
  { META_KEYBINDING_SWITCH_WINDOWS, handle_tab_forward,
    GINT_TO_POINTER (META_TAB_LIST_NORMAL) },
  { META_KEYBINDING_SWITCH_WINDOWS_BACKWARD, handle_tab_backward,
    GINT_TO_POINTER (META_TAB_LIST_NORMAL) },
  { META_KEYBINDING_SWITCH_PANELS, handle_tab_forward,
    GINT_TO_POINTER (META_TAB_LIST_DOCKS) },
  { META_KEYBINDING_SWITCH_PANELS_BACKWARD, handle_tab_backward,
    GINT_TO_POINTER (META_TAB_LIST_DOCKS) },
  { META_KEYBINDING_SWITCH_GROUP, handle_tab_forward,
    GINT_TO_POINTER (META_TAB_LIST_GROUP) },
  { META_KEYBINDING_SWITCH_GROUP_BACKWARD, handle_tab_backward,
    GINT_TO_POINTER (META_TAB_LIST_GROUP) },
  { META_KEYBINDING_CYCLE_GROUP, handle_cycle_forward,
    GINT_TO_POINTER (META_TAB_LIST_GROUP) },
  { META_KEYBINDING_CYCLE_GROUP_BACKWARD, handle_cycle_backward,
    GINT_TO_POINTER (META_TAB_LIST_GROUP) },
  { META_KEYBINDING_CYCLE_WINDOWS, handle_cycle_forward,
    GINT_TO_POINTER (META_TAB_LIST_NORMAL) },
  { META_KEYBINDING_CYCLE_WINDOWS_BACKWARD, handle_cycle_backward,
    GINT_TO_POINTER (META_TAB_LIST_NORMAL) },
  { META_KEYBINDING_CYCLE_PANELS, handle_cycle_forward,
    GINT_TO_POINTER (META_TAB_LIST_DOCKS) },  
  { META_KEYBINDING_CYCLE_PANELS_BACKWARD, handle_cycle_backward,
    GINT_TO_POINTER (META_TAB_LIST_DOCKS) },  
  { META_KEYBINDING_SHOW_DESKTOP, handle_toggle_desktop,
    NULL },
  { META_KEYBINDING_PANEL_MAIN_MENU, handle_panel_keybinding,
    GINT_TO_POINTER (META_KEYBINDING_ACTION_PANEL_MAIN_MENU) },
  { META_KEYBINDING_PANEL_RUN_DIALOG, handle_panel_keybinding,
    GINT_TO_POINTER (META_KEYBINDING_ACTION_PANEL_RUN_DIALOG) },
  { META_KEYBINDING_COMMAND_1, handle_run_command,
    GINT_TO_POINTER (0) },
  { META_KEYBINDING_COMMAND_2, handle_run_command,
    GINT_TO_POINTER (1) },
  { META_KEYBINDING_COMMAND_3, handle_run_command,
    GINT_TO_POINTER (2) },
  { META_KEYBINDING_COMMAND_4, handle_run_command,
    GINT_TO_POINTER (3) },
  { META_KEYBINDING_COMMAND_5, handle_run_command,
    GINT_TO_POINTER (4) },
  { META_KEYBINDING_COMMAND_6, handle_run_command,
    GINT_TO_POINTER (5) },
  { META_KEYBINDING_COMMAND_7, handle_run_command,
    GINT_TO_POINTER (6) },
  { META_KEYBINDING_COMMAND_8, handle_run_command,
    GINT_TO_POINTER (7) },
  { META_KEYBINDING_COMMAND_9, handle_run_command,
    GINT_TO_POINTER (8) },
  { META_KEYBINDING_COMMAND_10, handle_run_command,
    GINT_TO_POINTER (9) },
  { META_KEYBINDING_COMMAND_11, handle_run_command,
    GINT_TO_POINTER (10) },
  { META_KEYBINDING_COMMAND_12, handle_run_command,
    GINT_TO_POINTER (11) },
  { META_KEYBINDING_COMMAND_13, handle_run_command,
    GINT_TO_POINTER (12) },
  { META_KEYBINDING_COMMAND_14, handle_run_command,
    GINT_TO_POINTER (13) },
  { META_KEYBINDING_COMMAND_15, handle_run_command,
    GINT_TO_POINTER (14) },
  { META_KEYBINDING_COMMAND_16, handle_run_command,
    GINT_TO_POINTER (15) },
  { META_KEYBINDING_COMMAND_17, handle_run_command,
    GINT_TO_POINTER (16) },
  { META_KEYBINDING_COMMAND_18, handle_run_command,
    GINT_TO_POINTER (17) },
  { META_KEYBINDING_COMMAND_19, handle_run_command,
    GINT_TO_POINTER (18) },
  { META_KEYBINDING_COMMAND_20, handle_run_command,
    GINT_TO_POINTER (19) },
  { META_KEYBINDING_COMMAND_21, handle_run_command,
    GINT_TO_POINTER (20) },
  { META_KEYBINDING_COMMAND_22, handle_run_command,
    GINT_TO_POINTER (21) },
  { META_KEYBINDING_COMMAND_23, handle_run_command,
    GINT_TO_POINTER (22) },
  { META_KEYBINDING_COMMAND_24, handle_run_command,
    GINT_TO_POINTER (23) },
  { META_KEYBINDING_COMMAND_25, handle_run_command,
    GINT_TO_POINTER (24) },
  { META_KEYBINDING_COMMAND_26, handle_run_command,
    GINT_TO_POINTER (25) },
  { META_KEYBINDING_COMMAND_27, handle_run_command,
    GINT_TO_POINTER (26) },
  { META_KEYBINDING_COMMAND_28, handle_run_command,
    GINT_TO_POINTER (27) },
  { META_KEYBINDING_COMMAND_29, handle_run_command,
    GINT_TO_POINTER (28) },
  { META_KEYBINDING_COMMAND_30, handle_run_command,
    GINT_TO_POINTER (29) },
  { META_KEYBINDING_COMMAND_31, handle_run_command,
    GINT_TO_POINTER (30) },
  { META_KEYBINDING_COMMAND_32, handle_run_command,
    GINT_TO_POINTER (31) },
  { META_KEYBINDING_COMMAND_SCREENSHOT, handle_run_command,
    GINT_TO_POINTER (32) },
  { META_KEYBINDING_COMMAND_WIN_SCREENSHOT, handle_run_command,
    GINT_TO_POINTER (33) },
  { META_KEYBINDING_RUN_COMMAND_TERMINAL, handle_run_terminal,
    NULL },
  { META_KEYBINDING_SET_SPEW_MARK, handle_spew_mark, NULL },
  { NULL, NULL, NULL }
};
  
static const MetaKeyHandler window_handlers[] = {
  { META_KEYBINDING_WINDOW_MENU, handle_activate_menu, NULL },
  { META_KEYBINDING_TOGGLE_FULLSCREEN, handle_toggle_fullscreen, NULL },
  { META_KEYBINDING_TOGGLE_ABOVE, handle_toggle_above, NULL },
  { META_KEYBINDING_TOGGLE_MAXIMIZE, handle_toggle_maximize, NULL },
  { META_KEYBINDING_MAXIMIZE, handle_maximize, NULL },
  { META_KEYBINDING_UNMAXIMIZE, handle_unmaximize, NULL },
  { META_KEYBINDING_TOGGLE_SHADE, handle_toggle_shade, NULL },
  { META_KEYBINDING_CLOSE, handle_close_window, NULL },
  { META_KEYBINDING_MINIMIZE, handle_minimize_window, NULL },
  { META_KEYBINDING_BEGIN_MOVE, handle_begin_move, },
  { META_KEYBINDING_BEGIN_RESIZE, handle_begin_resize, },
  { META_KEYBINDING_TOGGLE_STICKY, handle_toggle_sticky, },
  { META_KEYBINDING_MOVE_WORKSPACE_1, handle_move_to_workspace,
    GINT_TO_POINTER (0) },
  { META_KEYBINDING_MOVE_WORKSPACE_2, handle_move_to_workspace,
    GINT_TO_POINTER (1) },
  { META_KEYBINDING_MOVE_WORKSPACE_3, handle_move_to_workspace,
    GINT_TO_POINTER (2) },
  { META_KEYBINDING_MOVE_WORKSPACE_4, handle_move_to_workspace,
    GINT_TO_POINTER (3) },
  { META_KEYBINDING_MOVE_WORKSPACE_5, handle_move_to_workspace,
    GINT_TO_POINTER (4) },
  { META_KEYBINDING_MOVE_WORKSPACE_6, handle_move_to_workspace,
    GINT_TO_POINTER (5) },
  { META_KEYBINDING_MOVE_WORKSPACE_7, handle_move_to_workspace,
    GINT_TO_POINTER (6) },
  { META_KEYBINDING_MOVE_WORKSPACE_8, handle_move_to_workspace,
    GINT_TO_POINTER (7) },
  { META_KEYBINDING_MOVE_WORKSPACE_9, handle_move_to_workspace,
    GINT_TO_POINTER (8) },
  { META_KEYBINDING_MOVE_WORKSPACE_10, handle_move_to_workspace,
    GINT_TO_POINTER (9) },
  { META_KEYBINDING_MOVE_WORKSPACE_11, handle_move_to_workspace,
    GINT_TO_POINTER (10) },
  { META_KEYBINDING_MOVE_WORKSPACE_12, handle_move_to_workspace,
    GINT_TO_POINTER (11) },
  { META_KEYBINDING_MOVE_WORKSPACE_LEFT, handle_move_to_workspace_flip,
    GINT_TO_POINTER (META_MOTION_LEFT) },
  { META_KEYBINDING_MOVE_WORKSPACE_RIGHT, handle_move_to_workspace_flip,
    GINT_TO_POINTER (META_MOTION_RIGHT) },
  { META_KEYBINDING_MOVE_WORKSPACE_UP, handle_move_to_workspace_flip,
    GINT_TO_POINTER (META_MOTION_UP) },
  { META_KEYBINDING_MOVE_WORKSPACE_DOWN, handle_move_to_workspace_flip,
    GINT_TO_POINTER (META_MOTION_DOWN) },
  { META_KEYBINDING_RAISE_OR_LOWER, handle_raise_or_lower, NULL},
  { META_KEYBINDING_RAISE, handle_raise, NULL},
  { META_KEYBINDING_LOWER, handle_lower, NULL},
  { META_KEYBINDING_MAXIMIZE_VERTICALLY, handle_maximize_vert, NULL },
  { META_KEYBINDING_MAXIMIZE_HORIZONTALLY, handle_maximize_horiz, NULL },
  { META_KEYBINDING_MOVE_TO_SIDE_N, handle_move_to_side_n, NULL },
  { META_KEYBINDING_MOVE_TO_SIDE_S, handle_move_to_side_s, NULL },
  { META_KEYBINDING_MOVE_TO_SIDE_E, handle_move_to_side_e, NULL },
  { META_KEYBINDING_MOVE_TO_SIDE_W, handle_move_to_side_w, NULL },
  { META_KEYBINDING_MOVE_TO_CORNER_NW, handle_move_to_corner_nw, NULL },
  { META_KEYBINDING_MOVE_TO_CORNER_NE, handle_move_to_corner_ne, NULL },
  { META_KEYBINDING_MOVE_TO_CORNER_SW, handle_move_to_corner_sw, NULL },
  { META_KEYBINDING_MOVE_TO_CORNER_SE, handle_move_to_corner_se, NULL },
  { NULL, NULL, NULL }
};

static void
reload_keymap (MetaDisplay *display)
{
  if (display->keymap)
    meta_XFree (display->keymap);

  /* XXX USE XI!!!!! (when it implements these functions...) */
  display->keymap = XGetKeyboardMapping (display->xdisplay,
                                         display->min_keycode,
                                         display->max_keycode -
                                         display->min_keycode + 1,
                                         &display->keysyms_per_keycode);  
}

static void
reload_modmap (MetaDisplay *display)
{
  XModifierKeymap *modmap;
  int map_size;
  int i;
  
  /* XXX USE XI!!!!! (when it implements these functions...) */
  if (display->modmap)
    XFreeModifiermap (display->modmap);

  modmap = XGetModifierMapping (display->xdisplay);
  display->modmap = modmap;

  display->ignored_modifier_mask = 0;

  /* Multiple bits may get set in each of these */
  display->num_lock_mask = 0;
  display->scroll_lock_mask = 0;
  display->meta_mask = 0;
  display->hyper_mask = 0;
  display->super_mask = 0;
  
  /* there are 8 modifiers, and the first 3 are shift, shift lock,
   * and control
   */
  map_size = 8 * modmap->max_keypermod;
  i = 3 * modmap->max_keypermod;
  while (i < map_size)
    {
      /* get the key code at this point in the map,
       * see if its keysym is one we're interested in
       */
      int keycode = modmap->modifiermap[i];
      
      if (keycode >= display->min_keycode &&
          keycode <= display->max_keycode)
        {
          int j = 0;
          KeySym *syms = display->keymap +
            (keycode - display->min_keycode) * display->keysyms_per_keycode;

          while (j < display->keysyms_per_keycode)
            {
              if (syms[j] != 0)
                {
                  const char *str;
                  
                  str = XKeysymToString (syms[j]);
                  meta_topic (META_DEBUG_KEYBINDINGS,
                              "Keysym %s bound to modifier 0x%x\n",
                              str ? str : "none",
                              (1 << ( i / modmap->max_keypermod)));
                }
              
              if (syms[j] == XK_Num_Lock)
                {
                  /* Mod1Mask is 1 << 3 for example, i.e. the
                   * fourth modifier, i / keyspermod is the modifier
                   * index
                   */
                  
                  display->num_lock_mask |= (1 << ( i / modmap->max_keypermod));
                }
              else if (syms[j] == XK_Scroll_Lock)
                {
                  display->scroll_lock_mask |= (1 << ( i / modmap->max_keypermod));
                }
              else if (syms[j] == XK_Super_L ||
                       syms[j] == XK_Super_R)
                {
                  display->super_mask |= (1 << ( i / modmap->max_keypermod));
                }
              else if (syms[j] == XK_Hyper_L ||
                       syms[j] == XK_Hyper_R)
                {
                  display->hyper_mask |= (1 << ( i / modmap->max_keypermod));
                }              
              else if (syms[j] == XK_Meta_L ||
                       syms[j] == XK_Meta_R)
                {
                  display->meta_mask |= (1 << ( i / modmap->max_keypermod));
                }
              
              ++j;
            }
        }
      
      ++i;
    }

  display->ignored_modifier_mask = (display->num_lock_mask |
                                    display->scroll_lock_mask |
                                    LockMask);

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Ignoring modmask 0x%x num lock 0x%x scroll lock 0x%x hyper 0x%x super 0x%x meta 0x%x\n",
              display->ignored_modifier_mask,
              display->num_lock_mask,
              display->scroll_lock_mask,
              display->hyper_mask,
              display->super_mask,
              display->meta_mask);
}

static void
reload_keycodes (MetaDisplay *display)
{
  meta_topic (META_DEBUG_KEYBINDINGS,
              "Reloading keycodes for binding tables\n");
  
  if (display->screen_bindings)
    {
      int i;
      
      i = 0;
      while (i < display->n_screen_bindings)
        {
          if (display->screen_bindings[i].keycode == 0)
              display->screen_bindings[i].keycode = XKeysymToKeycode (
                      display->xdisplay, display->screen_bindings[i].keysym);
          
          ++i;
        }
    }

  if (display->window_bindings)
    {
      int i;
      
      i = 0;
      while (i < display->n_window_bindings)
        {
          if (display->window_bindings[i].keycode == 0)
              display->window_bindings[i].keycode = XKeysymToKeycode (
                      display->xdisplay, display->window_bindings[i].keysym);
          
          ++i;
        }
    }
}

static void
reload_modifiers (MetaDisplay *display)
{
  meta_topic (META_DEBUG_KEYBINDINGS,
              "Reloading keycodes for binding tables\n");
  
  if (display->screen_bindings)
    {
      int i;
      
      i = 0;
      while (i < display->n_screen_bindings)
        {
          meta_display_devirtualize_modifiers (display,
                                               display->screen_bindings[i].modifiers,
                                               &display->screen_bindings[i].mask);

          meta_topic (META_DEBUG_KEYBINDINGS,
                      " Devirtualized mods 0x%x -> 0x%x (%s)\n",
                      display->screen_bindings[i].modifiers,
                      display->screen_bindings[i].mask,
                      display->screen_bindings[i].name);          
          
          ++i;
        }
    }

  if (display->window_bindings)
    {
      int i;
      
      i = 0;
      while (i < display->n_window_bindings)
        {
          meta_display_devirtualize_modifiers (display,
                                               display->window_bindings[i].modifiers,
                                               &display->window_bindings[i].mask);

          meta_topic (META_DEBUG_KEYBINDINGS,
                      " Devirtualized mods 0x%x -> 0x%x (%s)\n",
                      display->window_bindings[i].modifiers,
                      display->window_bindings[i].mask,
                      display->window_bindings[i].name);
          
          ++i;
        }
    }
}

static int
count_bindings (const MetaKeyPref *prefs,
                int                n_prefs)
{
  int i;
  int count;

  count = 0;
  i = 0;
  while (i < n_prefs)
    {
      GSList *tmp = prefs[i].bindings;

      while (tmp)
        {
          MetaKeyCombo *combo = tmp->data;

          if (combo && (combo->keysym != None || combo->keycode != 0))
            {
              count += 1;

              if (prefs[i].add_shift &&
                  (combo->modifiers & META_VIRTUAL_SHIFT_MASK) == 0)
                count += 1;
            }

          tmp = tmp->next;
        }
      
      ++i;
    }

  return count;
}

static void
rebuild_binding_table (MetaDisplay        *display,
                       MetaKeyBinding    **bindings_p,
                       int                *n_bindings_p,
                       const MetaKeyPref  *prefs,
                       int                 n_prefs)
{
  int n_bindings;
  int src, dest;
  
  n_bindings = count_bindings (prefs, n_prefs);
  g_free (*bindings_p);
  *bindings_p = g_new0 (MetaKeyBinding, n_bindings);

  src = 0;
  dest = 0;
  while (src < n_prefs)
    {
      GSList *tmp = prefs[src].bindings;

      while (tmp)
        {
          MetaKeyCombo *combo = tmp->data;

          if (combo && (combo->keysym != None || combo->keycode != 0))
            {
              (*bindings_p)[dest].name = prefs[src].name;
              (*bindings_p)[dest].keysym = combo->keysym;
              (*bindings_p)[dest].keycode = combo->keycode;
              (*bindings_p)[dest].modifiers = combo->modifiers;
              (*bindings_p)[dest].mask = 0;
          
              ++dest;

              if (prefs[src].add_shift &&
                  (combo->modifiers & META_VIRTUAL_SHIFT_MASK) == 0)
                {
                  meta_topic (META_DEBUG_KEYBINDINGS,
                              "Binding %s also needs Shift grabbed\n",
                               prefs[src].name);
              
                  (*bindings_p)[dest].name = prefs[src].name;
                  (*bindings_p)[dest].keysym = combo->keysym;
                  (*bindings_p)[dest].keycode = combo->keycode;
                  (*bindings_p)[dest].modifiers = combo->modifiers |
                    META_VIRTUAL_SHIFT_MASK;
                  (*bindings_p)[dest].mask = 0;
              
                  ++dest;
                }
            }
            
          tmp = tmp->next;
        }
      
      ++src;
    }

  g_assert (dest == n_bindings);
  
  *n_bindings_p = dest;

  meta_topic (META_DEBUG_KEYBINDINGS,
              " %d bindings in table\n",
              *n_bindings_p);
}

static void
rebuild_screen_binding_table (MetaDisplay *display)
{
  const MetaKeyPref *prefs;
  int n_prefs;
  
  meta_topic (META_DEBUG_KEYBINDINGS,
              "Rebuilding screen binding table from preferences\n");
  
  meta_prefs_get_screen_bindings (&prefs, &n_prefs);
  rebuild_binding_table (display,
                         &display->screen_bindings,
                         &display->n_screen_bindings,
                         prefs, n_prefs);
}

static void
rebuild_window_binding_table (MetaDisplay *display)
{
  const MetaKeyPref *prefs;
  int n_prefs;
  
  meta_topic (META_DEBUG_KEYBINDINGS,
              "Rebuilding window binding table from preferences\n");
  
  meta_prefs_get_window_bindings (&prefs, &n_prefs);
  rebuild_binding_table (display,
                         &display->window_bindings,
                         &display->n_window_bindings,
                         prefs, n_prefs);
}

static void
regrab_screen_bindings (MetaDisplay *display)
{
  GSList *tmp;
  int idev;

  meta_error_trap_push (display); /* for efficiency push outer trap */
  
  tmp = display->screens;
  while (tmp != NULL)
    {
      MetaScreen *screen = tmp->data;
      for (idev = 0; idev < display->devices->keybsUsed; idev++)
        {
	  meta_screen_ungrab_keys (screen, &display->devices->keyboards[idev]);
          meta_screen_grab_keys (screen, &display->devices->keyboards[idev]);
	}

      tmp = tmp->next;
    }

  meta_error_trap_pop (display, FALSE);
}

static void
regrab_window_bindings (MetaDisplay *display)
{
  GSList *windows;
  GSList *tmp;
  int idev;

  windows = meta_display_list_windows (display);

  meta_error_trap_push (display); /* for efficiency push outer trap */
  tmp = windows;
  while (tmp != NULL)
    {
      MetaWindow *w = tmp->data;
      
      for (idev = 0; idev < display->devices->keybsUsed; idev++)
        {
	  meta_window_ungrab_keys (w, &display->devices->keyboards[idev]);
          meta_window_grab_keys (w, &display->devices->keyboards[idev]);
	}
      
      tmp = tmp->next;
    }
  meta_error_trap_pop (display, FALSE);

  g_slist_free (windows);
}

static MetaKeyBindingAction
display_get_keybinding_action (MetaDisplay  *display,
                               unsigned int  keysym,
                               unsigned int  keycode,
                               unsigned long mask)
{
  int i;

  i = display->n_screen_bindings - 1;
  while (i >= 0)
    {
      if (display->screen_bindings[i].keysym == keysym &&
          display->screen_bindings[i].keycode == keycode &&
          display->screen_bindings[i].mask == mask)
        {
          return meta_prefs_get_keybinding_action (display->screen_bindings[i].name);
        }
      
      --i;
    }

  return META_KEYBINDING_ACTION_NONE;
}

void
meta_display_process_mapping_event (MetaDisplay *display,
                                    XEvent      *event)
{ 
  /* XXX Is this Xi'able? */
  if (event->xmapping.request == MappingModifier)
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Received MappingModifier event, will reload modmap and redo keybindings\n");

      reload_modmap (display);

      reload_modifiers (display);
      
      regrab_screen_bindings (display);
      regrab_window_bindings (display);
    }
  else if (event->xmapping.request == MappingKeyboard)
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Received MappingKeyboard event, will reload keycodes and redo keybindings\n");

      reload_keymap (display);
      reload_modmap (display);
      
      reload_keycodes (display);

      regrab_screen_bindings (display);
      regrab_window_bindings (display);
    }
}

static void
bindings_changed_callback (MetaPreference pref,
                           void          *data)
{
  MetaDisplay *display;

  display = data;
  
  switch (pref)
    {
    case META_PREF_SCREEN_KEYBINDINGS:
      rebuild_screen_binding_table (display);
      reload_keycodes (display);
      reload_modifiers (display);
      regrab_screen_bindings (display);
      break;
    case META_PREF_WINDOW_KEYBINDINGS:
      rebuild_window_binding_table (display);
      reload_keycodes (display);
      reload_modifiers (display);
      regrab_window_bindings (display);
      break;
    default:
      break;
    }
}


void
meta_display_init_keys (MetaDisplay *display)
{
  /* Keybindings */
  display->keymap = NULL;
  display->keysyms_per_keycode = 0;
  display->modmap = NULL;
  display->min_keycode = 0;
  display->max_keycode = 0;
  display->ignored_modifier_mask = 0;
  display->num_lock_mask = 0;
  display->scroll_lock_mask = 0;
  display->hyper_mask = 0;
  display->super_mask = 0;
  display->meta_mask = 0;
  display->screen_bindings = NULL;
  display->n_screen_bindings = 0;
  display->window_bindings = NULL;
  display->n_window_bindings = 0;

  XDisplayKeycodes (display->xdisplay,
                    &display->min_keycode,
                    &display->max_keycode);

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Display has keycode range %d to %d\n",
              display->min_keycode,
              display->max_keycode);

  reload_keymap (display);
  reload_modmap (display);

  rebuild_window_binding_table (display);
  rebuild_screen_binding_table (display);

  reload_keycodes (display);
  reload_modifiers (display);
  
  /* Keys are actually grabbed in meta_screen_grab_keys() */
  
  meta_prefs_add_listener (bindings_changed_callback, display);
}

void
meta_display_shutdown_keys (MetaDisplay *display)
{
  /* Note that display->xdisplay is invalid in this function */
  
  meta_prefs_remove_listener (bindings_changed_callback, display);

  if (display->keymap)
    meta_XFree (display->keymap);
  
  if (display->modmap)
    XFreeModifiermap (display->modmap);
  g_free (display->screen_bindings);
  g_free (display->window_bindings);
}

static const char*
keysym_name (int keysym)
{
  const char *name;
  
  name = XKeysymToString (keysym);
  if (name == NULL)
    name = "(unknown)";

  return name;
}

/* Grab/ungrab, ignoring all annoying modifiers like NumLock etc. */
static void
meta_change_keygrab (MetaDisplay *display,
		     MetaDevInfo *dev,
                     Window       xwindow,
                     gboolean     grab,
                     int          keysym,
                     unsigned int keycode,
                     int          modmask)
{
  unsigned int ignored_mask;

  /* Grab keycode/modmask, together with
   * all combinations of ignored modifiers.
   * X provides no better way to do this.
   */

  meta_topic (META_DEBUG_KEYBINDINGS,
              "%s keybinding %s keycode %d mask 0x%x on 0x%lx\n",
              grab ? "Grabbing" : "Ungrabbing",
              keysym_name (keysym), keycode,
              modmask, xwindow);

  /* efficiency, avoid so many XSync() */
  meta_error_trap_push (display);
  
  ignored_mask = 0;
  while (ignored_mask <= display->ignored_modifier_mask)
    {
      if (ignored_mask & ~(display->ignored_modifier_mask))
        {
          /* Not a combination of ignored modifiers
           * (it contains some non-ignored modifiers)
           */
          ++ignored_mask;
          continue;
        }

      if (meta_is_debugging ())
        meta_error_trap_push_with_return (display);
      if (grab)
        XGrabDeviceKey (display->xdisplay,
	                dev->xdev,
			keycode,
                        modmask | ignored_mask,
			dev->xdev,
                        xwindow,
                        True,
			0,
			NULL,
                        GrabModeAsync, GrabModeSync);
      else
        XUngrabDeviceKey (display->xdisplay, dev->xdev, keycode,
                          modmask | ignored_mask,
                          dev->xdev, xwindow);

      if (meta_is_debugging ())
        {
          int result;
          
          result = meta_error_trap_pop_with_return (display, FALSE);
          
          if (grab && result != Success)
            {      
              if (result == BadAccess)
                meta_warning (_("Some other program is already using the key %s with modifiers %x as a binding\n"), keysym_name (keysym), modmask | ignored_mask);
              else
                meta_topic (META_DEBUG_KEYBINDINGS,
                            "Failed to grab key %s with modifiers %x\n",
                            keysym_name (keysym), modmask | ignored_mask);
            }
        }

      ++ignored_mask;
    }

  meta_error_trap_pop (display, FALSE);
}

static void
meta_grab_key (MetaDisplay *display,
	       MetaDevInfo *dev,
               Window       xwindow,
               int          keysym,
               unsigned int keycode,
               int          modmask)
{
  meta_change_keygrab (display, dev, xwindow, TRUE, keysym, keycode, modmask);
}

static void
grab_keys (MetaDevInfo    *dev,
           MetaKeyBinding *bindings,
           int             n_bindings,
           MetaDisplay    *display,
           Window          xwindow)
{
  int i;

  g_assert (n_bindings == 0 || bindings != NULL);

  meta_error_trap_push (display);
  
  i = 0;
  while (i < n_bindings)
    {
      if (bindings[i].keycode != 0)
        {
          meta_grab_key (display, dev, xwindow,
                         bindings[i].keysym,
                         bindings[i].keycode,
                         bindings[i].mask);
        }
      
      ++i;
    }

  meta_error_trap_pop (display, FALSE);
}

static void
ungrab_all_keys (MetaDisplay *display,
		 MetaDevInfo *dev,
                 Window       xwindow)
{
  if (meta_is_debugging ())
    meta_error_trap_push_with_return (display);
  else
    meta_error_trap_push (display);

  XUngrabDeviceKey (display->xdisplay, dev->xdev, AnyKey, AnyModifier,
                    dev->xdev, xwindow);

  if (meta_is_debugging ())
    {
      int result;
      
      result = meta_error_trap_pop_with_return (display, FALSE);
      
      if (result != Success)    
        meta_topic (META_DEBUG_KEYBINDINGS,
                    "Ungrabbing all keys on 0x%lx failed\n", xwindow);
    }
  else
    meta_error_trap_pop (display, FALSE);
}

void
meta_screen_grab_keys (MetaScreen *screen, MetaDevInfo *dev)
{
  if (meta_devices_list_is_member (&screen->all_keys_grabbed, dev))
    return;

  if (meta_devices_list_is_member (&screen->keys_grabbed, dev))
    return;

  grab_keys (dev,
             screen->display->screen_bindings,
             screen->display->n_screen_bindings,
             screen->display, screen->xroot);

  meta_devices_list_add (&screen->keys_grabbed, dev);
}

void
meta_screen_ungrab_keys (MetaScreen *screen, MetaDevInfo *dev)
{
  if (meta_devices_list_is_member (&screen->keys_grabbed, dev))
    {
      ungrab_all_keys (screen->display, dev, screen->xroot);
      meta_devices_list_remove (&screen->keys_grabbed, dev);
    }
}

void
meta_window_grab_keys (MetaWindow  *window, MetaDevInfo *dev)
{
  if (meta_devices_list_is_member(&window->all_keys_grabbed, dev))
    return;

  if (window->type == META_WINDOW_DOCK)
    {
      if (meta_devices_list_is_member(&window->keys_grabbed, dev))
        ungrab_all_keys (window->display, dev, window->xwindow);
      meta_devices_list_remove (&window->keys_grabbed, dev);
      return;
    }
  
  if (meta_devices_list_is_member (&window->keys_grabbed, dev))
    {
      if (window->frame && !window->grab_on_frame)
        ungrab_all_keys (window->display, dev, window->xwindow);
      else if (window->frame == NULL &&
               window->grab_on_frame)
        ; /* continue to regrab on client window */
      else
        return; /* already all good */
    }
  
  grab_keys (dev,
             window->display->window_bindings,
             window->display->n_window_bindings,
             window->display,
             window->frame ? window->frame->xwindow : window->xwindow);

  meta_devices_list_add (&window->keys_grabbed, dev);
  window->grab_on_frame = window->frame != NULL;
}

void
meta_window_ungrab_keys (MetaWindow *window, MetaDevInfo *dev)
{
  if (meta_devices_list_is_member (&window->keys_grabbed, dev))
    {
      if (window->grab_on_frame &&
          window->frame != NULL)
        ungrab_all_keys (window->display, dev,
                         window->frame->xwindow);
      else if (!window->grab_on_frame)
        ungrab_all_keys (window->display, dev,
                         window->xwindow);

      meta_devices_list_remove (&window->keys_grabbed, dev);
    }
}

#ifdef WITH_VERBOSE_MODE
static const char*
grab_status_to_string (int status)
{
  switch (status)
    {
    case AlreadyGrabbed:
      return "AlreadyGrabbed";
    case GrabSuccess:
      return "GrabSuccess";
    case GrabNotViewable:
      return "GrabNotViewable";
    case GrabFrozen:
      return "GrabFrozen";
    case GrabInvalidTime:
      return "GrabInvalidTime";
    default:
      return "(unknown)";
    }
}
#endif /* WITH_VERBOSE_MODE */

static gboolean
#ifdef MPX
grab_keyboard (MetaDisplay *display,
	       MetaDevInfo *dev,
               Window       xwindow,
               guint32      timestamp)
#else
grab_keyboard (MetaDisplay *display,
               Window       xwindow,
               guint32      timestamp)
#endif
{
  int result;
  int grab_status;
  
  /* Grab the keyboard, so we get key releases and all key
   * presses
   */
  meta_error_trap_push_with_return (display);

#ifdef MPX
  /* XXX No event classes?? */
  grab_status = XGrabDevice (display->xdisplay,
  			     dev->xdev,
			     xwindow,
			     True,
			     0,
			     NULL,
			     GrabModeAsync,
			     GrabModeAsync,
			     timestamp);
//  meta_warning("grabbed device %s with no evclasses! keybindings.c:1362\n",
//  	       dev->name);
#endif
  
  if (grab_status != GrabSuccess)
    {
      meta_error_trap_pop_with_return (display, TRUE);
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "XGrabDevice() returned failure status %s time %u\n",
                  grab_status_to_string (grab_status),
                  timestamp);
      return FALSE;
    }
  else
    {
      result = meta_error_trap_pop_with_return (display, TRUE);
      if (result != Success)
        {
          meta_topic (META_DEBUG_KEYBINDINGS,
                      "XGrabDevice() resulted in an error\n");
          return FALSE;
        }
    }
       
  meta_topic (META_DEBUG_KEYBINDINGS, "Grabbed all keys\n");
       
  return TRUE;
}

static void
ungrab_keyboard (MetaDisplay *display, 
		 MetaDevInfo *dev,
		 guint32 timestamp)
{
  meta_error_trap_push (display);

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Ungrabbing keyboard with timestamp %u\n",
              timestamp);
  XUngrabDevice (display->xdisplay, dev->xdev, timestamp);
  meta_error_trap_pop (display, FALSE);
}

gboolean
meta_screen_grab_all_keys (MetaScreen *screen, MetaDevInfo *dev,
			   guint32 timestamp)
{
  gboolean retval;

  if (meta_devices_list_is_member(&screen->all_keys_grabbed, dev))
    return FALSE;
  
  if (meta_devices_list_is_member(&screen->keys_grabbed, dev))
    meta_screen_ungrab_keys (screen, dev);

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Grabbing all keys on RootWindow for device %s\n", dev->name);
  retval = grab_keyboard (screen->display, dev, screen->xroot, timestamp);
  if (retval)
    meta_devices_list_add (&screen->all_keys_grabbed, dev);
  else
    meta_screen_grab_keys (screen, dev);

  return retval;
}

void
meta_screen_ungrab_all_keys (MetaScreen *screen, 
			     MetaDevInfo *dev,
			     guint32 timestamp)
{
  if (meta_devices_list_is_member (&screen->all_keys_grabbed, dev))
    {
      ungrab_keyboard (screen->display, dev, timestamp);

      meta_devices_list_remove (&screen->all_keys_grabbed, dev);
      meta_devices_list_remove (&screen->keys_grabbed, dev);

      /* Re-establish our standard bindings */
      meta_screen_grab_keys (screen, dev);
    }
}

gboolean
meta_window_grab_all_keys (MetaWindow  *window,
			   MetaDevInfo *dev,
                           guint32      timestamp)
{
  Window grabwindow;
  gboolean retval;
  
  if (meta_devices_list_is_member (&window->all_keys_grabbed, dev))
    return FALSE;
  
  if (meta_devices_list_is_member (&window->keys_grabbed, dev))
    meta_window_ungrab_keys (window, dev);

  /* Make sure the window is focused, otherwise the grab
   * won't do a lot of good.
   */
  meta_topic (META_DEBUG_FOCUS,
              "Focusing %s because we're grabbing all its keys\n",
              window->desc);
  meta_window_focus (window, dev, timestamp);
  
  grabwindow = window->frame ? window->frame->xwindow : window->xwindow;

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Grabbing all keys on window %s\n", window->desc);
  retval = grab_keyboard (window->display, dev, grabwindow, timestamp);
  if (retval)
    {
      meta_devices_list_remove (&window->keys_grabbed, dev);
      meta_devices_list_add (&window->all_keys_grabbed, dev);
      window->grab_on_frame = window->frame != NULL;
    }

  return retval;
}

void
meta_window_ungrab_all_keys (MetaWindow  *window,
			     MetaDevInfo *dev,
			     guint32      timestamp)
{


  if (meta_devices_list_is_member (&window->all_keys_grabbed, dev))
    {
      ungrab_keyboard (window->display, dev, timestamp);

      window->grab_on_frame = FALSE;
      meta_devices_list_remove (&window->all_keys_grabbed, dev);
      meta_devices_list_remove (&window->keys_grabbed, dev);

      /* Re-establish our standard bindings */
      meta_window_grab_keys (window, dev);
    }
}

static gboolean 
is_modifier (MetaDisplay *display,
             unsigned int keycode)
{
  int i;
  int map_size;
  gboolean retval = FALSE;  

  g_assert (display->modmap);
  
  map_size = 8 * display->modmap->max_keypermod;
  i = 0;
  while (i < map_size)
    {
      if (keycode == display->modmap->modifiermap[i])
        {
          retval = TRUE;
          break;
        }
      ++i;
    }
  
  return retval;
}

/* Indexes:
 * shift = 0
 * lock = 1
 * control = 2
 * mod1 = 3
 * mod2 = 4
 * mod3 = 5
 * mod4 = 6
 * mod5 = 7
 */

static gboolean 
is_specific_modifier (MetaDisplay *display,
                      unsigned int keycode,
                      unsigned int mask)
{
  int i;
  int end;
  gboolean retval = FALSE;
  int mod_index;
  
  g_assert (display->modmap);

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Checking whether code 0x%x is bound to modifier 0x%x\n",
              keycode, mask);
  
  mod_index = 0;
  mask = mask >> 1;
  while (mask != 0)
    {
      mod_index += 1;
      mask = mask >> 1;
    }

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Modifier has index %d\n", mod_index);
  
  end = (mod_index + 1) * display->modmap->max_keypermod;
  i = mod_index * display->modmap->max_keypermod;
  while (i < end)
    {
      if (keycode == display->modmap->modifiermap[i])
        {
          retval = TRUE;
          break;
        }
      ++i;
    }
  
  return retval;
}

static unsigned int
get_primary_modifier (MetaDisplay *display,
                      unsigned int entire_binding_mask)
{
  /* The idea here is to see if the "main" modifier
   * for Alt+Tab has been pressed/released. So if the binding
   * is Alt+Shift+Tab then releasing Alt is the thing that
   * ends the operation. It's pretty random how we order
   * these.
   */
  unsigned int masks[] = { Mod5Mask, Mod4Mask, Mod3Mask,
                           Mod2Mask, Mod1Mask, ControlMask,
                           ShiftMask, LockMask };

  int i;
  
  i = 0;
  while (i < (int) G_N_ELEMENTS (masks))
    {
      if (entire_binding_mask & masks[i])
        return masks[i];
      ++i;
    }

  return 0;
}

static gboolean
keycode_is_primary_modifier (MetaDisplay *display,
                             unsigned int keycode,
                             unsigned int entire_binding_mask)
{
  unsigned int primary_modifier;

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Checking whether code 0x%x is the primary modifier of mask 0x%x\n",
              keycode, entire_binding_mask);
  
  primary_modifier = get_primary_modifier (display, entire_binding_mask);
  if (primary_modifier != 0)
    return is_specific_modifier (display, keycode, primary_modifier);
  else
    return FALSE;
}

static gboolean
primary_modifier_still_pressed (MetaDisplay *display,
				MetaDevInfo *ptrDev,
                                unsigned int entire_binding_mask)
{
  unsigned int primary_modifier;
  int x, y, root_x, root_y;
  Window root, child;
  guint mask;
  MetaScreen  *random_screen;
  Window       random_xwindow;
  Bool         shared;
//  MetaDevInfo *pairedPointer;
  
  primary_modifier = get_primary_modifier (display, entire_binding_mask);
  
  random_screen = display->screens->data;
  random_xwindow = random_screen->no_focus_window;

//  pairedPointer = meta_devices_find_paired_mouse(display, dev->xdev->device_id);

  XQueryDevicePointer (display->xdisplay,
  		       ptrDev->xdev,
                       random_xwindow, /* some random window */
                       &root, &child,
                       &root_x, &root_y,
                       &x, &y,
                       &mask, &shared);
 

#if 0
#warning XQueryPointer
  XQueryPointer (display->xdisplay,
                 random_xwindow, /* some random window */
                 &root, &child,
                 &root_x, &root_y,
                 &x, &y,
                 &mask);
#endif

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Primary modifier 0x%x full grab mask 0x%x current state 0x%x\n",
              primary_modifier, entire_binding_mask, mask);
  
  if ((mask & primary_modifier) == 0)
    return FALSE;
  else
    return TRUE;
}

static const MetaKeyHandler*
find_handler (const MetaKeyHandler *handlers,
              const char           *name)
{
  const MetaKeyHandler *iter;

  iter = handlers;
  while (iter->name)
    {
      if (strcmp (iter->name,
                  name) == 0)
        return iter;

      ++iter;
    }

  return NULL;
}               

static void
process_event (MetaKeyBinding       *bindings,
               int                   n_bindings,
               const MetaKeyHandler *handlers,
               MetaDisplay          *display,
               MetaScreen           *screen,
               MetaWindow           *window,
               XDeviceKeyEvent      *xdke,
               KeySym                keysym)
{
  int i;
  /* we used to have release-based bindings but no longer. */
  if (xdke->type == display->dev_key_release_type)
    return;
  
  i = 0;
  while (i < n_bindings)
    {
      if (bindings[i].keycode == xdke->keycode && 
          ((xdke->state & ~(display->ignored_modifier_mask)) ==
           bindings[i].mask) &&
          xdke->type == display->dev_key_press_type)
        {
          const MetaKeyHandler *handler;

          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Binding keycode 0x%x mask 0x%x matches event 0x%x state 0x%x\n",
                      bindings[i].keycode, bindings[i].mask,
                      xdke->keycode, xdke->state);
          
          if (bindings[i].handler)
            handler = bindings[i].handler;
          else
            {
              handler = find_handler (handlers, bindings[i].name);
              bindings[i].handler = handler; /* cache */
            }

          if (handler == NULL)
            meta_bug ("Binding %s has no handler\n", bindings[i].name);
          else
            meta_topic (META_DEBUG_KEYBINDINGS,
                        "Running handler for %s\n",
                        bindings[i].name);
          
          /* Global keybindings count as a let-the-terminal-lose-focus
           * due to new window mapping until the user starts
           * interacting with the terminal again.
           */
          display->allow_terminal_deactivation = TRUE;

          (* handler->func) (display, screen, window, xdke,
                             &bindings[i]);
          return;
        }
      
      ++i;
    }

  meta_topic (META_DEBUG_KEYBINDINGS,
              "No handler found for this event in this binding table\n");
}

void
meta_display_process_key_event (MetaDisplay     *display,
                                MetaWindow      *window,
                                XDeviceKeyEvent *xdke)
{
  KeySym keysym;
  gboolean handled;
  gboolean all_keys_grabbed;
  const char *str;
  MetaScreen *screen;
  MetaDevInfo *dev;

  meta_warning("meta_display_process_key_event\n");

  dev = meta_devices_find_keyboard_by_id (display, xdke->deviceid);

  XAllowDeviceEvents (display->xdisplay, dev->xdev,
                all_bindings_disabled ? ReplayKeyboard : AsyncKeyboard,
                xdke->time);
  if (all_bindings_disabled)
    return;

  /* if key event was on root window, we have a shortcut */
  screen = meta_display_screen_for_root (display, xdke->window);
  
  /* else round-trip to server */
  if (screen == NULL)
    screen = meta_display_screen_for_xwindow (display,
                                              xdke->window);

  if (screen == NULL)
    return; /* event window is destroyed */
  
  /* ignore key events on popup menus and such. */
  if (window == NULL &&
      meta_ui_window_is_widget (screen->ui, xdke->window))
    return;
  
  /* window may be NULL */
  
  keysym = XKeycodeToKeysym (display->xdisplay, xdke->keycode, 0);

  str = XKeysymToString (keysym);
  
  meta_topic (META_DEBUG_KEYBINDINGS,
              "Processing key %s event, keysym: %s state: 0x%x window: %s\n",
              xdke->type == display->dev_key_press_type ? "press" : "release",
              str ? str : "none", xdke->state,
              window ? window->desc : "(no window)");

  all_keys_grabbed = window ? 
  		    meta_devices_list_is_member(&window->all_keys_grabbed, dev):
		    meta_devices_list_is_member(&screen->all_keys_grabbed, dev);
  if (!all_keys_grabbed)
    {
      /* Do the normal keybindings */
      process_event (display->screen_bindings,
                     display->n_screen_bindings,
                     screen_handlers,
                     display, screen, NULL, xdke, keysym);

      if (window)
        process_event (display->window_bindings,
                       display->n_window_bindings,
                       window_handlers,
                       display, screen, window, xdke, keysym);

      return;
    }

  if (dev->grab_op == NULL)
    return;    

  /* If we get here we have a global grab, because
   * we're in some special keyboard mode such as window move
   * mode.
   */
  
  handled = FALSE;

  if (window ? (window == dev->grab_op->window) :
               (screen == dev->grab_op->screen))
    {
      switch (dev->grab_op->op)
        {
        case META_GRAB_OP_MOVING:
        case META_GRAB_OP_RESIZING_SE:
        case META_GRAB_OP_RESIZING_S:      
        case META_GRAB_OP_RESIZING_SW:      
        case META_GRAB_OP_RESIZING_N:
        case META_GRAB_OP_RESIZING_NE:
        case META_GRAB_OP_RESIZING_NW:
        case META_GRAB_OP_RESIZING_W:
        case META_GRAB_OP_RESIZING_E:
          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Processing event for mouse-only move/resize\n");
          g_assert (window != NULL);
          handled = process_mouse_move_resize_grab (display, screen,
                                                    window, xdke, keysym);
          break;

        case META_GRAB_OP_KEYBOARD_MOVING:
          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Processing event for keyboard move\n");
          g_assert (window != NULL);
          handled = process_keyboard_move_grab (display, screen,
                                                window, xdke, keysym);
          break;

        case META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN:
        case META_GRAB_OP_KEYBOARD_RESIZING_S:
        case META_GRAB_OP_KEYBOARD_RESIZING_N:
        case META_GRAB_OP_KEYBOARD_RESIZING_W:
        case META_GRAB_OP_KEYBOARD_RESIZING_E:
        case META_GRAB_OP_KEYBOARD_RESIZING_SE:
        case META_GRAB_OP_KEYBOARD_RESIZING_NE:
        case META_GRAB_OP_KEYBOARD_RESIZING_SW:
        case META_GRAB_OP_KEYBOARD_RESIZING_NW:          
          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Processing event for keyboard resize\n");
          g_assert (window != NULL);
          handled = process_keyboard_resize_grab (display, screen,
                                                  window, xdke, keysym);
          break;

        case META_GRAB_OP_KEYBOARD_TABBING_NORMAL:
        case META_GRAB_OP_KEYBOARD_TABBING_DOCK:
        case META_GRAB_OP_KEYBOARD_TABBING_GROUP:
        case META_GRAB_OP_KEYBOARD_ESCAPING_NORMAL:
        case META_GRAB_OP_KEYBOARD_ESCAPING_DOCK:
        case META_GRAB_OP_KEYBOARD_ESCAPING_GROUP:
          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Processing event for keyboard tabbing/cycling\n");
          handled = process_tab_grab (display, screen, xdke, keysym);
          break;
          
        case META_GRAB_OP_KEYBOARD_WORKSPACE_SWITCHING:
          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Processing event for keyboard workspace switching\n");
          handled = process_workspace_switch_grab (display, screen, xdke, keysym);
          break;

        default:
          break;
        }
    }
  
  /* end grab if a key that isn't used gets pressed */
  if (!handled)
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Ending grab op %u on device %s key event sym %s\n",
                  dev->grab_op->op, dev->name, XKeysymToString (keysym));
      meta_display_end_grab_op (display, 
      			        dev,
				xdke->time);
    }
}

static gboolean
process_mouse_move_resize_grab (MetaDisplay     *display,
                                MetaScreen      *screen,
                                MetaWindow      *window,
                                XDeviceKeyEvent *xdke,
                                KeySym           keysym)
{
  MetaDevInfo *dev;

  /* don't care about releases, but eat them, don't end grab */
  if (xdke->type == display->dev_key_release_type)
    return TRUE;

  dev = meta_devices_find_keyboard_by_id (display, xdke->deviceid);

  if (keysym == XK_Escape)
    {
      /* End move or resize and restore to original state.  If the
       * window was a maximized window that had been "shaken loose" we
       * need to remaximize it.  In normal cases, we need to do a
       * moveresize now to get the position back to the original.  In
       * wireframe mode, we just need to set grab_was_cancelled to tru
       * to avoid avoid moveresizing to the position of the wireframe.
       */
      if (window->shaken_loose)
        meta_window_maximize (window,
			      dev,
                              META_MAXIMIZE_HORIZONTAL |
                              META_MAXIMIZE_VERTICAL);
      else if (!dev->grab_op->wireframe_active)
        meta_window_move_resize (dev->grab_op->window,
				 dev,
                                 TRUE,
                                 dev->grab_op->initial_window_pos.x,
                                 dev->grab_op->initial_window_pos.y,
                                 dev->grab_op->initial_window_pos.width,
                                 dev->grab_op->initial_window_pos.height);
      else
        dev->grab_op->was_cancelled = TRUE;

      /* End grab, since this was an "unhandled" keypress */
      return FALSE;
    }

  /* The keypress really isn't handled but we just want to ignore it, so
   * treat it as handled.
   */
  return TRUE;
}

static gboolean
process_keyboard_move_grab (MetaDisplay     *display,
                            MetaScreen      *screen,
                            MetaWindow      *window,
                            XDeviceKeyEvent *xdke,
                            KeySym           keysym)
{
  gboolean handled;
  int x, y;
  int incr;
  gboolean smart_snap;
  MetaDevInfo *dev;
  
  handled = FALSE;

  /* don't care about releases, but eat them, don't end grab */
  if (xdke->type == display->dev_key_release_type)
    return TRUE;

  dev = meta_devices_find_keyboard_by_id (display, xdke->deviceid);


  /* don't end grab on modifier key presses */
  if (is_modifier (display, xdke->keycode))
    return TRUE;

  if (dev->grab_op->wireframe_active)
    {
      x = dev->grab_op->wireframe_rect.x;
      y = dev->grab_op->wireframe_rect.y;
    }
  else
    {
      meta_window_get_position (window, &x, &y);
    }

  smart_snap = (xdke->state & ShiftMask) != 0;
  
#define SMALL_INCREMENT 1
#define NORMAL_INCREMENT 10

  if (smart_snap)
    incr = 1;
  else if (xdke->state & ControlMask)
    incr = SMALL_INCREMENT;
  else
    incr = NORMAL_INCREMENT;

  if (keysym == XK_Escape)
    {
      /* End move and restore to original state.  If the window was a
       * maximized window that had been "shaken loose" we need to
       * remaximize it.  In normal cases, we need to do a moveresize
       * now to get the position back to the original.  In wireframe
       * mode, we just need to set grab_was_cancelled to tru to avoid
       * avoid moveresizing to the position of the wireframe.
       */
      if (window->shaken_loose)
        meta_window_maximize (window,
			      dev,
                              META_MAXIMIZE_HORIZONTAL |
                              META_MAXIMIZE_VERTICAL);
      else if (!dev->grab_op->wireframe_active)
        meta_window_move_resize (dev->grab_op->window,
				 dev,
                                 TRUE,
                                 dev->grab_op->initial_window_pos.x,
                                 dev->grab_op->initial_window_pos.y,
                                 dev->grab_op->initial_window_pos.width,
                                 dev->grab_op->initial_window_pos.height);
      else
        dev->grab_op->was_cancelled = TRUE;
    }
  
  /* When moving by increments, we still snap to edges if the move
   * to the edge is smaller than the increment. This is because
   * Shift + arrow to snap is sort of a hidden feature. This way
   * people using just arrows shouldn't get too frustrated.
   */
  switch (keysym)
    {
    case XK_KP_Home:
    case XK_KP_Prior:
    case XK_Up:
    case XK_KP_Up:
      y -= incr;
      handled = TRUE;
      break;
    case XK_KP_End:
    case XK_KP_Next:
    case XK_Down:
    case XK_KP_Down:
      y += incr;
      handled = TRUE;
      break;
    }
  
  switch (keysym)
    {
    case XK_KP_Home:
    case XK_KP_End:
    case XK_Left:
    case XK_KP_Left:
      x -= incr;
      handled = TRUE;
      break;
    case XK_KP_Prior:
    case XK_KP_Next:
    case XK_Right:
    case XK_KP_Right:
      x += incr;
      handled = TRUE;
      break;
    }

  if (handled)
    {
      MetaRectangle old_rect;
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Computed new window location %d,%d due to keypress\n",
                  x, y);

      if (dev->grab_op->wireframe_active)
        old_rect = dev->grab_op->wireframe_rect;
      else
        meta_window_get_client_root_coords (window, &old_rect);

      meta_window_edge_resistance_for_move (window,
      					    dev, 
                                            old_rect.x,
                                            old_rect.y,
                                            &x,
                                            &y,
                                            NULL,
                                            smart_snap,
                                            TRUE);

      if (dev->grab_op->wireframe_active)
        {
          meta_window_update_wireframe (window, dev, x, y,
                                        dev->grab_op->wireframe_rect.width,
                                        dev->grab_op->wireframe_rect.height);
        }
      else
        {
          meta_window_move (window, dev, TRUE, x, y);
        }
      
      meta_window_update_keyboard_move (dev, window);
    }

  return handled;
}

static gboolean
process_keyboard_resize_grab_op_change (MetaDisplay     *display,
                                        MetaScreen      *screen,
                                        MetaWindow      *window,
                                        XDeviceKeyEvent *xdke,
                                        KeySym           keysym)
{
  gboolean handled;
  MetaDevInfo *dev;

  dev = meta_devices_find_keyboard_by_id (display, xdke->deviceid);

  if (!dev->grab_op)
    return FALSE;

  handled = FALSE;
  switch (dev->grab_op->op)
    {
    case META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN:
      switch (keysym)
        {
        case XK_Up:
        case XK_KP_Up:
          dev->grab_op->op = META_GRAB_OP_KEYBOARD_RESIZING_N;          
          handled = TRUE;
          break;
        case XK_Down:
        case XK_KP_Down:
          dev->grab_op->op = META_GRAB_OP_KEYBOARD_RESIZING_S;
          handled = TRUE;
          break;
        case XK_Left:
        case XK_KP_Left:
          dev->grab_op->op = META_GRAB_OP_KEYBOARD_RESIZING_W;
          handled = TRUE;
          break;
        case XK_Right:
        case XK_KP_Right:
          dev->grab_op->op = META_GRAB_OP_KEYBOARD_RESIZING_E;
          handled = TRUE;
          break;
        }
      break;
      
    case META_GRAB_OP_KEYBOARD_RESIZING_S:
      switch (keysym)
        {
        case XK_Left:
        case XK_KP_Left:
          dev->grab_op->op = META_GRAB_OP_KEYBOARD_RESIZING_W;
          handled = TRUE;
          break;
        case XK_Right:
        case XK_KP_Right:
          dev->grab_op->op = META_GRAB_OP_KEYBOARD_RESIZING_E;
          handled = TRUE;
          break;
        }
      break;

    case META_GRAB_OP_KEYBOARD_RESIZING_N:
      switch (keysym)
        {
        case XK_Left:
        case XK_KP_Left:
          dev->grab_op->op = META_GRAB_OP_KEYBOARD_RESIZING_W;
          handled = TRUE;
          break;
        case XK_Right:
        case XK_KP_Right:
          dev->grab_op->op = META_GRAB_OP_KEYBOARD_RESIZING_E;
          handled = TRUE;
          break;
        }
      break;
      
    case META_GRAB_OP_KEYBOARD_RESIZING_W:
      switch (keysym)
        {
        case XK_Up:
        case XK_KP_Up:
          dev->grab_op->op = META_GRAB_OP_KEYBOARD_RESIZING_N;          
          handled = TRUE;
          break;
        case XK_Down:
        case XK_KP_Down:
          dev->grab_op->op = META_GRAB_OP_KEYBOARD_RESIZING_S;
          handled = TRUE;
          break;
        }
      break;

    case META_GRAB_OP_KEYBOARD_RESIZING_E:
      switch (keysym)
        {
        case XK_Up:
        case XK_KP_Up:
          dev->grab_op->op = META_GRAB_OP_KEYBOARD_RESIZING_N; 
          handled = TRUE;
          break;
        case XK_Down:
        case XK_KP_Down:
          dev->grab_op->op = META_GRAB_OP_KEYBOARD_RESIZING_S;
          handled = TRUE;
          break;
        }
      break;
      
    case META_GRAB_OP_KEYBOARD_RESIZING_SE:
    case META_GRAB_OP_KEYBOARD_RESIZING_NE:
    case META_GRAB_OP_KEYBOARD_RESIZING_SW:
    case META_GRAB_OP_KEYBOARD_RESIZING_NW:
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  if (handled)
    {
      meta_window_update_keyboard_resize (dev, window, TRUE);
      return TRUE; 
    }

  return FALSE;
}

static gboolean
process_keyboard_resize_grab (MetaDisplay     *display,
                              MetaScreen      *screen,
                              MetaWindow      *window,
                              XDeviceKeyEvent *xdke,
                              KeySym           keysym)
{
  gboolean handled;
  int height_inc;
  int width_inc;
  int width, height;
  gboolean smart_snap;
  int gravity;
  MetaDevInfo *dev;
  
  handled = FALSE;
  /* don't care about releases, but eat them, don't end grab */
  if (xdke->type == display->dev_key_release_type)
    return TRUE;

  dev = meta_devices_find_keyboard_by_id (display, xdke->deviceid);

  /* don't end grab on modifier key presses */
  if (is_modifier (display, xdke->keycode))
    return TRUE;

  if (keysym == XK_Escape)
    {
      /* End resize and restore to original state.  If not in
       * wireframe mode, we need to do a moveresize now to get the
       * position back to the original.  If we are in wireframe mode,
       * we need to avoid moveresizing to the position of the
       * wireframe.
       */
      if (!dev->grab_op->wireframe_active)
        meta_window_move_resize (dev->grab_op->window,
				 dev,
                                 TRUE,
                                 dev->grab_op->initial_window_pos.x,
                                 dev->grab_op->initial_window_pos.y,
                                 dev->grab_op->initial_window_pos.width,
                                 dev->grab_op->initial_window_pos.height);
      else
        dev->grab_op->was_cancelled = TRUE;

      return FALSE;
    }

  if (process_keyboard_resize_grab_op_change (display, screen, window, 
                                              xdke, keysym))
    return TRUE;

  if (dev->grab_op->wireframe_active)
    {
      width = dev->grab_op->wireframe_rect.width;
      height = dev->grab_op->wireframe_rect.height;
    }
  else
    {
      width = window->rect.width;
      height = window->rect.height;
    }

  gravity = meta_resize_gravity_from_grab_op (dev->grab_op->op);

  smart_snap = (xdke->state & ShiftMask) != 0;
  
#define SMALL_INCREMENT 1
#define NORMAL_INCREMENT 10

  if (smart_snap)
    {
      height_inc = 1;
      width_inc = 1;
    }
  else if (xdke->state & ControlMask)
    {
      width_inc = SMALL_INCREMENT;
      height_inc = SMALL_INCREMENT;
    }  
  else
    {
      width_inc = NORMAL_INCREMENT;
      height_inc = NORMAL_INCREMENT;
    }

  /* If this is a resize increment window, make the amount we resize
   * the window by match that amount (well, unless snap resizing...)
   */
  if (window->size_hints.width_inc > 1)
    width_inc = window->size_hints.width_inc;
  if (window->size_hints.height_inc > 1)
    height_inc = window->size_hints.height_inc;
  
  switch (keysym)
    {
    case XK_Up:
    case XK_KP_Up:
      switch (gravity)
        {
        case NorthGravity:
        case NorthWestGravity:
        case NorthEastGravity:
          /* Move bottom edge up */
          height -= height_inc;
          break;

        case SouthGravity:
        case SouthWestGravity:
        case SouthEastGravity:
          /* Move top edge up */
          height += height_inc;
          break;

        case EastGravity:
        case WestGravity:
        case CenterGravity:
          g_assert_not_reached ();
          break;
        }
      
      handled = TRUE;
      break;

    case XK_Down:
    case XK_KP_Down:
      switch (gravity)
        {
        case NorthGravity:
        case NorthWestGravity:
        case NorthEastGravity:
          /* Move bottom edge down */
          height += height_inc;
          break;

        case SouthGravity:
        case SouthWestGravity:
        case SouthEastGravity:
          /* Move top edge down */
          height -= height_inc;
          break;

        case EastGravity:
        case WestGravity:
        case CenterGravity:
          g_assert_not_reached ();
          break;
        }
      
      handled = TRUE;
      break;
      
    case XK_Left:
    case XK_KP_Left:
      switch (gravity)
        {
        case EastGravity:
        case SouthEastGravity:
        case NorthEastGravity:
          /* Move left edge left */
          width += width_inc;
          break;

        case WestGravity:
        case SouthWestGravity:
        case NorthWestGravity:
          /* Move right edge left */
          width -= width_inc;
          break;

        case NorthGravity:
        case SouthGravity:
        case CenterGravity:
          g_assert_not_reached ();
          break;
        }
      
      handled = TRUE;
      break;
      
    case XK_Right:
    case XK_KP_Right:
      switch (gravity)
        {
        case EastGravity:
        case SouthEastGravity:
        case NorthEastGravity:
          /* Move left edge right */
          width -= width_inc;
          break;

        case WestGravity:
        case SouthWestGravity:
        case NorthWestGravity:
          /* Move right edge right */
          width += width_inc;
          break;

        case NorthGravity:
        case SouthGravity:
        case CenterGravity:
          g_assert_not_reached ();
          break;
        }
      
      handled = TRUE;
      break;
          
    default:
      break;
    }

  /* fixup hack (just paranoia, not sure it's required) */
  if (height < 1)
    height = 1;
  if (width < 1)
    width = 1;
  
  if (handled)
    {
      MetaRectangle old_rect;
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Computed new window size due to keypress: "
                  "%dx%d, gravity %s\n",
                  width, height, meta_gravity_to_string (gravity));
      
      if (dev->grab_op->wireframe_active)
        old_rect = dev->grab_op->wireframe_rect;
      else
        old_rect = window->rect;  /* Don't actually care about x,y */

      /* Do any edge resistance/snapping */
      meta_window_edge_resistance_for_resize (window,
      					      dev,
                                              old_rect.width,
                                              old_rect.height,
                                              &width,
                                              &height,
                                              gravity,
                                              NULL,
                                              smart_snap,
                                              TRUE);

      if (dev->grab_op->wireframe_active)
        {
          MetaRectangle new_position;
          meta_rectangle_resize_with_gravity (&dev->grab_op->wireframe_rect, 
                                              &new_position,
                                              gravity,
                                              width,
                                              height);
          meta_window_update_wireframe (window, dev,
                                        new_position.x,
                                        new_position.y,
                                        new_position.width, 
                                        new_position.height);
        }
      else
        {
          /* We don't need to update unless the specified width and height
           * are actually different from what we had before.
           */
          if (window->rect.width != width || window->rect.height != height)
            meta_window_resize_with_gravity (window,
	    				     dev,
                                             TRUE,
                                             width,
                                             height,
                                             gravity);
        }
      meta_window_update_keyboard_resize (dev, window, FALSE);
    }

  return handled;
}

static gboolean
end_keyboard_grab (MetaDisplay *display,
		   MetaDevInfo *dev,
		   unsigned int keycode)
{
#ifdef HAVE_XKB
  if (display->xkb_base_event_type > 0)
    {
      unsigned int primary_modifier;
      XkbStateRec state;
  
      primary_modifier = get_primary_modifier (display, dev->grab_op->mask);
      
      XkbGetState (display->xdisplay, XkbUseCoreKbd, &state);

      if (!(primary_modifier & state.mods))
	return TRUE;
    }
  else
#endif
    {
      if (keycode_is_primary_modifier (display, keycode, dev->grab_op->mask))
	return TRUE;
    }

  return FALSE;
}

static gboolean
process_tab_grab (MetaDisplay     *display,
                  MetaScreen      *screen,
                  XDeviceKeyEvent *xdke,
                  KeySym           keysym)
{
  MetaKeyBindingAction action;
  gboolean popup_not_showing;
  gboolean backward;
  gboolean key_used;
  Window      prev_xwindow;
  MetaWindow *prev_window;
  MetaDevInfo *dev;
 
  dev = meta_devices_find_keyboard_by_id (display, xdke->deviceid);

  if (screen != dev->grab_op->screen)
    return FALSE;

  g_return_val_if_fail (screen->tab_popup != NULL, FALSE);

  if (xdke->type == display->dev_key_release_type &&
      end_keyboard_grab (display, dev, xdke->keycode))
    {
      /* We're done, move to the new window. */
      Window target_xwindow;
      MetaWindow *target_window;

      target_xwindow =
        (Window) meta_ui_tab_popup_get_selected (screen->tab_popup);
      target_window =
        meta_display_lookup_x_window (display, target_xwindow);

      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Ending tab operation, primary modifier released\n");
      
      if (target_window)
        {
          target_window->tab_unminimized = FALSE;

          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Activating target window\n");

          meta_topic (META_DEBUG_FOCUS, "Activating %s due to tab popup "
                      "selection and turning mouse_mode off\n",
                      target_window->desc);
          display->mouse_mode = FALSE;
          meta_window_activate (target_window, dev, xdke->time);

          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Ending grab early so we can focus the target window\n");
          meta_display_end_grab_op (display, dev, xdke->time);

          return TRUE; /* we already ended the grab */
        }
      
      return FALSE; /* end grab */
    }
  
  /* don't care about other releases, but eat them, don't end grab */
  if (xdke->type == display->dev_key_release_type)
    return TRUE;

  /* don't end grab on modifier key presses */
  if (is_modifier (display, xdke->keycode))
    return TRUE;

  prev_xwindow = (Window) meta_ui_tab_popup_get_selected (screen->tab_popup);
  prev_window  = meta_display_lookup_x_window (display, prev_xwindow);
  action = display_get_keybinding_action (display,
                                          keysym,
                                          xdke->keycode,
                                          dev->grab_op->mask);

  /* Cancel when alt-Escape is pressed during using alt-Tab, and vice
   * versa.
   */
  switch (action)
    {
    case META_KEYBINDING_ACTION_CYCLE_PANELS:
    case META_KEYBINDING_ACTION_CYCLE_WINDOWS:
    case META_KEYBINDING_ACTION_CYCLE_PANELS_BACKWARD:
    case META_KEYBINDING_ACTION_CYCLE_WINDOWS_BACKWARD:
      /* CYCLE_* are traditionally Escape-based actions,
       * and should cancel traditionally Tab-based ones.
       */
       switch (dev->grab_op->op)
        {
        case META_GRAB_OP_KEYBOARD_ESCAPING_NORMAL:
        case META_GRAB_OP_KEYBOARD_ESCAPING_DOCK:
         /* carry on */
          break;
        default:
          return FALSE;
        }
       break;
    case META_KEYBINDING_ACTION_SWITCH_PANELS:
    case META_KEYBINDING_ACTION_SWITCH_WINDOWS:
    case META_KEYBINDING_ACTION_SWITCH_PANELS_BACKWARD:
    case META_KEYBINDING_ACTION_SWITCH_WINDOWS_BACKWARD:
      /* SWITCH_* are traditionally Tab-based actions,
       * and should cancel traditionally Escape-based ones.
       */
      switch (dev->grab_op->op)
        {
        case META_GRAB_OP_KEYBOARD_TABBING_NORMAL:
        case META_GRAB_OP_KEYBOARD_TABBING_DOCK:
          /* carry on */
          break;
        default:
          /* Also, we must re-lower and re-minimize whatever window
           * we'd previously raised and unminimized.
           */
          meta_stack_set_positions (screen->stack,
                                    dev->grab_op->old_window_stacking);
          if (prev_window && prev_window->tab_unminimized)
            {
              meta_window_minimize (prev_window);
              prev_window->tab_unminimized = FALSE;
            }
          return FALSE;
        }
      break;
    case META_KEYBINDING_ACTION_CYCLE_GROUP:
    case META_KEYBINDING_ACTION_CYCLE_GROUP_BACKWARD:
    case META_KEYBINDING_ACTION_SWITCH_GROUP:
    case META_KEYBINDING_ACTION_SWITCH_GROUP_BACKWARD:
      switch (dev->grab_op->op)
        {
        case META_GRAB_OP_KEYBOARD_ESCAPING_GROUP:
        case META_GRAB_OP_KEYBOARD_TABBING_GROUP:
          /* carry on */
             break;
        default:
             return FALSE;
        }
 
      break;
    default:
      break;
    }
   
  popup_not_showing = FALSE;
  key_used = FALSE;
  backward = FALSE;

  switch (action)
    {
    case META_KEYBINDING_ACTION_CYCLE_PANELS:
    case META_KEYBINDING_ACTION_CYCLE_WINDOWS:
    case META_KEYBINDING_ACTION_CYCLE_GROUP:
      popup_not_showing = TRUE;
      key_used = TRUE;
      break;
    case META_KEYBINDING_ACTION_CYCLE_PANELS_BACKWARD:
    case META_KEYBINDING_ACTION_CYCLE_WINDOWS_BACKWARD:
    case META_KEYBINDING_ACTION_CYCLE_GROUP_BACKWARD:
      popup_not_showing = TRUE;
      key_used = TRUE;
      backward = TRUE;
      break;
    case META_KEYBINDING_ACTION_SWITCH_PANELS:
    case META_KEYBINDING_ACTION_SWITCH_WINDOWS:
    case META_KEYBINDING_ACTION_SWITCH_GROUP:
      key_used = TRUE;
      break;
    case META_KEYBINDING_ACTION_SWITCH_PANELS_BACKWARD:
    case META_KEYBINDING_ACTION_SWITCH_WINDOWS_BACKWARD:
    case META_KEYBINDING_ACTION_SWITCH_GROUP_BACKWARD:
      key_used = TRUE;
      backward = TRUE;
      break;
    default:
      break;
    }
  
  if (key_used)
    {
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Key pressed, moving tab focus in popup\n");

      if (xdke->state & ShiftMask)
        backward = !backward;

      if (backward)
        meta_ui_tab_popup_backward (screen->tab_popup);
      else
        meta_ui_tab_popup_forward (screen->tab_popup);
      
      if (popup_not_showing)
        {
          /* We can't actually change window focus, due to the grab.
           * but raise the window.
           */
          Window target_xwindow;
          MetaWindow *target_window;

          meta_stack_set_positions (screen->stack,
                                    dev->grab_op->old_window_stacking);

          target_xwindow =
            (Window) meta_ui_tab_popup_get_selected (screen->tab_popup);
          target_window =
            meta_display_lookup_x_window (display, target_xwindow);
          
          if (prev_window && prev_window->tab_unminimized)
            {
              prev_window->tab_unminimized = FALSE;
              meta_window_minimize (prev_window);
            }

          if (target_window)
            {
              meta_window_raise (target_window);
              target_window->tab_unminimized = target_window->minimized;
              meta_window_unminimize (target_window);
            }
        }
    }
  else
    {
      /* end grab */
      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Ending tabbing/cycling, uninteresting key pressed\n");

      meta_topic (META_DEBUG_KEYBINDINGS, 
                  "Syncing to old stack positions.\n");
      meta_stack_set_positions (screen->stack,
                                dev->grab_op->old_window_stacking);

      if (prev_window && prev_window->tab_unminimized)
        {
          meta_window_minimize (prev_window);
          prev_window->tab_unminimized = FALSE;
        }
    }
  
  return key_used;
}

static void
handle_activate_workspace (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *event_window,
                           XDeviceKeyEvent *xdke,
                           MetaKeyBinding  *binding)
{
  int which;
  MetaWorkspace *workspace;
  MetaDevInfo   *dev;
  
  which = GPOINTER_TO_INT (binding->handler->data);
 
  workspace = NULL;
  if (which < 0)
    {      
      workspace = meta_workspace_get_neighbor (screen->active_workspace,
                                               which);
    }
  else
    {
      workspace = meta_screen_get_workspace_by_index (screen, which);
    }
  
  if (workspace)
    {
      dev = meta_devices_find_keyboard_by_id (display, xdke->deviceid);
      meta_workspace_activate (workspace, dev, xdke->time);
    }
  else
    {
      /* We could offer to create it I suppose */
    }
}

static void
error_on_generic_command (const char *key,
                          const char *command,
                          const char *message,
                          int         screen_number,
                          guint32     timestamp)
{
  GError *err;
  char *argv[10];
  char numbuf[32];
  char timestampbuf[32];
  
  sprintf (numbuf, "%d", screen_number);
  sprintf (timestampbuf, "%u", timestamp);
  
  argv[0] = METACITY_LIBEXECDIR"/metacity-dialog";
  argv[1] = "--screen";
  argv[2] = numbuf;
  argv[3] = "--timestamp";
  argv[4] = timestampbuf;
  argv[5] = "--command-failed-error";
  argv[6] = (char *)key;
  argv[7] = (char*) (command ? command : "");
  argv[8] = (char*) message;
  argv[9] = NULL;
  
  err = NULL;
  if (!g_spawn_async_with_pipes ("/",
                                 argv,
                                 NULL,
                                 0,
                                 NULL, NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 &err))
    {
      meta_warning (_("Error launching metacity-dialog to print an error about a command: %s\n"),
                    err->message);
      g_error_free (err);
    }
}

static void
error_on_command (int         command_index,
                  const char *command,
                  const char *message,
                  int         screen_number,
                  guint32     timestamp)
{
  char *key;
  
  meta_warning ("Error on command %d \"%s\": %s\n",
                command_index, command, message);  

  key = meta_prefs_get_gconf_key_for_command (command_index);

  error_on_generic_command (key, command, message, screen_number, timestamp);
  
  g_free (key);
}

static void
error_on_terminal_command (const char *command,
                           const char *message,
                           int         screen_number,
                           guint32     timestamp)
{
  const char *key;
  
  meta_warning ("Error on terminal command \"%s\": %s\n", command, message);  

  key = meta_prefs_get_gconf_key_for_terminal_command ();

  error_on_generic_command (key, command, message, screen_number, timestamp);
}

static void
set_display_setup_func (void *data)
{
  const char *screen_name = data;
  char *full;

  full = g_strdup_printf ("DISPLAY=%s", screen_name);

  putenv (full);

  /* do not free full, because putenv is lame */
} 

static gboolean
meta_spawn_command_line_async_on_screen (const gchar *command_line,
                                         MetaScreen  *screen,
                                         GError     **error)
{
  gboolean retval;
  gchar **argv = NULL;

  g_return_val_if_fail (command_line != NULL, FALSE);

  if (!g_shell_parse_argv (command_line,
                           NULL, &argv,
                           error))
    return FALSE;
  
  retval = g_spawn_async (NULL,
                          argv,
                          NULL,
                          G_SPAWN_SEARCH_PATH,
                          set_display_setup_func,
                          screen->screen_name,
                          NULL,
                          error);
  g_strfreev (argv);

  return retval;
}


static void
handle_run_command (MetaDisplay     *display,
                    MetaScreen      *screen,
                    MetaWindow      *window,
                    XDeviceKeyEvent *xdke,
                    MetaKeyBinding  *binding)
{
  int which;
  const char *command;
  GError *err;
  
  which = GPOINTER_TO_INT (binding->handler->data);
 
  command = meta_prefs_get_command (which);

  if (command == NULL)
    {
      char *s;

      meta_topic (META_DEBUG_KEYBINDINGS,
                  "No command %d to run in response to keybinding press\n",
                  which);
      
      s = g_strdup_printf (_("No command %d has been defined.\n"),
                           which + 1);
      error_on_command (which, NULL, s, screen->number, xdke->time);
      g_free (s);
      
      return;
    }

  err = NULL;
  if (!meta_spawn_command_line_async_on_screen (command, screen, &err))
    {
      error_on_command (which, command, err->message, screen->number, xdke->time);
      
      g_error_free (err);
    }
}


static void
handle_maximize_vert (MetaDisplay     *display,
                      MetaScreen      *screen,
                      MetaWindow      *window,
                      XDeviceKeyEvent *xdke,
                      MetaKeyBinding  *binding)
{
  MetaDevInfo *dev;

  dev = meta_devices_find_keyboard_by_id (display, xdke->deviceid);
  if (window && window->has_resize_func)
    {
      if (window->maximized_vertically)
        meta_window_unmaximize (window, dev, META_MAXIMIZE_VERTICAL);
      else
        meta_window_maximize (window, dev,  META_MAXIMIZE_VERTICAL);
    }
}

static void
handle_maximize_horiz (MetaDisplay     *display,
                       MetaScreen      *screen,
                       MetaWindow      *window,
                       XDeviceKeyEvent *xdke,
                       MetaKeyBinding  *binding)
{
  MetaDevInfo *dev;

  dev = meta_devices_find_keyboard_by_id (display, xdke->deviceid);
  if (window && window->has_resize_func)
    {
      if (window->maximized_horizontally)
        meta_window_unmaximize (window, dev, META_MAXIMIZE_HORIZONTAL);
      else
        meta_window_maximize (window, dev, META_MAXIMIZE_HORIZONTAL);
    }
}

/* Move a window to a corner; to_bottom/to_right are FALSE for the
 * top or left edge, or TRUE for the bottom/right edge.  xchange/ychange
 * are FALSE if that dimension is not to be changed, TRUE otherwise.
 * Together they describe which of the four corners, or four sides,
 * is desired.
 */
static void
handle_move_to_corner_backend (MetaDisplay    *display,
           MetaScreen     *screen,
           MetaWindow     *window,
	   MetaDevInfo    *dev,
           gboolean        xchange,
           gboolean        ychange,
           gboolean        to_right,
           gboolean        to_bottom)
{
  MetaRectangle work_area;
  MetaRectangle outer;
  int orig_x, orig_y;
  int new_x, new_y;
  int frame_width, frame_height;

  meta_window_get_work_area_all_xineramas (window, &work_area);
  meta_window_get_outer_rect (window, &outer);
  meta_window_get_position (window, &orig_x, &orig_y);

  frame_width = (window->frame ? window->frame->child_x : 0);
  frame_height = (window->frame ? window->frame->child_y : 0);

  if (xchange) {
    new_x = work_area.x + (to_right ?
            (work_area.width + frame_width) - outer.width :
            0);
  } else {
    new_x = orig_x;
  }

  if (ychange) {
    new_y = work_area.y + (to_bottom ?
            (work_area.height + frame_height) - outer.height :
            0);
  } else {
    new_y = orig_y;
  }

  meta_window_move_resize (window,
                           dev,
                           FALSE,
                           new_x,
                           new_y,
                           window->rect.width,
                           window->rect.height);
}

static void
handle_move_to_corner_nw  (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           XDeviceKeyEvent *xdke,
                           MetaKeyBinding  *binding)
{
  if (window)
    {
      handle_move_to_corner_backend (display, screen, window, 
                       meta_devices_find_keyboard_by_id(display, xdke->deviceid),
                                     TRUE, TRUE, FALSE, FALSE);
    }
}

static void
handle_move_to_corner_ne  (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           XDeviceKeyEvent *xdke,
                           MetaKeyBinding  *binding)
{
  if (window)
    {
      handle_move_to_corner_backend (display, screen, window, 
                       meta_devices_find_keyboard_by_id(display, xdke->deviceid),
      				     TRUE, TRUE, TRUE, FALSE);
    }
}

static void
handle_move_to_corner_sw  (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           XDeviceKeyEvent *xdke,
                           MetaKeyBinding  *binding)
{
  if (window)
    {
      handle_move_to_corner_backend (display, screen, window, 
                       meta_devices_find_keyboard_by_id(display, xdke->deviceid),
      				     TRUE, TRUE, FALSE, TRUE);
    }
}

static void
handle_move_to_corner_se  (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           XDeviceKeyEvent *xdke,
                           MetaKeyBinding  *binding)
{
  if (window)
    {
      handle_move_to_corner_backend (display, screen, window, 
                       meta_devices_find_keyboard_by_id(display, xdke->deviceid),
      				     TRUE, TRUE, TRUE, TRUE);
    }
}

static void
handle_move_to_side_n     (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           XDeviceKeyEvent *xdke,
                           MetaKeyBinding  *binding)
{
  if (window)
    {
      handle_move_to_corner_backend (display, screen, window, 
                       meta_devices_find_keyboard_by_id(display, xdke->deviceid),
      				     FALSE, TRUE, FALSE, FALSE);
    }
}

static void
handle_move_to_side_s     (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           XDeviceKeyEvent *xdke,
                           MetaKeyBinding  *binding)
{
  if (window)
    {
      handle_move_to_corner_backend (display, screen, window, 
                       meta_devices_find_keyboard_by_id(display, xdke->deviceid),
      				     FALSE, TRUE, FALSE, TRUE);
    }
}

static void
handle_move_to_side_e     (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           XDeviceKeyEvent *xdke,
                           MetaKeyBinding  *binding)
{
  if (window)
    {
      handle_move_to_corner_backend (display, screen, window,
                       meta_devices_find_keyboard_by_id(display, xdke->deviceid),
      				     TRUE, FALSE, TRUE, FALSE);
    }
}

static void
handle_move_to_side_w     (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           XDeviceKeyEvent *xdke,
                           MetaKeyBinding  *binding)
{
  if (window)
    {
      handle_move_to_corner_backend (display, screen, window,
                       meta_devices_find_keyboard_by_id(display, xdke->deviceid),
      				     TRUE, FALSE, FALSE, FALSE);
    }
}

static gboolean
process_workspace_switch_grab (MetaDisplay     *display,
                               MetaScreen      *screen,
                               XDeviceKeyEvent *xdke,
                               KeySym           keysym)
{
  MetaWorkspace *workspace;
  MetaDevInfo *dev;

  dev = meta_devices_find_keyboard_by_id (display, xdke->deviceid);

  if (!dev->grab_op)
    return FALSE;
  if (screen != dev->grab_op->screen)
    return FALSE;

  g_return_val_if_fail (screen->tab_popup != NULL, FALSE);

  if (xdke->type == display->dev_key_release_type &&
      end_keyboard_grab (display, dev, xdke->keycode))
    {
      /* We're done, move to the new workspace. */
      MetaWorkspace *target_workspace;

      target_workspace =
        (MetaWorkspace *) meta_ui_tab_popup_get_selected (screen->tab_popup);

      meta_topic (META_DEBUG_KEYBINDINGS,
                  "Ending workspace tab operation, primary modifier released\n");

      if (target_workspace == screen->active_workspace)
        {
          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Ending grab so we can focus on the target workspace\n");
          meta_display_end_grab_op (display, dev, xdke->time);

          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Focusing default window on target workspace\n");

          meta_workspace_focus_default_window (target_workspace,
	  				       dev,
                                               NULL,
                                               xdke->time);

          return TRUE; /* we already ended the grab */
        }

      /* Workspace switching should have already occurred on KeyPress */
      meta_warning ("target_workspace != active_workspace.  Some other event must have occurred.\n");
      
      return FALSE; /* end grab */
    }
  
  /* don't care about other releases, but eat them, don't end grab */
  if (xdke->type == display->dev_key_release_type)
    return TRUE;

  /* don't end grab on modifier key presses */
  if (is_modifier (display, xdke->keycode))
    return TRUE;

  /* select the next workspace in the tabpopup */
  workspace =
    (MetaWorkspace *) meta_ui_tab_popup_get_selected (screen->tab_popup);
  
  if (workspace)
    {
      MetaWorkspace *target_workspace;
      MetaKeyBindingAction action;

      action = display_get_keybinding_action (display,
                                              keysym,
                                              xdke->keycode,
                                              dev->grab_op->mask);

      switch (action)
        {
        case META_KEYBINDING_ACTION_WORKSPACE_UP:
          target_workspace = meta_workspace_get_neighbor (workspace,
                                                          META_MOTION_UP);
          break;

        case META_KEYBINDING_ACTION_WORKSPACE_DOWN:
          target_workspace = meta_workspace_get_neighbor (workspace,
                                                          META_MOTION_DOWN);
          break;

        case META_KEYBINDING_ACTION_WORKSPACE_LEFT:
          target_workspace = meta_workspace_get_neighbor (workspace,
                                                          META_MOTION_LEFT);
          break;

        case META_KEYBINDING_ACTION_WORKSPACE_RIGHT:
          target_workspace = meta_workspace_get_neighbor (workspace,
                                                          META_MOTION_RIGHT);
          break;

        default:
          target_workspace = NULL;
          break;
        }

      if (target_workspace)
        {
          meta_ui_tab_popup_select (screen->tab_popup,
                                    (MetaTabEntryKey) target_workspace);
          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Tab key pressed, moving tab focus in popup\n");

          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Activating target workspace\n");

          meta_workspace_activate (target_workspace, dev, xdke->time);

          return TRUE; /* we already ended the grab */
        }
    }

  /* end grab */
  meta_topic (META_DEBUG_KEYBINDINGS,
              "Ending workspace tabbing & focusing default window; uninteresting key pressed\n");
  workspace =
    (MetaWorkspace *) meta_ui_tab_popup_get_selected (screen->tab_popup);
  meta_workspace_focus_default_window (workspace, dev, NULL, xdke->time);
  return FALSE;
}

static void
handle_toggle_desktop (MetaDisplay     *display,
                       MetaScreen      *screen,
                       MetaWindow      *window,
                       XDeviceKeyEvent *xdke,
                       MetaKeyBinding  *binding)
{
  MetaDevInfo *dev;

  dev = meta_devices_find_keyboard_by_id (display, xdke->deviceid);
  if (screen->active_workspace->showing_desktop)
    {
      meta_screen_unshow_desktop (screen);
      meta_workspace_focus_default_window (screen->active_workspace,
      					   dev,
                                           NULL,
                                           xdke->time);
    }
  else
    meta_screen_show_desktop (screen, xdke->time);
}

static void
handle_panel_keybinding (MetaDisplay     *display,
                         MetaScreen      *screen,
                         MetaWindow      *window,
                         XDeviceKeyEvent *xdke,
                         MetaKeyBinding  *binding)
{
  MetaKeyBindingAction action;
  Atom action_atom;
  XClientMessageEvent ev;
  
  action = GPOINTER_TO_INT (binding->handler->data);

  action_atom = None;
  switch (action)
    {
    case META_KEYBINDING_ACTION_PANEL_MAIN_MENU:
      action_atom = display->atom_gnome_panel_action_main_menu;
      break;
    case META_KEYBINDING_ACTION_PANEL_RUN_DIALOG:
      action_atom = display->atom_gnome_panel_action_run_dialog;
      break;
    default:
      return;
    }
   
  ev.type = ClientMessage;
  ev.window = screen->xroot;
  ev.message_type = display->atom_gnome_panel_action;
  ev.format = 32;
  ev.data.l[0] = action_atom;
  ev.data.l[1] = xdke->time;

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Sending panel message with timestamp %lu, and turning mouse_mode "
              "off due to keybinding press\n", xdke->time);
  display->mouse_mode = FALSE;

  meta_error_trap_push (display);

  /* Release the grab for the panel before sending the event */
  XUngrabDevice (display->xdisplay,
   (meta_devices_find_keyboard_by_id (display, xdke->deviceid))->xdev, /* XXX */
  		 xdke->time);
#if 0
#warning XUngrabKeyboard
  XUngrabKeyboard (display->xdisplay, xdke->time);
#endif

  XSendEvent (display->xdisplay,
	      screen->xroot,
	      False,
	      StructureNotifyMask,
	      (XEvent*) &ev);

  meta_error_trap_pop (display, FALSE);
}

static void
handle_activate_menu (MetaDisplay     *display,
                      MetaScreen      *screen,
                      MetaWindow      *event_window,
                      XDeviceKeyEvent *xdke,
                      MetaKeyBinding  *binding)
{
  if (display->focus_window)
    {
      int x, y;

      meta_window_get_position (display->focus_window,
                                &x, &y);
      
      if (meta_ui_get_direction() == META_UI_DIRECTION_RTL)
	  x += display->focus_window->rect.width;

      meta_window_show_menu (display->focus_window,
                             x, y,
                             0,
                             xdke->time);
    }
}

static MetaGrabOp
tab_op_from_tab_type (MetaTabList type)
{
  switch (type)
    {
    case META_TAB_LIST_NORMAL:
      return META_GRAB_OP_KEYBOARD_TABBING_NORMAL;
    case META_TAB_LIST_DOCKS:
      return META_GRAB_OP_KEYBOARD_TABBING_DOCK;
    case META_TAB_LIST_GROUP:
      return META_GRAB_OP_KEYBOARD_TABBING_GROUP;
    }

  g_assert_not_reached ();
  
  return 0;
}

static MetaGrabOp
cycle_op_from_tab_type (MetaTabList type)
{
  switch (type)
    {
    case META_TAB_LIST_NORMAL:
      return META_GRAB_OP_KEYBOARD_ESCAPING_NORMAL;
    case META_TAB_LIST_DOCKS:
      return META_GRAB_OP_KEYBOARD_ESCAPING_DOCK;
    case META_TAB_LIST_GROUP:
      return META_GRAB_OP_KEYBOARD_ESCAPING_GROUP;
    }

  g_assert_not_reached ();
  
  return 0;
}

static void
do_choose_window (MetaDisplay     *display,
                  MetaScreen      *screen,
                  MetaWindow      *event_window,
                  XDeviceKeyEvent *xdke,
                  MetaKeyBinding  *binding,
                  gboolean         backward,
                  gboolean         show_popup)
{
  MetaTabList type;
  MetaWindow *initial_selection;

  MetaDevInfo *kbdDev;
  MetaDevInfo *pairedPtr;

  kbdDev = meta_devices_find_keyboard_by_id  (display, xdke->deviceid);
  pairedPtr = meta_devices_find_paired_mouse (display, xdke->deviceid);
  
  type = GPOINTER_TO_INT (binding->handler->data);
  
  meta_topic (META_DEBUG_KEYBINDINGS,
              "Tab list = %u show_popup = %d\n", type, show_popup);
  
  /* reverse direction if shift is down */
  if (xdke->state & ShiftMask)
    backward = !backward;
  
  initial_selection = meta_display_get_tab_next (display,
                                                 type,
                                                 screen,
                                                 screen->active_workspace,
                                                 NULL,
                                                 backward);

  /* Note that focus_window may not be in the tab chain, but it's OK */
  if (initial_selection == NULL)
    initial_selection = meta_display_get_tab_current (display,
                                                      type, screen,
                                                      screen->active_workspace);
  
  meta_topic (META_DEBUG_KEYBINDINGS,
              "Initially selecting window %s\n",
              initial_selection ? initial_selection->desc : "(none)");  

  if (initial_selection != NULL)
    {
      if (binding->mask == 0)
        {
          /* If no modifiers, we can't do the "hold down modifier to keep
           * moving" thing, so we just instaswitch by one window.
           */
          meta_topic (META_DEBUG_FOCUS,
                      "Activating %s and turning off mouse_mode due to "
                      "switch/cycle windows with no modifiers\n",
                      initial_selection->desc);
          display->mouse_mode = FALSE;
          meta_window_activate (initial_selection, kbdDev, xdke->time);
        }
      else if (meta_display_begin_grab_op (display,
                                           screen,
					   pairedPtr,
                                           NULL,
                                           show_popup ?
                                           tab_op_from_tab_type (type) :
                                           cycle_op_from_tab_type (type),
                                           FALSE,
                                           FALSE,
                                           0,
                                           binding->mask,
                                           xdke->time,
                                           0, 0))
        {
          if (!primary_modifier_still_pressed (display, pairedPtr,
                                               binding->mask))
            {
              /* This handles a race where modifier might be released
               * before we establish the grab. must end grab
               * prior to trying to focus a window.
               */
              meta_topic (META_DEBUG_FOCUS, 
                          "Ending grab, activating %s, and turning off "
                          "mouse_mode due to switch/cycle windows where "
                          "modifier was released prior to grab\n",
                          initial_selection->desc);
              meta_display_end_grab_op (display, pairedPtr, xdke->time);
              display->mouse_mode = FALSE;
              meta_window_activate (initial_selection, kbdDev, xdke->time);
            }
          else
            {
              meta_ui_tab_popup_select (screen->tab_popup,
                                        (MetaTabEntryKey) initial_selection->xwindow);
              
              if (show_popup)
                meta_ui_tab_popup_set_showing (screen->tab_popup,
                                               TRUE);
              else
                {
                  meta_window_raise (initial_selection);
                  initial_selection->tab_unminimized = 
                    initial_selection->minimized;
                  meta_window_unminimize (initial_selection);
                }
            }
        }
    }
}

static void
handle_tab_forward (MetaDisplay     *display,
                    MetaScreen      *screen,
                    MetaWindow      *event_window,
                    XDeviceKeyEvent *xdke,
                    MetaKeyBinding  *binding)
{
  do_choose_window (display, screen,
                    event_window, xdke, binding, FALSE, TRUE);
}

static void
handle_tab_backward (MetaDisplay     *display,
                     MetaScreen      *screen,
                     MetaWindow      *event_window,
                     XDeviceKeyEvent *xdke,
                     MetaKeyBinding  *binding)
{
  do_choose_window (display, screen,
                    event_window, xdke, binding, TRUE, TRUE);
}

static void
handle_cycle_forward (MetaDisplay     *display,
                      MetaScreen      *screen,
                      MetaWindow      *event_window,
                      XDeviceKeyEvent *xdke,
                      MetaKeyBinding  *binding)
{
  do_choose_window (display, screen,
                    event_window, xdke, binding, FALSE, FALSE); 
}

static void
handle_cycle_backward (MetaDisplay     *display,
                       MetaScreen      *screen,
                       MetaWindow      *event_window,
                       XDeviceKeyEvent *xdke,
                       MetaKeyBinding  *binding)
{
  do_choose_window (display, screen,
                    event_window, xdke, binding, TRUE, FALSE); 
}

static void
handle_toggle_fullscreen  (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           XDeviceKeyEvent *xdke,
                           MetaKeyBinding  *binding)
{
  MetaDevInfo *dev;

  dev = meta_devices_find_keyboard_by_id (display, xdke->deviceid);

  if (window)
    {
      if (window->fullscreen)
        meta_window_unmake_fullscreen (window, dev);
      else if (window->has_fullscreen_func)
        meta_window_make_fullscreen (window, dev);
    }
}

static void
handle_toggle_above       (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           XDeviceKeyEvent *xdke,
                           MetaKeyBinding  *binding)
{
  if (window)
    {
      if (window->wm_state_above)
        meta_window_unmake_above (window);
      else
        meta_window_make_above (window);
    }
}

static void
handle_toggle_maximize    (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           XDeviceKeyEvent *xdke,
                           MetaKeyBinding  *binding)
{
  MetaDevInfo *dev;

  dev = meta_devices_find_keyboard_by_id (display, xdke->deviceid);

  if (window)
    {
      if (META_WINDOW_MAXIMIZED (window))
        meta_window_unmaximize (window, dev,
                                META_MAXIMIZE_HORIZONTAL |
                                META_MAXIMIZE_VERTICAL);
      else if (window->has_maximize_func)
        meta_window_maximize (window, dev,
                              META_MAXIMIZE_HORIZONTAL |
                              META_MAXIMIZE_VERTICAL);
    }
}

static void
handle_maximize           (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           XDeviceKeyEvent *xdke,
                           MetaKeyBinding  *binding)
{
  if (window)
    {
      if (window->has_maximize_func)
        meta_window_maximize (window,
			      meta_devices_find_keyboard_by_id (display,
					                        xdke->deviceid),

                              META_MAXIMIZE_HORIZONTAL |
                              META_MAXIMIZE_VERTICAL);
    }
}

static void
handle_unmaximize         (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           XDeviceKeyEvent *xdke,
                           MetaKeyBinding  *binding)
{
  MetaDevInfo *dev;

  dev = meta_devices_find_keyboard_by_id (display, xdke->deviceid);

  if (window)
    {
      if (window->maximized_vertically || window->maximized_horizontally)
        meta_window_unmaximize (window, dev,
                                META_MAXIMIZE_HORIZONTAL |
                                META_MAXIMIZE_VERTICAL);
    }
}

static void
handle_toggle_shade       (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           XDeviceKeyEvent *xdke,
                           MetaKeyBinding  *binding)
{
  MetaDevInfo *dev;

  dev = meta_devices_find_keyboard_by_id (display, xdke->deviceid);
  if (window)
    {
      if (window->shaded)
        meta_window_unshade (window, dev, xdke->time);
      else if (window->has_shade_func)
        meta_window_shade (window, dev, xdke->time);
    }
}

static void
handle_close_window       (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           XDeviceKeyEvent *xdke,
                           MetaKeyBinding  *binding)
{
  if (window)
     if (window->has_close_func)
       meta_window_delete (window, xdke->time);
}

static void
handle_minimize_window (MetaDisplay     *display,
                        MetaScreen      *screen,
                        MetaWindow      *window,
                        XDeviceKeyEvent *xdke,
                        MetaKeyBinding  *binding)
{
  if (window)
     if (window->has_minimize_func)
       meta_window_minimize (window);
}

static void
handle_begin_move         (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           XDeviceKeyEvent *xdke,
                           MetaKeyBinding  *binding)
{
  /* XXX Shouldnt we grab the mouse? (not the keyboard) */
#if 0
  int idev;
  MetaDevInfo *dev;
  for (idev = 0; idev < display->devices->keybsUsed; idev++)
    if (((XDeviceKeyEvent*)event)->deviceid ==
      	display->devices->keyboards[idev].xdev->device_id)
      dev = &display->devices->keyboards[idev];
#endif
  MetaDevInfo *dev;
  dev = meta_devices_find_keyboard_by_id (display, xdke->deviceid);

  if (window && window->has_move_func)
    {
      meta_window_begin_grab_op (window,
      				 dev,
                                 META_GRAB_OP_KEYBOARD_MOVING,
                                 FALSE,
                                 xdke->time);
    }
}

static void
handle_begin_resize       (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           XDeviceKeyEvent *xdke,
                           MetaKeyBinding  *binding)
{
   /* XXX Shouldnt we grab the mouse? (not the keyboard) */
#if 0
  int idev;
  MetaDevInfo *dev;
  for (idev = 0; idev < display->devices->keybsUsed; idev++)
    if (((XDeviceKeyEvent*)event)->deviceid ==
      	display->devices->keyboards[idev].xdev->device_id)
      dev = &display->devices->keyboards[idev];
#endif
  MetaDevInfo *dev;
  dev = meta_devices_find_keyboard_by_id (display, xdke->deviceid);

  if (window && window->has_resize_func)
    {
      meta_window_begin_grab_op (window,
      				 dev,
                                 META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN,
                                 FALSE,
                                 xdke->time);
    }
}

static void
handle_toggle_sticky      (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           XDeviceKeyEvent *xdke,
                           MetaKeyBinding  *binding)
{
  if (window)
    {
      if (window->on_all_workspaces)
        meta_window_unstick (window);
      else
        meta_window_stick (window);
    }
}

static void
do_handle_move_to_workspace  (MetaDisplay     *display,
                              MetaScreen      *screen,
                              MetaWindow      *window,
                              XDeviceKeyEvent *xdke,
                              MetaKeyBinding  *binding,
                              gboolean         flip)
{
  int which;
  MetaWorkspace *workspace;
  MetaDevInfo   *dev;
  
  which = GPOINTER_TO_INT (binding->handler->data);

  if (window == NULL || window->always_sticky)
    return;
  
  workspace = NULL;
  if (which < 0)
    {      
      workspace = meta_workspace_get_neighbor (screen->active_workspace,
                                               which);
    }
  else
    {
      workspace = meta_screen_get_workspace_by_index (screen, which);
    }
  
  if (workspace)
    {
      /* Activate second, so the window is never unmapped */
      meta_window_change_workspace (window, workspace);
      if (flip)
        {
          meta_topic (META_DEBUG_FOCUS,
                      "Resetting mouse_mode to FALSE due to "
                      "do_handle_move_to_workspace() call with flip set.\n");
          workspace->screen->display->mouse_mode = FALSE;
	  dev = meta_devices_find_keyboard_by_id (display, xdke->deviceid);
          meta_workspace_activate_with_focus (workspace,
	  				      dev,
                                              window,
                                              xdke->time);
        }
    }
  else
    {
      /* We could offer to create it I suppose */
    }  
}

static void
handle_move_to_workspace  (MetaDisplay     *display,
                           MetaScreen      *screen,
                           MetaWindow      *window,
                           XDeviceKeyEvent *xdke,
                           MetaKeyBinding  *binding)
{
  do_handle_move_to_workspace (display,
                               screen,
                               window,
                               xdke,
                               binding,
                               FALSE);
}
static void
handle_move_to_workspace_flip  (MetaDisplay     *display,
                                MetaScreen      *screen,
                                MetaWindow      *window,
                                XDeviceKeyEvent *xdke,
                                MetaKeyBinding  *binding)
{
  do_handle_move_to_workspace (display,
                               screen,
                               window,
                               xdke,
                               binding,
                               TRUE);
}

static void 
handle_raise_or_lower (MetaDisplay     *display,
                       MetaScreen      *screen,
		       MetaWindow      *window,
		       XDeviceKeyEvent *xdke,
		       MetaKeyBinding  *binding)
{
  /* Get window at pointer */
  
  if (window)
    {
      MetaWindow *above = NULL;
      
      /* Check if top */
      if (meta_stack_get_top (window->screen->stack) == window)
	{
	  meta_window_lower (window);
	  return;
	}
      
      /* else check if windows in same layer are intersecting it */
      
      above = meta_stack_get_above (window->screen->stack, window, TRUE); 

      while (above)
	{
	  MetaRectangle tmp, win_rect, above_rect;

          if (above->mapped)
            {
              meta_window_get_outer_rect (window, &win_rect);
              meta_window_get_outer_rect (above, &above_rect);
              
              /* Check if obscured */
              if (meta_rectangle_intersect (&win_rect, &above_rect, &tmp))
                {
                  meta_window_raise (window);
                  return;
                }
            }
	  
	  above = meta_stack_get_above (window->screen->stack, above, TRUE); 
	}

      /* window is not obscured */
      meta_window_lower (window);
    }
}

static void
handle_raise (MetaDisplay     *display,
              MetaScreen      *screen,
              MetaWindow      *window,
              XDeviceKeyEvent *xdke,
              MetaKeyBinding  *binding)
{
  if (window)
    {
      meta_window_raise (window);
    }
}

static void
handle_lower (MetaDisplay     *display,
              MetaScreen      *screen,
              MetaWindow      *window,
              XDeviceKeyEvent *xdke,
              MetaKeyBinding  *binding)
{
  if (window)
    {
      meta_window_lower (window);
    }
}

static void
handle_workspace_switch  (MetaDisplay     *display,
                          MetaScreen      *screen,
                          MetaWindow      *window,
                          XDeviceKeyEvent *xdke,
                          MetaKeyBinding  *binding)
{
  int motion;
  unsigned int grab_mask;
  
  MetaDevInfo *kbdDev;
  MetaDevInfo *pairedPtr;

  kbdDev = meta_devices_find_keyboard_by_id  (display, xdke->deviceid);
  pairedPtr = meta_devices_find_paired_mouse (display, xdke->deviceid);
  
     
  motion = GPOINTER_TO_INT (binding->handler->data);

  g_assert (motion < 0); 

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Starting tab between workspaces, showing popup\n");

  /* FIXME should we use binding->mask ? */
  grab_mask = xdke->state & ~(display->ignored_modifier_mask);
  
  if (meta_display_begin_grab_op (display,
                                  screen,
				  pairedPtr,
                                  NULL,
                                  META_GRAB_OP_KEYBOARD_WORKSPACE_SWITCHING,
                                  FALSE,
                                  FALSE,
                                  0,
                                  grab_mask,
                                  xdke->time,
                                  0, 0))
    {
      MetaWorkspace *next;
      gboolean grabbed_before_release;
      
      next = meta_workspace_get_neighbor (screen->active_workspace, motion);
      g_assert (next); 

      grabbed_before_release = primary_modifier_still_pressed (display, pairedPtr, grab_mask);
      
      meta_topic (META_DEBUG_KEYBINDINGS,
		  "Activating target workspace\n");

      if (!grabbed_before_release)
        {
          /* end the grab right away, modifier possibly released
           * before we could establish the grab and receive the
           * release event. Must end grab before we can switch
           * spaces.
           */
          meta_display_end_grab_op (display, pairedPtr, xdke->time);
        }
      
      meta_workspace_activate (next, kbdDev, xdke->time);

      if (grabbed_before_release)
        {
          meta_ui_tab_popup_select (screen->tab_popup, (MetaTabEntryKey) next);
          
          /* only after selecting proper space */
          meta_ui_tab_popup_set_showing (screen->tab_popup, TRUE);
        }
    }
}

static void
handle_spew_mark (MetaDisplay     *display,
                  MetaScreen      *screen,
                  MetaWindow      *window,
                  XDeviceKeyEvent *xdke,
                  MetaKeyBinding  *binding)
{
  meta_verbose ("-- MARK MARK MARK MARK --\n");
}

void
meta_set_keybindings_disabled (gboolean setting)
{
  all_bindings_disabled = setting;
  meta_topic (META_DEBUG_KEYBINDINGS,
              "Keybindings %s\n", all_bindings_disabled ? "disabled" : "enabled");
}

static void
handle_run_terminal (MetaDisplay     *display,
                     MetaScreen      *screen,
                     MetaWindow      *window,
                     XDeviceKeyEvent *xdke,
                     MetaKeyBinding  *binding)
{
  const char *command;
  GError *err;

  command = meta_prefs_get_terminal_command ();

  if (command == NULL)
    {
      char *s;

      meta_topic (META_DEBUG_KEYBINDINGS,
                  "No terminal command to run in response to "
		  "keybinding press\n");
      
      s = g_strdup_printf (_("No terminal command has been defined.\n"));
      error_on_terminal_command (NULL, s, screen->number, xdke->time);
      g_free (s);
      
      return;
    }

  err = NULL;
  if (!meta_spawn_command_line_async_on_screen (command, screen, &err))
    {
      error_on_terminal_command (command, err->message, screen->number,
		      		 xdke->time);
      
      g_error_free (err);
    }
}
