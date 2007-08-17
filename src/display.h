/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity X display handler */

/* 
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002 Red Hat, Inc.
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

#ifndef META_DISPLAY_H
#define META_DISPLAY_H

#ifndef PACKAGE
#error "config.h not included"
#endif

#include <glib.h>
#include <X11/Xlib.h>
#include "eventqueue.h"
#include "common.h"
#include "boxes.h"

#ifdef MPX
#include "devices.h"
#endif

#ifdef HAVE_STARTUP_NOTIFICATION
#include <libsn/sn.h>
#endif

#ifdef HAVE_XSYNC
#include <X11/extensions/sync.h>
#endif

#define meta_XFree(p) do { if ((p)) XFree ((p)); } while (0)

typedef struct MetaCompositor  MetaCompositor;
/* typedef struct _MetaDisplay    MetaDisplay; XXX */
typedef struct _MetaFrame      MetaFrame;
typedef struct _MetaKeyBinding MetaKeyBinding;
typedef struct _MetaScreen     MetaScreen;
typedef struct _MetaStack      MetaStack;
typedef struct _MetaUISlave    MetaUISlave;
typedef struct _MetaWindow     MetaWindow;
typedef struct _MetaWorkspace  MetaWorkspace;

typedef struct _MetaWindowPropHooks MetaWindowPropHooks;
typedef struct _MetaGroupPropHooks  MetaGroupPropHooks;

typedef struct MetaEdgeResistanceData MetaEdgeResistanceData;

typedef void (* MetaWindowPingFunc) (MetaDisplay *display,
				     Window       xwindow,
				     guint32      timestamp,
				     gpointer     user_data);


#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

/* This is basically a bogus number, just has to be large enough
 * to handle the expected case of the alt+tab operation, where
 * we want to ignore serials from UnmapNotify on the tab popup,
 * and the LeaveNotify/EnterNotify from the pointer ungrab
 */
#define N_IGNORED_SERIALS           4

struct _MetaDisplay
{
  char *name;
  Display *xdisplay;

  Window leader_window;
  Window timestamp_pinging_window;
  
  Atom atom_net_wm_name;
  Atom atom_wm_protocols;
  Atom atom_wm_take_focus;
  Atom atom_wm_delete_window;
  Atom atom_wm_state;
  Atom atom_net_close_window;
  Atom atom_net_wm_state;
  Atom atom_motif_wm_hints;
  Atom atom_net_wm_state_shaded;
  Atom atom_net_wm_state_maximized_horz;
  Atom atom_net_wm_state_maximized_vert;
  Atom atom_net_wm_desktop;
  Atom atom_net_number_of_desktops;
  Atom atom_wm_change_state;
  Atom atom_sm_client_id;
  Atom atom_wm_client_leader;
  Atom atom_wm_window_role;
  Atom atom_net_current_desktop;
  Atom atom_net_supporting_wm_check;
  Atom atom_net_supported;
  Atom atom_net_wm_window_type;
  Atom atom_net_wm_window_type_desktop;
  Atom atom_net_wm_window_type_dock;
  Atom atom_net_wm_window_type_toolbar;
  Atom atom_net_wm_window_type_menu;
  Atom atom_net_wm_window_type_dialog;
  Atom atom_net_wm_window_type_normal;
  Atom atom_net_wm_state_modal;
  Atom atom_net_client_list;
  Atom atom_net_client_list_stacking;
  Atom atom_net_wm_state_skip_taskbar;
  Atom atom_net_wm_state_skip_pager;
  Atom atom_net_wm_icon_name;
  Atom atom_net_wm_icon;
  Atom atom_net_wm_icon_geometry;
  Atom atom_utf8_string;
  Atom atom_wm_icon_size;
  Atom atom_kwm_win_icon;
  Atom atom_net_wm_moveresize;
  Atom atom_net_active_window;
  Atom atom_metacity_restart_message;
  Atom atom_net_wm_strut;
  Atom atom_metacity_reload_theme_message;
  Atom atom_metacity_set_keybindings_message;
  Atom atom_net_wm_state_hidden;
  Atom atom_net_wm_window_type_utility;
  Atom atom_net_wm_window_type_splash;
  Atom atom_net_wm_ping;
  Atom atom_net_wm_pid;
  Atom atom_wm_client_machine;
  Atom atom_net_wm_state_fullscreen;
  Atom atom_net_workarea;
  Atom atom_net_showing_desktop;
  Atom atom_net_desktop_layout;
  Atom atom_manager;
  Atom atom_targets;
  Atom atom_multiple;
  Atom atom_timestamp;
  Atom atom_version;
  Atom atom_atom_pair;
  Atom atom_net_desktop_names;
  Atom atom_net_wm_allowed_actions;
  Atom atom_net_wm_action_move;
  Atom atom_net_wm_action_resize;
  Atom atom_net_wm_action_fullscreen;
  Atom atom_net_wm_action_minimize;
  Atom atom_net_wm_action_shade;
  Atom atom_net_wm_action_stick;
  Atom atom_net_wm_action_maximize_horz;
  Atom atom_net_wm_action_maximize_vert;
  Atom atom_net_wm_action_change_desktop;
  Atom atom_net_wm_action_close;
  Atom atom_net_wm_action_above;
  Atom atom_net_wm_action_below;
  Atom atom_net_wm_state_above;
  Atom atom_net_wm_state_below;
  Atom atom_net_startup_id;
  Atom atom_metacity_toggle_verbose;
  Atom atom_net_wm_sync_request;
  Atom atom_net_wm_sync_request_counter;
  Atom atom_gnome_panel_action;
  Atom atom_gnome_panel_action_main_menu;
  Atom atom_gnome_panel_action_run_dialog;
  Atom atom_metacity_sentinel;
  Atom atom_net_wm_strut_partial;
  Atom atom_net_frame_extents;
  Atom atom_net_request_frame_extents;
  Atom atom_net_wm_user_time;
  Atom atom_net_wm_state_demands_attention;
  Atom atom_net_restack_window;
  Atom atom_net_moveresize_window;
  Atom atom_net_desktop_geometry;
  Atom atom_net_desktop_viewport;
  Atom atom_metacity_version;
  Atom atom_net_wm_visible_name;
  Atom atom_net_wm_visible_icon_name;
  Atom atom_net_wm_user_time_window;

  /* This is the actual window from focus events,
   * not the one we last set
   */
  MetaWindow *focus_window; /* XXX Make this per-device */

  /* window we are expecting a FocusIn event for or the current focus
   * window if we are not expecting any FocusIn/FocusOut events; not
   * perfect because applications can call XSetInputFocus directly.
   * (It could also be messed up if a timestamp later than current
   * time is sent to meta_display_set_input_focus_window, though that
   * would be a programming error).  See bug 154598 for more info.
   */
  MetaWindow *expected_focus_window; /* XXX Make this per-device */

  /* last timestamp passed to XSetDeviceFocus */
  guint32 last_focus_time;

  /* last user interaction time in any app */
  guint32 last_user_time;

  /* whether we're using mousenav (only relevant for sloppy&mouse focus modes;
   * !mouse_mode means "keynav mode")
   */
  guint mouse_mode : 1;

  /* Helper var used when focus_new_windows setting is 'strict'; only
   * relevant in 'strict' mode and if the focus window is a terminal.
   * In that case, we don't allow new windows to take focus away from
   * a terminal, but if the user explicitly did something that should
   * allow a different window to gain focus (e.g. global keybinding or
   * clicking on a dock), then we will allow the transfer.
   */
  guint allow_terminal_deactivation : 1;

  guint static_gravity_works : 1;
  
  /*< private-ish >*/
  guint error_trap_synced_at_last_pop : 1;
  MetaEventQueue *events;
  GSList *screens;
  MetaScreen *active_screen;
  GHashTable *window_ids;
  int error_traps;
  int (* error_trap_handler) (Display     *display,
                              XErrorEvent *error);  
  int server_grab_count;

  /* serials of leave/unmap events that may
   * correspond to an enter event we should
   * ignore
   */
  unsigned long ignored_serials[N_IGNORED_SERIALS];
  Window ungrab_should_not_cause_focus_window;
  
  guint32 current_time;

  /* Pings which we're waiting for a reply from */
  GSList     *pending_pings;

  /* Pending autoraise */
  guint       autoraise_timeout_id;
  MetaWindow* autoraise_window;

  /* Alt+click button grabs */
  unsigned int window_grab_modifiers;
  
  /* current window operation */
  MetaGrabOp  grab_op;
  MetaScreen *grab_screen;
  MetaWindow *grab_window;
  Window      grab_xwindow;
  int         grab_button;
  int         grab_anchor_root_x;
  int         grab_anchor_root_y;
  MetaRectangle grab_anchor_window_pos;
  int         grab_latest_motion_x;
  int         grab_latest_motion_y;
  gulong      grab_mask;
  guint       grab_have_pointer : 1;
  guint	      grab_grabbed_pointer : 1;
  guint       grab_have_keyboard : 1;
  guint       grab_wireframe_active : 1;
  guint       grab_was_cancelled : 1;    /* Only used in wireframe mode */
  guint       grab_frame_action : 1;
  MetaRectangle grab_wireframe_rect;
  MetaRectangle grab_wireframe_last_xor_rect;
  MetaRectangle grab_initial_window_pos;
  int         grab_initial_x, grab_initial_y;  /* These are only relevant for */
  gboolean    grab_threshold_movement_reached; /* raise_on_click == FALSE.    */
  MetaResizePopup *grab_resize_popup;
  GTimeVal    grab_last_moveresize_time;
  guint32     grab_motion_notify_time;
  int         grab_wireframe_last_display_width;
  int         grab_wireframe_last_display_height;
  GList*      grab_old_window_stacking;
  MetaEdgeResistanceData *grab_edge_resistance_data;
  unsigned int grab_last_user_action_was_snap;

  /* we use property updates as sentinels for certain window focus events
   * to avoid some race conditions on EnterNotify events
   */
  int         sentinel_counter;

#ifdef HAVE_XKB
  int         xkb_base_event_type;
  guint32     last_bell_time;
#endif
#ifdef HAVE_XSYNC
  /* alarm monitoring client's _NET_WM_SYNC_REQUEST_COUNTER */
  XSyncAlarm  grab_sync_request_alarm;
#endif
  int	      grab_resize_timeout_id;

  /* Keybindings stuff */
  MetaKeyBinding *screen_bindings;
  int             n_screen_bindings;
  MetaKeyBinding *window_bindings;
  int             n_window_bindings;
  int             min_keycode;
  int             max_keycode;
  KeySym *keymap;
  int keysyms_per_keycode;
  XModifierKeymap *modmap;
  unsigned int ignored_modifier_mask;
  unsigned int num_lock_mask;
  unsigned int scroll_lock_mask;
  unsigned int hyper_mask;
  unsigned int super_mask;
  unsigned int meta_mask;
  
  /* Xinerama cache */
  unsigned int xinerama_cache_invalidated : 1;

  /* Opening the display */
  unsigned int display_opening : 1;

  /* Closing down the display */
  int closing;

  /* Managed by group.c */
  GHashTable *groups_by_leader;

  /* currently-active window menu if any */
  MetaWindowMenu *window_menu;
  MetaWindow *window_with_menu;

  /* Managed by window-props.c */
  MetaWindowPropHooks *prop_hooks;

  /* Managed by group-props.c */
  MetaGroupPropHooks *group_prop_hooks;

  /* Managed by compositor.c */
  MetaCompositor *compositor;
  
#ifdef HAVE_STARTUP_NOTIFICATION
  SnDisplay *sn_display;
#endif
#ifdef HAVE_XSYNC
  int xsync_event_base;
  int xsync_error_base;
#endif
#ifdef HAVE_SHAPE
  int shape_event_base;
  int shape_error_base;
#endif
#ifdef HAVE_RENDER
  int render_event_base;
  int render_error_base;
#endif
#ifdef HAVE_XSYNC
  unsigned int have_xsync : 1;
#define META_DISPLAY_HAS_XSYNC(display) ((display)->have_xsync)
#else
#define META_DISPLAY_HAS_XSYNC(display) FALSE
#endif
#ifdef HAVE_SHAPE
  unsigned int have_shape : 1;
#define META_DISPLAY_HAS_SHAPE(display) ((display)->have_shape)
#else
#define META_DISPLAY_HAS_SHAPE(display) FALSE
#endif
#ifdef HAVE_RENDER
  unsigned int have_render : 1;
#define META_DISPLAY_HAS_RENDER(display) ((display)->have_render)
#else
#define META_DISPLAY_HAS_RENDER(display) FALSE
#endif

#ifdef MPX
  int dev_btn_press_type;
  int dev_btn_release_type;
  int dev_motion_notify_type;
  int dev_ptr_motion_hint_type;
  int dev_focus_in_type;
  int dev_focus_out_type;
  int dev_key_press_type;
  int dev_key_release_type;
  int dev_enter_notify_type;
  int dev_leave_notify_type;
  MetaDevices *devices;
#endif
};

/* Xserver time can wraparound, thus comparing two timestamps needs to take
 * this into account.  Here's a little macro to help out.  If no wraparound
 * has occurred, this is equivalent to
 *   time1 < time2
 * Of course, the rest of the ugliness of this macro comes from accounting
 * for the fact that wraparound can occur and the fact that a timestamp of
 * 0 must be special-cased since it means older than anything else. 
 *
 * Note that this is NOT an equivalent for time1 <= time2; if that's what
 * you need then you'll need to swap the order of the arguments and negate
 * the result.
 */
#define XSERVER_TIME_IS_BEFORE_ASSUMING_REAL_TIMESTAMPS(time1, time2) \
  ( (( (time1) < (time2) ) && ( (time2) - (time1) < ((guint32)-1)/2 )) ||     \
    (( (time1) > (time2) ) && ( (time1) - (time2) > ((guint32)-1)/2 ))        \
  )
#define XSERVER_TIME_IS_BEFORE(time1, time2)                          \
  ( (time1) == 0 ||                                                     \
    (XSERVER_TIME_IS_BEFORE_ASSUMING_REAL_TIMESTAMPS(time1, time2) && \
     (time2) != 0)                                                      \
  )

gboolean      meta_display_open                (void);
void          meta_display_close               (MetaDisplay *display,
                                                guint32      timestamp);
MetaScreen*   meta_display_screen_for_root     (MetaDisplay *display,
                                                Window       xroot);
MetaScreen*   meta_display_screen_for_x_screen (MetaDisplay *display,
                                                Screen      *screen);
MetaScreen*   meta_display_screen_for_xwindow  (MetaDisplay *display,
                                                Window       xindow);
void          meta_display_grab                (MetaDisplay *display);
void          meta_display_ungrab              (MetaDisplay *display);

void          meta_display_unmanage_screen     (MetaDisplay *display,
                                                MetaScreen  *screen,
                                                guint32      timestamp);

void          meta_display_unmanage_windows_for_screen (MetaDisplay *display,
                                                        MetaScreen  *screen,
                                                        guint32      timestamp);

/* Utility function to compare the stacking of two windows */
int           meta_display_stack_cmp           (const void *a,
                                                const void *b);

/* A given MetaWindow may have various X windows that "belong"
 * to it, such as the frame window.
 */
MetaWindow* meta_display_lookup_x_window     (MetaDisplay *display,
                                              Window       xwindow);
void        meta_display_register_x_window   (MetaDisplay *display,
                                              Window      *xwindowp,
                                              MetaWindow  *window);
void        meta_display_unregister_x_window (MetaDisplay *display,
                                              Window       xwindow);
/* Return whether the xwindow is a no focus window for any of the screens */
gboolean    meta_display_xwindow_is_a_no_focus_window (MetaDisplay *display,
                                                       Window xwindow);

GSList*     meta_display_list_windows        (MetaDisplay *display);

MetaDisplay* meta_display_for_x_display  (Display     *xdisplay);
GSList*      meta_displays_list          (void);

Cursor         meta_display_create_x_cursor (MetaDisplay *display,
                                             MetaCursor   cursor);
#ifdef MPX
void     meta_display_set_grab_op_cursor (MetaDisplay *display,
                                          MetaScreen  *screen,
					  MetaDevInfo *dev,
                                          MetaGrabOp   op,
                                          gboolean     change_pointer,
                                          Window       grab_xwindow,
                                          guint32      timestamp);
#else
void     meta_display_set_grab_op_cursor (MetaDisplay *display,
                                          MetaScreen  *screen,
                                          MetaGrabOp   op,
                                          gboolean     change_pointer,
                                          Window       grab_xwindow,
                                          guint32      timestamp);

#endif

#ifdef MPX
gboolean meta_display_begin_grab_op (MetaDisplay *display,
                                     MetaScreen  *screen,
				     MetaDevInfo *dev,
                                     MetaWindow  *window,
                                     MetaGrabOp   op,
                                     gboolean     pointer_already_grabbed,
                                     gboolean     frame_action,
                                     int          button,
                                     gulong       modmask,
                                     guint32      timestamp,
                                     int          root_x,
                                     int          root_y);
#else
gboolean meta_display_begin_grab_op (MetaDisplay *display,
                                     MetaScreen  *screen,
                                     MetaWindow  *window,
                                     MetaGrabOp   op,
                                     gboolean     pointer_already_grabbed,
                                     gboolean     frame_action,
                                     int          button,
                                     gulong       modmask,
                                     guint32      timestamp,
                                     int          root_x,
                                     int          root_y);
#endif
void     meta_display_end_grab_op   (MetaDisplay *display,
				     MetaDevInfo *dev,
                                     guint32      timestamp);

void    meta_display_check_threshold_reached (MetaDisplay *display,
                                              int          x,
                                              int          y);
void     meta_display_grab_window_buttons    (MetaDisplay *display,
                                              Window       xwindow,
					      MetaDevInfo *dev);
void     meta_display_ungrab_window_buttons  (MetaDisplay *display,
                                              Window       xwindow,
					      MetaDevInfo *dev);

void meta_display_grab_focus_window_button   (MetaDisplay *display,
                                              MetaWindow  *window,
					      MetaDevInfo *dev);
void meta_display_ungrab_focus_window_button (MetaDisplay *display,
                                              MetaWindow  *window,
					      MetaDevInfo *dev);

/* Next two functions are defined in edge-resistance.c */
void meta_display_compute_resistance_and_snapping_edges (MetaDisplay *display);
void meta_display_cleanup_edges                         (MetaDisplay *display);

/* make a request to ensure the event serial has changed */
void     meta_display_increment_event_serial (MetaDisplay *display);

void     meta_display_update_active_window_hint (MetaDisplay *display);

guint32  meta_display_get_current_time           (MetaDisplay *display);
guint32  meta_display_get_current_time_roundtrip (MetaDisplay *display);

/* utility goo */
const char* meta_event_mode_to_string   (int m);
const char* meta_event_detail_to_string (int d);

void meta_display_queue_retheme_all_windows (MetaDisplay *display);
void meta_display_retheme_all (void);

void meta_display_set_cursor_theme (const char *theme, 
				    int         size);

void meta_display_ping_window      (MetaDisplay        *display,
                                    MetaWindow         *window,
                                    guint32             timestamp,
                                    MetaWindowPingFunc  ping_reply_func,
                                    MetaWindowPingFunc  ping_timeout_func,
                                    void               *user_data);
gboolean meta_display_window_has_pending_pings (MetaDisplay        *display,
						MetaWindow         *window);

typedef enum
{
  META_TAB_LIST_NORMAL,
  META_TAB_LIST_DOCKS,
  META_TAB_LIST_GROUP
} MetaTabList;

typedef enum
{
  META_TAB_SHOW_ICON,      /* Alt-Tab mode */
  META_TAB_SHOW_INSTANTLY  /* Alt-Esc mode */
} MetaTabShowType;

GList* meta_display_get_tab_list (MetaDisplay   *display,
                                  MetaTabList    type,
                                  MetaScreen    *screen,
                                  MetaWorkspace *workspace);

MetaWindow* meta_display_get_tab_next (MetaDisplay   *display,
                                       MetaTabList    type,
				       MetaScreen    *screen,
                                       MetaWorkspace *workspace,
                                       MetaWindow    *window,
                                       gboolean       backward);

MetaWindow* meta_display_get_tab_current (MetaDisplay   *display,
                                          MetaTabList    type,
                                          MetaScreen    *screen,
                                          MetaWorkspace *workspace);

int meta_resize_gravity_from_grab_op (MetaGrabOp op);

gboolean meta_grab_op_is_moving   (MetaGrabOp op);
gboolean meta_grab_op_is_resizing (MetaGrabOp op);

void meta_display_devirtualize_modifiers (MetaDisplay        *display,
                                          MetaVirtualModifier modifiers,
                                          unsigned int       *mask);

void meta_display_increment_focus_sentinel (MetaDisplay *display);
void meta_display_decrement_focus_sentinel (MetaDisplay *display);
gboolean meta_display_focus_sentinel_clear (MetaDisplay *display);

/* meta_display_set_input_focus_window is like XSetInputFocus, except
 * that (a) it can't detect timestamps later than the current time,
 * since Metacity isn't part of the XServer, and thus gives erroneous
 * behavior in this circumstance (so don't do it), (b) it uses
 * display->last_focus_time since we don't have access to the true
 * Xserver one, (c) it makes use of display->user_time since checking
 * whether a window should be allowed to be focused should depend
 * on user_time events (see bug 167358, comment 15 in particular)
 */
#ifdef MPX
void meta_display_set_input_focus_window   (MetaDisplay *display,
					    MetaDevInfo *dev,
                                            MetaWindow  *window,
                                            gboolean     focus_frame,
                                            guint32      timestamp);
#else
void meta_display_set_input_focus_window   (MetaDisplay *display,
                                            MetaWindow  *window,
                                            gboolean     focus_frame,
                                            guint32      timestamp);
#endif

/* meta_display_focus_the_no_focus_window is called when the
 * designated no_focus_window should be focused, but is otherwise the
 * same as meta_display_set_input_focus_window
 */
#ifdef MPX
void meta_display_focus_the_no_focus_window (MetaDisplay *display,
					     MetaDevInfo *dev,
                                             MetaScreen  *screen,
                                             guint32      timestamp);
#else
void meta_display_focus_the_no_focus_window (MetaDisplay *display,
                                             MetaScreen  *screen,
                                             guint32      timestamp);
#endif

void meta_display_queue_autoraise_callback  (MetaDisplay *display,
                                             MetaWindow  *window);
void meta_display_remove_autoraise_callback (MetaDisplay *display);

#endif
