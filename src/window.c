/* Metacity X managed windows */

/* 
 * Copyright (C) 2001 Havoc Pennington, Anders Carlsson
 * Copyright (C) 2002, 2003 Red Hat, Inc.
 * Copyright (C) 2003 Rob Adams
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
#include "window.h"
#include "util.h"
#include "frame.h"
#include "errors.h"
#include "workspace.h"
#include "stack.h"
#include "keybindings.h"
#include "ui.h"
#include "place.h"
#include "session.h"
#include "effects.h"
#include "prefs.h"
#include "resizepopup.h"
#include "xprops.h"
#include "group.h"
#include "window-props.h"
#include "constraints.h"

#include <X11/Xatom.h>
#include <string.h>

#ifdef HAVE_SHAPE
#include <X11/extensions/shape.h>
#endif

/* Xserver time can wraparound, thus comparing two timestamps needs to take
 * this into account.  Here's a little macro to help out.
 */
#define XSERVER_TIME_IS_LATER(time1, time2)                    \
  ( ((time1 >= time2) && (time1 - time2 < G_MAXULONG / 2)) ||  \
    ((time1 <  time2) && (time2 - time1 > G_MAXULONG / 2))     \
  )

typedef enum
{
  META_IS_CONFIGURE_REQUEST = 1 << 0,
  META_DO_GRAVITY_ADJUST    = 1 << 1,
  META_USER_MOVE_RESIZE     = 1 << 2
} MetaMoveResizeFlags;

static int destroying_windows_disallowed = 0;


static void     update_net_wm_state       (MetaWindow     *window);
static void     update_mwm_hints          (MetaWindow     *window);
static void     update_wm_class           (MetaWindow     *window);
static void     update_transient_for      (MetaWindow     *window);
static void     update_sm_hints           (MetaWindow     *window);
static void     update_role               (MetaWindow     *window);
static void     update_net_wm_type        (MetaWindow     *window);
static void     update_net_frame_extents  (MetaWindow     *window);
static void     recalc_window_type        (MetaWindow     *window);
static void     recalc_window_features    (MetaWindow     *window);
static void     invalidate_work_areas     (MetaWindow     *window);
static void     set_wm_state              (MetaWindow     *window,
                                           int             state);
static void     set_net_wm_state          (MetaWindow     *window);

static void     send_configure_notify     (MetaWindow     *window);
static gboolean process_property_notify   (MetaWindow     *window,
                                           XPropertyEvent *event);
static void     meta_window_show          (MetaWindow     *window);
static void     meta_window_hide          (MetaWindow     *window);

static void     meta_window_save_rect         (MetaWindow    *window);

static void meta_window_move_resize_internal (MetaWindow         *window,
                                              MetaMoveResizeFlags flags,
                                              int                 resize_gravity,
                                              int                 root_x_nw,
                                              int                 root_y_nw,
                                              int                 w,
                                              int                 h);

static void     ensure_mru_position_after (MetaWindow *window,
                                           MetaWindow *after_this_one);


void meta_window_move_resize_now (MetaWindow  *window);

/* FIXME we need an abstraction that covers all these queues. */   

void meta_window_unqueue_calc_showing (MetaWindow *window);
void meta_window_flush_calc_showing   (MetaWindow *window);

void meta_window_unqueue_move_resize  (MetaWindow *window);
void meta_window_flush_move_resize    (MetaWindow *window);

static void meta_window_update_icon_now (MetaWindow *window);
void meta_window_unqueue_update_icon    (MetaWindow *window);
void meta_window_flush_update_icon      (MetaWindow *window);

static void meta_window_apply_session_info (MetaWindow                  *window,
                                            const MetaWindowSessionInfo *info);

#ifdef WITH_VERBOSE_MODE
static const char*
wm_state_to_string (int state)
{
  switch (state)
    {
    case NormalState:
      return "NormalState";      
    case IconicState:
      return "IconicState";
    case WithdrawnState:
      return "WithdrawnState";
    }

  return "Unknown";
}
#endif

static gboolean
is_desktop_or_dock_foreach (MetaWindow *window,
                            void       *data)
{
  gboolean *result = data;

  *result =
    window->type == META_WINDOW_DESKTOP ||
    window->type == META_WINDOW_DOCK;
  if (*result)
    return FALSE; /* stop as soon as we find one */
  else
    return TRUE;
}

/* window is the window that's newly mapped provoking
 * the possible change
 */
static void
maybe_leave_show_desktop_mode (MetaWindow *window)
{
  gboolean is_desktop_or_dock;

  if (!window->screen->active_workspace->showing_desktop)
    return;

  /* If the window is a transient for the dock or desktop, don't
   * leave show desktop mode when the window opens. That's
   * so you can e.g. hide all windows, manipulate a file on
   * the desktop via a dialog, then unshow windows again.
   */
  is_desktop_or_dock = FALSE;
  is_desktop_or_dock_foreach (window,
                              &is_desktop_or_dock);

  meta_window_foreach_ancestor (window, is_desktop_or_dock_foreach,
                                &is_desktop_or_dock);

  if (!is_desktop_or_dock)
    {
      meta_screen_minimize_all_on_active_workspace_except (window->screen,
                                                           window);
      meta_screen_unshow_desktop (window->screen);      
    }
}

MetaWindow*
meta_window_new (MetaDisplay *display,
                 Window       xwindow,
                 gboolean     must_be_viewable)
{
  XWindowAttributes attrs;
  MetaWindow *window;
  
  meta_display_grab (display);
  meta_error_trap_push (display); /* Push a trap over all of window
                                   * creation, to reduce XSync() calls
                                   */
  
  meta_error_trap_push_with_return (display);
  
  XGetWindowAttributes (display->xdisplay,
                        xwindow, &attrs);
  
  if (meta_error_trap_pop_with_return (display, TRUE) != Success)
    {
      meta_verbose ("Failed to get attributes for window 0x%lx\n",
                    xwindow);
      meta_error_trap_pop (display, TRUE);
      meta_display_ungrab (display);
      return NULL;
    }
  window = meta_window_new_with_attrs (display, xwindow,
                                       must_be_viewable, &attrs);


  meta_error_trap_pop (display, FALSE);
  meta_display_ungrab (display);

  return window;
}

MetaWindow*
meta_window_new_with_attrs (MetaDisplay       *display,
                            Window             xwindow,
                            gboolean           must_be_viewable,
                            XWindowAttributes *attrs)
{
  MetaWindow *window;
  GSList *tmp;
  MetaWorkspace *space;
  gulong existing_wm_state;
  gulong event_mask;
#define N_INITIAL_PROPS 13
  Atom initial_props[N_INITIAL_PROPS];
  int i;
  gboolean has_shape;

  g_assert (attrs != NULL);
  g_assert (N_INITIAL_PROPS == (int) G_N_ELEMENTS (initial_props));
  
  meta_verbose ("Attempting to manage 0x%lx\n", xwindow);

  if (xwindow == display->no_focus_window)
    {
      meta_verbose ("Not managing no_focus_window 0x%lx\n",
                    xwindow);
      return NULL;
    }

  if (attrs->override_redirect)
    {
      meta_verbose ("Deciding not to manage override_redirect window 0x%lx\n", xwindow);
      return NULL;
    }
  
  /* Grab server */
  meta_display_grab (display);
  meta_error_trap_push (display); /* Push a trap over all of window
                                   * creation, to reduce XSync() calls
                                   */

  meta_verbose ("must_be_viewable = %d attrs->map_state = %d (%s)\n",
                must_be_viewable,
                attrs->map_state,
                (attrs->map_state == IsUnmapped) ?
                "IsUnmapped" :
                (attrs->map_state == IsViewable) ?
                "IsViewable" :
                (attrs->map_state == IsUnviewable) ?
                "IsUnviewable" :
                "(unknown)");
  
  existing_wm_state = WithdrawnState;
  if (must_be_viewable && attrs->map_state != IsViewable)
    {
      /* Only manage if WM_STATE is IconicState or NormalState */
      gulong state;

      /* WM_STATE isn't a cardinal, it's type WM_STATE, but is an int */
      if (!(meta_prop_get_cardinal_with_atom_type (display, xwindow,
                                                   display->atom_wm_state,
                                                   display->atom_wm_state,
                                                   &state) &&
            (state == IconicState || state == NormalState)))
        {
          meta_verbose ("Deciding not to manage unmapped or unviewable window 0x%lx\n", xwindow);
          meta_error_trap_pop (display, TRUE);
          meta_display_ungrab (display);
          return NULL;
        }

      existing_wm_state = state;
      meta_verbose ("WM_STATE of %lx = %s\n", xwindow,
                    wm_state_to_string (existing_wm_state));
    }
  
  meta_error_trap_push_with_return (display);
  
  XAddToSaveSet (display->xdisplay, xwindow);

  event_mask =
    PropertyChangeMask | EnterWindowMask | LeaveWindowMask |
    FocusChangeMask | ColormapChangeMask;

  XSelectInput (display->xdisplay, xwindow, event_mask);

  has_shape = FALSE;
#ifdef HAVE_SHAPE
  if (META_DISPLAY_HAS_SHAPE (display))
    {
      int x_bounding, y_bounding, x_clip, y_clip;
      unsigned w_bounding, h_bounding, w_clip, h_clip;
      int bounding_shaped, clip_shaped;

      XShapeSelectInput (display->xdisplay, xwindow, ShapeNotifyMask);

      XShapeQueryExtents (display->xdisplay, xwindow,
                          &bounding_shaped, &x_bounding, &y_bounding,
                          &w_bounding, &h_bounding,
                          &clip_shaped, &x_clip, &y_clip,
                          &w_clip, &h_clip);

      has_shape = bounding_shaped != FALSE;

      meta_topic (META_DEBUG_SHAPES,
                  "Window has_shape = %d extents %d,%d %d x %d\n",
                  has_shape, x_bounding, y_bounding,
                  w_bounding, h_bounding);
    }
#endif

  /* Get rid of any borders */
  if (attrs->border_width != 0)
    XSetWindowBorderWidth (display->xdisplay, xwindow, 0);

  /* Get rid of weird gravities */
  if (attrs->win_gravity != NorthWestGravity)
    {
      XSetWindowAttributes set_attrs;
      
      set_attrs.win_gravity = NorthWestGravity;
      
      XChangeWindowAttributes (display->xdisplay,
                               xwindow,
                               CWWinGravity,
                               &set_attrs);
    }
  
  if (meta_error_trap_pop_with_return (display, FALSE) != Success)
    {
      meta_verbose ("Window 0x%lx disappeared just as we tried to manage it\n",
                    xwindow);
      meta_error_trap_pop (display, FALSE);
      meta_display_ungrab (display);
      return NULL;
    }

  g_assert (!attrs->override_redirect);
  
  window = g_new (MetaWindow, 1);

  window->dialog_pid = -1;
  window->dialog_pipe = -1;
  
  window->xwindow = xwindow;
  
  /* this is in window->screen->display, but that's too annoying to
   * type
   */
  window->display = display;
  window->workspaces = NULL;

#ifdef HAVE_XSYNC
  window->sync_request_counter = None;
  window->sync_request_serial = 0;
  window->sync_request_time.tv_sec = 0;
  window->sync_request_time.tv_usec = 0;
#endif
  
  window->screen = NULL;
  tmp = display->screens;
  while (tmp != NULL)
    {
      MetaScreen *scr = tmp->data;

      if (scr->xroot == attrs->root)
        {
          window->screen = tmp->data;
          break;
        }
      
      tmp = tmp->next;
    }
  
  g_assert (window->screen);

  window->desc = g_strdup_printf ("0x%lx", window->xwindow);

  /* avoid tons of stack updates */
  meta_stack_freeze (window->screen->stack);

  window->has_shape = has_shape;
  
  /* Remember this rect is the actual window size */
  window->rect.x = attrs->x;
  window->rect.y = attrs->y;
  window->rect.width = attrs->width;
  window->rect.height = attrs->height;

  /* And border width, size_hints are the "request" */
  window->border_width = attrs->border_width;
  window->size_hints.x = attrs->x;
  window->size_hints.y = attrs->y;
  window->size_hints.width = attrs->width;
  window->size_hints.height = attrs->height;
  /* initialize the remaining size_hints as if size_hints.flags were zero */
  meta_set_normal_hints (window, NULL);

  /* And this is our unmaximized size */
  window->saved_rect = window->rect;
  window->user_rect = window->rect;
  
  window->depth = attrs->depth;
  window->xvisual = attrs->visual;
  window->colormap = attrs->colormap;
  
  window->title = NULL;
  window->icon_name = NULL;
  window->icon = NULL;
  window->mini_icon = NULL;
  meta_icon_cache_init (&window->icon_cache);
  window->wm_hints_pixmap = None;
  window->wm_hints_mask = None;

  window->frame = NULL;
  window->has_focus = FALSE;

  window->user_has_move_resized = FALSE;
  
  window->maximized = FALSE;
  window->maximize_after_placement = FALSE;
  window->fullscreen = FALSE;
  window->on_all_workspaces = FALSE;
  window->shaded = FALSE;
  window->initially_iconic = FALSE;
  window->minimized = FALSE;
  window->iconic = FALSE;
  window->mapped = attrs->map_state != IsUnmapped;
  /* if already mapped we don't want to do the placement thing */
  window->placed = window->mapped;
  if (window->placed)
    meta_topic (META_DEBUG_PLACEMENT,
                "Not placing window 0x%lx since it's already mapped\n",
                xwindow);
  window->unmanaging = FALSE;
  window->calc_showing_queued = FALSE;
  window->move_resize_queued = FALSE;
  window->keys_grabbed = FALSE;
  window->grab_on_frame = FALSE;
  window->all_keys_grabbed = FALSE;
  window->withdrawn = FALSE;
  window->initial_workspace_set = FALSE;
  window->initial_timestamp_set = FALSE;
  window->net_wm_user_time_set = FALSE;
  window->calc_placement = FALSE;
  window->shaken_loose = FALSE;
  window->have_focus_click_grab = FALSE;
  window->disable_sync = FALSE;
  
  window->unmaps_pending = 0;
  
  window->mwm_decorated = TRUE;
  window->mwm_border_only = FALSE;
  window->mwm_has_close_func = TRUE;
  window->mwm_has_minimize_func = TRUE;
  window->mwm_has_maximize_func = TRUE;
  window->mwm_has_move_func = TRUE;
  window->mwm_has_resize_func = TRUE;
  
  window->decorated = TRUE;
  window->has_close_func = TRUE;
  window->has_minimize_func = TRUE;
  window->has_maximize_func = TRUE;
  window->has_move_func = TRUE;
  window->has_resize_func = TRUE;

  window->has_shade_func = TRUE;

  window->has_fullscreen_func = TRUE;

  window->always_sticky = FALSE;
  
  window->wm_state_modal = FALSE;
  window->skip_taskbar = FALSE;
  window->skip_pager = FALSE;
  window->wm_state_skip_taskbar = FALSE;
  window->wm_state_skip_pager = FALSE;
  window->wm_state_above = FALSE;
  window->wm_state_below = FALSE;
  window->wm_state_demands_attention = FALSE;
  
  window->res_class = NULL;
  window->res_name = NULL;
  window->role = NULL;
  window->sm_client_id = NULL;
  window->wm_client_machine = NULL;
  window->startup_id = NULL;
  
  window->net_wm_pid = -1;
  
  window->xtransient_for = None;
  window->xclient_leader = None;
  window->transient_parent_is_root_window = FALSE;
  
  window->type = META_WINDOW_NORMAL;
  window->type_atom = None;

  window->struts = NULL;

  window->using_net_wm_name = FALSE;
  window->using_net_wm_icon_name = FALSE;

  window->need_reread_icon = TRUE;
  window->update_icon_queued = FALSE;
  
  window->layer = META_LAYER_LAST; /* invalid value */
  window->stack_position = -1;
  window->initial_workspace = 0; /* not used */
  window->initial_timestamp = 0; /* not used */
  
  meta_display_register_x_window (display, &window->xwindow, window);


  /* assign the window to its group, or create a new group if needed
   */
  window->group = NULL;
  window->xgroup_leader = None;
  meta_window_compute_group (window);
  
  /* Fill these in the order we want them to be gotten.
   * we want to get window name and class first
   * so we can use them in error messages and such.
   */
  i = 0;
  initial_props[i++] = display->atom_net_wm_name;
  initial_props[i++] = display->atom_wm_client_machine;
  initial_props[i++] = display->atom_net_wm_pid;
  initial_props[i++] = XA_WM_NAME;
  initial_props[i++] = display->atom_net_wm_icon_name;
  initial_props[i++] = XA_WM_ICON_NAME;
  initial_props[i++] = display->atom_net_wm_desktop;
  initial_props[i++] = display->atom_net_startup_id;
  initial_props[i++] = display->atom_net_wm_sync_request_counter;
  initial_props[i++] = XA_WM_NORMAL_HINTS;
  initial_props[i++] = display->atom_wm_protocols;
  initial_props[i++] = XA_WM_HINTS;
  initial_props[i++] = display->atom_net_wm_user_time;
  g_assert (N_INITIAL_PROPS == i);
  
  meta_window_reload_properties (window, initial_props, N_INITIAL_PROPS);

  update_net_wm_state (window);
  
  update_mwm_hints (window);
  update_wm_class (window);
  update_transient_for (window);
  update_sm_hints (window); /* must come after transient_for */
  update_role (window);
  update_net_wm_type (window);
  meta_window_update_icon_now (window);

  if (window->initially_iconic)
    {
      /* WM_HINTS said minimized */
      window->minimized = TRUE;
      meta_verbose ("Window %s asked to start out minimized\n", window->desc);
    }

  if (existing_wm_state == IconicState)
    {
      /* WM_STATE said minimized */
      window->minimized = TRUE;
      meta_verbose ("Window %s had preexisting WM_STATE = IconicState, minimizing\n",
                    window->desc);

      /* Assume window was previously placed, though perhaps it's
       * been iconic its whole life, we have no way of knowing.
       */
      window->placed = TRUE;
    }

  /* Apply any window attributes such as initial workspace
   * based on startup notification
   */
  meta_screen_apply_startup_properties (window->screen, window);
  
  /* FIXME we have a tendency to set this then immediately
   * change it again.
   */
  set_wm_state (window, window->iconic ? IconicState : NormalState);
  set_net_wm_state (window);

  if (window->decorated)
    meta_window_ensure_frame (window);

  meta_window_grab_keys (window);
  meta_display_grab_window_buttons (window->display, window->xwindow);
  meta_display_grab_focus_window_button (window->display, window);

  if (window->type == META_WINDOW_DESKTOP ||
      window->type == META_WINDOW_DOCK)
    {
      /* Change the default, but don't enforce this if the user
       * focuses the dock/desktop and unsticks it using key shortcuts.
       * Need to set this before adding to the workspaces so the MRU
       * lists will be updated.
       */
      window->on_all_workspaces = TRUE;
    }
  
  /* For the workspace, first honor hints,
   * if that fails put transients with parents,
   * otherwise put window on active space
   */
  
  if (window->initial_workspace_set)
    {
      if (window->initial_workspace == (int) 0xFFFFFFFF)
        {
          meta_topic (META_DEBUG_PLACEMENT,
                      "Window %s is initially on all spaces\n",
                      window->desc);
          
	  /* need to set on_all_workspaces first so that it will be
	   * added to all the MRU lists
	   */
          window->on_all_workspaces = TRUE;
          meta_workspace_add_window (window->screen->active_workspace, window);
        }
      else
        {
          meta_topic (META_DEBUG_PLACEMENT,
                      "Window %s is initially on space %d\n",
                      window->desc, window->initial_workspace);

          space =
            meta_screen_get_workspace_by_index (window->screen,
                                                window->initial_workspace);
          
          if (space)
            meta_workspace_add_window (space, window);
        }
    }
  
  if (window->workspaces == NULL && 
      window->xtransient_for != None)
    {
      /* Try putting dialog on parent's workspace */
      MetaWindow *parent;

      parent = meta_display_lookup_x_window (window->display,
                                             window->xtransient_for);

      if (parent)
        {
          GList *tmp_list;

          meta_topic (META_DEBUG_PLACEMENT,
                      "Putting window %s on some workspaces as parent %s\n",
                      window->desc, parent->desc);
          
          if (parent->on_all_workspaces)
            window->on_all_workspaces = TRUE;
          
          tmp_list = parent->workspaces;
          while (tmp_list != NULL)
            {
	      /* this will implicitly add to the appropriate MRU lists
	       */
              meta_workspace_add_window (tmp_list->data, window);
              
              tmp_list = tmp_list->next;
            }
        }
    }
  
  if (window->workspaces == NULL)
    {
      meta_topic (META_DEBUG_PLACEMENT,
                  "Putting window %s on active workspace\n",
                  window->desc);
      
      space = window->screen->active_workspace;

      meta_workspace_add_window (space, window);
    }
  
  /* for the various on_all_workspaces = TRUE possible above */
  meta_window_set_current_workspace_hint (window);
  
  meta_window_update_struts (window);

  /* Put our state back where it should be,
   * passing TRUE for is_configure_request, ICCCM says
   * initial map is handled same as configure request
   */
  meta_window_move_resize_internal (window,
                                    META_IS_CONFIGURE_REQUEST,
                                    NorthWestGravity,
                                    window->size_hints.x,
                                    window->size_hints.y,
                                    window->size_hints.width,
                                    window->size_hints.height);

  meta_stack_add (window->screen->stack, 
                  window);

  /* Now try applying saved stuff from the session */
  {
    const MetaWindowSessionInfo *info;

    info = meta_window_lookup_saved_state (window);

    if (info)
      {
        meta_window_apply_session_info (window, info);
        meta_window_release_saved_state (info);
      }
  }
  
  /* Sync stack changes */
  meta_stack_thaw (window->screen->stack);

  /* disable show desktop mode unless we're a desktop component */
  maybe_leave_show_desktop_mode (window);
  
  meta_window_queue_calc_showing (window);

  meta_error_trap_pop (display, FALSE); /* pop the XSync()-reducing trap */
  meta_display_ungrab (display);
  
  return window;
}

/* This function should only be called from the end of meta_window_new_with_attrs () */
static void
meta_window_apply_session_info (MetaWindow *window,
                                const MetaWindowSessionInfo *info)
{
  if (info->stack_position_set)
    {
      meta_topic (META_DEBUG_SM,
                  "Restoring stack position %d for window %s\n",
                  info->stack_position, window->desc);

      /* FIXME well, I'm not sure how to do this. */
    }

  if (info->minimized_set)
    {
      meta_topic (META_DEBUG_SM,
                  "Restoring minimized state %d for window %s\n",
                  info->minimized, window->desc);

      if (window->has_minimize_func && info->minimized)
        meta_window_minimize (window);
    }

  if (info->maximized_set)
    {
      meta_topic (META_DEBUG_SM,
                  "Restoring maximized state %d for window %s\n",
                  info->maximized, window->desc);
      
      if (window->has_maximize_func && info->maximized)
	{
	  meta_window_maximize (window);

          if (info->saved_rect_set)
            {
              meta_topic (META_DEBUG_SM,
                          "Restoring saved rect %d,%d %dx%d for window %s\n",
                          info->saved_rect.x,
                          info->saved_rect.y,
                          info->saved_rect.width,
                          info->saved_rect.height,
                          window->desc);
              
              window->saved_rect.x = info->saved_rect.x;
              window->saved_rect.y = info->saved_rect.y;
              window->saved_rect.width = info->saved_rect.width;
              window->saved_rect.height = info->saved_rect.height;
            }
	}
    }
  
  if (info->on_all_workspaces_set)
    {
      window->on_all_workspaces = info->on_all_workspaces;
      meta_topic (META_DEBUG_SM,
                  "Restoring sticky state %d for window %s\n",
                  window->on_all_workspaces, window->desc);
    }
  
  if (info->workspace_indices)
    {
      GSList *tmp;
      GSList *spaces;      

      spaces = NULL;
      
      tmp = info->workspace_indices;
      while (tmp != NULL)
        {
          MetaWorkspace *space;          

          space =
            meta_screen_get_workspace_by_index (window->screen,
                                                GPOINTER_TO_INT (tmp->data));
          
          if (space)
            spaces = g_slist_prepend (spaces, space);
          
          tmp = tmp->next;
        }

      if (spaces)
        {
          /* This briefly breaks the invariant that we are supposed
           * to always be on some workspace. But we paranoically
           * ensured that one of the workspaces from the session was
           * indeed valid, so we know we'll go right back to one.
           */
          while (window->workspaces)
            meta_workspace_remove_window (window->workspaces->data, window);

          tmp = spaces;
          while (tmp != NULL)
            {
              MetaWorkspace *space;

              space = tmp->data;
              
              meta_workspace_add_window (space, window);              

              meta_topic (META_DEBUG_SM,
                          "Restoring saved window %s to workspace %d\n",
                          window->desc,
                          meta_workspace_index (space));
              
              tmp = tmp->next;
            }

          g_slist_free (spaces);
        }
    }

  if (info->geometry_set)
    {
      int x, y, w, h;
      
      window->placed = TRUE; /* don't do placement algorithms later */

      x = info->rect.x;
      y = info->rect.y;

      w = window->size_hints.base_width +
        info->rect.width * window->size_hints.width_inc;
      h = window->size_hints.base_height +
        info->rect.height * window->size_hints.height_inc;

      /* Force old gravity, ignoring anything now set */
      window->size_hints.win_gravity = info->gravity;
      
      meta_topic (META_DEBUG_SM,
                  "Restoring pos %d,%d size %d x %d for %s\n",
                  x, y, w, h, window->desc);
      
      meta_window_move_resize_internal (window,
                                        META_DO_GRAVITY_ADJUST,
                                        NorthWestGravity,
                                        x, y, w, h);
    }
}

void
meta_window_free (MetaWindow  *window)
{
  GList *tmp;
  
  meta_verbose ("Unmanaging 0x%lx\n", window->xwindow);

  if (window->display->window_with_menu == window)
    {
      meta_ui_window_menu_free (window->display->window_menu);
      window->display->window_menu = NULL;
      window->display->window_with_menu = NULL;
    }
  
  if (destroying_windows_disallowed > 0)
    meta_bug ("Tried to destroy window %s while destruction was not allowed\n",
              window->desc);
  
  window->unmanaging = TRUE;

  if (window->fullscreen)
    {
      MetaGroup *group;

      /* If the window is fullscreen, it may be forcing
       * other windows in its group to a higher layer
       */

      meta_stack_freeze (window->screen->stack);
      group = meta_window_get_group (window);
      if (group)
        meta_group_update_layers (group);
      meta_stack_thaw (window->screen->stack);
    }

  meta_window_shutdown_group (window); /* safe to do this early as
                                        * group.c won't re-add to the
                                        * group if window->unmanaging
                                        */
  
  /* If we have the focus, focus some other window.
   * This is done first, so that if the unmap causes
   * an EnterNotify the EnterNotify will have final say
   * on what gets focused, maintaining sloppy focus
   * invariants.
   */
  if (window->has_focus)
    {
      meta_topic (META_DEBUG_FOCUS,
                  "Focusing default window since we're unmanaging %s\n",
                  window->desc);
      meta_workspace_focus_default_window (window->screen->active_workspace, window, meta_display_get_current_time_roundtrip (window->display));
    }
  else if (window->display->expected_focus_window == window)
    {
      meta_topic (META_DEBUG_FOCUS,
                  "Focusing default window since expected focus window freed %s\n",
                  window->desc);
      window->display->expected_focus_window = NULL;
      meta_workspace_focus_default_window (window->screen->active_workspace, window, meta_display_get_current_time_roundtrip (window->display));
    }
  else
    {
      meta_topic (META_DEBUG_FOCUS,
                  "Unmanaging window %s which doesn't currently have focus\n",
                  window->desc);
    }

  if (window->struts)
    {
      g_free (window->struts);
      window->struts = NULL;

      meta_topic (META_DEBUG_WORKAREA,
                  "Unmanaging window %s which has struts, so invalidating work areas\n",
                  window->desc);
      invalidate_work_areas (window);
    }
  
  if (window->display->grab_window == window)
    meta_display_end_grab_op (window->display,
                              meta_display_get_current_time (window->display));

  g_assert (window->display->grab_window != window);
  
  if (window->display->focus_window == window)
    window->display->focus_window = NULL;
  if (window->display->previously_focused_window == window)
    window->display->previously_focused_window = NULL;

  meta_window_unqueue_calc_showing (window);
  meta_window_unqueue_move_resize (window);
  meta_window_unqueue_update_icon (window);
  meta_window_free_delete_dialog (window);
  
  tmp = window->workspaces;
  while (tmp != NULL)
    {
      GList *next;

      next = tmp->next;

      /* pops front of list */
      meta_workspace_remove_window (tmp->data, window);

      tmp = next;
    }

  g_assert (window->workspaces == NULL);

#ifndef G_DISABLE_CHECKS
  tmp = window->screen->workspaces;
  while (tmp != NULL)
    {
      MetaWorkspace *workspace = tmp->data;

      g_assert (g_list_find (workspace->windows, window) == NULL);
      g_assert (g_list_find (workspace->mru_list, window) == NULL);

      tmp = tmp->next;
    }
#endif
  
  meta_stack_remove (window->screen->stack, window);
  
  /* FIXME restore original size if window has maximized */
  
  if (window->frame)
    meta_window_destroy_frame (window);
  
  if (window->withdrawn)
    {
      /* We need to clean off the window's state so it
       * won't be restored if the app maps it again.
       */
      meta_error_trap_push (window->display);
      meta_verbose ("Cleaning state from window %s\n", window->desc);
      XDeleteProperty (window->display->xdisplay,
                       window->xwindow,
                       window->display->atom_net_wm_desktop);
      XDeleteProperty (window->display->xdisplay,
                       window->xwindow,
                       window->display->atom_net_wm_state);
      set_wm_state (window, WithdrawnState);
      meta_error_trap_pop (window->display, FALSE);
    }
  else
    {
      /* We need to put WM_STATE so that others will understand it on
       * restart.
       */
      if (!window->minimized)
        {
          meta_error_trap_push (window->display);
          set_wm_state (window, NormalState);
          meta_error_trap_pop (window->display, FALSE);
        }

      /* And we need to be sure the window is mapped so other WMs
       * know that it isn't Withdrawn
       */
      meta_error_trap_push (window->display);
      XMapWindow (window->display->xdisplay,
                  window->xwindow);
      meta_error_trap_pop (window->display, FALSE);
    }

  meta_window_ungrab_keys (window);
  meta_display_ungrab_window_buttons (window->display, window->xwindow);
  meta_display_ungrab_focus_window_button (window->display, window);
  
  meta_display_unregister_x_window (window->display, window->xwindow);
  

  meta_error_trap_push (window->display);

  /* Put back anything we messed up */
  if (window->border_width != 0)
    XSetWindowBorderWidth (window->display->xdisplay,
                           window->xwindow,
                           window->border_width);

  /* No save set */
  XRemoveFromSaveSet (window->display->xdisplay,
                      window->xwindow);

  /* Don't get events on not-managed windows */
  XSelectInput (window->display->xdisplay,
                window->xwindow,
                NoEventMask);

#ifdef HAVE_SHAPE
  if (META_DISPLAY_HAS_SHAPE (window->display))
    XShapeSelectInput (window->display->xdisplay, window->xwindow, NoEventMask);
#endif
  
  meta_error_trap_pop (window->display, FALSE);

  if (window->icon)
    g_object_unref (G_OBJECT (window->icon));

  if (window->mini_icon)
    g_object_unref (G_OBJECT (window->mini_icon));

  meta_icon_cache_free (&window->icon_cache);
  
  g_free (window->sm_client_id);
  g_free (window->wm_client_machine);
  g_free (window->startup_id);
  g_free (window->role);
  g_free (window->res_class);
  g_free (window->res_name);
  g_free (window->title);
  g_free (window->icon_name);
  g_free (window->desc);
  g_free (window);
}

static void
set_wm_state (MetaWindow *window,
              int         state)
{
  unsigned long data[2];
  
  meta_verbose ("Setting wm state %s on %s\n",
                wm_state_to_string (state), window->desc);
  
  /* Metacity doesn't use icon windows, so data[1] should be None
   * according to the ICCCM 2.0 Section 4.1.3.1.
   */
  data[0] = state;
  data[1] = None;

  meta_error_trap_push (window->display);
  XChangeProperty (window->display->xdisplay, window->xwindow,
                   window->display->atom_wm_state,
                   window->display->atom_wm_state,
                   32, PropModeReplace, (guchar*) data, 2);
  meta_error_trap_pop (window->display, FALSE);
}

static void
set_net_wm_state (MetaWindow *window)
{
  int i;
  unsigned long data[11];
  
  i = 0;
  if (window->shaded)
    {
      data[i] = window->display->atom_net_wm_state_shaded;
      ++i;
    }
  if (window->wm_state_modal)
    {
      data[i] = window->display->atom_net_wm_state_modal;
      ++i;
    }
  if (window->skip_pager)
    {
      data[i] = window->display->atom_net_wm_state_skip_pager;
      ++i;
    }
  if (window->skip_taskbar)
    {
      data[i] = window->display->atom_net_wm_state_skip_taskbar;
      ++i;
    }
  if (window->maximized)
    {
      data[i] = window->display->atom_net_wm_state_maximized_horz;
      ++i;
      data[i] = window->display->atom_net_wm_state_maximized_vert;
      ++i;
    }
  if (window->fullscreen)
    {
      data[i] = window->display->atom_net_wm_state_fullscreen;
      ++i;
    }
  if (window->shaded || window->minimized)
    {
      data[i] = window->display->atom_net_wm_state_hidden;
      ++i;
    }
  if (window->wm_state_above)
    {
      data[i] = window->display->atom_net_wm_state_above;
      ++i;
    }
  if (window->wm_state_below)
    {
      data[i] = window->display->atom_net_wm_state_below;
      ++i;
    }
  if (window->wm_state_demands_attention)
    {
      data[i] = window->display->atom_net_wm_state_demands_attention;
      ++i;
    }

  meta_verbose ("Setting _NET_WM_STATE with %d atoms\n", i);
  
  meta_error_trap_push (window->display);
  XChangeProperty (window->display->xdisplay, window->xwindow,
                   window->display->atom_net_wm_state,
                   XA_ATOM,
                   32, PropModeReplace, (guchar*) data, i);
  meta_error_trap_pop (window->display, FALSE);
}

/* FIXME rename this, it makes it sound like map state is relevant */
gboolean
meta_window_visible_on_workspace (MetaWindow    *window,
                                  MetaWorkspace *workspace)
{
  return (window->on_all_workspaces && window->screen == workspace->screen) ||
    meta_workspace_contains_window (workspace, window);
}

static gboolean
is_minimized_foreach (MetaWindow *window,
                      void       *data)
{
  gboolean *result = data;

  *result = window->minimized;
  if (*result)
    return FALSE; /* stop as soon as we find one */
  else
    return TRUE;
}

static gboolean
ancestor_is_minimized (MetaWindow *window)
{
  gboolean is_minimized;

  is_minimized = FALSE;

  meta_window_foreach_ancestor (window, is_minimized_foreach, &is_minimized);

  return is_minimized;
}

static gboolean
window_should_be_showing (MetaWindow  *window)
{
  gboolean showing, on_workspace;
  gboolean is_desktop_or_dock;

  meta_verbose ("Should be showing for window %s\n", window->desc);

  /* 1. See if we're on the workspace */
  
  on_workspace = meta_window_visible_on_workspace (window, 
                                                   window->screen->active_workspace);

  showing = on_workspace;
  
  if (!on_workspace)
    meta_verbose ("Window %s is not on workspace %d\n",
                  window->desc,
                  meta_workspace_index (window->screen->active_workspace));
  else
    meta_verbose ("Window %s is on the active workspace %d\n",
                  window->desc,
                  meta_workspace_index (window->screen->active_workspace));

  if (window->on_all_workspaces)
    meta_verbose ("Window %s is on all workspaces\n", window->desc);

  /* 2. See if we're minimized */
  if (window->minimized)
    showing = FALSE;
  
  /* 3. See if we're in "show desktop" mode */
  is_desktop_or_dock = FALSE;
  is_desktop_or_dock_foreach (window,
                              &is_desktop_or_dock);

  meta_window_foreach_ancestor (window, is_desktop_or_dock_foreach,
                                &is_desktop_or_dock);

  if (showing &&      
      window->screen->active_workspace->showing_desktop &&
      !is_desktop_or_dock)
    {
      meta_verbose ("Window %s is on current workspace, but we're showing the desktop\n",
                    window->desc);
      showing = FALSE;
    }

  /* 4. See if an ancestor is minimized (note that
   *    ancestor's "mapped" field may not be up to date
   *    since it's being computed in this same idle queue)
   */
  
  if (showing)
    {
      if (ancestor_is_minimized (window))
        showing = FALSE;
    }

#if 0
  /* 5. See if we're drawing wireframe
   */
  if (window->display->grab_window == window &&
      window->display->grab_wireframe_active)    
    showing = FALSE;
#endif
  
  return showing;
}

static void
implement_showing (MetaWindow *window,
                   gboolean    showing)
{
  /* Actually show/hide the window */
  meta_verbose ("Implement showing = %d for window %s\n",
                showing, window->desc);
  
  if (!showing)
    {
      gboolean on_workspace;

      on_workspace = meta_window_visible_on_workspace (window, 
                                                       window->screen->active_workspace);
  
      /* Really this effects code should probably
       * be in meta_window_hide so the window->mapped
       * test isn't duplicated here. Anyhow, we animate
       * if we are mapped now, we are supposed to
       * be minimized, and we are on the current workspace.
       */
      if (on_workspace && window->minimized && window->mapped &&
          !meta_prefs_get_reduced_resources ())
        {
	  MetaRectangle icon_rect, window_rect;
	  gboolean result;
	  
	  /* Check if the window has an icon geometry */
	  result = meta_window_get_icon_geometry (window, &icon_rect);
          
          if (!result)
            {
              /* just animate into the corner somehow - maybe
               * not a good idea...
               */              
              icon_rect.x = window->screen->width;
              icon_rect.y = window->screen->height;
              icon_rect.width = 1;
              icon_rect.height = 1;
            }

          meta_window_get_outer_rect (window, &window_rect);
          
          /* Draw a nice cool animation */
          meta_effects_draw_box_animation (window->screen,
                                           &window_rect,
                                           &icon_rect,
                                           META_MINIMIZE_ANIMATION_LENGTH,
                                           META_BOX_ANIM_SCALE);
	}

      meta_window_hide (window);
    }
  else
    {
      meta_window_show (window);
    }
}

void
meta_window_calc_showing (MetaWindow  *window)
{
  implement_showing (window, window_should_be_showing (window));
}

static guint calc_showing_idle = 0;
static GSList *calc_showing_pending = NULL;

static int
stackcmp (gconstpointer a, gconstpointer b)
{
  MetaWindow *aw = (gpointer) a;
  MetaWindow *bw = (gpointer) b;

  if (aw->screen != bw->screen)
    return 0; /* don't care how they sort with respect to each other */
  else
    return meta_stack_windows_cmp (aw->screen->stack,
                                   aw, bw);
}

static gboolean
idle_calc_showing (gpointer data)
{
  GSList *tmp;
  GSList *copy;
  GSList *should_show;
  GSList *should_hide;
  GSList *unplaced;
  GSList *displays;

  meta_topic (META_DEBUG_WINDOW_STATE,
              "Clearing the calc_showing queue\n");

  /* Work with a copy, for reentrancy. The allowed reentrancy isn't
   * complete; destroying a window while we're in here would result in
   * badness. But it's OK to queue/unqueue calc_showings.
   */
  copy = g_slist_copy (calc_showing_pending);
  g_slist_free (calc_showing_pending);
  calc_showing_pending = NULL;
  calc_showing_idle = 0;

  destroying_windows_disallowed += 1;
  
  /* We map windows from top to bottom and unmap from bottom to
   * top, to avoid extra expose events. The exception is
   * for unplaced windows, which have to be mapped from bottom to
   * top so placement works.
   */
  should_show = NULL;
  should_hide = NULL;
  unplaced = NULL;
  displays = NULL;

  tmp = copy;
  while (tmp != NULL)
    {
      MetaWindow *window;

      window = tmp->data;

      if (!window->placed)
        unplaced = g_slist_prepend (unplaced, window);
      else if (window_should_be_showing (window))
        should_show = g_slist_prepend (should_show, window);
      else
        should_hide = g_slist_prepend (should_hide, window);
      
      tmp = tmp->next;
    }

  /* bottom to top */
  unplaced = g_slist_sort (unplaced, stackcmp);
  should_hide = g_slist_sort (should_hide, stackcmp);
  /* top to bottom */
  should_show = g_slist_sort (should_show, stackcmp);
  should_show = g_slist_reverse (should_show);
  
  tmp = unplaced;
  while (tmp != NULL)
    {
      MetaWindow *window;

      window = tmp->data;

      meta_window_calc_showing (window);
      
      tmp = tmp->next;
    }

  tmp = should_hide;
  while (tmp != NULL)
    {
      MetaWindow *window;

      window = tmp->data;

      implement_showing (window, FALSE);
      
      tmp = tmp->next;
    }
  
  tmp = should_show;
  while (tmp != NULL)
    {
      MetaWindow *window;

      window = tmp->data;

      implement_showing (window, TRUE);
      
      tmp = tmp->next;
    }
  
  tmp = copy;
  while (tmp != NULL)
    {
      MetaWindow *window;

      window = tmp->data;

      /* important to set this here for reentrancy -
       * if we queue a window again while it's in "copy",
       * then queue_calc_showing will just return since
       * calc_showing_queued = TRUE still
       */
      window->calc_showing_queued = FALSE;
      
      tmp = tmp->next;
    }

  g_slist_free (copy);

  g_slist_free (unplaced);
  g_slist_free (should_show);
  g_slist_free (should_hide);
  g_slist_free (displays);
  
  destroying_windows_disallowed -= 1;
  
  return FALSE;
}

void
meta_window_unqueue_calc_showing (MetaWindow *window)
{
  if (!window->calc_showing_queued)
    return;

  meta_topic (META_DEBUG_WINDOW_STATE,
              "Removing %s from the calc_showing queue\n",
              window->desc);

  /* Note that window may not actually be in move_resize_pending
   * because it may have been in "copy" inside the idle handler
   */  
  calc_showing_pending = g_slist_remove (calc_showing_pending, window);
  window->calc_showing_queued = FALSE;
  
  if (calc_showing_pending == NULL &&
      calc_showing_idle != 0)
    {
      g_source_remove (calc_showing_idle);
      calc_showing_idle = 0;
    }
}

void
meta_window_flush_calc_showing (MetaWindow *window)
{
  if (window->calc_showing_queued)
    {
      meta_window_unqueue_calc_showing (window);
      meta_window_calc_showing (window);
    }
}

void
meta_window_queue_calc_showing (MetaWindow  *window)
{
  /* if withdrawn = TRUE then unmanaging should also be TRUE,
   * really.
   */
  if (window->unmanaging || window->withdrawn)
    return;

  if (window->calc_showing_queued)
    return;

  meta_topic (META_DEBUG_WINDOW_STATE,
              "Putting %s in the calc_showing queue\n",
              window->desc);
  
  window->calc_showing_queued = TRUE;
  
  if (calc_showing_idle == 0)
    calc_showing_idle = g_idle_add (idle_calc_showing, NULL);

  calc_showing_pending = g_slist_prepend (calc_showing_pending, window);
}

static gboolean
window_takes_focus_on_map (MetaWindow *window)
{
  Time compare;

  /* don't initially focus windows that are intended to not accept
   * focus
   */
  if (!(window->input || window->take_focus))
    return FALSE;

  switch (window->type)
    {
    case META_WINDOW_DOCK:
    case META_WINDOW_DESKTOP:
    case META_WINDOW_UTILITY:
    case META_WINDOW_SPLASHSCREEN:
    case META_WINDOW_TOOLBAR:
    case META_WINDOW_MENU:
      /* don't focus these */
      break;
    case META_WINDOW_NORMAL:
    case META_WINDOW_DIALOG:
    case META_WINDOW_MODAL_DIALOG:
      /* Disable the focus-stealing-prevention stuff for now; see #149028 */
      return TRUE;

      meta_topic (META_DEBUG_STARTUP,
                  "COMPARISON:\n"
                  "  net_wm_user_time_set : %d\n"
                  "  net_wm_user_time     : %lu\n"
                  "  initial_timestamp_set: %d\n"
                  "  initial_timestamp    : %lu\n",
                  window->net_wm_user_time_set,
                  window->net_wm_user_time,
                  window->initial_timestamp_set,
                  window->initial_timestamp);
      if (window->display->focus_window != NULL) {
        meta_topic (META_DEBUG_STARTUP,
                    "COMPARISON (continued):\n"
                    "  focus_window         : %s\n"
                    "  fw->net_wm_user_time : %lu\n",
                    window->display->focus_window->desc,
                    window->display->focus_window->net_wm_user_time);
      }

      /* We expect the most common case for not focusing a new window
       * to be when a hint to not focus it has been set.  Since we can
       * deal with that case rapidly, we use special case it--this is
       * merely a preliminary optimization.  :)
       */
      if ( ((window->net_wm_user_time_set == TRUE) &&
	   (window->net_wm_user_time == 0))
          ||
           ((window->initial_timestamp_set == TRUE) &&
	   (window->initial_timestamp == 0)))
        {
          meta_topic (META_DEBUG_STARTUP,
                      "window %s explicitly requested no focus\n",
                      window->desc);
          return FALSE;
        }

      if (!(window->net_wm_user_time_set) && !(window->initial_timestamp_set))
        {
          meta_topic (META_DEBUG_STARTUP,
                      "no information about window %s found\n",
                      window->desc);
          return TRUE;
        }

      /* To determine the "launch" time of an application,
       * startup-notification can set the TIMESTAMP and the
       * application (usually via its toolkit such as gtk or qt) can
       * set the _NET_WM_USER_TIME.  If both are set, then it means
       * the user has interacted with the application since it
       * launched, and _NET_WM_USER_TIME is the value that should be
       * used in the comparison.
       */
      compare = window->initial_timestamp_set ? window->initial_timestamp : 0;
      compare = window->net_wm_user_time_set  ? window->net_wm_user_time  : compare;

      if ((window->display->focus_window == NULL) ||
          (XSERVER_TIME_IS_LATER (compare, window->display->focus_window->net_wm_user_time)))
        {
          meta_topic (META_DEBUG_STARTUP,
                      "new window %s with no intervening events\n",
                      window->desc);
          return TRUE;
        }
      else
        {
          meta_topic (META_DEBUG_STARTUP,
                      "window %s focus prevented by other activity; %lu is before %lu\n",
                      window->desc, compare, window->display->focus_window->net_wm_user_time);
          return FALSE;
        }

      break;
    }

  return FALSE;
}

void
meta_window_show (MetaWindow *window)
{
  gboolean did_placement;
  gboolean did_show;
  gboolean takes_focus_on_map;

  meta_topic (META_DEBUG_WINDOW_STATE,
              "Showing window %s, shaded: %d iconic: %d placed: %d\n",
              window->desc, window->shaded, window->iconic, window->placed);

  did_show = FALSE;
  did_placement = FALSE;
  takes_focus_on_map = window_takes_focus_on_map (window);

  if ( (!takes_focus_on_map) && (window->display->focus_window != NULL) )
    {
      meta_window_stack_just_below (window, window->display->focus_window);
      ensure_mru_position_after (window, window->display->focus_window);
    }

  if (!window->placed)
    {
      /* We have to recalc the placement here since other windows may
       * have been mapped/placed since we last did constrain_position
       */

      /* calc_placement is an efficiency hack to avoid
       * multiple placement calculations before we finally
       * show the window.
       */
      window->calc_placement = TRUE;
      meta_window_move_resize_now (window);
      window->calc_placement = FALSE;

      /* don't ever do the initial position constraint thing again.
       * This is toggled here so that initially-iconified windows
       * still get placed when they are ultimately shown.
       */
      window->placed = TRUE;
      did_placement = TRUE;
    }
  
  /* Shaded means the frame is mapped but the window is not */
  
  if (window->frame && !window->frame->mapped)
    {
      meta_topic (META_DEBUG_WINDOW_STATE,
                  "Frame actually needs map\n");
      window->frame->mapped = TRUE;
      meta_ui_map_frame (window->screen->ui, window->frame->xwindow);
      did_show = TRUE;
    }

  if (window->shaded)
    {
      if (window->mapped)
        {          
          meta_topic (META_DEBUG_WINDOW_STATE,
                      "%s actually needs unmap (shaded)\n", window->desc);
          meta_topic (META_DEBUG_WINDOW_STATE,
                      "Incrementing unmaps_pending on %s for shade\n",
                      window->desc);
          window->mapped = FALSE;
          window->unmaps_pending += 1;
          meta_error_trap_push (window->display);
          XUnmapWindow (window->display->xdisplay, window->xwindow);
          meta_error_trap_pop (window->display, FALSE);
        }

      if (!window->iconic)
        {
          window->iconic = TRUE;
          set_wm_state (window, IconicState);
        }
    }
  else
    {
      if (!window->mapped)
        {
          meta_topic (META_DEBUG_WINDOW_STATE,
                      "%s actually needs map\n", window->desc);
          window->mapped = TRUE;
          meta_error_trap_push (window->display);
          XMapWindow (window->display->xdisplay, window->xwindow);
          meta_error_trap_pop (window->display, FALSE);
          did_show = TRUE;
        }      
      
      if (window->iconic)
        {
          window->iconic = FALSE;
          set_wm_state (window, NormalState);
        }
    }

  if (did_placement)
    {
      if (window->xtransient_for != None)
        {
          MetaWindow *parent;

          parent =
            meta_display_lookup_x_window (window->display,
                                          window->xtransient_for);
          
          if (parent && parent->has_focus &&
              (window->input || window->take_focus))
            {
              meta_topic (META_DEBUG_FOCUS,
                          "Focusing transient window '%s' since parent had focus\n",
                          window->desc);
              meta_window_focus (window,
                                 meta_display_get_current_time (window->display));
            }
        }

      if (takes_focus_on_map)
        {                
          meta_window_focus (window,
                             meta_display_get_current_time (window->display));
        }
      else
        window->wm_state_demands_attention = TRUE;
    }

  if (did_show)
    {
      set_net_wm_state (window);

      if (window->struts)
        {
          meta_topic (META_DEBUG_WORKAREA,
                      "Mapped window %s with struts, so invalidating work areas\n",
                      window->desc);
          invalidate_work_areas (window);
        }
    }
}

void
meta_window_hide (MetaWindow *window)
{
  gboolean did_hide;
  
  meta_topic (META_DEBUG_WINDOW_STATE,
              "Hiding window %s\n", window->desc);

  did_hide = FALSE;
  
  if (window->frame && window->frame->mapped)
    {
      meta_topic (META_DEBUG_WINDOW_STATE, "Frame actually needs unmap\n");
      window->frame->mapped = FALSE;
      meta_ui_unmap_frame (window->screen->ui, window->frame->xwindow);
      did_hide = TRUE;
    }

  if (window->mapped)
    {
      meta_topic (META_DEBUG_WINDOW_STATE,
                  "%s actually needs unmap\n", window->desc);
      meta_topic (META_DEBUG_WINDOW_STATE,
                  "Incrementing unmaps_pending on %s for hide\n",
                  window->desc);
      window->mapped = FALSE;
      window->unmaps_pending += 1;
      meta_error_trap_push (window->display);      
      XUnmapWindow (window->display->xdisplay, window->xwindow);
      meta_error_trap_pop (window->display, FALSE);
      did_hide = TRUE;
    }

  if (!window->iconic)
    {
      window->iconic = TRUE;
      set_wm_state (window, IconicState);
    }
  
  if (did_hide)
    {
      set_net_wm_state (window);
      
      if (window->struts)
        {
          meta_topic (META_DEBUG_WORKAREA,
                      "Unmapped window %s with struts, so invalidating work areas\n",
                      window->desc);
          invalidate_work_areas (window);
        }
    }
}

static gboolean
queue_calc_showing_func (MetaWindow *window,
                         void       *data)
{
  meta_window_queue_calc_showing (window);
  return TRUE;
}

void
meta_window_minimize (MetaWindow  *window)
{
  if (!window->minimized)
    {
      window->minimized = TRUE;
      meta_window_queue_calc_showing (window);

      meta_window_foreach_transient (window,
                                     queue_calc_showing_func,
                                     NULL);
      
      if (window->has_focus)
        {
          meta_topic (META_DEBUG_FOCUS,
                      "Focusing default window due to minimization of focus window %s\n",
                      window->desc);
          meta_workspace_focus_default_window (window->screen->active_workspace, window, meta_display_get_current_time_roundtrip (window->display));
        }
      else
        {
          meta_topic (META_DEBUG_FOCUS,
                      "Minimizing window %s which doesn't have the focus\n",
                      window->desc);
        }
    }
}

void
meta_window_unminimize (MetaWindow  *window)
{
  if (window->minimized)
    {
      window->minimized = FALSE;
      meta_window_queue_calc_showing (window);

      meta_window_foreach_transient (window,
                                     queue_calc_showing_func,
                                     NULL);
    }
}

static void
meta_window_save_rect (MetaWindow *window)
{
  if (!(window->maximized || window->fullscreen))
    {
      /* save size/pos as appropriate args for move_resize */
      window->saved_rect = window->rect;
      if (window->frame)
        {
          window->saved_rect.x += window->frame->rect.x;
          window->saved_rect.y += window->frame->rect.y;
        }
    }
}

void
meta_window_maximize_internal (MetaWindow    *window,
                               MetaRectangle *saved_rect)
{
  meta_topic (META_DEBUG_WINDOW_OPS,
              "Maximizing %s\n", window->desc);
  
  if (saved_rect != NULL)
    window->saved_rect = *saved_rect;
  else
    meta_window_save_rect (window);
  
  window->maximized = TRUE;
  
  recalc_window_features (window);
  set_net_wm_state (window);
}

void
meta_window_maximize (MetaWindow  *window)
{
  if (!window->maximized)
    {
      if (window->shaded)
        meta_window_unshade (window);
      
      /* if the window hasn't been placed yet, we'll maximize it then
       */
      if (!window->placed)
	{
	  window->maximize_after_placement = TRUE;
	  return;
	}

      meta_window_maximize_internal (window, NULL);

      /* move_resize with new maximization constraints
       */
      meta_window_queue_move_resize (window);
    }
}

void
meta_window_unmaximize (MetaWindow  *window)
{
  if (window->maximized)
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Unmaximizing %s\n", window->desc);
      
      window->maximized = FALSE;

      /* When we unmaximize, if we're doing a mouse move also we could
       * get the window suddenly jumping to the upper left corner of
       * the workspace, since that's where it was when the grab op
       * started.  So we need to update the grab state.
       */
      if (meta_grab_op_is_moving (window->display->grab_op) &&
          window->display->grab_window == window)
        {
          window->display->grab_anchor_window_pos = window->saved_rect;
        }

      meta_window_move_resize (window,
                               TRUE,
                               window->saved_rect.x,
                               window->saved_rect.y,
                               window->saved_rect.width,
                               window->saved_rect.height);

      recalc_window_features (window);
      set_net_wm_state (window);
    }
}

void
meta_window_make_above (MetaWindow  *window)
{
  window->wm_state_above = TRUE;
  meta_window_update_layer (window);
  meta_window_raise (window);
  set_net_wm_state (window);
}

void
meta_window_unmake_above (MetaWindow  *window)
{
  window->wm_state_above = FALSE;
  meta_window_raise (window);
  meta_window_update_layer (window);
  set_net_wm_state (window);
}

void
meta_window_make_fullscreen (MetaWindow  *window)
{
  if (!window->fullscreen)
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Fullscreening %s\n", window->desc);

      if (window->shaded)
        meta_window_unshade (window);

      meta_window_save_rect (window);
      
      window->fullscreen = TRUE;

      meta_stack_freeze (window->screen->stack);
      meta_window_update_layer (window);
      
      meta_window_raise (window);
      meta_stack_thaw (window->screen->stack);
      
      /* move_resize with new constraints
       */
      meta_window_queue_move_resize (window);

      recalc_window_features (window);
      set_net_wm_state (window);
    }
}

void
meta_window_unmake_fullscreen (MetaWindow  *window)
{
  if (window->fullscreen)
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Unfullscreening %s\n", window->desc);
      
      window->fullscreen = FALSE;

      meta_window_update_layer (window);
      
      meta_window_move_resize (window,
                               TRUE,
                               window->saved_rect.x,
                               window->saved_rect.y,
                               window->saved_rect.width,
                               window->saved_rect.height);

      recalc_window_features (window);
      set_net_wm_state (window);
    }
}

void
meta_window_shade (MetaWindow  *window)
{
  meta_topic (META_DEBUG_WINDOW_OPS,
              "Shading %s\n", window->desc);
  if (!window->shaded)
    {
#if 0
      if (window->mapped)
        {
          /* Animation */
          MetaRectangle starting_size;
          MetaRectangle titlebar_size;
          
          meta_window_get_outer_rect (window, &starting_size);
          if (window->frame)
            {
              starting_size.y += window->frame->child_y;
              starting_size.height -= window->frame->child_y;
            }
          titlebar_size = starting_size;
          titlebar_size.height = 0;
          
          meta_effects_draw_box_animation (window->screen,
                                           &starting_size,
                                           &titlebar_size,
                                           META_SHADE_ANIMATION_LENGTH,
                                           META_BOX_ANIM_SLIDE_UP);
        }
#endif
      
      window->shaded = TRUE;

      meta_window_queue_move_resize (window);
      meta_window_queue_calc_showing (window);

      /* After queuing the calc showing, since _focus flushes it,
       * and we need to focus the frame
       */
      meta_topic (META_DEBUG_FOCUS,
                  "Re-focusing window %s after shading it\n",
                  window->desc);
      meta_window_focus (window,
                         meta_display_get_current_time (window->display));
      
      set_net_wm_state (window);
    }
}

void
meta_window_unshade (MetaWindow  *window)
{
  meta_topic (META_DEBUG_WINDOW_OPS,
              "Unshading %s\n", window->desc);
  if (window->shaded)
    {
      window->shaded = FALSE;
      meta_window_queue_move_resize (window);
      meta_window_queue_calc_showing (window);

      /* focus the window */
      meta_topic (META_DEBUG_FOCUS,
                  "Focusing window %s after unshading it\n",
                  window->desc);
      meta_window_focus (window, meta_display_get_current_time (window->display));

      set_net_wm_state (window);
    }
}

static gboolean
unminimize_func (MetaWindow *window,
                 void       *data)
{
  meta_window_unminimize (window);
  return TRUE;
}

static void
unminimize_window_and_all_transient_parents (MetaWindow *window)
{
  meta_window_unminimize (window);
  meta_window_foreach_ancestor (window, unminimize_func, NULL);
}

void
meta_window_activate (MetaWindow *window,
                      guint32     timestamp)
{
  if (timestamp != 0)
    window->net_wm_user_time = timestamp;

  /* disable show desktop mode unless we're a desktop component */
  maybe_leave_show_desktop_mode (window);
 
  /* Get window on current workspace */
  if (!meta_window_visible_on_workspace (window,
                                         window->screen->active_workspace))
    meta_window_change_workspace (window,
                                  window->screen->active_workspace);
  
  if (window->shaded)
    meta_window_unshade (window);

  unminimize_window_and_all_transient_parents (window);
  
  meta_window_raise (window);
  meta_topic (META_DEBUG_FOCUS,
              "Focusing window %s due to activation\n",
              window->desc);
  meta_window_focus (window, timestamp);
}

/* returns values suitable for meta_window_move
 * i.e. static gravity
 */
static void
adjust_for_gravity (MetaWindow        *window,
                    MetaFrameGeometry *fgeom,
                    gboolean           coords_assume_border,
                    int                x,
                    int                y,
                    int                width,
                    int                height,
                    int               *xp,
                    int               *yp)
{
  int ref_x, ref_y;
  int bw;
  int child_x, child_y;
  int frame_width, frame_height;
  
  if (coords_assume_border)
    bw = window->border_width;
  else
    bw = 0;

  if (fgeom)
    {
      child_x = fgeom->left_width;
      child_y = fgeom->top_height;
      frame_width = child_x + width + fgeom->right_width;
      frame_height = child_y + height + fgeom->bottom_height;
    }
  else
    {
      child_x = 0;
      child_y = 0;
      frame_width = width;
      frame_height = height;
    }
  
  /* We're computing position to pass to window_move, which is
   * the position of the client window (StaticGravity basically)
   *
   * (see WM spec description of gravity computation, but note that
   * their formulas assume we're honoring the border width, rather
   * than compensating for having turned it off)
   */
  switch (window->size_hints.win_gravity)
    {
    case NorthWestGravity:
      ref_x = x;
      ref_y = y;
      break;
    case NorthGravity:
      ref_x = x + width / 2 + bw;
      ref_y = y;
      break;
    case NorthEastGravity:
      ref_x = x + width + bw * 2;
      ref_y = y;
      break;
    case WestGravity:
      ref_x = x;
      ref_y = y + height / 2 + bw;
      break;
    case CenterGravity:
      ref_x = x + width / 2 + bw;
      ref_y = y + height / 2 + bw;
      break;
    case EastGravity:
      ref_x = x + width + bw * 2;
      ref_y = y + height / 2 + bw;
      break;
    case SouthWestGravity:
      ref_x = x;
      ref_y = y + height + bw * 2;
      break;
    case SouthGravity:
      ref_x = x + width / 2 + bw;
      ref_y = y + height + bw * 2;
      break;
    case SouthEastGravity:
      ref_x = x + width + bw * 2;
      ref_y = y + height + bw * 2;
      break;
    case StaticGravity:
    default:
      ref_x = x;
      ref_y = y;
      break;
    }

  switch (window->size_hints.win_gravity)
    {
    case NorthWestGravity:
      *xp = ref_x + child_x;
      *yp = ref_y + child_y;
      break;
    case NorthGravity:
      *xp = ref_x - frame_width / 2 + child_x;
      *yp = ref_y + child_y;
      break;
    case NorthEastGravity:
      *xp = ref_x - frame_width + child_x;
      *yp = ref_y + child_y;
      break;
    case WestGravity:
      *xp = ref_x + child_x;
      *yp = ref_y - frame_height / 2 + child_y;
      break;
    case CenterGravity:
      *xp = ref_x - frame_width / 2 + child_x;
      *yp = ref_y - frame_height / 2 + child_y;
      break;
    case EastGravity:
      *xp = ref_x - frame_width + child_x;
      *yp = ref_y - frame_height / 2 + child_y;
      break;
    case SouthWestGravity:
      *xp = ref_x + child_x;
      *yp = ref_y - frame_height + child_y;
      break;
    case SouthGravity:
      *xp = ref_x - frame_width / 2 + child_x;
      *yp = ref_y - frame_height + child_y;
      break;
    case SouthEastGravity:
      *xp = ref_x - frame_width + child_x;
      *yp = ref_y - frame_height + child_y;
      break;
    case StaticGravity:
    default:
      *xp = ref_x;
      *yp = ref_y;
      break;
    }
}

static gboolean
static_gravity_works (MetaDisplay *display)
{
  return display->static_gravity_works;
}

static void
get_mouse_deltas_for_resize (MetaWindow *window,
                             int         resize_gravity,
                             int         w,
                             int         h,
                             int        *x_delta,
                             int        *y_delta)
{
  switch (meta_x_direction_from_gravity (resize_gravity))
    {
    case META_RESIZE_LEFT_OR_TOP:
      *x_delta = window->rect.width - w;
      break;
    case META_RESIZE_RIGHT_OR_BOTTOM:
      *x_delta = w - window->rect.width;
      break;
    case META_RESIZE_CENTER:
      /* FIXME this implies that with center gravity you have to grow
       * in increments of two
       */
      *x_delta = (w - window->rect.width) / 2;
      break;
    }
  
  switch (meta_y_direction_from_gravity (resize_gravity))
    {
    case META_RESIZE_LEFT_OR_TOP:
      *y_delta = window->rect.height - h;
      break;
    case META_RESIZE_RIGHT_OR_BOTTOM:
      *y_delta = h - window->rect.height;
      break;
    case META_RESIZE_CENTER:
      /* FIXME this implies that with center gravity you have to grow
       * in increments of two
       */
      *y_delta = (h - window->rect.height) / 2;
      break;
    }
}

#ifdef HAVE_XSYNC
static void
send_sync_request (MetaWindow *window)
{
  XSyncValue value;
  XClientMessageEvent ev;

  window->sync_request_serial++;

  XSyncIntToValue (&value, window->sync_request_serial);
  
  ev.type = ClientMessage;
  ev.window = window->xwindow;
  ev.message_type = window->display->atom_wm_protocols;
  ev.format = 32;
  ev.data.l[0] = window->display->atom_net_wm_sync_request;
  ev.data.l[1] = meta_display_get_current_time (window->display);
  ev.data.l[2] = XSyncValueLow32 (value);
  ev.data.l[3] = XSyncValueHigh32 (value);

  /* We don't need to trap errors here as we are already
   * inside an error_trap_push()/pop() pair.
   */
  XSendEvent (window->display->xdisplay,
	      window->xwindow, False, 0, (XEvent*) &ev);

  g_get_current_time (&window->sync_request_time);
}
#endif

static void
meta_window_move_resize_internal (MetaWindow  *window,
                                  MetaMoveResizeFlags flags,
                                  int          resize_gravity,
                                  int          root_x_nw,
                                  int          root_y_nw,
                                  int          w,
                                  int          h)
{
  XWindowChanges values;
  unsigned int mask;
  gboolean need_configure_notify;
  MetaFrameGeometry fgeom;
  gboolean need_move_client = FALSE;
  gboolean need_move_frame = FALSE;
  gboolean need_resize_client = FALSE;
  gboolean need_resize_frame = FALSE;
  int frame_size_dx;
  int frame_size_dy;
  int size_dx;
  int size_dy;
  gboolean is_configure_request;
  gboolean do_gravity_adjust;
  gboolean is_user_action;
  gboolean configure_frame_first;
  gboolean use_static_gravity;
  /* used for the configure request, but may not be final
   * destination due to StaticGravity etc.
   */
  int client_move_x;
  int client_move_y;
  int x_delta;
  int y_delta;
  MetaRectangle new_rect;
  MetaRectangle old_rect;
  
  is_configure_request = (flags & META_IS_CONFIGURE_REQUEST) != 0;
  do_gravity_adjust = (flags & META_DO_GRAVITY_ADJUST) != 0;
  is_user_action = (flags & META_USER_MOVE_RESIZE) != 0;
  
  /* We don't need it in the idle queue anymore. */
  meta_window_unqueue_move_resize (window);

  old_rect = window->rect;
  meta_window_get_position (window, &old_rect.x, &old_rect.y);
  
  meta_topic (META_DEBUG_GEOMETRY,
              "Move/resize %s to %d,%d %dx%d%s%s from %d,%d %dx%d\n",
              window->desc, root_x_nw, root_y_nw, w, h,
              is_configure_request ? " (configure request)" : "",
              is_user_action ? " (user move/resize)" : "",
              old_rect.x, old_rect.y, old_rect.width, old_rect.height);
  
  if (window->frame)
    meta_frame_calc_geometry (window->frame,
                              &fgeom);
  
  if (is_configure_request || do_gravity_adjust)
    {      
      adjust_for_gravity (window,
                          window->frame ? &fgeom : NULL,
                          /* configure request coords assume
                           * the border width existed
                           */
                          is_configure_request,
                          root_x_nw,
                          root_y_nw,
                          w, h,
                          &root_x_nw,
                          &root_y_nw);
      
      meta_topic (META_DEBUG_GEOMETRY,
                  "Compensated position for gravity, new pos %d,%d\n",
                  root_x_nw, root_y_nw);
    }

  get_mouse_deltas_for_resize (window, resize_gravity, w, h,
                               &x_delta, &y_delta);
  
  meta_window_constrain (window,
                         window->frame ? &fgeom : NULL,
                         &old_rect,
                         root_x_nw - old_rect.x,
                         root_y_nw - old_rect.y,
                         meta_x_direction_from_gravity (resize_gravity),
                         x_delta,
                         meta_y_direction_from_gravity (resize_gravity),
                         y_delta,
                         &new_rect);

  w = new_rect.width;
  h = new_rect.height;
  root_x_nw = new_rect.x;
  root_y_nw = new_rect.y;
  
  if (w != window->rect.width ||
      h != window->rect.height)
    need_resize_client = TRUE;  
  
  window->rect.width = w;
  window->rect.height = h;
  
  if (window->frame)
    {
      int new_w, new_h;

      new_w = window->rect.width + fgeom.left_width + fgeom.right_width;

      if (window->shaded)
        new_h = fgeom.top_height;
      else
        new_h = window->rect.height + fgeom.top_height + fgeom.bottom_height;

      frame_size_dx = new_w - window->frame->rect.width;
      frame_size_dy = new_h - window->frame->rect.height;

      need_resize_frame = (frame_size_dx != 0 || frame_size_dy != 0);
      
      window->frame->rect.width = new_w;
      window->frame->rect.height = new_h;

      meta_topic (META_DEBUG_GEOMETRY,
                  "Calculated frame size %dx%d\n",
                  window->frame->rect.width,
                  window->frame->rect.height);
    }
  else
    {
      frame_size_dx = 0;
      frame_size_dy = 0;
    }

  /* For nice effect, when growing the window we want to move/resize
   * the frame first, when shrinking the window we want to move/resize
   * the client first. If we grow one way and shrink the other,
   * see which way we're moving "more"
   *
   * Mail from Owen subject "Suggestion: Gravity and resizing from the left"
   * http://mail.gnome.org/archives/wm-spec-list/1999-November/msg00088.html
   *
   * An annoying fact you need to know in this code is that StaticGravity
   * does nothing if you _only_ resize or _only_ move the frame;
   * it must move _and_ resize, otherwise you get NorthWestGravity
   * behavior. The move and resize must actually occur, it is not
   * enough to set CWX | CWWidth but pass in the current size/pos.
   */
      
  if (window->frame)
    {
      int new_x, new_y;
      int frame_pos_dx, frame_pos_dy;
      
      /* Compute new frame coords */
      new_x = root_x_nw - fgeom.left_width;
      new_y = root_y_nw - fgeom.top_height;

      frame_pos_dx = new_x - window->frame->rect.x;
      frame_pos_dy = new_y - window->frame->rect.y;

      need_move_frame = (frame_pos_dx != 0 || frame_pos_dy != 0);
      
      window->frame->rect.x = new_x;
      window->frame->rect.y = new_y;      

      /* If frame will both move and resize, then StaticGravity
       * on the child window will kick in and implicitly move
       * the child with respect to the frame. The implicit
       * move will keep the child in the same place with
       * respect to the root window. If frame only moves
       * or only resizes, then the child will just move along
       * with the frame.
       */

      /* window->rect.x, window->rect.y are relative to frame,
       * remember they are the server coords
       */
          
      new_x = fgeom.left_width;
      new_y = fgeom.top_height;

      if (need_resize_frame && need_move_frame &&
          static_gravity_works (window->display))
        {
          /* static gravity kicks in because frame
           * is both moved and resized
           */
          /* when we move the frame by frame_pos_dx, frame_pos_dy the
           * client will implicitly move relative to frame by the
           * inverse delta.
           * 
           * When moving client then frame, we move the client by the
           * frame delta, to be canceled out by the implicit move by
           * the inverse frame delta, resulting in a client at new_x,
           * new_y.
           *
           * When moving frame then client, we move the client
           * by the same delta as the frame, because the client
           * was "left behind" by the frame - resulting in a client
           * at new_x, new_y.
           *
           * In both cases we need to move the client window
           * in all cases where we had to move the frame window.
           */
          
          client_move_x = new_x + frame_pos_dx;
          client_move_y = new_y + frame_pos_dy;

          if (need_move_frame)
            need_move_client = TRUE;

          use_static_gravity = TRUE;
        }
      else
        {
          client_move_x = new_x;
          client_move_y = new_y;

          if (client_move_x != window->rect.x ||
              client_move_y != window->rect.y)
            need_move_client = TRUE;

          use_static_gravity = FALSE;
        }

      /* This is the final target position, but not necessarily what
       * we pass to XConfigureWindow, due to StaticGravity implicit
       * movement.
       */      
      window->rect.x = new_x;
      window->rect.y = new_y;
    }
  else
    {
      if (root_x_nw != window->rect.x ||
          root_y_nw != window->rect.y)
        need_move_client = TRUE;
      
      window->rect.x = root_x_nw;
      window->rect.y = root_y_nw;

      client_move_x = window->rect.x;
      client_move_y = window->rect.y;

      use_static_gravity = FALSE;
    }

  /* If frame extents have changed, fill in other frame fields and
     change frame's extents property. */
  if (window->frame &&
      (window->frame->child_x != fgeom.left_width ||
          window->frame->child_y != fgeom.top_height ||
          window->frame->right_width != fgeom.right_width ||
          window->frame->bottom_height != fgeom.bottom_height))
    {
      window->frame->child_x = fgeom.left_width;
      window->frame->child_y = fgeom.top_height;
      window->frame->right_width = fgeom.right_width;
      window->frame->bottom_height = fgeom.bottom_height;

      update_net_frame_extents (window);
    }

  /* See ICCCM 4.1.5 for when to send ConfigureNotify */
  
  need_configure_notify = FALSE;
  
  /* If this is a configure request and we change nothing, then we
   * must send configure notify.
   */
  if  (is_configure_request &&
       !(need_move_client || need_move_frame ||
         need_resize_client || need_resize_frame ||
         window->border_width != 0))
    need_configure_notify = TRUE;

  /* We must send configure notify if we move but don't resize, since
   * the client window may not get a real event
   */
  if ((need_move_client || need_move_frame) &&
      !(need_resize_client || need_resize_frame))
    need_configure_notify = TRUE;
  
  /* The rest of this function syncs our new size/pos with X as
   * efficiently as possible
   */

  /* configure frame first if we grow more than we shrink
   */
  size_dx = w - window->rect.width;
  size_dy = h - window->rect.height;

  configure_frame_first = (size_dx + size_dy >= 0);

  if (use_static_gravity)
    meta_window_set_gravity (window, StaticGravity);  
  
  if (configure_frame_first && window->frame)
    meta_frame_sync_to_window (window->frame,
                               resize_gravity,
                               need_move_frame, need_resize_frame);

  values.border_width = 0;
  values.x = client_move_x;
  values.y = client_move_y;
  values.width = window->rect.width;
  values.height = window->rect.height;
  
  mask = 0;
  if (is_configure_request && window->border_width != 0)
    mask |= CWBorderWidth; /* must force to 0 */
  if (need_move_client)
    mask |= (CWX | CWY);
  if (need_resize_client)
    mask |= (CWWidth | CWHeight);

  if (mask != 0)
    {
      {
        int newx, newy;
        meta_window_get_position (window, &newx, &newy);
        meta_topic (META_DEBUG_GEOMETRY,
                    "Syncing new client geometry %d,%d %dx%d, border: %s pos: %s size: %s\n",
                    newx, newy,
                    window->rect.width, window->rect.height,
                    mask & CWBorderWidth ? "true" : "false",
                    need_move_client ? "true" : "false",
                    need_resize_client ? "true" : "false");
      }
      
      meta_error_trap_push (window->display);

#ifdef HAVE_XSYNC
      if (window->sync_request_counter != None &&
	  window->display->grab_sync_request_alarm != None &&
	  window->sync_request_time.tv_usec == 0 &&
	  window->sync_request_time.tv_sec == 0)
	{
	  send_sync_request (window);
	}
#endif

      XConfigureWindow (window->display->xdisplay,
                        window->xwindow,
                        mask,
                        &values);
      
      meta_error_trap_pop (window->display, FALSE);
    }

  if (!configure_frame_first && window->frame)
    meta_frame_sync_to_window (window->frame,
                               resize_gravity,
                               need_move_frame, need_resize_frame);  

  /* Put gravity back to be nice to lesser window managers */
  if (use_static_gravity)
    meta_window_set_gravity (window, NorthWestGravity);  
  
  if (need_configure_notify)
    send_configure_notify (window);

  if (is_user_action)
    {
      window->user_has_move_resized = TRUE;

      window->user_rect.width = window->rect.width;
      window->user_rect.height = window->rect.height;

      meta_window_get_position (window, 
                                &window->user_rect.x,
                                &window->user_rect.y);
    }
  
  if (need_move_frame || need_resize_frame ||
      need_move_client || need_resize_client)
    {
      int newx, newy;
      meta_window_get_position (window, &newx, &newy);
      meta_topic (META_DEBUG_GEOMETRY,
                  "New size/position %d,%d %dx%d (user %d,%d %dx%d)\n",
                  newx, newy, window->rect.width, window->rect.height,
                  window->user_rect.x, window->user_rect.y,
                  window->user_rect.width, window->user_rect.height);
    }
  else
    {
      meta_topic (META_DEBUG_GEOMETRY, "Size/position not modified\n");
    }
  
  meta_window_refresh_resize_popup (window);
  
  /* Invariants leaving this function are:
   *   a) window->rect and frame->rect reflect the actual
   *      server-side size/pos of window->xwindow and frame->xwindow
   *   b) all constraints are obeyed by window->rect and frame->rect
   */
}

void
meta_window_resize (MetaWindow  *window,
                    gboolean     user_op,
                    int          w,
                    int          h)
{
  int x, y;

  meta_window_get_position (window, &x, &y);
  
  meta_window_move_resize_internal (window,
                                    user_op ? META_USER_MOVE_RESIZE : 0,
                                    NorthWestGravity,
                                    x, y, w, h);
}

void
meta_window_move (MetaWindow  *window,
                  gboolean     user_op,
                  int          root_x_nw,
                  int          root_y_nw)
{
  meta_window_move_resize_internal (window,
                                    user_op ? META_USER_MOVE_RESIZE : 0,
                                    NorthWestGravity,
                                    root_x_nw, root_y_nw,
                                    window->rect.width,
                                    window->rect.height);
}

void
meta_window_move_resize (MetaWindow  *window,
                         gboolean     user_op,
                         int          root_x_nw,
                         int          root_y_nw,
                         int          w,
                         int          h)
{
  meta_window_move_resize_internal (window,
                                    user_op ? META_USER_MOVE_RESIZE : 0,
                                    NorthWestGravity,
                                    root_x_nw, root_y_nw,
                                    w, h);
}

void
meta_window_resize_with_gravity (MetaWindow *window,
                                 gboolean     user_op,
                                 int          w,
                                 int          h,
                                 int          gravity)
{
  int x, y;

  meta_window_get_position (window, &x, &y);
  
  meta_window_move_resize_internal (window,
                                    user_op ? META_USER_MOVE_RESIZE : 0,
                                    gravity,
                                    x, y, w, h);
}

void
meta_window_move_resize_now (MetaWindow  *window)
{
  int x, y;

  /* If constraints have changed then we'll snap back to wherever
   * the user had the window
   */
  meta_window_get_user_position (window, &x, &y);

  /* This used to use the user width/height if the user hadn't resized,
   * but it turns out that breaks things pretty often, because configure
   * requests from the app or size hints changes from the app frequently
   * reflect user actions such as changing terminal font size
   * or expanding a disclosure triangle.
   */
  meta_window_move_resize (window, FALSE, x, y,
                           window->rect.width,
                           window->rect.height);
}

static void
check_maximize_to_work_area (MetaWindow          *window,
                             const MetaRectangle *work_area)
{
  /* If we now fill the screen, maximize.
   * the point here is that fill horz + fill vert = maximized
   */
  MetaRectangle rect;

  if (!window->has_maximize_func)
    return;
  
  meta_window_get_outer_rect (window, &rect);

  if ( rect.x >= work_area->x &&
       rect.y >= work_area->y &&
       (((work_area->width - work_area->x) - rect.width) <
        window->size_hints.width_inc) &&
       (((work_area->height - work_area->y) - rect.height) <
        window->size_hints.height_inc) )
    meta_window_maximize (window);
}

void
meta_window_fill_horizontal (MetaWindow  *window)
{
  MetaRectangle work_area;
  int x, y, w, h;
  
  meta_window_get_user_position (window, &x, &y);

  w = window->rect.width;
  h = window->rect.height;
  
  meta_window_get_work_area_current_xinerama (window, &work_area);
  
  x = work_area.x;
  w = work_area.width;
  
  if (window->frame != NULL)
    {
      x += window->frame->child_x;
      w -= (window->frame->child_x + window->frame->right_width);
    }
  
  meta_window_move_resize (window, TRUE,
                           x, y, w, h);

  check_maximize_to_work_area (window, &work_area);
}

void
meta_window_fill_vertical (MetaWindow  *window)
{
  MetaRectangle work_area;
  int x, y, w, h;
  
  meta_window_get_user_position (window, &x, &y);

  w = window->rect.width;
  h = window->rect.height;

  meta_window_get_work_area_current_xinerama (window, &work_area);

  y = work_area.y;
  h = work_area.height;
  
  if (window->frame != NULL)
    {
      y += window->frame->child_y;
      h -= (window->frame->child_y + window->frame->bottom_height);
    }
  
  meta_window_move_resize (window, TRUE,
                           x, y, w, h);

  check_maximize_to_work_area (window, &work_area);
}

static guint move_resize_idle = 0;
static GSList *move_resize_pending = NULL;

static gboolean
idle_move_resize (gpointer data)
{
  GSList *tmp;
  GSList *copy;

  meta_topic (META_DEBUG_GEOMETRY, "Clearing the move_resize queue\n");

  /* Work with a copy, for reentrancy. The allowed reentrancy isn't
   * complete; destroying a window while we're in here would result in
   * badness. But it's OK to queue/unqueue move_resizes.
   */
  copy = g_slist_copy (move_resize_pending);
  g_slist_free (move_resize_pending);
  move_resize_pending = NULL;
  move_resize_idle = 0;

  destroying_windows_disallowed += 1;
  
  tmp = copy;
  while (tmp != NULL)
    {
      MetaWindow *window;

      window = tmp->data;

      /* As a side effect, sets window->move_resize_queued = FALSE */
      meta_window_move_resize_now (window); 
      
      tmp = tmp->next;
    }

  g_slist_free (copy);

  destroying_windows_disallowed -= 1;
  
  return FALSE;
}

void
meta_window_unqueue_move_resize (MetaWindow *window)
{
  if (!window->move_resize_queued)
    return;

  meta_topic (META_DEBUG_GEOMETRY,
              "Removing %s from the move_resize queue\n",
              window->desc);

  /* Note that window may not actually be in move_resize_pending
   * because it may have been in "copy" inside the idle handler
   */
  move_resize_pending = g_slist_remove (move_resize_pending, window);
  window->move_resize_queued = FALSE;
  
  if (move_resize_pending == NULL &&
      move_resize_idle != 0)
    {
      g_source_remove (move_resize_idle);
      move_resize_idle = 0;
    }
}

void
meta_window_flush_move_resize (MetaWindow *window)
{
  if (window->move_resize_queued)
    {
      meta_window_unqueue_move_resize (window);
      meta_window_move_resize_now (window);
    }
}

/* The move/resize queue is only used when we need to
 * recheck the constraints on the window, e.g. when
 * maximizing or when changing struts. Configure requests
 * and such always have to be handled synchronously,
 * they can't be done via a queue.
 */
void
meta_window_queue_move_resize (MetaWindow  *window)
{
  if (window->unmanaging)
    return;

  if (window->move_resize_queued)
    return;

  meta_topic (META_DEBUG_GEOMETRY,
              "Putting %s in the move_resize queue\n",
              window->desc);
  
  window->move_resize_queued = TRUE;
  
  if (move_resize_idle == 0)
    move_resize_idle = g_idle_add_full (META_PRIORITY_RESIZE,
                                        idle_move_resize, NULL, NULL);
  
  move_resize_pending = g_slist_prepend (move_resize_pending, window);
}

void
meta_window_get_position (MetaWindow  *window,
                          int         *x,
                          int         *y)
{
  if (window->frame)
    {
      if (x)
        *x = window->frame->rect.x + window->frame->child_x;
      if (y)
        *y = window->frame->rect.y + window->frame->child_y;
    }
  else
    {
      if (x)
        *x = window->rect.x;
      if (y)
        *y = window->rect.y;
    }
}

void
meta_window_get_user_position  (MetaWindow  *window,
                                int         *x,
                                int         *y)
{
  if (window->user_has_move_resized)
    {
      if (x)
        *x = window->user_rect.x;
      if (y)
        *y = window->user_rect.y;
    }
  else
    {
      meta_window_get_position (window, x, y);
    }
}

void
meta_window_get_gravity_position (MetaWindow  *window,
                                  int         *root_x,
                                  int         *root_y)
{
  MetaRectangle frame_extents;
  int w, h;
  int x, y;
  
  w = window->rect.width;
  h = window->rect.height;

  if (window->size_hints.win_gravity == StaticGravity)
    {
      frame_extents = window->rect;
      if (window->frame)
        {
          frame_extents.x = window->frame->rect.x + window->frame->child_x;
          frame_extents.y = window->frame->rect.y + window->frame->child_y;
        }
    }
  else
    {
      if (window->frame == NULL)
        frame_extents = window->rect;
      else
        frame_extents = window->frame->rect;
    }

  x = frame_extents.x;
  y = frame_extents.y;
  
  switch (window->size_hints.win_gravity)
    {
    case NorthGravity:
    case CenterGravity:
    case SouthGravity:
      /* Find center of frame. */
      x += frame_extents.width / 2;
      /* Center client window on that point. */
      x -= w / 2;
      break;
      
    case SouthEastGravity:
    case EastGravity:
    case NorthEastGravity:
      /* Find right edge of frame */
      x += frame_extents.width;
      /* Align left edge of client at that point. */
      x -= w;
      break;
    default:
      break;
    }
  
  switch (window->size_hints.win_gravity)
    {
    case WestGravity:
    case CenterGravity:
    case EastGravity:
      /* Find center of frame. */
      y += frame_extents.height / 2;
      /* Center client window there. */
      y -= h / 2;
      break;
    case SouthWestGravity:
    case SouthGravity:
    case SouthEastGravity:
      /* Find south edge of frame */
      y += frame_extents.height;
      /* Place bottom edge of client there */
      y -= h;
      break;
    default:
      break;
    }
  
  if (root_x)
    *root_x = x;
  if (root_y)
    *root_y = y;
}

void
meta_window_get_geometry (MetaWindow  *window,
                          int         *x,
                          int         *y,
                          int         *width,
                          int         *height)
{
  meta_window_get_gravity_position (window, x, y);

  *width = (window->rect.width - window->size_hints.base_width) /
    window->size_hints.width_inc;
  *height = (window->rect.height - window->size_hints.base_height) /
    window->size_hints.height_inc;
}

void
meta_window_get_outer_rect (MetaWindow    *window,
                            MetaRectangle *rect)
{
  if (window->frame)
    *rect = window->frame->rect;
  else
    *rect = window->rect;
}

void
meta_window_get_xor_rect (MetaWindow          *window,
                          const MetaRectangle *grab_wireframe_rect,
                          MetaRectangle       *xor_rect)
{
  if (window->frame)
    {
      xor_rect->x = grab_wireframe_rect->x - window->frame->child_x;
      xor_rect->y = grab_wireframe_rect->y - window->frame->child_y;
      xor_rect->width = grab_wireframe_rect->width + window->frame->child_x + window->frame->right_width;

      if (window->shaded)
        xor_rect->height = window->frame->child_y;
      else
        xor_rect->height = grab_wireframe_rect->height + window->frame->child_y + window->frame->bottom_height;
    }
  else
    *xor_rect = *grab_wireframe_rect;
}

const char*
meta_window_get_startup_id (MetaWindow *window)
{
  if (window->startup_id == NULL)
    {
      MetaGroup *group;

      group = meta_window_get_group (window);

      if (group != NULL)
        return meta_group_get_startup_id (group);
    }

  return window->startup_id;
}

void
meta_window_focus (MetaWindow  *window,
                   Time         timestamp)
{  
  meta_topic (META_DEBUG_FOCUS,
              "Setting input focus to window %s, input: %d take_focus: %d\n",
              window->desc, window->input, window->take_focus);
  
  if (window->display->grab_window &&
      window->display->grab_window->all_keys_grabbed)
    {
      meta_topic (META_DEBUG_FOCUS,
                  "Current focus window %s has global keygrab, not focusing window %s after all\n",
                  window->display->grab_window->desc, window->desc);
      return;
    }

  meta_window_flush_calc_showing (window);

  if (!window->mapped && !window->shaded)
    {
      meta_topic (META_DEBUG_FOCUS,
                  "Window %s is not showing, not focusing after all\n",
                  window->desc);
      return;
    }

  /* For output-only or shaded windows, focus the frame.
   * This seems to result in the client window getting key events
   * though, so I don't know if it's icccm-compliant.
   *
   * Still, we have to do this or keynav breaks for these windows.
   */
  if (window->frame &&
      (window->shaded ||
       !(window->input || window->take_focus)))
    {
      if (window->frame)
        {
          meta_topic (META_DEBUG_FOCUS,
                      "Focusing frame of %s\n", window->desc);
          XSetInputFocus (window->display->xdisplay,
                          window->frame->xwindow,
                          RevertToPointerRoot,
                          timestamp);
          window->display->expected_focus_window = window;
        }
    }
  else
    {
      meta_error_trap_push (window->display);
      
      if (window->input)
        {
          meta_topic (META_DEBUG_FOCUS,
                      "Calling XSetInputFocus() on client window %s since input = true\n",
                      window->desc);
          XSetInputFocus (window->display->xdisplay,
                          window->xwindow,
                          RevertToPointerRoot,
                          timestamp);
          window->display->expected_focus_window = window;
        }
      
      if (window->take_focus)
        {
          meta_topic (META_DEBUG_FOCUS,
                      "Sending WM_TAKE_FOCUS to %s since take_focus = true\n",
                      window->desc);
          meta_window_send_icccm_message (window,
                                          window->display->atom_wm_take_focus,
                                          timestamp);
          window->display->expected_focus_window = window;
        }
      
      meta_error_trap_pop (window->display, FALSE);
    }

  if (window->wm_state_demands_attention)
    {
      window->wm_state_demands_attention = FALSE;
      set_net_wm_state (window);
    }  

  /* Check if there's an autoraise timeout for a different window */
  if (window != window->display->autoraise_window)
    meta_display_remove_autoraise_callback (window->display);
}

static void
meta_window_change_workspace_without_transients (MetaWindow    *window,
                                                 MetaWorkspace *workspace)
{
  GList *next;
  
  meta_verbose ("Changing window %s to workspace %d\n",
                window->desc, meta_workspace_index (workspace));
  
  /* unstick if stuck. meta_window_unstick would call 
   * meta_window_change_workspace recursively if the window
   * is not in the active workspace.
   */
  if (window->on_all_workspaces)
    meta_window_unstick (window);

  /* See if we're already on this space. If not, make sure we are */
  if (g_list_find (window->workspaces, workspace) == NULL)
    meta_workspace_add_window (workspace, window);
  
  /* Remove from all other spaces */
  next = window->workspaces;
  while (next != NULL)
    {
      MetaWorkspace *remove;
      remove = next->data;
      next = next->next;

      if (remove != workspace)
        meta_workspace_remove_window (remove, window);
    }

  /* list size == 1 */
  g_assert (window->workspaces != NULL);
  g_assert (window->workspaces->next == NULL);
}

static gboolean
change_workspace_foreach (MetaWindow *window,
                          void       *data)
{
  meta_window_change_workspace_without_transients (window, data);
  return TRUE;
}

void
meta_window_change_workspace (MetaWindow    *window,
                              MetaWorkspace *workspace)
{
  meta_window_change_workspace_without_transients (window, workspace);

  meta_window_foreach_transient (window, change_workspace_foreach,
                                 workspace);
}

void
meta_window_stick (MetaWindow  *window)
{
  GList *tmp; 
  MetaWorkspace *workspace;

  meta_verbose ("Sticking window %s current on_all_workspaces = %d\n",
                window->desc, window->on_all_workspaces);
  
  if (window->on_all_workspaces)
    return;

  /* We don't change window->workspaces, because we revert
   * to that original workspace list if on_all_workspaces is
   * toggled back off.
   */
  window->on_all_workspaces = TRUE;

  /* We do, however, change the MRU lists of all the workspaces
   */
  tmp = window->screen->workspaces;
  while (tmp)
    {
      workspace = (MetaWorkspace *) tmp->data;
      if (!g_list_find (workspace->mru_list, window))
        workspace->mru_list = g_list_prepend (workspace->mru_list, window);

      tmp = tmp->next;
    }

  meta_window_set_current_workspace_hint (window);
  
  meta_window_queue_calc_showing (window);
}

void
meta_window_unstick (MetaWindow  *window)
{
  GList *tmp;
  MetaWorkspace *workspace;

  if (!window->on_all_workspaces)
    return;

  /* Revert to window->workspaces */

  window->on_all_workspaces = FALSE;

  /* Remove window from MRU lists that it doesn't belong in */
  tmp = window->screen->workspaces;
  while (tmp)
    {
      workspace = (MetaWorkspace *) tmp->data;
      if (!meta_workspace_contains_window (workspace, window))
        workspace->mru_list = g_list_remove (workspace->mru_list, window);
      tmp = tmp->next;
    }

  /* We change ourselves to the active workspace, since otherwise you'd get
   * a weird window-vaporization effect. Once we have UI for being
   * on more than one workspace this should probably be add_workspace
   * not change_workspace.
   */
  if (!meta_workspace_contains_window (window->screen->active_workspace,
                                       window))
    meta_window_change_workspace (window, window->screen->active_workspace);
  
  meta_window_set_current_workspace_hint (window);
  
  meta_window_queue_calc_showing (window);
}

unsigned long
meta_window_get_net_wm_desktop (MetaWindow *window)
{
  if (window->on_all_workspaces ||
      g_list_length (window->workspaces) > 1)
    return 0xFFFFFFFF;
  else
    return meta_workspace_index (window->workspaces->data);
}

static void
update_net_frame_extents (MetaWindow *window)
{
  unsigned long data[4] = { 0, 0, 0, 0 };

  if (window->frame)
    {
      /* Left */
      data[0] = window->frame->child_x;
      /* Right */
      data[1] = window->frame->right_width;
      /* Top */
      data[2] = window->frame->child_y;
      /* Bottom */
      data[3] = window->frame->bottom_height;
    }

  meta_topic (META_DEBUG_GEOMETRY,
              "Setting _NET_FRAME_EXTENTS on managed window 0x%lx "
              "to top = %ld, left = %ld, bottom = %ld, right = %ld\n",
              window->xwindow, data[0], data[1], data[2], data[3]);

  meta_error_trap_push (window->display);
  XChangeProperty (window->display->xdisplay, window->xwindow,
                   window->display->atom_net_frame_extents,
                   XA_CARDINAL,
                   32, PropModeReplace, (guchar*) data, 4);
  meta_error_trap_pop (window->display, FALSE);
}

void
meta_window_set_current_workspace_hint (MetaWindow *window)
{
  /* FIXME if on more than one workspace, we claim to be "sticky",
   * the WM spec doesn't say what to do here.
   */
  unsigned long data[1];

  if (window->workspaces == NULL)
    {
      /* this happens when unmanaging windows */      
      return;
    }
  
  data[0] = meta_window_get_net_wm_desktop (window);

  meta_verbose ("Setting _NET_WM_DESKTOP of %s to %ld\n",
                window->desc, data[0]);
  
  meta_error_trap_push (window->display);
  XChangeProperty (window->display->xdisplay, window->xwindow,
                   window->display->atom_net_wm_desktop,
                   XA_CARDINAL,
                   32, PropModeReplace, (guchar*) data, 1);
  meta_error_trap_pop (window->display, FALSE);
}

void
meta_window_raise (MetaWindow  *window)
{
  meta_topic (META_DEBUG_WINDOW_OPS,
              "Raising window %s\n", window->desc);

  meta_stack_raise (window->screen->stack, window);
}

void
meta_window_lower (MetaWindow  *window)
{
  meta_topic (META_DEBUG_WINDOW_OPS,
              "Lowering window %s\n", window->desc);

  meta_stack_lower (window->screen->stack, window);
}

void
meta_window_send_icccm_message (MetaWindow *window,
                                Atom        atom,
                                Time        timestamp)
{
  /* This comment and code are from twm, copyright
   * Open Group, Evans & Sutherland, etc.
   */
  
  /*
   * ICCCM Client Messages - Section 4.2.8 of the ICCCM dictates that all
   * client messages will have the following form:
   *
   *     event type	ClientMessage
   *     message type	_XA_WM_PROTOCOLS
   *     window		tmp->w
   *     format		32
   *     data[0]		message atom
   *     data[1]		time stamp
   */
  
    XClientMessageEvent ev;
    
    ev.type = ClientMessage;
    ev.window = window->xwindow;
    ev.message_type = window->display->atom_wm_protocols;
    ev.format = 32;
    ev.data.l[0] = atom;
    ev.data.l[1] = timestamp;

    meta_error_trap_push (window->display);
    XSendEvent (window->display->xdisplay,
                window->xwindow, False, 0, (XEvent*) &ev);
    meta_error_trap_pop (window->display, FALSE);
}

gboolean
meta_window_configure_request (MetaWindow *window,
                               XEvent     *event)
{
  int x, y, width, height;
  gboolean only_resize;
  gboolean allow_position_change;
  gboolean in_grab_op;

  /* We ignore configure requests while the user is moving/resizing
   * the window, since these represent the app sucking and fighting
   * the user, most likely due to a bug in the app (e.g. pfaedit
   * seemed to do this)
   *
   * Still have to do the ConfigureNotify and all, but pretend the
   * app asked for the current size/position instead of the new one.
   */  
  in_grab_op = FALSE;
  if (window->display->grab_op != META_GRAB_OP_NONE &&
      window == window->display->grab_window)
    {
      switch (window->display->grab_op)
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
          in_grab_op = TRUE;
          break;
        default:
          break;
        }
    }
  
  /* it's essential to use only the explicitly-set fields,
   * and otherwise use our current up-to-date position.
   *
   * Otherwise you get spurious position changes when the app changes
   * size, for example, if window->rect is not in sync with the
   * server-side position in effect when the configure request was
   * generated.
   */

  meta_window_get_gravity_position (window, &x, &y);

  only_resize = TRUE;

  allow_position_change = FALSE;
  
  if (meta_prefs_get_disable_workarounds ())
    {
      if (window->type == META_WINDOW_DIALOG ||
          window->type == META_WINDOW_MODAL_DIALOG ||
          window->type == META_WINDOW_SPLASHSCREEN)
        ; /* No position change for these */
      else if ((window->size_hints.flags & PPosition) ||
               /* USPosition is just stale if window is placed;
                * no --geometry involved here.
                */
               ((window->size_hints.flags & USPosition) &&
                !window->placed))
        allow_position_change = TRUE;
    }
  else
    {
      allow_position_change = TRUE;
    }

  if (in_grab_op)
    allow_position_change = FALSE;
  
  if (allow_position_change)
    {
      if (event->xconfigurerequest.value_mask & CWX)
        x = event->xconfigurerequest.x;
      
      if (event->xconfigurerequest.value_mask & CWY)
        y = event->xconfigurerequest.y;

      if (event->xconfigurerequest.value_mask & (CWX | CWY))
        {
          only_resize = FALSE;

          /* Once manually positioned, windows shouldn't be placed
           * by the window manager.
           */
          window->placed = TRUE;
        }
    }
  else
    {
      meta_topic (META_DEBUG_GEOMETRY,
		  "Not allowing position change for window %s PPosition 0x%lx USPosition 0x%lx type %d\n", 
		  window->desc, window->size_hints.flags & PPosition, 
		  window->size_hints.flags & USPosition,
		  window->type);     
    }

  width = window->rect.width;
  height = window->rect.height;

  if (!in_grab_op)
    {
      if (event->xconfigurerequest.value_mask & CWWidth)
        width = event->xconfigurerequest.width;
      
      if (event->xconfigurerequest.value_mask & CWHeight)
        height = event->xconfigurerequest.height;
    }

  /* ICCCM 4.1.5 */
  
  /* Note that x, y is the corner of the window border,
   * and width, height is the size of the window inside
   * its border, but that we always deny border requests
   * and give windows a border of 0. But we save the
   * requested border here.
   */
  window->border_width = event->xconfigurerequest.border_width;

  /* We're ignoring the value_mask here, since sizes
   * not in the mask will be the current window geometry.
   */
  
  window->size_hints.x = x;
  window->size_hints.y = y;
  window->size_hints.width = width;
  window->size_hints.height = height;

  /* FIXME passing the gravity on only_resize thing is kind of crack-rock.
   * Basically I now have several ways of handling gravity, and things
   * don't make too much sense. I think I am doing the math in a couple
   * places and could do it in only one function, and remove some of the
   * move_resize_internal arguments.
   */
  
  meta_window_move_resize_internal (window, META_IS_CONFIGURE_REQUEST,
                                    only_resize ?
                                    window->size_hints.win_gravity : NorthWestGravity,
                                    window->size_hints.x,
                                    window->size_hints.y,
                                    window->size_hints.width,
                                    window->size_hints.height);

  /* Handle stacking. We only handle raises/lowers, mostly because
   * stack.c really can't deal with anything else.  I guess we'll fix
   * that if a client turns up that really requires it. Only a very
   * few clients even require the raise/lower (and in fact all client
   * attempts to deal with stacking order are essentially broken,
   * since they have no idea what other clients are involved or how
   * the stack looks).
   *
   * I'm pretty sure no interesting client uses TopIf, BottomIf, or
   * Opposite anyway, so the only possible missing thing is
   * Above/Below with a sibling set. For now we just pretend there's
   * never a sibling set and always do the full raise/lower instead of
   * the raise-just-above/below-sibling.
   */
  if (event->xconfigurerequest.value_mask & CWStackMode)
    {
      switch (event->xconfigurerequest.detail)
        {
        case Above:
          meta_window_raise (window);
          break;
        case Below:
          meta_window_lower (window);
          break;
        case TopIf:
        case BottomIf:
        case Opposite:
          break;
        }
    }      
  
  return TRUE;
}

gboolean
meta_window_property_notify (MetaWindow *window,
                             XEvent     *event)
{
  return process_property_notify (window, &event->xproperty);  
}

#define _NET_WM_MOVERESIZE_SIZE_TOPLEFT      0
#define _NET_WM_MOVERESIZE_SIZE_TOP          1
#define _NET_WM_MOVERESIZE_SIZE_TOPRIGHT     2
#define _NET_WM_MOVERESIZE_SIZE_RIGHT        3
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT  4
#define _NET_WM_MOVERESIZE_SIZE_BOTTOM       5
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT   6
#define _NET_WM_MOVERESIZE_SIZE_LEFT         7
#define _NET_WM_MOVERESIZE_MOVE              8
#define _NET_WM_MOVERESIZE_SIZE_KEYBOARD     9
#define _NET_WM_MOVERESIZE_MOVE_KEYBOARD    10

gboolean
meta_window_client_message (MetaWindow *window,
                            XEvent     *event)
{
  MetaDisplay *display;

  display = window->display;
  
  if (event->xclient.message_type ==
      display->atom_net_close_window)
    {
      Time timestamp;

      if (event->xclient.data.l[0] != 0)
	timestamp = event->xclient.data.l[0];
      else
	timestamp = meta_display_get_current_time (window->display);
      
      meta_window_delete (window, timestamp);

      return TRUE;
    }
  else if (event->xclient.message_type ==
           display->atom_net_wm_desktop)
    {
      int space;
      MetaWorkspace *workspace;
              
      space = event->xclient.data.l[0];
              
      meta_verbose ("Request to move %s to workspace %d\n",
                    window->desc, space);

      workspace =
        meta_screen_get_workspace_by_index (window->screen,
                                            space);

      if (workspace)
        {
          if (window->on_all_workspaces)
            meta_window_unstick (window);
          meta_window_change_workspace (window, workspace);
        }
      else if (space == (int) 0xFFFFFFFF)
        {
          meta_window_stick (window);
        }
      else
        {
          meta_verbose ("No such workspace %d for screen\n", space);
        }

      meta_verbose ("Window %s now on_all_workspaces = %d\n",
                    window->desc, window->on_all_workspaces);
      
      return TRUE;
    }
  else if (event->xclient.message_type ==
           display->atom_net_wm_state)
    {
      gulong action;
      Atom first;
      Atom second;

      action = event->xclient.data.l[0];
      first = event->xclient.data.l[1];
      second = event->xclient.data.l[2];
      
      if (meta_is_verbose ())
        {
          char *str1;
          char *str2;

          meta_error_trap_push_with_return (display);
          str1 = XGetAtomName (display->xdisplay, first);
          if (meta_error_trap_pop_with_return (display, TRUE) != Success)
            str1 = NULL;

          meta_error_trap_push_with_return (display);
          str2 = XGetAtomName (display->xdisplay, second); 
          if (meta_error_trap_pop_with_return (display, TRUE) != Success)
            str2 = NULL;
          
          meta_verbose ("Request to change _NET_WM_STATE action %ld atom1: %s atom2: %s\n",
                        action,
                        str1 ? str1 : "(unknown)",
                        str2 ? str2 : "(unknown)");

          meta_XFree (str1);
          meta_XFree (str2);
        }

      if (first == display->atom_net_wm_state_shaded ||
          second == display->atom_net_wm_state_shaded)
        {
          gboolean shade;

          shade = (action == _NET_WM_STATE_ADD ||
                   (action == _NET_WM_STATE_TOGGLE && !window->shaded));
          if (shade && window->has_shade_func)
            meta_window_shade (window);
          else
            meta_window_unshade (window);
        }

      if (first == display->atom_net_wm_state_fullscreen ||
          second == display->atom_net_wm_state_fullscreen)
        {
          gboolean make_fullscreen;

          make_fullscreen = (action == _NET_WM_STATE_ADD ||
                             (action == _NET_WM_STATE_TOGGLE && !window->fullscreen));
          if (make_fullscreen && window->has_fullscreen_func)
            meta_window_make_fullscreen (window);
          else
            meta_window_unmake_fullscreen (window);
        }
      
      if (first == display->atom_net_wm_state_maximized_horz ||
          second == display->atom_net_wm_state_maximized_horz ||
          first == display->atom_net_wm_state_maximized_vert ||
          second == display->atom_net_wm_state_maximized_vert)
        {
          gboolean max;

          max = (action == _NET_WM_STATE_ADD ||
                 (action == _NET_WM_STATE_TOGGLE && !window->maximized));
          if (max && window->has_maximize_func)
            meta_window_maximize (window);
          else
            meta_window_unmaximize (window);
        }

      if (first == display->atom_net_wm_state_modal ||
          second == display->atom_net_wm_state_modal)
        {
          window->wm_state_modal =
            (action == _NET_WM_STATE_ADD) ||
            (action == _NET_WM_STATE_TOGGLE && !window->wm_state_modal);
          
          recalc_window_type (window);
          meta_window_queue_move_resize (window);
        }

      if (first == display->atom_net_wm_state_skip_pager ||
          second == display->atom_net_wm_state_skip_pager)
        {
          window->wm_state_skip_pager = 
            (action == _NET_WM_STATE_ADD) ||
            (action == _NET_WM_STATE_TOGGLE && !window->skip_pager);

          recalc_window_features (window);
          set_net_wm_state (window);
        }

      if (first == display->atom_net_wm_state_skip_taskbar ||
          second == display->atom_net_wm_state_skip_taskbar)
        {
          window->wm_state_skip_taskbar =
            (action == _NET_WM_STATE_ADD) ||
            (action == _NET_WM_STATE_TOGGLE && !window->skip_taskbar);

          recalc_window_features (window);
          set_net_wm_state (window);
        }

      if (first == display->atom_net_wm_state_above ||
          second == display->atom_net_wm_state_above)
        {
          window->wm_state_above = 
            (action == _NET_WM_STATE_ADD) ||
            (action == _NET_WM_STATE_TOGGLE && !window->wm_state_above);

          meta_window_update_layer (window);
          set_net_wm_state (window);
        }

      if (first == display->atom_net_wm_state_below ||
          second == display->atom_net_wm_state_below)
        {
          window->wm_state_below = 
            (action == _NET_WM_STATE_ADD) ||
            (action == _NET_WM_STATE_TOGGLE && !window->wm_state_below);

          meta_window_update_layer (window);
          set_net_wm_state (window);
        }

      if (first == display->atom_net_wm_state_demands_attention ||
          second == display->atom_net_wm_state_demands_attention)
        {
          window->wm_state_demands_attention = 
            (action == _NET_WM_STATE_ADD) ||
            (action == _NET_WM_STATE_TOGGLE && !window->wm_state_demands_attention);

          set_net_wm_state (window);
        }
      
      return TRUE;
    }
  else if (event->xclient.message_type ==
           display->atom_wm_change_state)
    {
      meta_verbose ("WM_CHANGE_STATE client message, state: %ld\n",
                    event->xclient.data.l[0]);
      if (event->xclient.data.l[0] == IconicState &&
          window->has_minimize_func)
        {
          meta_window_minimize (window);

          /* If the window being minimized was the one with focus,
           * then we should focus a new window.  We often receive this
           * wm_change_state message when a user has clicked on the
           * tasklist on the panel; in those cases, the current
           * focus_window is the panel, but the previous focus_window
           * will have been this one.  This is a special case where we
           * need to manually handle focusing a new window because
           * meta_window_minimize will get it wrong.
           */
          if (window->display->focus_window &&
              (window->display->focus_window->type == META_WINDOW_DOCK ||
               window->display->focus_window->type == META_WINDOW_DESKTOP) &&
              window->display->previously_focused_window == window)
            {
              meta_topic (META_DEBUG_FOCUS,
                          "Focusing default window because of minimization of former focus window %s, which was due to a wm_change_state client message\n",
                      window->desc);
              meta_workspace_focus_default_window (window->screen->active_workspace, window, meta_display_get_current_time_roundtrip (window->display));
            }
        }

      return TRUE;
    }
  else if (event->xclient.message_type ==
           display->atom_net_wm_moveresize)
    {
      int x_root;
      int y_root;
      int action;
      MetaGrabOp op;
      int button;
      
      x_root = event->xclient.data.l[0];
      y_root = event->xclient.data.l[1];
      action = event->xclient.data.l[2];
      button = event->xclient.data.l[3];

      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Received _NET_WM_MOVERESIZE message on %s, %d,%d action = %d, button %d\n",
                  window->desc,
                  x_root, y_root, action, button);
      
      op = META_GRAB_OP_NONE;
      switch (action)
        {
        case _NET_WM_MOVERESIZE_SIZE_TOPLEFT:
          op = META_GRAB_OP_RESIZING_NW;
          break;
        case _NET_WM_MOVERESIZE_SIZE_TOP:
          op = META_GRAB_OP_RESIZING_N;
          break;
        case _NET_WM_MOVERESIZE_SIZE_TOPRIGHT:
          op = META_GRAB_OP_RESIZING_NE;
          break;
        case _NET_WM_MOVERESIZE_SIZE_RIGHT:
          op = META_GRAB_OP_RESIZING_E;
          break;
        case _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT:
          op = META_GRAB_OP_RESIZING_SE;
          break;
        case _NET_WM_MOVERESIZE_SIZE_BOTTOM:          
          op = META_GRAB_OP_RESIZING_S;
          break;
        case _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT:
          op = META_GRAB_OP_RESIZING_SW;
          break;
        case _NET_WM_MOVERESIZE_SIZE_LEFT:
          op = META_GRAB_OP_RESIZING_W;
          break;
        case _NET_WM_MOVERESIZE_MOVE:
          op = META_GRAB_OP_MOVING;
          break;
        case _NET_WM_MOVERESIZE_SIZE_KEYBOARD:
          op = META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN;
          break;
        case _NET_WM_MOVERESIZE_MOVE_KEYBOARD:
          op = META_GRAB_OP_KEYBOARD_MOVING;
          break;
        default:
          break;
        }

      if (op != META_GRAB_OP_NONE &&
          ((window->has_move_func && op == META_GRAB_OP_KEYBOARD_MOVING) ||
           (window->has_resize_func && op == META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN)))
        {

          meta_window_begin_grab_op (window,
                                     op,
                                     meta_display_get_current_time (window->display));
        }
      else if (op != META_GRAB_OP_NONE &&
               ((window->has_move_func && op == META_GRAB_OP_MOVING) ||
               (window->has_resize_func && 
                (op != META_GRAB_OP_MOVING && 
                 op != META_GRAB_OP_KEYBOARD_MOVING))))
        {
          /*
           * the button SHOULD already be included in the message
           */
          if (button == 0)
            {
              int x, y, query_root_x, query_root_y;
              Window root, child;
              guint mask;

              /* The race conditions in this _NET_WM_MOVERESIZE thing
               * are mind-boggling
               */
              mask = 0;
              meta_error_trap_push (window->display);
              XQueryPointer (window->display->xdisplay,
                             window->xwindow,
                             &root, &child,
                             &query_root_x, &query_root_y,
                             &x, &y,
                             &mask);
              meta_error_trap_pop (window->display, TRUE);

              if (mask & Button1Mask)
                button = 1;
              else if (mask & Button2Mask)
                button = 2;
              else if (mask & Button3Mask)
                button = 3;
              else
                button = 0;
            }

          if (button != 0)
            {
              meta_topic (META_DEBUG_WINDOW_OPS,
                          "Beginning move/resize with button = %d\n", button);
              meta_display_begin_grab_op (window->display,
                                          window->screen,
                                          window,
                                          op,
                                          FALSE, 0 /* event_serial */,
                                          button, 0,
                                          meta_display_get_current_time (window->display),
                                          x_root,
                                          y_root);
            }
        }

      return TRUE;
    }
  else if (event->xclient.message_type ==
           display->atom_net_active_window)
    {
      meta_verbose ("_NET_ACTIVE_WINDOW request for window '%s', activating",
                    window->desc);

      if (event->xclient.data.l[0] != 0)
        {
          /* Client supports newer _NET_ACTIVE_WINDOW with a
           * convenient timestamp
           */
          meta_window_activate (window,
                                event->xclient.data.l[1]);
          
        }
      else
        {
          /* Client using older EWMH _NET_ACTIVE_WINDOW without a
           * timestamp
           */
          meta_window_activate (window, meta_display_get_current_time (window->display));
        }
      return TRUE;
    }
  
  return FALSE;
}

static gboolean
is_in_dock_group (MetaWindow *window)
{
  MetaGroup *group;
  GSList *list;
  GSList *tmp;
  
  if (META_WINDOW_IN_DOCK_TAB_CHAIN (window))
    return TRUE;

  if (window->type == META_WINDOW_NORMAL)
    return FALSE;

  /* If a transient-type window is in dock group,
   * return TRUE
   */
  group = meta_window_get_group (window);
  list = meta_group_list_windows (group);
  tmp = list;
  while (tmp != NULL)
    {
      MetaWindow *gw = tmp->data;

      if (META_WINDOW_IN_DOCK_TAB_CHAIN (gw))
        {
          g_slist_free (list);
          return TRUE;
        }
      
      tmp = tmp->next;
    }

  g_slist_free (list);
  return FALSE;
}

static int
docks_at_end_cmp (const void *a,
                  const void *b)
{
  MetaWindow *window_a = (MetaWindow*) a;
  MetaWindow *window_b = (MetaWindow*) b;

  if (META_WINDOW_IN_DOCK_TAB_CHAIN (window_a))
    {
      if (META_WINDOW_IN_DOCK_TAB_CHAIN (window_b))
        return 0;
      else
        return 1; /* a > b since a is a dock */
    }
  else
    {
      if (META_WINDOW_IN_DOCK_TAB_CHAIN (window_b))
        return -1; /* b > a since b is a dock */
      else
        return 0;
    }
}

static void
shuffle_docks_to_end (GList **mru_list_p)
{
  *mru_list_p = g_list_sort (*mru_list_p,
                             docks_at_end_cmp);
}

gboolean
meta_window_notify_focus (MetaWindow *window,
                          XEvent     *event)
{
  /* note the event can be on either the window or the frame,
   * we focus the frame for shaded windows
   */
  
  /* The event can be FocusIn, FocusOut, or UnmapNotify.
   * On UnmapNotify we have to pretend it's focus out,
   * because we won't get a focus out if it occurs, apparently.
   */

  /* We ignore grabs, though this is questionable.
   * It may be better to increase the intelligence of
   * the focus window tracking.
   *
   * The problem is that keybindings for windows are done with
   * XGrabKey, which means focus_window disappears and the front of
   * the MRU list gets confused from what the user expects once a
   * keybinding is used.
   */  
  meta_topic (META_DEBUG_FOCUS,
              "Focus %s event received on %s 0x%lx (%s) "
              "mode %s detail %s\n",
              event->type == FocusIn ? "in" :
              event->type == FocusOut ? "out" :
              event->type == UnmapNotify ? "unmap" :
              "???",
              window->desc, event->xany.window,
              event->xany.window == window->xwindow ?
              "client window" :
              (window->frame && event->xany.window == window->frame->xwindow) ?
              "frame window" :
              "unknown window",
              event->type != UnmapNotify ?
              meta_event_mode_to_string (event->xfocus.mode) : "n/a",
              event->type != UnmapNotify ?
              meta_event_detail_to_string (event->xfocus.detail) : "n/a");

  /* FIXME our pointer tracking is broken; see how
   * gtk+/gdk/x11/gdkevents-x11.c or XFree86/xc/programs/xterm/misc.c
   * handle it for the correct way.  In brief you need to track
   * pointer focus and regular focus, and handle EnterNotify in
   * PointerRoot mode with no window manager.  However as noted above,
   * accurate focus tracking will break things because we want to keep
   * windows "focused" when using keybindings on them, and also we
   * sometimes "focus" a window by focusing its frame or
   * no_focus_window; so this all needs rethinking massively.
   *
   * My suggestion is to change it so that we clearly separate
   * actual keyboard focus tracking using the xterm algorithm,
   * and metacity's "pretend" focus window, and go through all
   * the code and decide which one should be used in each place;
   * a hard bit is deciding on a policy for that.
   *
   * http://bugzilla.gnome.org/show_bug.cgi?id=90382
   */
  
  if ((event->type == FocusIn ||
       event->type == FocusOut) &&
      (event->xfocus.mode == NotifyGrab ||
       event->xfocus.mode == NotifyUngrab ||
       /* From WindowMaker, ignore all funky pointer root events */
       event->xfocus.detail > NotifyNonlinearVirtual))
    {
      meta_topic (META_DEBUG_FOCUS,
                  "Ignoring focus event generated by a grab or other weirdness\n");
      return TRUE;
    }
    
  if (event->type == FocusIn)
    {
      if (window != window->display->focus_window)
        {
          meta_topic (META_DEBUG_FOCUS,
                      "* Focus --> %s\n", window->desc);
          window->display->focus_window = window;
          window->has_focus = TRUE;

          /* Move to the front of the focusing workspace's MRU list.
           * We should only be "removing" it from the MRU list if it's
           * not already there.  Note that it's possible that we might
           * be processing this FocusIn after we've changed to a
           * different workspace; we should therefore update the MRU
           * list only if the window is actually on the active
           * workspace.
           */
          if (window->screen->active_workspace &&
              meta_window_visible_on_workspace (window, 
                                                window->screen->active_workspace))
            {
              GList* link;
              link = g_list_find (window->screen->active_workspace->mru_list, 
                                  window);
              g_assert (link);

              window->screen->active_workspace->mru_list = 
                g_list_remove_link (window->screen->active_workspace->mru_list,
                                    link);
              g_list_free (link);

              window->screen->active_workspace->mru_list = 
                g_list_prepend (window->screen->active_workspace->mru_list, 
                                window);
              if (!is_in_dock_group (window))
                shuffle_docks_to_end (&window->screen->active_workspace->mru_list);
            }

          if (window->frame)
            meta_frame_queue_draw (window->frame);
          
          meta_error_trap_push (window->display);
          XInstallColormap (window->display->xdisplay,
                            window->colormap);
          meta_error_trap_pop (window->display, FALSE);

          /* move into FOCUSED_WINDOW layer */
          meta_window_update_layer (window);

          /* Ungrab click to focus button since the sync grab can interfere
           * with some things you might do inside the focused window, by
           * causing the client to get funky enter/leave events.
           */
          if (meta_prefs_get_focus_mode () == META_FOCUS_MODE_CLICK)
            meta_display_ungrab_focus_window_button (window->display, window);
        }
    }
  else if (event->type == FocusOut ||
           event->type == UnmapNotify)
    {
      if (event->type == FocusOut &&
          event->xfocus.detail == NotifyInferior)
        {
          /* This event means the client moved focus to a subwindow */
          meta_topic (META_DEBUG_FOCUS,
                      "Ignoring focus out on %s with NotifyInferior\n",
                      window->desc);
          return TRUE;
        }
      
      if (window == window->display->focus_window)
        {
          meta_topic (META_DEBUG_FOCUS,
                      "%s is now the previous focus window due to being focused out or unmapped\n",
                      window->desc);

          meta_topic (META_DEBUG_FOCUS,
                      "* Focus --> NULL (was %s)\n", window->desc);
          
          window->display->previously_focused_window =
            window->display->focus_window;
          window->display->focus_window = NULL;
          window->has_focus = FALSE;
          if (window->frame)
            meta_frame_queue_draw (window->frame);

          meta_error_trap_push (window->display);
          XUninstallColormap (window->display->xdisplay,
                              window->colormap);
          meta_error_trap_pop (window->display, FALSE);

          /* move out of FOCUSED_WINDOW layer */
          meta_window_update_layer (window);

          /* Re-grab for click to focus, if necessary */
          if (meta_prefs_get_focus_mode () == META_FOCUS_MODE_CLICK)
            meta_display_grab_focus_window_button (window->display, window);
       }
    }

  /* Now set _NET_ACTIVE_WINDOW hint */
  meta_display_update_active_window_hint (window->display);
  
  return FALSE;
}

static gboolean
process_property_notify (MetaWindow     *window,
                         XPropertyEvent *event)
{
  /* FIXME once we move entirely to the window-props.h framework, we
   * can just call reload on the property in the event and get rid of
   * this if-else chain.
   */
  
  if (event->atom == XA_WM_NAME)
    {
      meta_verbose ("Property notify on %s for WM_NAME\n", window->desc);

      /* don't bother reloading WM_NAME if using _NET_WM_NAME already */
      if (!window->using_net_wm_name)
        meta_window_reload_property (window, XA_WM_NAME);
    }
  else if (event->atom == window->display->atom_net_wm_name)
    {
      meta_verbose ("Property notify on %s for NET_WM_NAME\n", window->desc);
      meta_window_reload_property (window, window->display->atom_net_wm_name);
      
      /* if _NET_WM_NAME was unset, reload WM_NAME */
      if (!window->using_net_wm_name)
        meta_window_reload_property (window, XA_WM_NAME);      
    }
  else if (event->atom == XA_WM_ICON_NAME)
    {
      meta_verbose ("Property notify on %s for WM_ICON_NAME\n", window->desc);

      /* don't bother reloading WM_ICON_NAME if using _NET_WM_ICON_NAME already */
      if (!window->using_net_wm_icon_name)
        meta_window_reload_property (window, XA_WM_ICON_NAME);
    }
  else if (event->atom == window->display->atom_net_wm_icon_name)
    {
      meta_verbose ("Property notify on %s for NET_WM_ICON_NAME\n", window->desc);
      meta_window_reload_property (window, window->display->atom_net_wm_icon_name);
      
      /* if _NET_WM_ICON_NAME was unset, reload WM_ICON_NAME */
      if (!window->using_net_wm_icon_name)
        meta_window_reload_property (window, XA_WM_ICON_NAME);
    }  
  else if (event->atom == XA_WM_NORMAL_HINTS)
    {
      meta_verbose ("Property notify on %s for WM_NORMAL_HINTS\n", window->desc);
      
      meta_window_reload_property (window, XA_WM_NORMAL_HINTS);
      
      /* See if we need to constrain current size */
      meta_window_queue_move_resize (window);
    }
  else if (event->atom == window->display->atom_wm_protocols)
    {
      meta_verbose ("Property notify on %s for WM_PROTOCOLS\n", window->desc);
      
      meta_window_reload_property (window, window->display->atom_wm_protocols);
    }
  else if (event->atom == XA_WM_HINTS)
    {
      meta_verbose ("Property notify on %s for WM_HINTS\n", window->desc);

      meta_window_reload_property (window, XA_WM_HINTS);
    }
  else if (event->atom == window->display->atom_motif_wm_hints)
    {
      meta_verbose ("Property notify on %s for MOTIF_WM_HINTS\n", window->desc);
      
      update_mwm_hints (window);
      
      if (window->decorated)
        meta_window_ensure_frame (window);
      else
        meta_window_destroy_frame (window);

      meta_window_queue_move_resize (window);
      /* because ensure/destroy frame may unmap */
      meta_window_queue_calc_showing (window);
    }
  else if (event->atom == XA_WM_CLASS)
    {
      meta_verbose ("Property notify on %s for WM_CLASS\n", window->desc);
      
      update_wm_class (window);
    }
  else if (event->atom == XA_WM_TRANSIENT_FOR)
    {
      meta_verbose ("Property notify on %s for WM_TRANSIENT_FOR\n", window->desc);
      
      update_transient_for (window);

      meta_window_queue_move_resize (window);
    }
  else if (event->atom ==
           window->display->atom_wm_window_role)
    {
      meta_verbose ("Property notify on %s for WM_WINDOW_ROLE\n", window->desc);
      
      update_role (window);
    }
  else if (event->atom ==
           window->display->atom_wm_client_leader ||
           event->atom ==
           window->display->atom_sm_client_id)
    {
      meta_warning ("Broken client! Window %s changed client leader window or SM client ID\n", window->desc);
    }
  else if (event->atom ==
           window->display->atom_net_wm_window_type)
    {
      meta_verbose ("Property notify on %s for NET_WM_WINDOW_TYPE\n", window->desc);
      update_net_wm_type (window);
    }
  else if (event->atom == window->display->atom_net_wm_icon)
    {
      meta_verbose ("Property notify on %s for NET_WM_ICON\n", window->desc);
      meta_icon_cache_property_changed (&window->icon_cache,
                                        window->display,
                                        event->atom);
      meta_window_queue_update_icon (window);
    }
  else if (event->atom == window->display->atom_kwm_win_icon)
    {
      meta_verbose ("Property notify on %s for KWM_WIN_ICON\n", window->desc);

      meta_icon_cache_property_changed (&window->icon_cache,
                                        window->display,
                                        event->atom);
      meta_window_queue_update_icon (window);
    }
  else if ((event->atom == window->display->atom_net_wm_strut) ||
	   (event->atom == window->display->atom_net_wm_strut_partial))
    {
      meta_verbose ("Property notify on %s for _NET_WM_STRUT\n", window->desc);
      meta_window_update_struts (window);
    }
  else if (event->atom == window->display->atom_net_startup_id)
    {
      meta_verbose ("Property notify on %s for _NET_STARTUP_ID\n", window->desc);
      
      meta_window_reload_property (window,
                                   window->display->atom_net_startup_id);
    }
  else if (event->atom == window->display->atom_net_wm_sync_request_counter)
    {
      meta_verbose ("Property notify on %s for _NET_WM_SYNC_REQUEST_COUNTER\n", window->desc);
      
      meta_window_reload_property (window,
                                   window->display->atom_net_wm_sync_request_counter);
    }
  else if (event->atom == window->display->atom_net_wm_user_time)
    {
      meta_verbose ("Property notify on %s for _NET_WM_USER_TIME\n", window->desc);
      
      meta_window_reload_property (window,
                                   window->display->atom_net_wm_user_time);
    }

  return TRUE;
}

static void
send_configure_notify (MetaWindow *window)
{
  XEvent event;

  /* from twm */
  
  event.type = ConfigureNotify;
  event.xconfigure.display = window->display->xdisplay;
  event.xconfigure.event = window->xwindow;
  event.xconfigure.window = window->xwindow;
  event.xconfigure.x = window->rect.x - window->border_width;
  event.xconfigure.y = window->rect.y - window->border_width;
  if (window->frame)
    {
      /* Need to be in root window coordinates */
      event.xconfigure.x += window->frame->rect.x;
      event.xconfigure.y += window->frame->rect.y;
    }
  event.xconfigure.width = window->rect.width;
  event.xconfigure.height = window->rect.height;
  event.xconfigure.border_width = window->border_width; /* requested not actual */
  event.xconfigure.above = None; /* FIXME */
  event.xconfigure.override_redirect = False;

  meta_topic (META_DEBUG_GEOMETRY,
              "Sending synthetic configure notify to %s with x: %d y: %d w: %d h: %d\n",
              window->desc,
              event.xconfigure.x, event.xconfigure.y,
              event.xconfigure.width, event.xconfigure.height);
  
  meta_error_trap_push (window->display);
  XSendEvent (window->display->xdisplay,
              window->xwindow,
              False, StructureNotifyMask, &event);
  meta_error_trap_pop (window->display, FALSE);
}

static void
update_net_wm_state (MetaWindow *window)
{
  /* We know this is only on initial window creation,
   * clients don't change the property.
   */

  int n_atoms;
  Atom *atoms;

  window->shaded = FALSE;
  window->maximized = FALSE;
  window->wm_state_modal = FALSE;
  window->wm_state_skip_taskbar = FALSE;
  window->wm_state_skip_pager = FALSE;
  window->wm_state_above = FALSE;
  window->wm_state_below = FALSE;
  window->wm_state_demands_attention = FALSE;
  
  if (meta_prop_get_atom_list (window->display, window->xwindow,
                               window->display->atom_net_wm_state,
                               &atoms, &n_atoms))
    {
      int i;
      
      i = 0;
      while (i < n_atoms)
        {
          if (atoms[i] == window->display->atom_net_wm_state_shaded)
            window->shaded = TRUE;
          else if (atoms[i] == window->display->atom_net_wm_state_maximized_horz)
            window->maximize_after_placement = TRUE;
          else if (atoms[i] == window->display->atom_net_wm_state_maximized_vert)
            window->maximize_after_placement = TRUE;
          else if (atoms[i] == window->display->atom_net_wm_state_modal)
            window->wm_state_modal = TRUE;
          else if (atoms[i] == window->display->atom_net_wm_state_skip_taskbar)
            window->wm_state_skip_taskbar = TRUE;
          else if (atoms[i] == window->display->atom_net_wm_state_skip_pager)
            window->wm_state_skip_pager = TRUE;
          else if (atoms[i] == window->display->atom_net_wm_state_fullscreen)
            window->fullscreen = TRUE;
          else if (atoms[i] == window->display->atom_net_wm_state_above)
            window->wm_state_above = TRUE;
          else if (atoms[i] == window->display->atom_net_wm_state_below)
            window->wm_state_below = TRUE;
          else if (atoms[i] == window->display->atom_net_wm_state_demands_attention)
            window->wm_state_demands_attention = TRUE;
          
          ++i;
        }
  
      meta_XFree (atoms);
    }
  
  recalc_window_type (window);
}


static void
update_mwm_hints (MetaWindow *window)
{
  MotifWmHints *hints;

  window->mwm_decorated = TRUE;
  window->mwm_border_only = FALSE;
  window->mwm_has_close_func = TRUE;
  window->mwm_has_minimize_func = TRUE;
  window->mwm_has_maximize_func = TRUE;
  window->mwm_has_move_func = TRUE;
  window->mwm_has_resize_func = TRUE;

  if (!meta_prop_get_motif_hints (window->display, window->xwindow,
                                  window->display->atom_motif_wm_hints,
                                  &hints))
    {
      meta_verbose ("Window %s has no MWM hints\n", window->desc);
      return;
    }
  
  /* We support those MWM hints deemed non-stupid */

  meta_verbose ("Window %s has MWM hints\n",
                window->desc);
  
  if (hints->flags & MWM_HINTS_DECORATIONS)
    {
      meta_verbose ("Window %s sets MWM_HINTS_DECORATIONS 0x%lx\n",
                    window->desc, hints->decorations);

      if (hints->decorations == 0)
        window->mwm_decorated = FALSE;
      /* some input methods use this */
      else if (hints->decorations == MWM_DECOR_BORDER)
        window->mwm_border_only = TRUE;
    }
  else
    meta_verbose ("Decorations flag unset\n");
  
  if (hints->flags & MWM_HINTS_FUNCTIONS)
    {
      gboolean toggle_value;
      
      meta_verbose ("Window %s sets MWM_HINTS_FUNCTIONS 0x%lx\n",
                    window->desc, hints->functions);

      /* If _ALL is specified, then other flags indicate what to turn off;
       * if ALL is not specified, flags are what to turn on.
       * at least, I think so
       */
      
      if ((hints->functions & MWM_FUNC_ALL) == 0)
        {
          toggle_value = TRUE;

          meta_verbose ("Window %s disables all funcs then reenables some\n",
                        window->desc);
          window->mwm_has_close_func = FALSE;
          window->mwm_has_minimize_func = FALSE;
          window->mwm_has_maximize_func = FALSE;
          window->mwm_has_move_func = FALSE;
          window->mwm_has_resize_func = FALSE;
        }
      else
        {
          meta_verbose ("Window %s enables all funcs then disables some\n",
                        window->desc);
          toggle_value = FALSE;
        }
      
      if ((hints->functions & MWM_FUNC_CLOSE) != 0)
        {
          meta_verbose ("Window %s toggles close via MWM hints\n",
                        window->desc);
          window->mwm_has_close_func = toggle_value;
        }
      if ((hints->functions & MWM_FUNC_MINIMIZE) != 0)
        {
          meta_verbose ("Window %s toggles minimize via MWM hints\n",
                        window->desc);
          window->mwm_has_minimize_func = toggle_value;
        }
      if ((hints->functions & MWM_FUNC_MAXIMIZE) != 0)
        {
          meta_verbose ("Window %s toggles maximize via MWM hints\n",
                        window->desc);
          window->mwm_has_maximize_func = toggle_value;
        }
      if ((hints->functions & MWM_FUNC_MOVE) != 0)
        {
          meta_verbose ("Window %s toggles move via MWM hints\n",
                        window->desc);
          window->mwm_has_move_func = toggle_value;
        }
      if ((hints->functions & MWM_FUNC_RESIZE) != 0)
        {
          meta_verbose ("Window %s toggles resize via MWM hints\n",
                        window->desc);
          window->mwm_has_resize_func = toggle_value;
        }
    }
  else
    meta_verbose ("Functions flag unset\n");

  meta_XFree (hints);

  recalc_window_features (window);
}

gboolean
meta_window_get_icon_geometry (MetaWindow    *window,
                               MetaRectangle *rect)
{
  gulong *geometry = NULL;
  int nitems;

  if (meta_prop_get_cardinal_list (window->display,
                                   window->xwindow,
                                   window->display->atom_net_wm_icon_geometry,
                                   &geometry, &nitems))
    {
      if (nitems != 4)
        {
          meta_verbose ("_NET_WM_ICON_GEOMETRY on %s has %d values instead of 4\n",
                        window->desc, nitems);
          meta_XFree (geometry);
          return FALSE;
        }
  
      if (rect)
        {
          rect->x = geometry[0];
          rect->y = geometry[1];
          rect->width = geometry[2];
          rect->height = geometry[3];
        }

      meta_XFree (geometry);

      return TRUE;
    }

  return FALSE;
}

static void
update_wm_class (MetaWindow *window)
{
  XClassHint ch;
  
  if (window->res_class)
    g_free (window->res_class);
  if (window->res_name)
    g_free (window->res_name);

  window->res_class = NULL;
  window->res_name = NULL;

  ch.res_name = NULL;
  ch.res_class = NULL;

  meta_prop_get_class_hint (window->display,
                            window->xwindow,
                            XA_WM_CLASS,
                            &ch);

  if (ch.res_name)
    {
      window->res_name = g_strdup (ch.res_name);
      XFree (ch.res_name);
    }

  if (ch.res_class)
    {
      window->res_class = g_strdup (ch.res_class);
      XFree (ch.res_class);
    }

  meta_verbose ("Window %s class: '%s' name: '%s'\n",
                window->desc,
                window->res_class ? window->res_class : "none",
                window->res_name ? window->res_name : "none");
}

static Window
read_client_leader (MetaDisplay *display,
                    Window       xwindow)
{
  Window retval = None;
  
  meta_prop_get_window (display, xwindow,
                        display->atom_wm_client_leader,
                        &retval);

  return retval;
}

typedef struct
{
  Window leader;  
} ClientLeaderData;

static gboolean
find_client_leader_func (MetaWindow *ancestor,
                         void       *data)
{
  ClientLeaderData *d;

  d = data;

  d->leader = read_client_leader (ancestor->display,
                                  ancestor->xwindow);

  /* keep going if no client leader found */
  return d->leader == None;
}

static void
update_sm_hints (MetaWindow *window)
{
  Window leader;
  
  window->xclient_leader = None;
  window->sm_client_id = NULL;

  /* If not on the current window, we can get the client
   * leader from transient parents. If we find a client
   * leader, we read the SM_CLIENT_ID from it.
   */
  leader = read_client_leader (window->display, window->xwindow);
  if (leader == None)
    {
      ClientLeaderData d;
      d.leader = None;
      meta_window_foreach_ancestor (window, find_client_leader_func,
                                    &d);
      leader = d.leader;
    }
      
  if (leader != None)
    {
      char *str;
      
      window->xclient_leader = leader;

      if (meta_prop_get_latin1_string (window->display, leader,
                                       window->display->atom_sm_client_id,
                                       &str))
        {
          window->sm_client_id = g_strdup (str);
          meta_XFree (str);
        }
    }
  else
    {
      meta_verbose ("Didn't find a client leader for %s\n", window->desc);

      if (!meta_prefs_get_disable_workarounds ())
        {
          /* Some broken apps (kdelibs fault?) set SM_CLIENT_ID on the app
           * instead of the client leader
           */
          char *str;

          str = NULL;
          if (meta_prop_get_latin1_string (window->display, window->xwindow,
                                           window->display->atom_sm_client_id,
                                           &str))
            {
              if (window->sm_client_id == NULL) /* first time through */
                meta_warning (_("Window %s sets SM_CLIENT_ID on itself, instead of on the WM_CLIENT_LEADER window as specified in the ICCCM.\n"),
                              window->desc);
              
              window->sm_client_id = g_strdup (str);
              meta_XFree (str);
            }
        }
    }

  meta_verbose ("Window %s client leader: 0x%lx SM_CLIENT_ID: '%s'\n",
                window->desc, window->xclient_leader,
                window->sm_client_id ? window->sm_client_id : "none");
}

static void
update_role (MetaWindow *window)
{
  char *str;
  
  if (window->role)
    g_free (window->role);
  window->role = NULL;

  if (meta_prop_get_latin1_string (window->display, window->xwindow,
                                   window->display->atom_wm_window_role,
                                   &str))
    {
      window->role = g_strdup (str);
      meta_XFree (str);
    }

  meta_verbose ("Updated role of %s to '%s'\n",
                window->desc, window->role ? window->role : "null");
}

static void
update_transient_for (MetaWindow *window)
{
  Window w;

  meta_error_trap_push (window->display);
  w = None;
  XGetTransientForHint (window->display->xdisplay,
                        window->xwindow,
                        &w);
  meta_error_trap_pop (window->display, TRUE);
  window->xtransient_for = w;

  window->transient_parent_is_root_window =
    window->xtransient_for == window->screen->xroot;
  
  if (window->xtransient_for != None)
    meta_verbose ("Window %s transient for 0x%lx (root = %d)\n", window->desc,
                  window->xtransient_for, window->transient_parent_is_root_window);
  else
    meta_verbose ("Window %s is not transient\n", window->desc);
  
  /* may now be a dialog */
  recalc_window_type (window);

  /* update stacking constraints */
  meta_stack_update_transient (window->screen->stack, window);
}

/* some legacy cruft */
typedef enum
{
  WIN_LAYER_DESKTOP     = 0,
  WIN_LAYER_BELOW       = 2,
  WIN_LAYER_NORMAL      = 4,
  WIN_LAYER_ONTOP       = 6,
  WIN_LAYER_DOCK        = 8,
  WIN_LAYER_ABOVE_DOCK  = 10
} GnomeWinLayer;

static void
update_net_wm_type (MetaWindow *window)
{
  int n_atoms;
  Atom *atoms;
  int i;

  window->type_atom = None;
  n_atoms = 0;
  atoms = NULL;
  
  meta_prop_get_atom_list (window->display, window->xwindow, 
                           window->display->atom_net_wm_window_type,
                           &atoms, &n_atoms);

  i = 0;
  while (i < n_atoms)
    {
      /* We break as soon as we find one we recognize,
       * supposed to prefer those near the front of the list
       */
      if (atoms[i] == window->display->atom_net_wm_window_type_desktop ||
          atoms[i] == window->display->atom_net_wm_window_type_dock ||
          atoms[i] == window->display->atom_net_wm_window_type_toolbar ||
          atoms[i] == window->display->atom_net_wm_window_type_menu ||
          atoms[i] == window->display->atom_net_wm_window_type_dialog ||
          atoms[i] == window->display->atom_net_wm_window_type_normal ||
          atoms[i] == window->display->atom_net_wm_window_type_utility ||
          atoms[i] == window->display->atom_net_wm_window_type_splash)
        {
          window->type_atom = atoms[i];
          break;
        }
      
      ++i;
    }
  
  meta_XFree (atoms);

  if (meta_is_verbose ())
    {
      char *str;

      str = NULL;
      if (window->type_atom != None)
        {
          meta_error_trap_push (window->display);
          str = XGetAtomName (window->display->xdisplay, window->type_atom);
          meta_error_trap_pop (window->display, TRUE);
        }

      meta_verbose ("Window %s type atom %s\n", window->desc,
                    str ? str : "(none)");

      if (str)
        meta_XFree (str);
    }
  
  recalc_window_type (window);
}

static void
redraw_icon (MetaWindow *window)
{
  /* We could probably be smart and just redraw the icon here,
   * instead of the whole frame.
   */
  if (window->frame && (window->mapped || window->frame->mapped))
    meta_ui_queue_frame_draw (window->screen->ui, window->frame->xwindow);
}

static void
meta_window_update_icon_now (MetaWindow *window)
{
  GdkPixbuf *icon;
  GdkPixbuf *mini_icon;

  icon = NULL;
  mini_icon = NULL;
  
  if (meta_read_icons (window->screen,
                       window->xwindow,
                       &window->icon_cache,
                       window->wm_hints_pixmap,
                       window->wm_hints_mask,
                       &icon,
                       META_ICON_WIDTH, META_ICON_HEIGHT,
                       &mini_icon,
                       META_MINI_ICON_WIDTH,
                       META_MINI_ICON_HEIGHT))
    {
      if (window->icon)
        g_object_unref (G_OBJECT (window->icon));
      
      if (window->mini_icon)
        g_object_unref (G_OBJECT (window->mini_icon));
      
      window->icon = icon;
      window->mini_icon = mini_icon;

      redraw_icon (window);
    }
  
  g_assert (window->icon);
  g_assert (window->mini_icon);
}

static guint update_icon_idle = 0;
static GSList *update_icon_pending = NULL;

static gboolean
idle_update_icon (gpointer data)
{
  GSList *tmp;
  GSList *copy;

  meta_topic (META_DEBUG_GEOMETRY, "Clearing the update_icon queue\n");

  /* Work with a copy, for reentrancy. The allowed reentrancy isn't
   * complete; destroying a window while we're in here would result in
   * badness. But it's OK to queue/unqueue update_icons.
   */
  copy = g_slist_copy (update_icon_pending);
  g_slist_free (update_icon_pending);
  update_icon_pending = NULL;
  update_icon_idle = 0;

  destroying_windows_disallowed += 1;
  
  tmp = copy;
  while (tmp != NULL)
    {
      MetaWindow *window;

      window = tmp->data;

      meta_window_update_icon_now (window); 
      window->update_icon_queued = FALSE;
      
      tmp = tmp->next;
    }

  g_slist_free (copy);

  destroying_windows_disallowed -= 1;
  
  return FALSE;
}

void
meta_window_unqueue_update_icon (MetaWindow *window)
{
  if (!window->update_icon_queued)
    return;

  meta_topic (META_DEBUG_GEOMETRY,
              "Removing %s from the update_icon queue\n",
              window->desc);

  /* Note that window may not actually be in update_icon_pending
   * because it may have been in "copy" inside the idle handler
   */
  update_icon_pending = g_slist_remove (update_icon_pending, window);
  window->update_icon_queued = FALSE;
  
  if (update_icon_pending == NULL &&
      update_icon_idle != 0)
    {
      g_source_remove (update_icon_idle);
      update_icon_idle = 0;
    }
}

void
meta_window_flush_update_icon (MetaWindow *window)
{
  if (window->update_icon_queued)
    {
      meta_window_unqueue_update_icon (window);
      meta_window_update_icon_now (window);
    }
}

void
meta_window_queue_update_icon (MetaWindow *window)
{
  if (window->unmanaging)
    return;

  if (window->update_icon_queued)
    return;

  meta_topic (META_DEBUG_GEOMETRY,
              "Putting %s in the update_icon queue\n",
              window->desc);
  
  window->update_icon_queued = TRUE;
  
  if (update_icon_idle == 0)
    update_icon_idle = g_idle_add (idle_update_icon, NULL);
  
  update_icon_pending = g_slist_prepend (update_icon_pending, window);
}

GList*
meta_window_get_workspaces (MetaWindow *window)
{
  if (window->on_all_workspaces)
    return window->screen->workspaces;
  else
    return window->workspaces;
}

static void
invalidate_work_areas (MetaWindow *window)
{
  GList *tmp;

  tmp = meta_window_get_workspaces (window);
  
  while (tmp != NULL)
    {
      meta_workspace_invalidate_work_area (tmp->data);
      tmp = tmp->next;
    }
}

void
meta_window_update_struts (MetaWindow *window)
{
  gulong *struts = NULL;
  int nitems;
  gboolean old_has_struts;
  gboolean new_has_struts;

  MetaRectangle old_left;
  MetaRectangle old_right;
  MetaRectangle old_top;
  MetaRectangle old_bottom;

  MetaRectangle new_left;
  MetaRectangle new_right;
  MetaRectangle new_top;
  MetaRectangle new_bottom;

  /**
   * This gap must be kept to at least 75 pixels, since otherwise
   * struts on opposite sides of the screen left/right could interfere
   * in each other in a way that makes it so there is no feasible
   * solution to the constraint satisfaction problem.  See
   * constraints.c.
   */
#define MIN_EMPTY (76)
  
  meta_verbose ("Updating struts for %s\n", window->desc);
  
  if (window->struts)
    {
      old_has_struts = TRUE;
      old_left = window->struts->left;
      old_right = window->struts->right;
      old_top = window->struts->top;
      old_bottom = window->struts->bottom;
    }
  else
    {
      old_has_struts = FALSE;
    }

  new_has_struts = FALSE;
  new_left.width = 0;
  new_left.x = 0;
  new_left.y = 0;
  new_left.height = window->screen->height;

  new_right.width = 0;
  new_right.x = window->screen->width;
  new_right.y = 0;
  new_right.height = window->screen->height;

  new_top.height = 0;
  new_top.y = 0;
  new_top.x = 0;
  new_top.width = window->screen->width;

  new_bottom.height = 0;
  new_bottom.y = window->screen->height;
  new_bottom.x = 0;
  new_bottom.width = window->screen->width;
  
  if (meta_prop_get_cardinal_list (window->display,
                                   window->xwindow,
                                   window->display->atom_net_wm_strut_partial,
                                   &struts, &nitems))
    {
      if (nitems != 12)
        {
          meta_verbose ("_NET_WM_STRUT_PARTIAL on %s has %d values instead of 12\n",
                        window->desc, nitems);
        }
      else
        {
          int gap;
          gap = window->screen->width - struts[0] - struts[1];
          gap -= MIN_EMPTY;
          new_has_struts = TRUE;
          new_left.width = (int) struts[0] + MIN (0, gap/2);
          new_right.width = (int) struts[1] + MIN (0, gap/2);
          gap = window->screen->height - struts[2] - struts[3];
          gap -= MIN_EMPTY;
          new_top.height = (int)struts[2] + MIN (0, gap/2);
          new_bottom.height = (int)struts[3] + MIN (0, gap/2);
          new_right.x = window->screen->width - 
            new_right.width;
          new_bottom.y = window->screen->height - 
            new_bottom.height;
          new_left.y = struts[4];
          new_left.height = struts[5] - new_left.y + 1;
          new_right.y = struts[6];
          new_right.height = struts[7] - new_right.y + 1;
          new_top.x = struts[8];
          new_top.width = struts[9] - new_top.x + 1;
          new_bottom.x = struts[10];
          new_bottom.width = struts[11] - new_bottom.x + 1;
          
          meta_verbose ("_NET_WM_STRUT_PARTIAL struts %d %d %d %d for window %s\n",
                        new_left.width,
                        new_right.width,
                        new_top.height,
                        new_bottom.height,
                        window->desc);
          
        }
      meta_XFree (struts);
    }
  else
    {
      meta_verbose ("No _NET_WM_STRUT property for %s\n",
                    window->desc);
    }

  if (!new_has_struts)
    {
      if (meta_prop_get_cardinal_list (window->display,
                                       window->xwindow,
                                       window->display->atom_net_wm_strut,
                                       &struts, &nitems))
        {
          if (nitems != 4)
            {
              meta_verbose ("_NET_WM_STRUT on %s has %d values instead of 4\n",
                            window->desc, nitems);
            }
          else
            {
              int gap;
              gap = window->screen->width - struts[0] - struts[1];
              gap -= MIN_EMPTY;
              new_has_struts = TRUE;
              new_left.width = (int) struts[0] + MIN (0, gap/2);
              new_right.width = (int) struts[1] + MIN (0, gap/2);
              gap = window->screen->height - struts[2] - struts[3];
              gap -= MIN_EMPTY;
              new_top.height = (int)struts[2] + MIN (0, gap/2);
              new_bottom.height = (int)struts[3] + MIN (0, gap/2);
              new_left.x = 0;
              new_right.x = window->screen->width - 
                new_right.width;
              new_top.y = 0;
              new_bottom.y = window->screen->height -
                new_bottom.height;
              
              meta_verbose ("_NET_WM_STRUT struts %d %d %d %d for window %s\n",
                            new_left.width,
                            new_right.width,
                            new_top.height,
                            new_bottom.height,
                            window->desc);
              
            }
          meta_XFree (struts);
        }
      else
        {
          meta_verbose ("No _NET_WM_STRUT property for %s\n",
                        window->desc);
        }
    }
 
  if (old_has_struts != new_has_struts ||
      (new_has_struts && old_has_struts &&
       (!meta_rectangle_equal(&old_left, &new_left) ||
        !meta_rectangle_equal(&old_right, &new_right) ||
        !meta_rectangle_equal(&old_top, &new_top) ||
        !meta_rectangle_equal(&old_bottom, &new_bottom))))
    {
      if (new_has_struts)
        {
          if (!window->struts)
            window->struts = g_new (MetaStruts, 1);
              
          window->struts->left = new_left;
          window->struts->right = new_right;
          window->struts->top = new_top;
          window->struts->bottom = new_bottom;
        }
      else
        {
          g_free (window->struts);
          window->struts = NULL;
        }
      meta_topic (META_DEBUG_WORKAREA,
                  "Invalidating work areas of window %s due to struts update\n",
                  window->desc);
      invalidate_work_areas (window);
    }
  else
    {
      meta_topic (META_DEBUG_WORKAREA,
                  "Struts on %s were unchanged\n", window->desc);
    }
}

static void
recalc_window_type (MetaWindow *window)
{
  MetaWindowType old_type;

  old_type = window->type;
  
  if (window->type_atom != None)
    {
      if (window->type_atom  == window->display->atom_net_wm_window_type_desktop)
        window->type = META_WINDOW_DESKTOP;
      else if (window->type_atom  == window->display->atom_net_wm_window_type_dock)
        window->type = META_WINDOW_DOCK;
      else if (window->type_atom  == window->display->atom_net_wm_window_type_toolbar)
        window->type = META_WINDOW_TOOLBAR;
      else if (window->type_atom  == window->display->atom_net_wm_window_type_menu)
        window->type = META_WINDOW_MENU;
      else if (window->type_atom  == window->display->atom_net_wm_window_type_dialog)
        window->type = META_WINDOW_DIALOG;
      else if (window->type_atom  == window->display->atom_net_wm_window_type_normal)
        window->type = META_WINDOW_NORMAL;
      else if (window->type_atom  == window->display->atom_net_wm_window_type_utility)
        window->type = META_WINDOW_UTILITY;
      else if (window->type_atom  == window->display->atom_net_wm_window_type_splash)
        window->type = META_WINDOW_SPLASHSCREEN;
      else
        meta_bug ("Set a type atom for %s that wasn't handled in recalc_window_type\n",
                  window->desc);
    }
  else if (window->xtransient_for != None)
    {
      window->type = META_WINDOW_DIALOG;
    }
  else
    {
      window->type = META_WINDOW_NORMAL;
    }

  if (window->type == META_WINDOW_DIALOG &&
      window->wm_state_modal)
    window->type = META_WINDOW_MODAL_DIALOG;
  
  meta_verbose ("Calculated type %d for %s, old type %d\n",
                window->type, window->desc, old_type);

  if (old_type != window->type)
    {
      recalc_window_features (window);
      
      set_net_wm_state (window);
      
      /* Update frame */
      if (window->decorated)
        meta_window_ensure_frame (window);
      else
        meta_window_destroy_frame (window);
      
      /* update stacking constraints */
      meta_window_update_layer (window);

      meta_window_grab_keys (window);
    }
}

static void
set_allowed_actions_hint (MetaWindow *window)
{
#define MAX_N_ACTIONS 10
  unsigned long data[MAX_N_ACTIONS];
  int i;

  i = 0;
  if (window->has_move_func)
    {
      data[i] = window->display->atom_net_wm_action_move;
      ++i;
    }
  if (window->has_resize_func)
    {
      data[i] = window->display->atom_net_wm_action_resize;
      ++i;
      data[i] = window->display->atom_net_wm_action_fullscreen;
      ++i;
    }
  if (window->has_minimize_func)
    {
      data[i] = window->display->atom_net_wm_action_minimize;
      ++i;
    }
  if (window->has_shade_func)
    {
      data[i] = window->display->atom_net_wm_action_shade;
      ++i;
    }
  /* sticky according to EWMH is different from metacity's sticky;
   * metacity doesn't support EWMH sticky
   */
  if (window->has_maximize_func)
    {
      data[i] = window->display->atom_net_wm_action_maximize_horz;
      ++i;
      data[i] = window->display->atom_net_wm_action_maximize_vert;
      ++i;
    }
  /* We always allow this */
  data[i] = window->display->atom_net_wm_action_change_desktop;
  ++i;
  if (window->has_close_func)
    {
      data[i] = window->display->atom_net_wm_action_close;
      ++i;
    }

  g_assert (i <= MAX_N_ACTIONS);

  meta_verbose ("Setting _NET_WM_ALLOWED_ACTIONS with %d atoms\n", i);
  
  meta_error_trap_push (window->display);
  XChangeProperty (window->display->xdisplay, window->xwindow,
                   window->display->atom_net_wm_allowed_actions,
                   XA_ATOM,
                   32, PropModeReplace, (guchar*) data, i);
  meta_error_trap_pop (window->display, FALSE);
#undef MAX_N_ACTIONS
}

void
meta_window_recalc_features (MetaWindow *window)
{
  recalc_window_features (window);
}

static void
recalc_window_features (MetaWindow *window)
{
  gboolean old_has_close_func;
  gboolean old_has_minimize_func;
  gboolean old_has_move_func;
  gboolean old_has_resize_func;
  gboolean old_has_shade_func;
  gboolean old_always_sticky;

  old_has_close_func = window->has_close_func;
  old_has_minimize_func = window->has_minimize_func;
  old_has_move_func = window->has_move_func;
  old_has_resize_func = window->has_resize_func;
  old_has_shade_func = window->has_shade_func;
  old_always_sticky = window->always_sticky;

  /* Use MWM hints initially */
  window->decorated = window->mwm_decorated;
  window->border_only = window->mwm_border_only;
  window->has_close_func = window->mwm_has_close_func;
  window->has_minimize_func = window->mwm_has_minimize_func;
  window->has_maximize_func = window->mwm_has_maximize_func;
  window->has_move_func = window->mwm_has_move_func;
    
  window->has_resize_func = TRUE;  

  /* If min_size == max_size, then don't allow resize */
  if (window->size_hints.min_width == window->size_hints.max_width &&
      window->size_hints.min_height == window->size_hints.max_height)
    window->has_resize_func = FALSE;
  else if (!window->mwm_has_resize_func)
    {
      /* We ignore mwm_has_resize_func because WM_NORMAL_HINTS is the
       * authoritative source for that info. Some apps such as mplayer or
       * xine disable resize via MWM but not WM_NORMAL_HINTS, but that
       * leads to e.g. us not fullscreening their windows.  Apps that set
       * MWM but not WM_NORMAL_HINTS are basically broken. We complain
       * about these apps but make them work.
       */
      
      meta_warning (_("Window %s sets an MWM hint indicating it isn't resizable, but sets min size %d x %d and max size %d x %d; this doesn't make much sense.\n"),
                    window->desc,
                    window->size_hints.min_width,
                    window->size_hints.min_height,
                    window->size_hints.max_width,
                    window->size_hints.max_height);
    }

  window->has_shade_func = TRUE;
  window->has_fullscreen_func = TRUE;

  window->always_sticky = FALSE;
  
  /* Semantic category overrides the MWM hints */
  if (window->type == META_WINDOW_TOOLBAR)
    window->decorated = FALSE;

  if (window->type == META_WINDOW_DESKTOP ||
      window->type == META_WINDOW_DOCK)
    window->always_sticky = TRUE;
  
  if (window->type == META_WINDOW_DESKTOP ||
      window->type == META_WINDOW_DOCK ||
      window->type == META_WINDOW_SPLASHSCREEN)
    {
      window->decorated = FALSE;
      window->has_close_func = FALSE;
      window->has_shade_func = FALSE;

      /* FIXME this keeps panels and things from using
       * NET_WM_MOVERESIZE; the problem is that some
       * panels (edge panels) have fixed possible locations,
       * and others ("floating panels") do not.
       *
       * Perhaps we should require edge panels to explicitly
       * disable movement?
       */
      window->has_move_func = FALSE;
      window->has_resize_func = FALSE;
    }
  
  if (window->type != META_WINDOW_NORMAL)
    {
      window->has_minimize_func = FALSE;
      window->has_maximize_func = FALSE;
      window->has_fullscreen_func = FALSE;
    }

  if (!window->has_resize_func)
    {      
      window->has_maximize_func = FALSE;
      
      /* don't allow fullscreen if we can't resize, unless the size
       * is entire screen size (kind of broken, because we
       * actually fullscreen to xinerama head size not screen size)
       */
      if (window->size_hints.min_width == window->screen->width &&
          window->size_hints.min_height == window->screen->height &&
          !window->decorated)
        ; /* leave fullscreen available */
      else
        window->has_fullscreen_func = FALSE;
    }

  /* We leave fullscreen windows decorated, just push the frame outside
   * the screen. This avoids flickering to unparent them.
   *
   * Note that setting has_resize_func = FALSE here must come after the
   * above code that may disable fullscreen, because if the window
   * is not resizable purely due to fullscreen, we don't want to
   * disable fullscreen mode.
   */
  if (window->fullscreen)
    {
      window->has_shade_func = FALSE;
      window->has_move_func = FALSE;
      window->has_resize_func = FALSE;
      window->has_maximize_func = FALSE;
    }

  meta_topic (META_DEBUG_WINDOW_OPS,
              "Window %s fullscreen = %d not resizable, maximizable = %d fullscreenable = %d min size %dx%d max size %dx%d\n",
              window->desc,
              window->fullscreen,
              window->has_maximize_func, window->has_fullscreen_func,
              window->size_hints.min_width,
              window->size_hints.min_height,
              window->size_hints.max_width,
              window->size_hints.max_height);
  
  /* no shading if not decorated */
  if (!window->decorated || window->border_only)
    window->has_shade_func = FALSE;

  window->skip_taskbar = FALSE;
  window->skip_pager = FALSE;

  if (window->wm_state_skip_taskbar)
    window->skip_taskbar = TRUE;
  
  if (window->wm_state_skip_pager)
    window->skip_pager = TRUE;
  
  switch (window->type)
    {
      /* Force skip taskbar/pager on these window types */
    case META_WINDOW_DESKTOP:
    case META_WINDOW_DOCK:
    case META_WINDOW_TOOLBAR:
    case META_WINDOW_MENU:
    case META_WINDOW_UTILITY:
    case META_WINDOW_SPLASHSCREEN:
      window->skip_taskbar = TRUE;
      window->skip_pager = TRUE;
      break;

    case META_WINDOW_DIALOG:
    case META_WINDOW_MODAL_DIALOG:
      /* only skip taskbar if we have a real transient parent */
      if (window->xtransient_for != None &&
          window->xtransient_for != window->screen->xroot)
        window->skip_taskbar = TRUE;
      break;
      
    case META_WINDOW_NORMAL:
      break;
    }

  meta_topic (META_DEBUG_WINDOW_OPS,
              "Window %s decorated = %d border_only = %d has_close = %d has_minimize = %d has_maximize = %d has_move = %d has_shade = %d skip_taskbar = %d skip_pager = %d\n",
              window->desc,
              window->decorated,
              window->border_only,
              window->has_close_func,
              window->has_minimize_func,
              window->has_maximize_func,
              window->has_move_func,
              window->has_shade_func,
              window->skip_taskbar,
              window->skip_pager);
  
  /* FIXME:
   * Lame workaround for recalc_window_features
   * being used overzealously. The fix is to
   * only recalc_window_features when something
   * has actually changed.
   */
  if (old_has_close_func != window->has_close_func       ||
      old_has_minimize_func != window->has_minimize_func ||
      old_has_move_func != window->has_move_func         ||
      old_has_resize_func != window->has_resize_func     ||
      old_has_shade_func != window->has_shade_func       ||
      old_always_sticky != window->always_sticky)
    set_allowed_actions_hint (window);
    
  /* FIXME perhaps should ensure if we don't have a shade func,
   * we aren't shaded, etc.
   */
}

static void
menu_callback (MetaWindowMenu *menu,
               Display        *xdisplay,
               Window          client_xwindow,
	       Time	       timestamp,
               MetaMenuOp      op,
               int             workspace_index,
               gpointer        data)
{
  MetaDisplay *display;
  MetaWindow *window;
  MetaWorkspace *workspace;
  
  display = meta_display_for_x_display (xdisplay);
  window = meta_display_lookup_x_window (display, client_xwindow);
  workspace = NULL;
  
  if (window != NULL) /* window can be NULL */
    {
      meta_verbose ("Menu op %d on %s\n", op, window->desc);
      
      /* op can be 0 for none */
      switch (op)
        {
        case META_MENU_OP_DELETE:
          meta_window_delete (window, timestamp);
          break;

        case META_MENU_OP_MINIMIZE:
          meta_window_minimize (window);
          break;

        case META_MENU_OP_UNMAXIMIZE:
          meta_window_unmaximize (window);
          break;
      
        case META_MENU_OP_MAXIMIZE:
          meta_window_maximize (window);
          break;

        case META_MENU_OP_UNSHADE:
          meta_window_unshade (window);
          break;
      
        case META_MENU_OP_SHADE:
          meta_window_shade (window);
          break;
      
        case META_MENU_OP_MOVE_LEFT:
          workspace = meta_workspace_get_neighbor (window->screen->active_workspace,
                                                   META_MOTION_LEFT);
          break;

        case META_MENU_OP_MOVE_RIGHT:
          workspace = meta_workspace_get_neighbor (window->screen->active_workspace,
                                                   META_MOTION_RIGHT);
          break;

        case META_MENU_OP_MOVE_UP:
          workspace = meta_workspace_get_neighbor (window->screen->active_workspace,
                                                   META_MOTION_UP);
          break;

        case META_MENU_OP_MOVE_DOWN:
          workspace = meta_workspace_get_neighbor (window->screen->active_workspace,
                                                   META_MOTION_DOWN);
          break;

        case META_MENU_OP_WORKSPACES:
          workspace = meta_screen_get_workspace_by_index (window->screen,
                                                          workspace_index);
          break;

        case META_MENU_OP_STICK:
          meta_window_stick (window);
          break;

        case META_MENU_OP_UNSTICK:
          meta_window_unstick (window);
          break;

        case META_MENU_OP_ABOVE:
          meta_window_make_above (window);
          break;

        case META_MENU_OP_UNABOVE:
          meta_window_unmake_above (window);
          break;

        case META_MENU_OP_MOVE:
          meta_window_begin_grab_op (window,
                                     META_GRAB_OP_KEYBOARD_MOVING,
                                     meta_display_get_current_time (window->display));
          break;

        case META_MENU_OP_RESIZE:
          meta_window_begin_grab_op (window,
                                     META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN,
                                     meta_display_get_current_time (window->display));
          break;
          
        case 0:
          /* nothing */
          break;
          
        default:
          meta_warning (G_STRLOC": Unknown window op\n");
          break;
        }

      if (workspace)
	{
	  meta_window_change_workspace (window,
					workspace);
#if 0
	  meta_workspace_activate (workspace);
	  meta_window_raise (window);
#endif
	}
    }
  else
    {
      meta_verbose ("Menu callback on nonexistent window\n");
    }

  if (display->window_menu == menu)
    {
      display->window_menu = NULL;
      display->window_with_menu = NULL;
    }
  
  meta_ui_window_menu_free (menu);
}

void
meta_window_show_menu (MetaWindow *window,
                       int         root_x,
                       int         root_y,
                       int         button,
                       Time        timestamp)
{
  MetaMenuOp ops;
  MetaMenuOp insensitive;
  MetaWindowMenu *menu;
  MetaWorkspaceLayout layout;
  int n_workspaces;
  
  if (window->display->window_menu)
    {
      meta_ui_window_menu_free (window->display->window_menu);
      window->display->window_menu = NULL;
      window->display->window_with_menu = NULL;
    }

  ops = 0;
  insensitive = 0;

  ops |= (META_MENU_OP_DELETE | META_MENU_OP_MINIMIZE | META_MENU_OP_MOVE | META_MENU_OP_RESIZE);

  n_workspaces = meta_screen_get_n_workspaces (window->screen);

  if (n_workspaces > 1)
    ops |= META_MENU_OP_WORKSPACES;

  meta_screen_calc_workspace_layout (window->screen,
                                     n_workspaces,
                                     meta_workspace_index ( window->screen->active_workspace),
                                     &layout);

  if (!window->on_all_workspaces)
    {

      if (layout.current_col > 0)
        ops |= META_MENU_OP_MOVE_LEFT;
      if ((layout.current_col < layout.cols - 1) &&
          (layout.current_row * layout.cols + (layout.current_col + 1) < n_workspaces))
        ops |= META_MENU_OP_MOVE_RIGHT;
      if (layout.current_row > 0)
        ops |= META_MENU_OP_MOVE_UP;
      if ((layout.current_row < layout.rows - 1) &&
          ((layout.current_row + 1) * layout.cols + layout.current_col < n_workspaces))
        ops |= META_MENU_OP_MOVE_DOWN;
    }

  if (window->maximized)
    ops |= META_MENU_OP_UNMAXIMIZE;
  else
    ops |= META_MENU_OP_MAXIMIZE;
  
#if 0
  if (window->shaded)
    ops |= META_MENU_OP_UNSHADE;
  else
    ops |= META_MENU_OP_SHADE;
#endif

  if (window->on_all_workspaces)
    ops |= META_MENU_OP_UNSTICK;
  else
    ops |= META_MENU_OP_STICK;

  if (window->wm_state_above)
    ops |= META_MENU_OP_UNABOVE;
  else
    ops |= META_MENU_OP_ABOVE;
  
  if (!window->has_maximize_func)
    insensitive |= META_MENU_OP_UNMAXIMIZE | META_MENU_OP_MAXIMIZE;
  
  if (!window->has_minimize_func)
    insensitive |= META_MENU_OP_MINIMIZE;
  
  if (!window->has_close_func)
    insensitive |= META_MENU_OP_DELETE;

  if (!window->has_shade_func)
    insensitive |= META_MENU_OP_SHADE | META_MENU_OP_UNSHADE;

  if (!META_WINDOW_ALLOWS_MOVE (window))
    insensitive |= META_MENU_OP_MOVE;

  if (!META_WINDOW_ALLOWS_RESIZE (window))
    insensitive |= META_MENU_OP_RESIZE;

   if (window->always_sticky)
     insensitive |= META_MENU_OP_UNSTICK | META_MENU_OP_WORKSPACES;

  if ((window->type == META_WINDOW_DESKTOP) ||
      (window->type == META_WINDOW_DOCK) ||
      (window->type == META_WINDOW_SPLASHSCREEN))
    insensitive |= META_MENU_OP_ABOVE | META_MENU_OP_UNABOVE;
  
  menu =
    meta_ui_window_menu_new (window->screen->ui,
                             window->xwindow,
                             ops,
                             insensitive,
                             meta_window_get_net_wm_desktop (window),
                             meta_screen_get_n_workspaces (window->screen),
                             menu_callback,
                             NULL); 

  window->display->window_menu = menu;
  window->display->window_with_menu = window;
  
  meta_verbose ("Popping up window menu for %s\n", window->desc);
  
  meta_ui_window_menu_popup (menu, root_x, root_y, button, timestamp);
}

static double
timeval_to_ms (const GTimeVal *timeval)
{
  return (timeval->tv_sec * G_USEC_PER_SEC + timeval->tv_usec) / 1000.0;
}

static double
time_diff (const GTimeVal *first,
	   const GTimeVal *second)
{
  double first_ms = timeval_to_ms (first);
  double second_ms = timeval_to_ms (second);

  return first_ms - second_ms;
}

static gboolean
check_moveresize_frequency (MetaWindow *window, 
			    gdouble    *remaining)
{
  GTimeVal current_time;
  
  g_get_current_time (&current_time);

#ifdef HAVE_XSYNC
  if (!window->disable_sync &&
      window->display->grab_sync_request_alarm != None)
    {
      if (window->sync_request_time.tv_sec != 0 ||
	  window->sync_request_time.tv_usec != 0)
	{
	  double elapsed =
	    time_diff (&current_time, &window->sync_request_time);

	  if (elapsed < 1000.0)
	    {
	      /* We want to be sure that the timeout happens at
	       * a time where elapsed will definitely be
	       * greater than 1000, so we can disable sync
	       */
	      if (remaining)
		*remaining = 1000.0 - elapsed + 100;
	      
	      return FALSE;
	    }
	  else
	    {
	      /* We have now waited for more than a second for the
	       * application to respond to the sync request
	       */
	      window->disable_sync = TRUE;
	      return TRUE;
	    }
	}
      else
	{
	  /* No outstanding sync requests. Go ahead and resize
	   */
	  return TRUE;
	}
    }
  else
#endif /* HAVE_XSYNC */
    {
      const double max_resizes_per_second = 25.0;
      const double ms_between_resizes = 1000.0 / max_resizes_per_second;
      double elapsed;

      elapsed = time_diff (&current_time, &window->display->grab_last_moveresize_time);

      if (elapsed >= 0.0 && elapsed < ms_between_resizes)
	{
	  meta_topic (META_DEBUG_RESIZING,
		      "Delaying move/resize as only %g of %g ms elapsed\n",
		      elapsed, ms_between_resizes);
	  
	  if (remaining)
	    *remaining = (ms_between_resizes - elapsed);

	  return FALSE;
	}
      
      meta_topic (META_DEBUG_RESIZING,
		  " Checked moveresize freq, allowing move/resize now (%g of %g seconds elapsed)\n",
		  elapsed / 1000.0, 1.0 / max_resizes_per_second);
      
      return TRUE;
    }
}

static void
update_move (MetaWindow  *window,
             unsigned int mask,
             int          x,
             int          y)
{
  int dx, dy;
  int new_x, new_y;
  int shake_threshold;
  
  window->display->grab_latest_motion_x = x;
  window->display->grab_latest_motion_y = y;
  
  dx = x - window->display->grab_anchor_root_x;
  dy = y - window->display->grab_anchor_root_y;

  new_x = window->display->grab_anchor_window_pos.x + dx;
  new_y = window->display->grab_anchor_window_pos.y + dy;

  meta_verbose ("x,y = %d,%d anchor ptr %d,%d anchor pos %d,%d dx,dy %d,%d\n",
                x, y,
                window->display->grab_anchor_root_x,
                window->display->grab_anchor_root_y,
                window->display->grab_anchor_window_pos.x,
                window->display->grab_anchor_window_pos.y,
                dx, dy);
  
  /* shake loose (unmaximize) maximized window if dragged beyond the threshold
   * in the Y direction. You can't pull a window loose via X motion.
   */

#define DRAG_THRESHOLD_TO_SHAKE_THRESHOLD_FACTOR 6
  shake_threshold = meta_ui_get_drag_threshold (window->screen->ui) *
    DRAG_THRESHOLD_TO_SHAKE_THRESHOLD_FACTOR;
    
  if (window->maximized && ABS (dy) >= shake_threshold)
    {
      double prop;

      /* Shake loose */
      window->shaken_loose = TRUE;
                  
      /* move the unmaximized window to the cursor */
      prop = 
        ((double)(x - window->display->grab_initial_window_pos.x)) / 
        ((double)window->display->grab_initial_window_pos.width);

      window->display->grab_initial_window_pos.x = 
        x - window->saved_rect.width * prop;
      window->display->grab_initial_window_pos.y = y;

      if (window->frame)
        {
          window->display->grab_initial_window_pos.y += window->frame->child_y / 2;
        }

      window->saved_rect.x = window->display->grab_initial_window_pos.x;
      window->saved_rect.y = window->display->grab_initial_window_pos.y;
      window->display->grab_anchor_root_x = x;
      window->display->grab_anchor_root_y = y;

      meta_window_unmaximize (window);

      return;
    }
  /* remaximize window on an other xinerama monitor if window has
   * been shaken loose or it is still maximized (then move straight)
   */
  else if (window->shaken_loose || window->maximized)
    {
      const MetaXineramaScreenInfo *wxinerama;
      MetaRectangle work_area;
      int monitor;

      wxinerama = meta_screen_get_xinerama_for_window (window->screen, window);

      for (monitor = 0; monitor < window->screen->n_xinerama_infos; monitor++)
        {
          meta_window_get_work_area_for_xinerama (window, monitor, &work_area);

          /* check if cursor is near the top of a xinerama work area */ 
          if (x >= work_area.x &&
              x < (work_area.x + work_area.width) &&
              y >= work_area.y &&
              y < (work_area.y + shake_threshold))
            {
              /* move the saved rect if window will become maximized on an
               * other monitor so user isn't surprised on a later unmaximize
               */
              if (wxinerama->number != monitor)
                {
                  window->saved_rect.x = work_area.x;
                  window->saved_rect.y = work_area.y;
                  
                  if (window->frame) 
                    {
                      window->saved_rect.x += window->frame->child_x;
                      window->saved_rect.y += window->frame->child_y;
                    }

                  meta_window_unmaximize (window);
                }

              window->display->grab_initial_window_pos = work_area;
              window->display->grab_anchor_root_x = x;
              window->display->grab_anchor_root_y = y;
              window->shaken_loose = FALSE;
              
              meta_window_maximize (window);

              return;
            }
        }
    }

  /* don't allow a maximized window to move */
  if (window->maximized)
    return;

  if (window->display->grab_wireframe_active)
    {
      /* FIXME Horribly broken, does not honor position
       * constraints
       */
      MetaRectangle new_xor;

      window->display->grab_wireframe_rect.x = new_x;
      window->display->grab_wireframe_rect.y = new_y;

      meta_window_get_xor_rect (window,
                                &window->display->grab_wireframe_rect,
                                &new_xor);

      meta_effects_update_wireframe (window->screen,
                                     &window->display->grab_wireframe_last_xor_rect,
                                     &new_xor);
      window->display->grab_wireframe_last_xor_rect = new_xor;
    }
  else
    {
      /* FIXME, edge snapping broken in wireframe mode */
      if (mask & ShiftMask)
        {
          /* snap to edges */
          new_x = meta_window_find_nearest_vertical_edge (window, new_x);
          new_y = meta_window_find_nearest_horizontal_edge (window, new_y);
        }
      
      meta_window_move (window, TRUE, new_x, new_y);
    }
}

static void update_resize (MetaWindow *window,
			   int         x,
			   int         y,
			   gboolean    force);

static gboolean
update_resize_timeout (gpointer data)
{
  MetaWindow *window = data;

  update_resize (window, 
		 window->display->grab_latest_motion_x,
		 window->display->grab_latest_motion_y,
		 TRUE);
  return FALSE;
}

static void
update_resize (MetaWindow *window,
               int x, int y,
	       gboolean force)
{
  int dx, dy;
  int new_w, new_h;
  int gravity;
  MetaRectangle old;
  int new_x, new_y;
  double remaining;
  
  window->display->grab_latest_motion_x = x;
  window->display->grab_latest_motion_y = y;
  
  dx = x - window->display->grab_anchor_root_x;
  dy = y - window->display->grab_anchor_root_y;

  new_w = window->display->grab_anchor_window_pos.width;
  new_h = window->display->grab_anchor_window_pos.height;

  /* FIXME this is only used in wireframe mode */
  new_x = window->display->grab_anchor_window_pos.x;
  new_y = window->display->grab_anchor_window_pos.y;
  
  switch (window->display->grab_op)
    {
    case META_GRAB_OP_RESIZING_SE:
    case META_GRAB_OP_RESIZING_NE:
    case META_GRAB_OP_RESIZING_E:
    case META_GRAB_OP_KEYBOARD_RESIZING_SE:
    case META_GRAB_OP_KEYBOARD_RESIZING_NE:
    case META_GRAB_OP_KEYBOARD_RESIZING_E:
      new_w += dx;
      break;

    case META_GRAB_OP_RESIZING_NW:
    case META_GRAB_OP_RESIZING_SW:
    case META_GRAB_OP_RESIZING_W:
    case META_GRAB_OP_KEYBOARD_RESIZING_NW:
    case META_GRAB_OP_KEYBOARD_RESIZING_SW:
    case META_GRAB_OP_KEYBOARD_RESIZING_W:
      new_w -= dx;
      new_x += dx;
      break;
      
    default:
      break;
    }
  
  switch (window->display->grab_op)
    {
    case META_GRAB_OP_RESIZING_SE:
    case META_GRAB_OP_RESIZING_S:
    case META_GRAB_OP_RESIZING_SW:
    case META_GRAB_OP_KEYBOARD_RESIZING_SE:
    case META_GRAB_OP_KEYBOARD_RESIZING_S:
    case META_GRAB_OP_KEYBOARD_RESIZING_SW:
      new_h += dy;
      break;
      
    case META_GRAB_OP_RESIZING_N:
    case META_GRAB_OP_RESIZING_NE:
    case META_GRAB_OP_RESIZING_NW:
    case META_GRAB_OP_KEYBOARD_RESIZING_N:
    case META_GRAB_OP_KEYBOARD_RESIZING_NE:
    case META_GRAB_OP_KEYBOARD_RESIZING_NW:
      new_h -= dy;
      new_y += dy;
      break;
    default:
      break;
    }

  if (!check_moveresize_frequency (window, &remaining) && !force)
    {
      /* we are ignoring an event here, so we schedule a
       * compensation event when we would otherwise not ignore
       * an event. Otherwise we can become stuck if the user never
       * generates another event.
       */
      if (!window->display->grab_resize_timeout_id)
	{
	  window->display->grab_resize_timeout_id = 
	    g_timeout_add ((int)remaining, update_resize_timeout, window);
	}

      return;
    }

  /* Remove any scheduled compensation events */
  if (window->display->grab_resize_timeout_id)
    {
      g_source_remove (window->display->grab_resize_timeout_id);
      window->display->grab_resize_timeout_id = 0;
    }
  
  old = window->rect;

  /* compute gravity of client during operation */
  gravity = meta_resize_gravity_from_grab_op (window->display->grab_op);
  g_assert (gravity >= 0);
  
  if (window->display->grab_wireframe_active)
    {
      /* FIXME This is crap. For example, the wireframe isn't
       * constrained in the way that a real resize would be. An
       * obvious elegant solution is to unmap the window during
       * wireframe, but still resize it; however, that probably
       * confuses broken clients that have problems with opaque
       * resize, they probably don't track their visibility.
       */
      MetaRectangle new_xor;
      
      if ((new_x + new_w <= new_x) || (new_y + new_h <= new_y))
        return;
      
      window->display->grab_wireframe_rect.x = new_x;
      window->display->grab_wireframe_rect.y = new_y;
      window->display->grab_wireframe_rect.width = new_w;
      window->display->grab_wireframe_rect.height = new_h;

      meta_window_get_xor_rect (window, &window->display->grab_wireframe_rect,
                                &new_xor);
      
      meta_effects_update_wireframe (window->screen,
                                     &window->display->grab_wireframe_last_xor_rect,
                                     &new_xor);
      window->display->grab_wireframe_last_xor_rect = new_xor;

      /* do this after drawing the wires, so we don't draw over it */
      meta_window_refresh_resize_popup (window);
    }
  else
    {
      meta_window_resize_with_gravity (window, TRUE, new_w, new_h, gravity);
    }

  /* Store the latest resize time, if we actually resized. */
  if (window->rect.width != old.width &&
      window->rect.height != old.height)
    {
      g_get_current_time (&window->display->grab_last_moveresize_time);
    }
}

typedef struct
{
  const XEvent *current_event;
  int count;
  Time last_time;
} EventScannerData;

static Bool
find_last_time_predicate (Display  *display,
                          XEvent   *xevent,
                          XPointer  arg)
{
  EventScannerData *esd = (void*) arg;

  if (esd->current_event->type == xevent->type &&
      esd->current_event->xany.window == xevent->xany.window)
    {
      esd->count += 1;
      esd->last_time = xevent->xmotion.time;
    }

  return False;
}

static gboolean
check_use_this_motion_notify (MetaWindow *window,
                              XEvent     *event)
{
  EventScannerData esd;
  XEvent useless;

  /* This code is copied from Owen's GDK code. */
  
  if (window->display->grab_motion_notify_time != 0)
    {
      /* == is really the right test, but I'm all for paranoia */
      if (window->display->grab_motion_notify_time <=
          event->xmotion.time)
        {
          meta_topic (META_DEBUG_RESIZING,
                      "Arrived at event with time %lu (waiting for %lu), using it\n",
                      (unsigned long) event->xmotion.time,
                      (unsigned long) window->display->grab_motion_notify_time);
          window->display->grab_motion_notify_time = 0;
          return TRUE;
        }
      else
        return FALSE; /* haven't reached the saved timestamp yet */
    }
  
  esd.current_event = event;
  esd.count = 0;
  esd.last_time = 0;

  /* "useless" isn't filled in because the predicate never returns True */
  XCheckIfEvent (window->display->xdisplay,
                 &useless,
                 find_last_time_predicate,
                 (XPointer) &esd);

  if (esd.count > 0)
    meta_topic (META_DEBUG_RESIZING,
                "Will skip %d motion events and use the event with time %lu\n",
                esd.count, (unsigned long) esd.last_time);
  
  if (esd.last_time == 0)
    return TRUE;
  else
    {
      /* Save this timestamp, and ignore all motion notify
       * until we get to the one with this stamp.
       */
      window->display->grab_motion_notify_time = esd.last_time;
      return FALSE;
    }
}

void
meta_window_handle_mouse_grab_op_event (MetaWindow *window,
                                        XEvent     *event)
{
#ifdef HAVE_XSYNC
  if (event->type == (window->display->xsync_event_base + XSyncAlarmNotify))
    {
      meta_topic (META_DEBUG_RESIZING,
                  "Alarm event received last motion x = %d y = %d\n",
                  window->display->grab_latest_motion_x,
                  window->display->grab_latest_motion_y);

      /* If sync was previously disabled, turn it back on and hope
       * the application has come to its senses (maybe it was just
       * busy with a pagefault or a long computation).
       */
      window->disable_sync = FALSE;
      window->sync_request_time.tv_sec = 0;
      window->sync_request_time.tv_usec = 0;
      
      /* This means we are ready for another configure. */
      switch (window->display->grab_op)
        {
        case META_GRAB_OP_RESIZING_E:
        case META_GRAB_OP_RESIZING_W:
        case META_GRAB_OP_RESIZING_S:
        case META_GRAB_OP_RESIZING_N:
        case META_GRAB_OP_RESIZING_SE:
        case META_GRAB_OP_RESIZING_SW:
        case META_GRAB_OP_RESIZING_NE:
        case META_GRAB_OP_RESIZING_NW:
        case META_GRAB_OP_KEYBOARD_RESIZING_S:
        case META_GRAB_OP_KEYBOARD_RESIZING_N:
        case META_GRAB_OP_KEYBOARD_RESIZING_W:
        case META_GRAB_OP_KEYBOARD_RESIZING_E:
        case META_GRAB_OP_KEYBOARD_RESIZING_SE:
        case META_GRAB_OP_KEYBOARD_RESIZING_NE:
        case META_GRAB_OP_KEYBOARD_RESIZING_SW:
        case META_GRAB_OP_KEYBOARD_RESIZING_NW:
          /* no pointer round trip here, to keep in sync */
          update_resize (window,
                         window->display->grab_latest_motion_x,
                         window->display->grab_latest_motion_y,
			 TRUE);
          break;
          
        default:
          break;
        }
    }
#endif /* HAVE_XSYNC */
  
  switch (event->type)
    {
    case ButtonRelease:      
      if (meta_grab_op_is_moving (window->display->grab_op))
        {
          if (event->xbutton.root == window->screen->xroot)
            update_move (window, event->xbutton.state,
                         event->xbutton.x_root, event->xbutton.y_root);
        }
      else if (meta_grab_op_is_resizing (window->display->grab_op))
        {
          if (event->xbutton.root == window->screen->xroot)
            update_resize (window,
			   event->xbutton.x_root,
			   event->xbutton.y_root,
			   TRUE);
        }

      meta_display_end_grab_op (window->display, event->xbutton.time);
      break;    

    case MotionNotify:
      if (meta_grab_op_is_moving (window->display->grab_op))
        {
          if (event->xmotion.root == window->screen->xroot)
            {
              if (check_use_this_motion_notify (window,
                                                event))
                update_move (window,
                             event->xmotion.state,
                             event->xmotion.x_root,
                             event->xmotion.y_root);
            }
        }
      else if (meta_grab_op_is_resizing (window->display->grab_op))
        {
          if (event->xmotion.root == window->screen->xroot)
            {
              if (check_use_this_motion_notify (window,
                                                event))
                update_resize (window,
                               event->xmotion.x_root,
                               event->xmotion.y_root,
			       FALSE);
            }
        }
      break;

    case EnterNotify:
    case LeaveNotify:
      if (meta_grab_op_is_moving (window->display->grab_op))
        {
          if (event->xcrossing.root == window->screen->xroot)
            update_move (window,
                         event->xcrossing.state,
                         event->xcrossing.x_root,
                         event->xcrossing.y_root);
        }
      else if (meta_grab_op_is_resizing (window->display->grab_op))
        {
          if (event->xcrossing.root == window->screen->xroot)
            update_resize (window,
                           event->xcrossing.x_root,
                           event->xcrossing.y_root,
			   FALSE);
        }
      break;
    default:
      break;
    }
}

gboolean
meta_window_shares_some_workspace (MetaWindow *window,
                                   MetaWindow *with)
{
  GList *tmp;
  
  if (window->on_all_workspaces ||
      with->on_all_workspaces)
    return TRUE;
  
  tmp = window->workspaces;
  while (tmp != NULL)
    {
      if (g_list_find (with->workspaces, tmp->data) != NULL)
        return TRUE;

      tmp = tmp->next;
    }

  return FALSE;
}

void
meta_window_set_gravity (MetaWindow *window,
                         int         gravity)
{
  XSetWindowAttributes attrs;

  meta_verbose ("Setting gravity of %s to %d\n", window->desc, gravity);

  attrs.win_gravity = gravity;
  
  meta_error_trap_push (window->display);

  XChangeWindowAttributes (window->display->xdisplay,
                           window->xwindow,
                           CWWinGravity,
                           &attrs);
  
  meta_error_trap_pop (window->display, FALSE);
}

static void
get_work_area_xinerama (MetaWindow    *window,
                        MetaRectangle *area,
                        int            which_xinerama)
{
  MetaRectangle space_area;
  GList *tmp;  
  int left_strut;
  int right_strut;
  int top_strut;
  int bottom_strut;  
  int xinerama_origin_x;
  int xinerama_origin_y;
  int xinerama_width;
  int xinerama_height;
  
  g_assert (which_xinerama >= 0);

  xinerama_origin_x = window->screen->xinerama_infos[which_xinerama].x_origin;
  xinerama_origin_y = window->screen->xinerama_infos[which_xinerama].y_origin;
  xinerama_width = window->screen->xinerama_infos[which_xinerama].width;
  xinerama_height = window->screen->xinerama_infos[which_xinerama].height;
  
  left_strut = 0;
  right_strut = 0;
  top_strut = 0;
  bottom_strut = 0;
  
  tmp = meta_window_get_workspaces (window);  
  while (tmp != NULL)
    {
      meta_workspace_get_work_area_for_xinerama (tmp->data,
                                                 which_xinerama,
                                                 &space_area);

      left_strut = MAX (left_strut, space_area.x - xinerama_origin_x);
      right_strut = MAX (right_strut,
			 (xinerama_width - 
			  (space_area.x - xinerama_origin_x) - 
			  space_area.width));
      top_strut = MAX (top_strut, space_area.y - xinerama_origin_y);
      bottom_strut = MAX (bottom_strut,
			  (xinerama_height - 
			   (space_area.y - xinerama_origin_y) - 
			   space_area.height));
      tmp = tmp->next;
    }
  
  area->x = xinerama_origin_x + left_strut;
  area->y = xinerama_origin_y + top_strut;
  area->width = xinerama_width - left_strut - right_strut;
  area->height = xinerama_height - top_strut - bottom_strut;

  meta_topic (META_DEBUG_WORKAREA,
              "Window %s xinerama %d has work area %d,%d %d x %d\n",
              window->desc, which_xinerama,
              area->x, area->y, area->width, area->height);
}

void
meta_window_get_work_area_current_xinerama (MetaWindow    *window,
					    MetaRectangle *area)
{
  const MetaXineramaScreenInfo *xinerama = NULL;
  xinerama = meta_screen_get_xinerama_for_window (window->screen,
						  window);

  meta_window_get_work_area_for_xinerama (window,
                                          xinerama->number,
                                          area);
}

void
meta_window_get_work_area_for_xinerama (MetaWindow    *window,
					int            which_xinerama,
					MetaRectangle *area)
{
  g_return_if_fail (which_xinerama >= 0);
  
  get_work_area_xinerama (window,
                          area,
                          which_xinerama);
}

void
meta_window_get_work_area_all_xineramas (MetaWindow    *window,
                                         MetaRectangle *area)
{
  MetaRectangle space_area;
  GList *tmp;  
  int left_strut;
  int right_strut;
  int top_strut;
  int bottom_strut;  
  int screen_origin_x;
  int screen_origin_y;
  int screen_width;
  int screen_height;

  screen_origin_x = 0;
  screen_origin_y = 0;
  screen_width = window->screen->width;
  screen_height = window->screen->height;
  
  left_strut = 0;
  right_strut = 0;
  top_strut = 0;
  bottom_strut = 0;
  
  tmp = meta_window_get_workspaces (window);  
  while (tmp != NULL)
    {
      meta_workspace_get_work_area_all_xineramas (tmp->data,
                                                  &space_area);

      left_strut = MAX (left_strut, space_area.x - screen_origin_x);
      right_strut = MAX (right_strut,
			 (screen_width - 
			  (space_area.x - screen_origin_x) - 
			  space_area.width));
      top_strut = MAX (top_strut, space_area.y - screen_origin_y);
      bottom_strut = MAX (bottom_strut,
			  (screen_height - 
			   (space_area.y - screen_origin_y) - 
			   space_area.height));      
      tmp = tmp->next;
    }
  
  area->x = screen_origin_x + left_strut;
  area->y = screen_origin_y + top_strut;
  area->width = screen_width - left_strut - right_strut;
  area->height = screen_height - top_strut - bottom_strut;

  meta_topic (META_DEBUG_WORKAREA,
              "Window %s has whole-screen work area %d,%d %d x %d\n",
              window->desc, area->x, area->y, area->width, area->height);
}


gboolean
meta_window_same_application (MetaWindow *window,
                              MetaWindow *other_window)
{
  return
    meta_window_get_group (window) ==
    meta_window_get_group (other_window);
}

void
meta_window_refresh_resize_popup (MetaWindow *window)
{
  if (window->display->grab_op == META_GRAB_OP_NONE)
    return;

  if (window->display->grab_window != window)
    return;

  /* FIXME for now we bail out when doing wireframe, because our
   * server grab keeps us from being able to redraw the stuff
   * underneath the resize popup.
   */
  if (window->display->grab_wireframe_active)
    return;
  
  switch (window->display->grab_op)
    {
    case META_GRAB_OP_RESIZING_SE:
    case META_GRAB_OP_RESIZING_S:
    case META_GRAB_OP_RESIZING_SW:
    case META_GRAB_OP_RESIZING_N:
    case META_GRAB_OP_RESIZING_NE:
    case META_GRAB_OP_RESIZING_NW:
    case META_GRAB_OP_RESIZING_W:
    case META_GRAB_OP_RESIZING_E:
    case META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN:
    case META_GRAB_OP_KEYBOARD_RESIZING_S:
    case META_GRAB_OP_KEYBOARD_RESIZING_N:
    case META_GRAB_OP_KEYBOARD_RESIZING_W:
    case META_GRAB_OP_KEYBOARD_RESIZING_E:
    case META_GRAB_OP_KEYBOARD_RESIZING_SE:
    case META_GRAB_OP_KEYBOARD_RESIZING_NE:
    case META_GRAB_OP_KEYBOARD_RESIZING_SW:
    case META_GRAB_OP_KEYBOARD_RESIZING_NW:
      break;

    default:
      /* Not resizing */
      return;
    }
      
  if (window->display->grab_resize_popup == NULL)
    {
      if (window->size_hints.width_inc > 1 ||
          window->size_hints.height_inc > 1)
        window->display->grab_resize_popup =
          meta_ui_resize_popup_new (window->display->xdisplay,
                                    window->screen->number);
    }
  
  if (window->display->grab_resize_popup != NULL)
    {
      int gravity;
      int x, y, width, height;
      MetaFrameGeometry fgeom;

      if (window->frame)
        meta_frame_calc_geometry (window->frame, &fgeom);
      else
        {
          fgeom.left_width = 0;
          fgeom.right_width = 0;
          fgeom.top_height = 0;
          fgeom.bottom_height = 0;
        }
      
      gravity = meta_resize_gravity_from_grab_op (window->display->grab_op);
      g_assert (gravity >= 0);

      if (window->display->grab_wireframe_active)
        {
          x = window->display->grab_wireframe_rect.x;
          y = window->display->grab_wireframe_rect.y;
          width = window->display->grab_wireframe_rect.width;
          height = window->display->grab_wireframe_rect.height;
        }
      else
        {
          meta_window_get_position (window, &x, &y);
          width = window->rect.width;
          height = window->rect.height;
        }
      
      meta_ui_resize_popup_set (window->display->grab_resize_popup,
                                gravity,
                                x, y,
                                width, height,
                                window->size_hints.base_width,
                                window->size_hints.base_height,
                                window->size_hints.min_width,
                                window->size_hints.min_height,
                                window->size_hints.width_inc,
                                window->size_hints.height_inc,
                                fgeom.left_width,
                                fgeom.right_width,
                                fgeom.top_height,
                                fgeom.bottom_height);

      meta_ui_resize_popup_set_showing (window->display->grab_resize_popup,
                                        TRUE);
    }
}

void
meta_window_foreach_transient (MetaWindow            *window,
                               MetaWindowForeachFunc  func,
                               void                  *data)
{
  GSList *windows;
  GSList *tmp;

  windows = meta_display_list_windows (window->display);

  tmp = windows;
  while (tmp != NULL)
    {
      MetaWindow *transient = tmp->data;
      
      if (meta_window_is_ancestor_of_transient (window, transient))
        {
          if (!(* func) (transient, data))
            break;
        }
      
      tmp = tmp->next;
    }

  g_slist_free (windows);
}

void
meta_window_foreach_ancestor (MetaWindow            *window,
                              MetaWindowForeachFunc  func,
                              void                  *data)
{
  MetaWindow *w;
  MetaWindow *tortoise;
  
  w = window;
  tortoise = window;
  while (TRUE)
    {
      if (w->xtransient_for == None ||
          w->transient_parent_is_root_window)
        break;
      
      w = meta_display_lookup_x_window (w->display, w->xtransient_for);
      
      if (w == NULL || w == tortoise)
        break;

      if (!(* func) (w, data))
        break;      
      
      if (w->xtransient_for == None ||
          w->transient_parent_is_root_window)
        break;

      w = meta_display_lookup_x_window (w->display, w->xtransient_for);
      
      if (w == NULL || w == tortoise)
        break;

      if (!(* func) (w, data))
        break;
      
      tortoise = meta_display_lookup_x_window (tortoise->display,
                                               tortoise->xtransient_for);

      /* "w" should have already covered all ground covered by the
       * tortoise, so the following must hold.
       */
      g_assert (tortoise != NULL);
      g_assert (tortoise->xtransient_for != None);
      g_assert (!tortoise->transient_parent_is_root_window);
    }
}

typedef struct
{
  MetaWindow *ancestor;
  gboolean found;
} FindAncestorData;

static gboolean
find_ancestor_func (MetaWindow *window,
                    void       *data)
{
  FindAncestorData *d = data;

  if (window == d->ancestor)
    {
      d->found = TRUE;
      return FALSE;
    }

  return TRUE;
}

gboolean
meta_window_is_ancestor_of_transient (MetaWindow *window,
                                      MetaWindow *transient)
{
  FindAncestorData d;

  d.ancestor = window;
  d.found = FALSE;

  meta_window_foreach_ancestor (transient, find_ancestor_func, &d);

  return d.found;
}

/* Warp pointer to location appropriate for grab,
 * return root coordinates where pointer ended up.
 */
static gboolean
warp_grab_pointer (MetaWindow          *window,
                   MetaGrabOp           grab_op,
                   int                 *x,
                   int                 *y)
{
  MetaRectangle rect;

  /* We may not have done begin_grab_op yet, i.e. may not be in a grab
   */
  
  if (window == window->display->grab_window &&
      window->display->grab_wireframe_active)
    rect = window->display->grab_wireframe_rect;
  else
    {
      rect = window->rect;
      meta_window_get_position (window, &rect.x, &rect.y);
    }
  
  switch (grab_op)
    {
      case META_GRAB_OP_KEYBOARD_MOVING:
      case META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN:
        *x = rect.width / 2;
        *y = rect.height / 2;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_S:
        *x = rect.width / 2;
        *y = rect.height;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_N:
        *x = rect.width / 2;
        *y = 0;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_W:
        *x = 0;
        *y = rect.height / 2;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_E:
        *x = rect.width;
        *y = rect.height / 2;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_SE:
        *x = rect.width;
        *y = rect.height;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_NE:
        *x = rect.width;
        *y = 0;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_SW:
        *x = 0;
        *y = rect.height;
        break;

      case META_GRAB_OP_KEYBOARD_RESIZING_NW:
        *x = 0;
        *y = 0;
        break;

      default:
        return FALSE;
    }

  *x += rect.x;
  *y += rect.y;
  
  meta_error_trap_push_with_return (window->display);

  meta_topic (META_DEBUG_WINDOW_OPS,
              "Warping pointer to %d,%d with window at %d,%d\n",
              *x, *y, rect.x, rect.y);
  
  XWarpPointer (window->display->xdisplay,
                None,
                window->screen->xroot,
                0, 0, 0, 0, 
                *x, *y);

  if (meta_error_trap_pop_with_return (window->display, FALSE) != Success)
    {
      meta_verbose ("Failed to warp pointer for window %s\n",
                    window->desc);
      return FALSE;
    }
  
  return TRUE;
}

void
meta_window_begin_grab_op (MetaWindow *window,
                           MetaGrabOp  op,
                           Time        timestamp)
{
  int x, y;
  gulong grab_start_serial;

  grab_start_serial = XNextRequest (window->display->xdisplay);
  
  meta_window_raise (window);

  warp_grab_pointer (window,
                     op, &x, &y);

  meta_display_begin_grab_op (window->display,
                              window->screen,
                              window,
                              op,
                              FALSE,
                              grab_start_serial /* event_serial */,
                              0 /* button */,
                              0,
                              timestamp,
                              x, y);

  /* We override the one set in display_begin_grab_op since we
   * did additional stuff as part of the grabbing process
   */
  window->display->grab_start_serial = grab_start_serial;
}

void
meta_window_update_keyboard_resize (MetaWindow *window,
                                    gboolean    update_cursor)
{
  int x, y;
  
  warp_grab_pointer (window,
                     window->display->grab_op,
                     &x, &y);

  {
    /* As we warped the pointer, we have to reset the anchor state,
     * since if the mouse moves we want to use those events to do the
     * right thing. Also, this means that the motion notify
     * from the pointer warp comes back as a no-op.
     */
    int dx, dy;

    dx = x - window->display->grab_anchor_root_x;
    dy = y - window->display->grab_anchor_root_y;
    
    window->display->grab_anchor_root_x += dx;
    window->display->grab_anchor_root_y += dy;
    if (window->display->grab_wireframe_active)
      {
        window->display->grab_anchor_window_pos =
          window->display->grab_wireframe_rect;
      }
    else
      {
        window->display->grab_anchor_window_pos = window->rect;
        meta_window_get_position (window,
                                  &window->display->grab_anchor_window_pos.x,
                                  &window->display->grab_anchor_window_pos.y);
      }
  }
  
  if (update_cursor)
    {
      meta_display_set_grab_op_cursor (window->display,
                                       NULL,
                                       window->display->grab_op,
                                       TRUE,
                                       window->display->grab_xwindow,
                                       meta_display_get_current_time (window->display));
    }
}

void
meta_window_update_keyboard_move (MetaWindow *window)
{
  int x, y;
  
  warp_grab_pointer (window,
                     window->display->grab_op,
                     &x, &y);
}

void
meta_window_update_layer (MetaWindow *window)
{
  MetaGroup *group;
  
  meta_stack_freeze (window->screen->stack);
  group = meta_window_get_group (window);
  if (group)
    meta_group_update_layers (group);
  else
    meta_stack_update_layer (window->screen->stack, window);
  meta_stack_thaw (window->screen->stack);
}

/* ensure_mru_position_after ensures that window appears after
 * below_this_one in the active_workspace's mru_list (i.e. it treats
 * window as having been less recently used than below_this_one)
 */
static void
ensure_mru_position_after (MetaWindow *window,
                           MetaWindow *after_this_one)
{
  /* This is sort of slow since it runs through the entire list more
   * than once (especially considering the fact that we expect the
   * windows of interest to be the first two elements in the list),
   * but it doesn't matter while we're only using it on new window
   * map.
   */

  GList* active_mru_list;
  GList* window_position;
  GList* after_this_one_position;

  active_mru_list         = window->screen->active_workspace->mru_list;
  window_position         = g_list_find (active_mru_list, window);
  after_this_one_position = g_list_find (active_mru_list, after_this_one);

  /* after_this_one_position is NULL when we switch workspaces, but in
   * that case we don't need to do any MRU shuffling so we can simply
   * return.
   */
  if (after_this_one_position == NULL)
    return;

  if (g_list_length (window_position) > g_list_length (after_this_one_position))
    {
      window->screen->active_workspace->mru_list =
        g_list_delete_link (window->screen->active_workspace->mru_list,
                            window_position);

      window->screen->active_workspace->mru_list =
        g_list_insert_before (window->screen->active_workspace->mru_list,
                              after_this_one_position->next,
                              window);
    }
}

void
meta_window_stack_just_below (MetaWindow *window,
                              MetaWindow *below_this_one)
{
  g_return_if_fail (window         != NULL);
  g_return_if_fail (below_this_one != NULL);

  if (window->stack_position > below_this_one->stack_position)
    {
      meta_topic (META_DEBUG_STACK,
                  "Setting stack position of window %s to %d (making it below window %s).\n",
                  window->desc, 
                  below_this_one->stack_position, 
                  below_this_one->desc);
      meta_window_set_stack_position (window, below_this_one->stack_position);
    }
  else
    {
      meta_topic (META_DEBUG_STACK,
                  "Window %s  was already below window %s.\n",
                  window->desc, below_this_one->desc);
    }
}
