/* 
 * Copyright (C) 2003, 2004 Red Hat, Inc.
 * Copyright (C) 2003 Keith Packard
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
#include "compositor.h"
#include "screen.h"
#include "errors.h"
#include "window.h"
#include "frame.h"
#include "matrix.h"
#include <math.h>
#include "snow.h"
#include "workspace.h"

#include <math.h>

#include <cm/node.h>
#include <cm/drawable-node.h>

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>
#include <X11/extensions/shape.h>

#include <cm/ws.h>
#include <cm/wsint.h>


#ifdef HAVE_COMPOSITE_EXTENSIONS
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrender.h>
#include "lmcbits.h"
#include "lmctexture.h"

#endif /* HAVE_COMPOSITE_EXTENSIONS */

#define FRAME_INTERVAL_MILLISECONDS ((int)(1000.0/40.0))

struct MetaCompositor
{
    MetaDisplay *display;
    
    Ws *ws;
    
    World *world;
    
    int composite_error_base;
    int composite_event_base;
    int damage_error_base;
    int damage_event_base;
    int fixes_error_base;
    int fixes_event_base;
    
    GHashTable *window_hash;
    
    guint repair_idle;
    
    guint enabled : 1;
    guint have_composite : 1;
    guint have_damage : 1;
    guint have_fixes : 1;
    guint have_name_window_pixmap : 1;
    guint debug_updates : 1;
    
    GList *ignored_damage;
    
    WsWindow *glw;
};

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
free_window_hash_value (void *v)
{
    DrawableNode *drawable_node = v;
    
    drawable_node_unref (drawable_node);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

MetaCompositor*
meta_compositor_new (MetaDisplay *display)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
    MetaCompositor *compositor;
    
    compositor = g_new0 (MetaCompositor, 1);
    
    compositor->ws = ws_new (NULL);
    
    ws_init_test (compositor->ws);
    ws_set_ignore_grabs (compositor->ws, TRUE);
    
    compositor->display = display;
    
    if (!XCompositeQueryExtension (display->xdisplay,
				   &compositor->composite_event_base,
				   &compositor->composite_error_base))
    {
	compositor->composite_event_base = 0;
	compositor->composite_error_base = 0;
    }
    else
    {
	int composite_major, composite_minor;
	
	compositor->have_composite = TRUE;
	
	XCompositeQueryVersion (display->xdisplay,
				&composite_major, &composite_minor);
	
    }
    
    meta_topic (META_DEBUG_COMPOSITOR, "Composite extension event base %d error base %d\n",
		compositor->composite_event_base,
		compositor->composite_error_base);
    
    if (!XDamageQueryExtension (display->xdisplay,
				&compositor->damage_event_base,
				&compositor->damage_error_base))
    {
	compositor->damage_event_base = 0;
	compositor->damage_error_base = 0;
    }
    else
	compositor->have_damage = TRUE;
    
    meta_topic (META_DEBUG_COMPOSITOR, "Damage extension event base %d error base %d\n",
		compositor->damage_event_base,
		compositor->damage_error_base);
    
    if (!XFixesQueryExtension (display->xdisplay,
			       &compositor->fixes_event_base,
			       &compositor->fixes_error_base))
    {
	compositor->fixes_event_base = 0;
	compositor->fixes_error_base = 0;
    }
    else
	compositor->have_fixes = TRUE;
    
    meta_topic (META_DEBUG_COMPOSITOR, "Fixes extension event base %d error base %d\n",
		compositor->fixes_event_base,
		compositor->fixes_error_base);
    
    if (!(compositor->have_composite &&
	  compositor->have_fixes &&
	  compositor->have_damage &&
	  META_DISPLAY_HAS_RENDER (compositor->display)))
    {
	meta_topic (META_DEBUG_COMPOSITOR, "Failed to find all extensions needed for compositing manager, disabling compositing manager\n");
	
	if (!compositor->have_composite)
	    g_print ("no composite\n");
	if (!compositor->have_fixes)
	    g_print ("no fixes\n");
	if (!compositor->have_damage)
	    g_print ("have damage\n");
	
	
	g_assert (!compositor->enabled);
	return compositor;
    }
    
    compositor->window_hash = g_hash_table_new_full (meta_unsigned_long_hash,
						     meta_unsigned_long_equal,
						     NULL,
						     free_window_hash_value);
    
    compositor->enabled = TRUE;
    
    return compositor;
#else /* HAVE_COMPOSITE_EXTENSIONS */
    return (void*) 0xdeadbeef; /* non-NULL value */
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

void
meta_compositor_set_debug_updates (MetaCompositor *compositor,
				   gboolean	   debug_updates)
{
    compositor->debug_updates = !!debug_updates;
}

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
remove_repair_idle (MetaCompositor *compositor)
{
    if (compositor->repair_idle)
    {
	meta_topic (META_DEBUG_COMPOSITOR, "Damage idle removed\n");
	
	g_source_remove (compositor->repair_idle);
	compositor->repair_idle = 0;
    }
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

void
meta_compositor_unref (MetaCompositor *compositor)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
    /* There isn't really a refcount at the moment since
     * there's no ref()
     */
    remove_repair_idle (compositor);
    
    if (compositor->window_hash)
	g_hash_table_destroy (compositor->window_hash);
    
    g_free (compositor);
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

double tmp;

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
draw_windows (MetaScreen *screen,
	      GList      *list)
{
    Node *node;
    
    if (!list)
	return;
    
    node = list->data;
    
    draw_windows (screen, list->next);
    node->render (node);
}
#endif

#include <X11/Xlib.h>

static MetaScreen *
node_get_screen (Display *dpy,
		 DrawableNode *node)
{
    /* FIXME: we should probably have a reverse mapping
     * from nodes to screens
     */
    
    Screen *screen = XDefaultScreenOfDisplay (dpy);
    return meta_screen_for_x_screen (screen);
}

static void
handle_restacking (MetaCompositor *compositor,
		   DrawableNode *node,
		   DrawableNode *above)
{
    GList *window_link, *above_link;
    MetaScreen *screen;
    
    screen = node_get_screen (compositor->display->xdisplay, node);
    
    window_link = g_list_find (screen->compositor_windows, node);
    above_link  = g_list_find (screen->compositor_windows, above);
    
    if (!window_link || !above_link)
	return;
    
    if (window_link == above_link)
    {
	/* This can happen if the topmost window is raise above
	 * the GL window
	 */
	return;
    }
    
    if (window_link->next != above_link)
    {
	screen->compositor_windows = g_list_delete_link (
	    screen->compositor_windows, window_link);
	screen->compositor_windows = g_list_insert_before (
	    screen->compositor_windows, above_link, node);
    }
}

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_configure_notify (MetaCompositor  *compositor,
                          XConfigureEvent *event)
{
    WsWindow *above_window;
    DrawableNode *node = g_hash_table_lookup (compositor->window_hash,
					      &event->window);
    DrawableNode *above_node;
    MetaScreen *screen;
    
    if (!node)
	return;
    
    screen = node_get_screen (compositor->display->xdisplay, node);
    
    above_window = ws_window_lookup (node->drawable->ws, event->above);
    
    if (above_window == compositor->glw)
    {
	above_node = screen->compositor_windows->data;
    }
    else
    {
	above_node = g_hash_table_lookup (compositor->window_hash,
					  &event->above);
    }
    
    handle_restacking (compositor, node, above_node);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */


#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_expose (MetaCompositor     *compositor,
                XExposeEvent       *event)
{
    XserverRegion region;
    MetaScreen *screen;
    XRectangle r;
    
    screen = meta_display_screen_for_root (compositor->display,
					   event->window);
    
    if (screen == NULL)
	return;
    
    r.x = 0;
    r.y = 0;
    r.width = screen->width;
    r.height = screen->height;
    region = XFixesCreateRegion (compositor->display->xdisplay, &r, 1);
    
    XFixesDestroyRegion (compositor->display->xdisplay, region);
}

#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_map (MetaCompositor     *compositor,
             XMapEvent          *event)
{
    DrawableNode *node;
    MetaScreen *screen;
    
    /* See if window was mapped as child of root */
    screen = meta_display_screen_for_root (compositor->display,
					   event->event);
    
    if (screen == NULL)
    {
	meta_topic (META_DEBUG_COMPOSITOR,
		    "MapNotify received on non-root 0x%lx for 0x%lx\n",
		    event->event, event->window);
	return; /* MapNotify wasn't for a child of the root */
    }
    
    node = g_hash_table_lookup (compositor->window_hash,
				&event->window);
    if (node == NULL)
    {
	XWindowAttributes attrs;
	
	meta_error_trap_push_with_return (compositor->display);
	
	XGetWindowAttributes (compositor->display->xdisplay,
			      event->window, &attrs);
	
	if (meta_error_trap_pop_with_return (compositor->display, TRUE) != Success)
        {
	    meta_topic (META_DEBUG_COMPOSITOR, "Failed to get attributes for window 0x%lx\n",
			event->window);
        }
	else
	{
	    g_print ("Map window 0x%lx\n", event->window);
	    meta_compositor_add_window (compositor,
					event->window, &attrs);
        }
    }
    else
    {
	drawable_node_set_viewable (node, TRUE);
    }
    
    /* We don't actually need to invalidate anything, because we will
     * get damage events as the server fills the background and the client
     * draws the window
     */
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_unmap (MetaCompositor     *compositor,
               XUnmapEvent        *event)
{
    DrawableNode *node;
    MetaScreen *screen;
    
    /* See if window was unmapped as child of root */
    screen = meta_display_screen_for_root (compositor->display,
					   event->event);
    
    if (screen == NULL)
    {
	meta_topic (META_DEBUG_COMPOSITOR,
		    "UnmapNotify received on non-root 0x%lx for 0x%lx\n",
		    event->event, event->window);
	return; /* UnmapNotify wasn't for a child of the root */
    }
    
    node = g_hash_table_lookup (compositor->window_hash,
				&event->window);
    if (node != NULL)
    {
	drawable_node_set_viewable (node, FALSE);
    }
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_create (MetaCompositor     *compositor,
                XCreateWindowEvent *event)
{
    MetaScreen *screen;
    XWindowAttributes attrs;
    
    screen = meta_display_screen_for_root (compositor->display,
					   event->parent);
    
    if (screen == NULL)
    {
	meta_topic (META_DEBUG_COMPOSITOR,
		    "CreateNotify received on non-root 0x%lx for 0x%lx\n",
		    event->parent, event->window);
	return;
    }
    
    meta_error_trap_push_with_return (compositor->display);
    
    XGetWindowAttributes (compositor->display->xdisplay,
			  event->window, &attrs);
    
    if (meta_error_trap_pop_with_return (compositor->display, TRUE) != Success)
    {
	meta_topic (META_DEBUG_COMPOSITOR, "Failed to get attributes for window 0x%lx\n",
		    event->window);
    }
    else
    {
	meta_topic (META_DEBUG_COMPOSITOR,
		    "Create window 0x%lx, adding\n", event->window);
	
	meta_compositor_add_window (compositor,
				    event->window, &attrs);
    }
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_destroy (MetaCompositor      *compositor,
                 XDestroyWindowEvent *event)
{
    MetaScreen *screen;
    
    screen = meta_display_screen_for_root (compositor->display,
					   event->event);
    
    if (screen == NULL)
    {
	meta_topic (META_DEBUG_COMPOSITOR,
		    "DestroyNotify received on non-root 0x%lx for 0x%lx\n",
		    event->event, event->window);
	return;
    }
    
    meta_topic (META_DEBUG_COMPOSITOR,
		"Destroy window 0x%lx\n", event->window);
    meta_compositor_remove_window (compositor, event->window);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */


#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_reparent (MetaCompositor      *compositor,
                  XReparentEvent      *event)
{
    /* Reparent from one screen to another doesn't happen now, but
     * it's been suggested as a future extension
     */
    MetaScreen *event_screen;
    MetaScreen *parent_screen;
    DrawableNode *node;
    XWindowAttributes attrs;
    
    event_screen = meta_display_screen_for_root (compositor->display,
						 event->event);
    
    g_print ("reparent\n");
    
    if (event_screen == NULL)
    {
	meta_topic (META_DEBUG_COMPOSITOR,
		    "ReparentNotify received on non-root 0x%lx for 0x%lx\n",
		    event->event, event->window);
	return;
    }
    
    meta_topic (META_DEBUG_COMPOSITOR,
		"Reparent window 0x%lx new parent 0x%lx received on 0x%lx\n",
		event->window, event->parent, event->event);
    
    parent_screen = meta_display_screen_for_root (compositor->display,
						  event->parent);
    
    if (parent_screen == NULL)
    {
	meta_topic (META_DEBUG_COMPOSITOR,
		    "ReparentNotify 0x%lx to a non-screen or unmanaged screen 0x%lx\n",
		    event->window, event->parent);
	meta_compositor_remove_window (compositor, event->window);
	return;
    }
    
    node = g_hash_table_lookup (compositor->window_hash,
				&event->window);
    
    meta_error_trap_push_with_return (compositor->display);
    
    XGetWindowAttributes (compositor->display->xdisplay,
			  event->window, &attrs);
    
    if (meta_error_trap_pop_with_return (compositor->display, TRUE) != Success)
    {
	g_print ("attrs\n");
	meta_topic (META_DEBUG_COMPOSITOR, "Failed to get attributes for window 0x%lx\n",
		    event->window);
    }
    else
    {
	meta_topic (META_DEBUG_COMPOSITOR,
		    "Reparent window 0x%lx into screen 0x%lx, adding\n",
		    event->window, event->parent);
	g_print ("adding\n");
	meta_compositor_add_window (compositor,
				    event->window, &attrs);
    }
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

void
meta_compositor_process_event (MetaCompositor *compositor,
                               XEvent         *event,
                               MetaWindow     *window)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
    if (!compositor->enabled)
	return; /* no extension */
    
    /* FIXME support CirculateNotify */
    
    if (event->type == (compositor->damage_event_base + XDamageNotify))
    {
#if 0
	process_damage_notify (compositor,
			       (XDamageNotifyEvent*) event);
#endif
    }
    else if (event->type == ConfigureNotify)
    {
	process_configure_notify (compositor,
				  (XConfigureEvent*) event);
    }
    else if (event->type == Expose)
    {
	process_expose (compositor,
			(XExposeEvent*) event);
    }
    else if (event->type == UnmapNotify)
    {
	process_unmap (compositor,
		       (XUnmapEvent*) event);
    }
    else if (event->type == MapNotify)
    {
	process_map (compositor,
		     (XMapEvent*) event);
    }
    else if (event->type == ReparentNotify)
    {
	process_reparent (compositor,
			  (XReparentEvent*) event);
    }
    else if (event->type == CreateNotify)
    {
	process_create (compositor,
			(XCreateWindowEvent*) event);
    }
    else if (event->type == DestroyNotify)
    {
	process_destroy (compositor,
			 (XDestroyWindowEvent*) event);
    }
    
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

static void
wavy (double time,
      double in_x, double in_y,
      double *out_x, double *out_y,
      gpointer data)
{
    static int m;
    time = time * 5;
    double dx = 0.0025 * sin (time + 35 * in_y);
    double dy = 0.0025 * cos (time + 35 * in_x);

    *out_x = in_x + dx;
    *out_y = in_y + dy;

    m++;
}

/* This is called when metacity does its XQueryTree() on startup
 * and when a new window is mapped.
 */
void
meta_compositor_add_window (MetaCompositor    *compositor,
                            Window             xwindow,
                            XWindowAttributes *attrs)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
    DrawableNode *node;
    MetaScreen *screen;
    WsDrawable *drawable;
    
    if (!compositor->enabled)
	return; /* no extension */
    
    screen = meta_screen_for_x_screen (attrs->screen);
    g_assert (screen != NULL);
    
    node = g_hash_table_lookup (compositor->window_hash,
				&xwindow);
    
    if (node != NULL)
    {
	meta_topic (META_DEBUG_COMPOSITOR,
		    "Window 0x%lx already added\n", xwindow);
	return;
    }
    
    drawable = (WsDrawable *)ws_window_lookup (compositor->ws, xwindow);
    
    if (ws_window_query_input_only ((WsWindow *)drawable) ||
	drawable == (WsDrawable *)compositor->glw)
    {
	return;
    }
    else
    {
	node = drawable_node_new (drawable);

#if 0
	drawable_node_set_deformation_func (node, wavy, NULL);
#endif
    }
    
    /* FIXME: we should probably just store xid's directly */
    g_hash_table_insert (compositor->window_hash,
			 &(node->drawable->xid), node);
    
    /* assume cwindow is at the top of the stack as it was either just
     * created or just reparented to the root window
     */
    screen->compositor_windows = g_list_prepend (screen->compositor_windows,
						 node);
    
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

void
meta_compositor_remove_window (MetaCompositor    *compositor,
                               Window             xwindow)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
    DrawableNode *node;
    MetaScreen *screen;
    
    if (!compositor->enabled)
	return; /* no extension */
    
    node = g_hash_table_lookup (compositor->window_hash,
				&xwindow);
    
    if (node == NULL)
    {
	meta_topic (META_DEBUG_COMPOSITOR,
		    "Window 0x%lx already removed\n", xwindow);
	return;
    }
    
#if 0
    meta_topic (META_DEBUG_COMPOSITOR,
		"Removing window 0x%lx (%s) from compositor\n",
		xwindow,
		cwindow_get_viewable (cwindow) ? "mapped" : "unmapped");
#endif
    
    screen = node_get_screen (compositor->display->xdisplay, node);
    
    screen->compositor_windows = g_list_remove (screen->compositor_windows,
						node);
    
    /* Frees node as side effect */
    g_hash_table_remove (compositor->window_hash,
			 &xwindow);
    
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

typedef struct Info
{
    MetaScreen  *screen;
    WsWindow	*window;
} Info;

Info *the_info;

static gboolean
update (gpointer data)
{
    Info *info = data;
    
    MetaScreen *screen = info->screen;
    WsWindow *gl_window = info->window;
    
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();
    gluOrtho2D (0, 1.0, 0.0, 1.0);
    
    ws_window_raise (gl_window);
    
    glClearColor (0.0, 0.5, 0.5, 0.0);
    glClear (GL_COLOR_BUFFER_BIT);
    
    glColor4f (1.0, 0.0, 0.0, 1.0);
    
    glDisable (GL_TEXTURE_2D);
    
    glBegin (GL_QUADS);
    
    glVertex2f (0.2, 0.2);
    glVertex2f (0.2, 0.4);
    glVertex2f (0.4, 0.4);
    glVertex2f (0.4, 0.2);
    
    glEnd ();
    
    
    glEnable (GL_TEXTURE_2D);
    draw_windows (screen, screen->compositor_windows);
    
    /* FIXME: we should probably grab the server around the raise/swap */
    
    ws_window_gl_swap_buffers (gl_window);
    
    return TRUE;
}

void
debug_update (void)
{
    update (the_info);
}

void
meta_compositor_manage_screen (MetaCompositor *compositor,
                               MetaScreen     *screen)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
    WsScreen *ws_screen = ws_screen_get_from_number (
	compositor->ws, screen->number);
    WsWindow *root = ws_screen_get_root_window (ws_screen);
    Info *info;
    WsRegion *region;

    compositor->glw = ws_window_new_gl (root);
    
    ws_init_composite (compositor->ws);
    ws_init_damage (compositor->ws);
    ws_init_fixes (compositor->ws);
    
    ws_window_redirect_subwindows (root);
    ws_window_set_override_redirect (compositor->glw, TRUE);
    ws_window_unredirect (compositor->glw);
    
    region = ws_region_new (compositor->ws);
    ws_window_set_input_shape (compositor->glw, region);
    ws_region_unref (region);
    
    ws_window_map (compositor->glw);
    
    ws_sync (compositor->ws);
    
    info = g_new (Info, 1);
    info->window = compositor->glw;
    info->screen = screen;
    
    g_idle_add (update, info);
    
    the_info = info;
#endif
}

void
meta_compositor_unmanage_screen (MetaCompositor *compositor,
                                 MetaScreen     *screen)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS  
    if (!compositor->enabled)
	return; /* no extension */
    
    while (screen->compositor_windows != NULL)
    {
	DrawableNode *node = screen->compositor_windows->data;
	
	meta_compositor_remove_window (compositor, node->drawable->xid);
    }
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

static DrawableNode *
window_to_node (MetaCompositor *compositor,
		MetaWindow *window)
{
    Window xwindow;
    DrawableNode *node;
    
    if (window->frame)
	xwindow = window->frame->xwindow;
    else
	xwindow = window->xwindow;
    
    node = g_hash_table_lookup (compositor->window_hash,
				&xwindow);
    
    return node;
}

typedef struct
{
    double x;
    double y;
    double width;
    double height;
} DoubleRect;

typedef struct
{
    MetaWindow *window;
    DrawableNode *node;

    DoubleRect start;
    DoubleRect target;
    
    double start_time;
    int idle_id;
} MiniInfo;

static gdouble
interpolate (gdouble t, gdouble begin, gdouble end, double power)
{
    return (begin + (end - begin) * pow (t, power));
}

static gboolean
stop_minimize (gpointer data)
{
    MiniInfo *info = data;
    
    drawable_node_set_deformation_func (info->node, NULL, NULL);

    meta_window_hide (info->window);
    meta_workspace_focus_default_window (info->window->screen->active_workspace, info->window, meta_display_get_current_time_roundtrip (info->window->display));
    
    g_free (info);
    
    return FALSE;
}

static void
minimize_deformation (gdouble time,
		      double in_x,
		      double in_y,
		      double *out_x,
		      double *out_y,
		      gpointer data)
{
#define MINIMIZE_TIME 0.5
    MiniInfo *info = data;
    gdouble elapsed;
    gdouble pos;

    if (info->start_time == -1)
	info->start_time = time;

    elapsed = time - info->start_time;
    pos = elapsed / MINIMIZE_TIME;
    
    *out_x = interpolate (pos, in_x, info->target.x + info->target.width * ((in_x - info->start.x)  / info->start.width), 10 * in_y);
    *out_y = interpolate (pos, in_y, info->target.y + info->target.height * ((in_y - info->start.y)  / info->start.height), 1.0);

    if (elapsed > MINIMIZE_TIME)
    {
	g_assert (info->node);
	if (!info->idle_id)
	    info->idle_id = g_idle_add (stop_minimize, info);
    }
}

static void
convert (MetaScreen *screen,
	 int x, int y, int width, int height,
	 DoubleRect *rect)
{
    rect->x = x / (double)screen->width;
    rect->y = y / (double)screen->height;
    rect->width = width / (double)screen->width;
    rect->height = height / (double)screen->height;
}

void
meta_compositor_minimize (MetaCompositor *compositor,
			  MetaWindow *window,
			  int         x,
			  int         y,
			  int         width,
			  int         height)
{
    MiniInfo *info = g_new (MiniInfo, 1);
    DrawableNode *node = window_to_node (compositor, window);
    WsRectangle start;
    MetaScreen *screen = window->screen;
    
    info->node = node;

    info->idle_id = 0;
    
    ws_drawable_query_geometry (node->drawable, &start);

    convert (screen, start.x, start.y, start.width, start.height,
	     &info->start);
    convert (screen, x, y, width, height,
	     &info->target);

    g_print ("start: %f %f %f %f\n",
	     info->start.x, info->start.y,
	     info->start.width, info->start.height);
    
    g_print ("target: %f %f %f %f\n",
	     info->target.x, info->target.y,
	     info->target.width, info->target.height);

    info->window = window;
    
    info->target.y = 1 - info->target.y;
    
    info->start_time = -1;
    
    drawable_node_set_deformation_func (node, minimize_deformation, info);
}

MetaDisplay *
meta_compositor_get_display (MetaCompositor *compositor)
{
    return compositor->display;
}
