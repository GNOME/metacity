/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity preferences */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2006 Elijah Newren
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

#ifndef META_PREFS_H
#define META_PREFS_H

/* This header is a "common" one between the UI and core side */
#include "common.h"
#include "meta-compositor.h"
#include "types.h"
#include <libmetacity/meta-theme.h>
#include <pango/pango-font.h>
#include <gdesktop-enums.h>

/* Keep in sync with GSettings schemas! */
typedef enum
{
  META_PREF_MOUSE_BUTTON_MODS,
  META_PREF_FOCUS_MODE,
  META_PREF_FOCUS_NEW_WINDOWS,
  META_PREF_ATTACH_MODAL_DIALOGS,
  META_PREF_RAISE_ON_CLICK,
  META_PREF_ACTION_DOUBLE_CLICK_TITLEBAR,
  META_PREF_ACTION_MIDDLE_CLICK_TITLEBAR,
  META_PREF_ACTION_RIGHT_CLICK_TITLEBAR,
  META_PREF_AUTO_RAISE,
  META_PREF_AUTO_RAISE_DELAY,
  META_PREF_THEME_NAME,
  META_PREF_THEME_TYPE,
  META_PREF_TITLEBAR_FONT,
  META_PREF_NUM_WORKSPACES,
  META_PREF_KEYBINDINGS,
  META_PREF_DISABLE_WORKAROUNDS,
  META_PREF_BUTTON_LAYOUT,
  META_PREF_WORKSPACE_NAMES,
  META_PREF_VISUAL_BELL,
  META_PREF_AUDIBLE_BELL,
  META_PREF_VISUAL_BELL_TYPE,
  META_PREF_GNOME_ACCESSIBILITY,
  META_PREF_GNOME_ANIMATIONS,
  META_PREF_CURSOR_THEME,
  META_PREF_CURSOR_SIZE,
  META_PREF_COMPOSITOR,
  META_PREF_RESIZE_WITH_RIGHT_BUTTON,
  META_PREF_EDGE_TILING,
  META_PREF_FORCE_FULLSCREEN,
  META_PREF_PLACEMENT_MODE,
  META_PREF_ALT_TAB_THUMBNAILS
} MetaPreference;

typedef enum
{
  META_PLACEMENT_MODE_SMART,
  META_PLACEMENT_MODE_CASCADE,
  META_PLACEMENT_MODE_CENTER,
  META_PLACEMENT_MODE_ORIGIN,
  META_PLACEMENT_MODE_RANDOM
} MetaPlacementMode;

typedef void (* MetaPrefsChangedFunc) (MetaPreference pref,
                                       gpointer       data);

void meta_prefs_add_listener    (MetaPrefsChangedFunc func,
                                 gpointer             data);
void meta_prefs_remove_listener (MetaPrefsChangedFunc func,
                                 gpointer             data);

void meta_prefs_init (void);
const char* meta_preference_to_string (MetaPreference pref);

MetaVirtualModifier         meta_prefs_get_mouse_button_mods  (void);
guint                       meta_prefs_get_mouse_button_resize (void);
guint                       meta_prefs_get_mouse_button_menu  (void);
GDesktopFocusMode           meta_prefs_get_focus_mode         (void);
GDesktopFocusNewWindows     meta_prefs_get_focus_new_windows  (void);
gboolean                    meta_prefs_get_attach_modal_dialogs (void);
gboolean                    meta_prefs_get_raise_on_click     (void);

const gchar                *meta_prefs_get_theme_name         (void);
MetaThemeType               meta_prefs_get_theme_type         (void);

/* returns NULL if GTK default should be used */
const PangoFontDescription* meta_prefs_get_titlebar_font      (void);
int                         meta_prefs_get_num_workspaces     (void);
gboolean                    meta_prefs_get_disable_workarounds (void);
gboolean                    meta_prefs_get_auto_raise         (void);
int                         meta_prefs_get_auto_raise_delay   (void);
gboolean                    meta_prefs_get_gnome_accessibility (void);
gboolean                    meta_prefs_get_gnome_animations   (void);
gboolean                    meta_prefs_get_edge_tiling        (void);

const char*                 meta_prefs_get_screenshot_command (void);

const char*                 meta_prefs_get_window_screenshot_command (void);

const char*                 meta_prefs_get_terminal_command   (void);

const gchar*                meta_prefs_get_button_layout (void);

/* Double, right, middle click can be configured to any titlebar meta-action */
GDesktopTitlebarAction       meta_prefs_get_action_double_click_titlebar (void);
GDesktopTitlebarAction       meta_prefs_get_action_middle_click_titlebar (void);
GDesktopTitlebarAction       meta_prefs_get_action_right_click_titlebar (void);

void meta_prefs_set_num_workspaces (int n_workspaces);

const char* meta_prefs_get_workspace_name    (int         i);
void        meta_prefs_change_workspace_name (int         i,
                                              const char *name);

const char* meta_prefs_get_cursor_theme      (void);
int         meta_prefs_get_cursor_size       (void);
gboolean    meta_prefs_get_force_fullscreen  (void);

MetaPlacementMode meta_prefs_get_placement_mode (void);

gboolean    meta_prefs_get_alt_tab_thumbnails (void);

MetaCompositorType meta_prefs_get_compositor (void);

void               meta_prefs_set_compositor (MetaCompositorType compositor);

void meta_prefs_set_force_fullscreen (gboolean whether);

/* XXX FIXME This should be x-macroed, but isn't yet because it would be
 * difficult (or perhaps impossible) to add the suffixes using the current
 * system.  It needs some more thought, perhaps after the current system
 * evolves a little.
 */
typedef enum _MetaKeyBindingAction
{
  META_KEYBINDING_ACTION_NONE = -1,
  META_KEYBINDING_ACTION_WORKSPACE_1,
  META_KEYBINDING_ACTION_WORKSPACE_2,
  META_KEYBINDING_ACTION_WORKSPACE_3,
  META_KEYBINDING_ACTION_WORKSPACE_4,
  META_KEYBINDING_ACTION_WORKSPACE_5,
  META_KEYBINDING_ACTION_WORKSPACE_6,
  META_KEYBINDING_ACTION_WORKSPACE_7,
  META_KEYBINDING_ACTION_WORKSPACE_8,
  META_KEYBINDING_ACTION_WORKSPACE_9,
  META_KEYBINDING_ACTION_WORKSPACE_10,
  META_KEYBINDING_ACTION_WORKSPACE_11,
  META_KEYBINDING_ACTION_WORKSPACE_12,
  META_KEYBINDING_ACTION_WORKSPACE_LEFT,
  META_KEYBINDING_ACTION_WORKSPACE_RIGHT,
  META_KEYBINDING_ACTION_WORKSPACE_UP,
  META_KEYBINDING_ACTION_WORKSPACE_DOWN,
  META_KEYBINDING_ACTION_SWITCH_APPLICATIONS,
  META_KEYBINDING_ACTION_SWITCH_APPLICATIONS_BACKWARD,
  META_KEYBINDING_ACTION_SWITCH_GROUP,
  META_KEYBINDING_ACTION_SWITCH_GROUP_BACKWARD,
  META_KEYBINDING_ACTION_SWITCH_WINDOWS,
  META_KEYBINDING_ACTION_SWITCH_WINDOWS_BACKWARD,
  META_KEYBINDING_ACTION_SWITCH_PANELS,
  META_KEYBINDING_ACTION_SWITCH_PANELS_BACKWARD,
  META_KEYBINDING_ACTION_CYCLE_GROUP,
  META_KEYBINDING_ACTION_CYCLE_GROUP_BACKWARD,
  META_KEYBINDING_ACTION_CYCLE_WINDOWS,
  META_KEYBINDING_ACTION_CYCLE_WINDOWS_BACKWARD,
  META_KEYBINDING_ACTION_CYCLE_PANELS,
  META_KEYBINDING_ACTION_CYCLE_PANELS_BACKWARD,
  META_KEYBINDING_ACTION_SHOW_DESKTOP,
  META_KEYBINDING_ACTION_PANEL_MAIN_MENU,
  META_KEYBINDING_ACTION_PANEL_RUN_DIALOG,
  META_KEYBINDING_ACTION_SET_SPEW_MARK,
  META_KEYBINDING_ACTION_ACTIVATE_WINDOW_MENU,
  META_KEYBINDING_ACTION_TOGGLE_FULLSCREEN,
  META_KEYBINDING_ACTION_TOGGLE_MAXIMIZED,
  META_KEYBINDING_ACTION_TOGGLE_TILED_LEFT,
  META_KEYBINDING_ACTION_TOGGLE_TILED_RIGHT,
  META_KEYBINDING_ACTION_TOGGLE_ABOVE,
  META_KEYBINDING_ACTION_MAXIMIZE,
  META_KEYBINDING_ACTION_UNMAXIMIZE,
  META_KEYBINDING_ACTION_MINIMIZE,
  META_KEYBINDING_ACTION_CLOSE,
  META_KEYBINDING_ACTION_BEGIN_MOVE,
  META_KEYBINDING_ACTION_BEGIN_RESIZE,
  META_KEYBINDING_ACTION_TOGGLE_ON_ALL_WORKSPACES,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_1,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_2,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_3,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_4,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_5,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_6,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_7,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_8,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_9,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_10,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_11,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_12,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_LEFT,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_RIGHT,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_UP,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_DOWN,
  META_KEYBINDING_ACTION_RAISE_OR_LOWER,
  META_KEYBINDING_ACTION_RAISE,
  META_KEYBINDING_ACTION_LOWER,
  META_KEYBINDING_ACTION_MAXIMIZE_VERTICALLY,
  META_KEYBINDING_ACTION_MAXIMIZE_HORIZONTALLY,
  META_KEYBINDING_ACTION_MOVE_TO_CORNER_NW,
  META_KEYBINDING_ACTION_MOVE_TO_CORNER_NE,
  META_KEYBINDING_ACTION_MOVE_TO_CORNER_SW,
  META_KEYBINDING_ACTION_MOVE_TO_CORNER_SE,
  META_KEYBINDING_ACTION_MOVE_TO_SIDE_N,
  META_KEYBINDING_ACTION_MOVE_TO_SIDE_S,
  META_KEYBINDING_ACTION_MOVE_TO_SIDE_E,
  META_KEYBINDING_ACTION_MOVE_TO_SIDE_W,
  META_KEYBINDING_ACTION_MOVE_TO_CENTER,
} MetaKeyBindingAction;

typedef enum
{
  META_KEY_BINDING_NONE,
  META_KEY_BINDING_PER_WINDOW  = 1 << 0,
  META_KEY_BINDING_REVERSES    = 1 << 1,
  META_KEY_BINDING_IS_REVERSED = 1 << 2
} MetaKeyBindingFlags;

typedef struct
{
  unsigned int keysym;
  unsigned int keycode;
  MetaVirtualModifier modifiers;
} MetaKeyCombo;

/**
 * MetaKeyHandlerFunc: (skip)
 *
 */
typedef void (* MetaKeyHandlerFunc) (MetaDisplay    *display,
                                     MetaScreen     *screen,
                                     MetaWindow     *window,
                                     XEvent         *event,
                                     MetaKeyBinding *binding);

typedef struct _MetaKeyHandler MetaKeyHandler;

typedef struct
{
  char *name;
  char *schema;

  MetaKeyBindingAction action;

  /**
   * A list of MetaKeyCombos. Each of them is bound to
   * this keypref. If one has keysym==modifiers==0, it is
   * ignored. For historical reasons, the first entry is
   * governed by the pref FOO and the remainder are
   * governed by the pref FOO_list.
   */
  GSList *bindings;

  /** for keybindings that can have shift or not like Alt+Tab */
  gboolean      add_shift:1;

  /** for keybindings that apply only to a window */
  gboolean      per_window:1;
} MetaKeyPref;

GList *meta_prefs_get_keybindings (void);

MetaKeyBindingAction meta_prefs_get_keybinding_action (const char *name);

void meta_prefs_get_window_binding (const char          *name,
                                    unsigned int        *keysym,
                                    MetaVirtualModifier *modifiers);

gboolean           meta_prefs_get_visual_bell      (void);
gboolean           meta_prefs_bell_is_audible      (void);
GDesktopVisualBellType meta_prefs_get_visual_bell_type (void);

#endif
