/* Metacity compositing manager */

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
    
#if 0
    g_print ("freeing cwindow %lx\n", cwindow_get_xwindow (cwindow));
#endif
    drawable_node_unref (drawable_node);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#if 0
static void
print_region (Display *dpy, const char *name, XserverRegion region)
{
    XRectangle *rects;
    int i, n_rects;
    
    rects = XFixesFetchRegion (dpy, region, &n_rects);
    
    g_print ("region \"%s\":\n", name);
    for (i = 0; i < n_rects; ++i)
	g_print ("  %d %d %d %d\n", rects[i].x, rects[i].y, rects[i].width, rects[i].height);
    XFree (rects);
}
#endif

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
static GTimer *timer ;

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

static XserverRegion
do_paint_screen (MetaCompositor *compositor,
		 MetaScreen *screen,
		 XserverRegion damage_region)
{
    Display *xdisplay = compositor->display->xdisplay;
    XserverRegion region;
    if (damage_region == None)
    {
	XRectangle  r;
	
	r.x = 0;
	r.y = 0;
	r.width = screen->width;
	r.height = screen->height;
	
	region = XFixesCreateRegion (xdisplay, &r, 1);
    }
    else
    {
	region = XFixesCreateRegion (xdisplay, NULL, 0);
	
	XFixesCopyRegion (compositor->display->xdisplay,
			  region,
			  damage_region);
    }
    
#if 0
    {
	XGCValues values;
	values.subwindow_mode = IncludeInferiors;
	values.foreground = 0x00ff00ff; /* shocking pink */
	GC gc = XCreateGC (screen->display->xdisplay,
			   screen->xroot,
			   GCSubwindowMode | GCForeground, &values);
	
	XFixesSetGCClipRegion (screen->display->xdisplay, gc,
			       0, 0, region);
	
	XFillRectangle (screen->display->xdisplay, screen->xroot, gc,
			0, 0, screen->width, screen->height);
	XSync (screen->display->xdisplay, False);
    }
#endif
    
    
    {
	glShadeModel (GL_FLAT);
	glDisable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
	
	glViewport (0, 0, screen->width, screen->height);

	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();
	gluOrtho2D (0, 1.0, 1.0, 0.0);
	
	g_print ("clearing\n");
	
	glClearColor (0.0, 0.5, 0.5, 0.0);
	glClear (GL_COLOR_BUFFER_BIT);
	
#if 0
	glLoadIdentity ();
	
	glColor4f (0.8, 0.3, 0.8, 0.8);
	
	glBegin (GL_TRIANGLES);
	
	glVertex2f (0.0, 0.0);
	glVertex2f (10, 100);
	glVertex2f (20, 0.0);
	
	glEnd ();
	
	glColor4f (0.2, 0.2, 1.0, 0.5);
	
	glBegin (GL_TRIANGLES);
	
	glVertex2f (-1.0, 0.0);
	glVertex2f (120, 100);
	glVertex2f (200, 23.0);
	
	glEnd();
#endif
	
#if 0
	glColor4f (g_random_double (),
		   g_random_double (),
		   g_random_double (),
		   1.0);
#endif
	
#if 0
	glBegin (GL_QUADS);
	
	glVertex2f (-1.0, -1.0);
	glVertex2f (1.0, -1.0);
	glVertex2f (1.0, 1.0);
	glVertex2f (-1.0, 1.0);
	
	glEnd ();
#endif
	
	glLoadIdentity();
	
	gluOrtho2D (0, screen->width, screen->height, 0);
	glDisable (GL_SCISSOR_TEST);
    }
    
    if (!timer)
	timer = g_timer_new ();
    
#if 0
    g_print ("outside: %f\n", (tmp = g_timer_elapsed (timer, NULL)) - last);
    last = tmp;
#endif
    
    ws_window_raise (compositor->glw);
    draw_windows (screen, screen->compositor_windows);
    {
	glShadeModel (GL_FLAT);
	glDisable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
	
	/* FIXME: we should probably grab the server around the raise/swap */

	ws_window_gl_swap_buffers (compositor->glw);
    }
    
    
    return region;
}

#ifdef HAVE_COMPOSITE_EXTENSIONS
#if 0
void
meta_compositor_invalidate_region (MetaCompositor *compositor,
				   MetaScreen	*screen,
				   XserverRegion   invalid_area)
{
    if (screen->damage_region == None)
	screen->damage_region =
	    XFixesCreateRegion (compositor->display->xdisplay, NULL, 0);
    
#if 0
    if (compositor->debug_updates)
    {
#     define ALPHA 0.1
	XRenderColor hilight = { ALPHA * 0x0000, ALPHA * 0x1000, ALPHA * 0xFFFF, ALPHA * 0xFFFF };
	XFixesSetPictureClipRegion (screen->display->xdisplay, screen->root_picture, 0, 0, invalid_area);
	XRenderFillRectangle (screen->display->xdisplay, PictOpOver,
			      screen->root_picture, &hilight,
			      0, 0, screen->width, screen->height);
	XSync (screen->display->xdisplay, False);
    }
#endif
    
#if 0
    print_region (compositor->display->xdisplay, "invalidate", invalid_area);
    meta_print_top_of_stack (4);
#endif
    
    XFixesUnionRegion (compositor->display->xdisplay,
		       screen->damage_region,
		       invalid_area, screen->damage_region);
}
#endif
#endif /* HAVE_COMPOSITE_EXTENSIONS */

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
	g_print ("yup\n");
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
    
    if (screen == NULL || screen->root_picture == None)
	return;
    
    r.x = 0;
    r.y = 0;
    r.width = screen->width;
    r.height = screen->height;
    region = XFixesCreateRegion (compositor->display->xdisplay, &r, 1);
    
#if 0
    meta_compositor_invalidate_region (compositor,
				       screen,
				       region);
#endif
    
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
    
    if (screen == NULL || screen->root_picture == None)
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
    
    if (screen == NULL || screen->root_picture == None)
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
    
    if (screen == NULL || screen->root_picture == None)
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
    
    if (screen == NULL || screen->root_picture == None)
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
    
    if (event_screen == NULL || event_screen->root_picture == None)
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
    
#if 0
    if (xwindow == compositor->gl_window)
	return;
#endif
    
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
	g_print ("allocating drawable\n");
	node = drawable_node_new (drawable);
    }
    
    /* FIXME: this will not work - the hash table should be fixed */
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

#if 0
    if (cwindow_get_last_painted_extents (cwindow))
    {
	meta_compositor_invalidate_region (compositor,
					   screen,
					   cwindow_get_last_painted_extents (cwindow));
	cwindow_set_last_painted_extents (cwindow, None);
    }
#endif
    
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
    
#if 0
    g_print ("raise/swap\n");
#endif
    
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
    XRenderPictureAttributes pa;
    
    if (!compositor->enabled)
	return; /* no extension */
    
    /* FIXME we need to handle root window resize by recreating the
     * root_picture
     */
    
    g_assert (screen->root_picture == None);
    
    /* FIXME add flag for whether we're composite-managing each
     * screen and detect failure here
     */
    
#if 0
    /* CompositeRedirectSubwindows result in all mapped windows
     * getting unmapped/mapped, which confuses Metacity since it
     * thinks that non-override redirect windows should only
     * be mapped by the window manager.
     *
     * To work around this, we discard all events generated
     * by XCompositeRedirectSubwindows
     */
    
    XSync (screen->display->xdisplay, False);
    
    /* FIXME add flag for whether we're composite-managing each
     * screen and detect failure here
     */
    XCompositeRedirectSubwindows (screen->display->xdisplay,
				  screen->xroot,
				  CompositeRedirectManual);
    
    XSync (screen->display->xdisplay, True);
#endif
    
    /* FIMXE: This shouldn't be needed for the gl compositor, but it is. Various stuff gets
     * confused if root_picture is None
     */
    pa.subwindow_mode = IncludeInferiors;
    
    screen->root_picture =
	XRenderCreatePicture (compositor->display->xdisplay,
			      screen->xroot, 
			      XRenderFindVisualFormat (compositor->display->xdisplay,
						       DefaultVisual (compositor->display->xdisplay,
								      screen->number)),
			      CPSubwindowMode,
			      &pa);
    
    {
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
    }
#endif
}

void
meta_compositor_unmanage_screen (MetaCompositor *compositor,
                                 MetaScreen     *screen)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS  
    if (!compositor->enabled)
	return; /* no extension */
    
    XRenderFreePicture (screen->display->xdisplay,
			screen->root_picture);
    screen->root_picture = None;
    XRenderFreePicture (screen->display->xdisplay,
			screen->trans_picture);
    screen->trans_picture = None;
    XFreePixmap (screen->display->xdisplay,
		 screen->trans_pixmap);
    screen->trans_pixmap = None;
    
    while (screen->compositor_windows != NULL)
    {
	DrawableNode *node = screen->compositor_windows->data;
	
#if 0
	meta_topic (META_DEBUG_COMPOSITOR,
		    "Unmanage screen for 0x%lx\n", cwindow_get_xwindow (cwindow));
#endif
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

void
meta_compositor_damage_window (MetaCompositor *compositor,
                               MetaWindow     *window)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
    
#if 0
    if (!compositor->enabled)
	return;
    
    if (window->screen->root_picture == None)
	return;
    
    cwindow = window_to_cwindow (compositor, window);
    
    if (cwindow == NULL)
	return;
    
    region = cwindow_extents (cwindow);
    
    meta_compositor_invalidate_region (compositor, cwindow_get_screen (cwindow), region);
    
    XFixesDestroyRegion (compositor->display->xdisplay, region);
#endif
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

void
meta_compositor_stop_compositing (MetaCompositor *compositor,
				  MetaWindow     *window)
{
#if 0
    DrawableNode *node = window_to_node (compositor, window);
    
    if (cwindow)
	cwindow_freeze (cwindow);
#endif
}

void
meta_compositor_start_compositing (MetaCompositor *compositor,
				   MetaWindow     *window)
{
    DrawableNode *node;
    node = window_to_node (compositor, window);
    
#if 0
    if (cwindow)
	cwindow_thaw (cwindow);
#endif
    
#if 0
    meta_compositor_repair_now (compositor);
#endif
}

#if 0
void
meta_compositor_genie (MetaCompositor *compositor,
		       MetaWindow       *window)
{
    int i;
    int x, y, w, h;
    
    /* It's unusably slow right now, and also ugly */
    return;
    
    Quad start1;
    Quad end1;
    
    Quad start2;
    Quad end2;
    
    CWindow *cwindow = window_to_cwindow (compositor, window);
    
    if (!cwindow)
	return;
    
    x = cwindow_get_x (cwindow);
    y = cwindow_get_y (cwindow);
    w = cwindow_get_width (cwindow) + cwindow_get_border_width (cwindow) * 2;
    h = cwindow_get_height (cwindow) + cwindow_get_border_width (cwindow) * 2;
    
    start1.points[0].x = x;
    start1.points[0].y = y;
    start1.points[1].x = x + w/2 - 1;
    start1.points[1].y = y;
    start1.points[2].x = x;
    start1.points[2].y = y + h - 1;
    start1.points[3].x = x + w/2 - 1;
    start1.points[3].y = y + h - 1;
    
    end1.points[0].x = 500;
    end1.points[0].y = 200 + 50;
    end1.points[1].x = 600;
    end1.points[1].y = 200 + 50;
    end1.points[2].x = 500;
    end1.points[2].y = 200 + 80;
    end1.points[3].x = 600;
    end1.points[3].y = 200 + 80;
    
    start2.points[0].x = x + w/2;
    start2.points[0].y = y;
    start2.points[1].x = x + w - 1;
    start2.points[1].y = y;
    start2.points[2].x = x + w/2;
    start2.points[2].y = y + h - 1;
    start2.points[3].x = x + w - 1;
    start2.points[3].y = y + h - 1;
    
    end2.points[0].x = 600;
    end2.points[0].y = 200 + 50;
    end2.points[1].x = 700;
    end2.points[1].y = 200 + 50;
    end2.points[2].x = 600;
    end2.points[2].y = 200 + 80;
    end2.points[3].x = 700;
    end2.points[3].y = 200 + 130;
    
#define STEPS 50
    
    for  (i = 0; i < STEPS; ++i)
    {
	Distortion distortion[2];
	Picture buffer;
	Pixmap pixmap;
	
	quad_to_quad_interpolate (&start1, &end1, &distortion[0].destination, i/((double)STEPS - 1));
	quad_to_quad_interpolate (&start2, &end2, &distortion[1].destination, i/((double)STEPS - 1));
	
	distortion[0].source.x = 0;
	distortion[0].source.y = 0;
	distortion[0].source.width = cwindow_get_width (cwindow) / 2;
	distortion[0].source.height = cwindow_get_height (cwindow);
	
	distortion[1].source.x = cwindow_get_width (cwindow) / 2;
	distortion[1].source.y = 0;
	distortion[1].source.width = cwindow_get_width (cwindow) / 2;
	distortion[1].source.height = cwindow_get_height (cwindow);
	
	cwindow_set_transformation (cwindow, distortion, 2);
	
	buffer = create_root_buffer (window->screen, &pixmap);
	
	{
	    XRectangle  r;
	    XserverRegion region;
	    
	    r.x = 0;
	    r.y = 0;
	    r.width = window->screen->width;
	    r.height = window->screen->height;
	    
	    region = XFixesCreateRegion (compositor->display->xdisplay, &r, 1);
	    
	    XFixesDestroyRegion (compositor->display->xdisplay,
				 do_paint_screen (compositor, window->screen, buffer, region));
	    XFixesDestroyRegion (compositor->display->xdisplay, region);
	}
	
	cwindow_draw (cwindow, buffer, None);
	
	XFixesSetPictureClipRegion (compositor->display->xdisplay,
				    window->screen->root_picture, 0, 0, None);
	/* Copy buffer to root window */
	paint_buffer (window->screen, pixmap, None);
	
	XFreePixmap (compositor->display->xdisplay, pixmap);
	
	XRenderFreePicture (compositor->display->xdisplay, buffer);
	
	XSync (compositor->display->xdisplay, False);
    }
    
    cwindow_set_transformation (cwindow, NULL, 0);
}
#endif

MetaDisplay *
meta_compositor_get_display (MetaCompositor *compositor)
{
    return compositor->display;
}

void
meta_compositor_set_translucent (MetaCompositor *compositor,
				 MetaWindow *window,
				 gboolean translucent)
{
    DrawableNode *node = window_to_node (compositor, window);

    if (node)
    {
#if 0
	cwindow_set_translucent (cwindow, translucent);
#endif
    }
}
