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
#include "cwindow.h"

#ifdef HAVE_COMPOSITE_EXTENSIONS
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrender.h>

#endif /* HAVE_COMPOSITE_EXTENSIONS */

#define SHADOW_OFFSET 10
#define FRAME_INTERVAL_MILLISECONDS ((int)(1000.0/40.0))

struct MetaCompositor
{
  MetaDisplay *display;
  
  int composite_error_base;
  int composite_event_base;
  int damage_error_base;
  int damage_event_base;
  int fixes_error_base;
  int fixes_event_base;
  
  GHashTable *window_hash;
  
  guint repair_idle;
  guint repair_timeout;
  
  guint enabled : 1;
  guint have_composite : 1;
  guint have_damage : 1;
  guint have_fixes : 1;
  guint have_name_window_pixmap : 1;
  
  GList *ignored_damage;
};

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
free_window_hash_value (void *v)
{
  CWindow *cwindow = v;
  
  cwindow_free (cwindow);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

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

MetaCompositor*
meta_compositor_new (MetaDisplay *display)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaCompositor *compositor;
  
  compositor = g_new0 (MetaCompositor, 1);
  
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

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
remove_repair_idle (MetaCompositor *compositor)
{
  if (compositor->repair_idle || compositor->repair_timeout)
    meta_topic (META_DEBUG_COMPOSITOR, "Damage idle removed\n");
  
  if (compositor->repair_idle != 0)
    {
      g_source_remove (compositor->repair_idle);
      compositor->repair_idle = 0;
    }
  
  if (compositor->repair_timeout != 0)
    {
      g_source_remove (compositor->repair_timeout);
      compositor->repair_timeout = 0;
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

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
draw_windows (MetaCompositor *compositor,
	      MetaScreen *screen,
	      GList *list,
	      XserverRegion damaged_region,
	      Picture picture)
{
  CWindow *cwindow;
  XserverRegion region_below;
  Display *dpy = compositor->display->xdisplay;
  
  if (!list)
    return;
  
  cwindow = list->data;
  
  region_below = XFixesCreateRegion (dpy, NULL, 0);
  XFixesCopyRegion (dpy, region_below, damaged_region);
  
  if (!cwindow_is_translucent (cwindow) && !cwindow_get_input_only (cwindow) && cwindow_get_viewable (cwindow))
    {
      XserverRegion opaque = cwindow_get_opaque_region (cwindow);

      XFixesSubtractRegion (dpy, region_below, region_below, opaque);

      XFixesDestroyRegion (dpy, opaque);
    }

  draw_windows (compositor, screen, list->next, region_below, picture);

  XFixesDestroyRegion (dpy, region_below);
      
#if 0
  if (window_region)
  {
      XserverRegion clip = XFixesCreateRegion (dpy, NULL, 0);
      XFixesCopyRegion (dpy, clip, damaged_region);
      XFixesIntersectRegion (dpy, clip, clip, window_region);

#if 0
      print_region (dpy, "clip1", clip);
#endif
      
      XRenderColor hilit = { rand(), rand(), rand(), 0xffff };

      XFixesSetPictureClipRegion (dpy, screen->root_picture, 0, 0, clip);
      XRenderFillRectangle (compositor->display->xdisplay,
			    PictOpSrc,
			    screen->root_picture, &hilit,
			    0, 0,
			    screen->width, screen->height);
      XSync (compositor->display->xdisplay, False);
      g_usleep (50000);

#if 0
      print_region (dpy, "clip2", clip);
  }
#endif
#endif
  XFixesSetPictureClipRegion (dpy, picture, 0, 0, damaged_region);
  cwindow_new_draw (cwindow, picture);
}

static Picture
create_root_buffer (MetaScreen *screen)
{
  GC gc;
  Display *display = screen->display->xdisplay;
  Pixmap buffer_pixmap;
  XGCValues value;
  XRenderPictFormat *format;
  Picture buffer_picture;
  
  buffer_pixmap = XCreatePixmap (display, screen->xroot,
				 screen->width,
				 screen->height,
				 DefaultDepth (display,
					       screen->number));
  
  value.function = GXcopy;
  value.subwindow_mode = IncludeInferiors;
  
  gc = XCreateGC (display, buffer_pixmap, GCFunction | GCSubwindowMode, &value);
  XSetForeground (display, gc, WhitePixel (display, screen->number));
  
  XSetForeground (display, gc, 0x00ff0099);
#if 0
  XFillRectangle (display, buffer_pixmap, gc, 0, 0,
		  screen->width, screen->height);
#endif
  
  format = XRenderFindVisualFormat (display,
				    DefaultVisual (display,
						   screen->number));
  
  buffer_picture = XRenderCreatePicture (display,
					 buffer_pixmap,
					 format,
					 0, 0);
  
  XFreePixmap (display, buffer_pixmap);
  XFreeGC (display, gc);
  
  return buffer_picture;
}

static void
paint_buffer (MetaScreen *screen, Picture buffer)
{
#if 0
  XFixesSetPictureClipRegion (screen->display->xdisplay,
			      screen->root_picture, 0, 0, damaged_region);
#endif
  
  XRenderComposite (screen->display->xdisplay,
		    PictOpSrc,
		    buffer, None,
		    screen->root_picture,
		    0, 0, 0, 0, 0, 0,
		    screen->width, screen->height);
}

static XserverRegion
do_paint_screen (MetaCompositor *compositor,
		 MetaScreen *screen,
		 Picture picture,
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
  
  
  meta_error_trap_push (compositor->display);
  XFixesSetPictureClipRegion (xdisplay,
			      picture, 0, 0,
			      region);
  draw_windows (compositor, screen, screen->compositor_windows, region, picture);
  meta_error_trap_pop (compositor->display, FALSE);
  
  
  return region;
}

static void
paint_screen (MetaCompositor *compositor,
              MetaScreen     *screen,
              XserverRegion   damage_region)
{
  Picture buffer_picture;
  Display *xdisplay;
  XserverRegion region;
  
  meta_topic (META_DEBUG_COMPOSITOR, "Repainting screen %d root 0x%lx\n",
              screen->number, screen->xroot);
  
  /* meta_display_grab (screen->display); */
  
  xdisplay = screen->display->xdisplay;
  
  buffer_picture = create_root_buffer (screen);

  /* set clip */
  region = do_paint_screen (compositor, screen, buffer_picture, damage_region);
  
  /* Copy buffer to root window */
  meta_topic (META_DEBUG_COMPOSITOR, "Copying buffer to root window 0x%lx picture 0x%lx\n",
              screen->xroot, screen->root_picture);
  
  XFixesSetPictureClipRegion (xdisplay,
                              screen->root_picture,
                              0, 0, region);

#if 0
  XRenderColor hilit = { 0xFFFF, 0x0000, 0x0000, 0xFFFF } ;
  XRenderFillRectangle (compositor->display->xdisplay,
                        PictOpSrc,
                        screen->root_picture, &hilit, 0, 0, screen->width, screen->height);
#endif

  XSync (xdisplay, False);

#if 0
  g_usleep (370000);
#endif

  XFixesSetPictureClipRegion (xdisplay, buffer_picture, 0, 0, None);
  
  paint_buffer (screen, buffer_picture);
  
  XFixesDestroyRegion (xdisplay, region);
  
  
  
  /* XFixesSetPictureClipRegion (xdisplay, buffer_picture, 0, 0, None); */
  XRenderFreePicture (xdisplay, buffer_picture);
  
  /* meta_display_ungrab (screen->display); */
  
  XSync (screen->display->xdisplay, False);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
do_repair (MetaCompositor *compositor)
{
  GSList *tmp;
  
  tmp = compositor->display->screens;
  while (tmp != NULL)
    {
      MetaScreen *s = tmp->data;
      
      if (s->damage_region != None)
        {
          paint_screen (compositor, s,
                        s->damage_region);
          XFixesDestroyRegion (s->display->xdisplay,
                               s->damage_region);
          s->damage_region = None;
        }
      
      tmp = tmp->next;
    }
  
  remove_repair_idle (compositor);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static gboolean
repair_idle_func (void *data)
{
  MetaCompositor *compositor = data;
  
  compositor->repair_idle = 0;
  do_repair (compositor);
  
  return FALSE;
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */


#ifdef HAVE_COMPOSITE_EXTENSIONS
static gboolean
repair_timeout_func (void *data)
{
  MetaCompositor *compositor = data;
  
  compositor->repair_timeout = 0;
  do_repair (compositor);
  
  return FALSE;
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
ensure_repair_idle (MetaCompositor *compositor)
{
  if (compositor->repair_idle != 0)
    return;
  
  compositor->repair_idle = g_idle_add_full (META_PRIORITY_COMPOSITE,
                                             repair_idle_func, compositor, NULL);
  compositor->repair_timeout = g_timeout_add (FRAME_INTERVAL_MILLISECONDS,
                                              repair_timeout_func, compositor);
  
  meta_topic (META_DEBUG_COMPOSITOR, "Damage idle queued\n");
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#if 0
#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
merge_and_destroy_damage_region (MetaCompositor *compositor,
                                 MetaScreen     *screen,
                                 XserverRegion   region)
{
  if (screen->damage_region != None)
    {
      XFixesUnionRegion (compositor->display->xdisplay,
                         screen->damage_region,
                         region, screen->damage_region);
      XFixesDestroyRegion (compositor->display->xdisplay,
                           region);
    }
  else
    {
      screen->damage_region = region;
    }
  
  ensure_repair_idle (compositor);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */
#endif

#ifdef HAVE_COMPOSITE_EXTENSIONS
void
meta_compositor_invalidate_region (MetaCompositor *compositor,
				   MetaScreen	*screen,
				   XserverRegion   invalid_area)
{
  if (screen->damage_region == None)
    screen->damage_region =
      XFixesCreateRegion (compositor->display->xdisplay, NULL, 0);
  
  XFixesUnionRegion (compositor->display->xdisplay,
                     screen->damage_region,
                     invalid_area, screen->damage_region);
  
  ensure_repair_idle (compositor);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_damage_notify (MetaCompositor     *compositor,
                       XDamageNotifyEvent *event)
{
  CWindow *cwindow;
  
  cwindow = g_hash_table_lookup (compositor->window_hash,
                                 &event->drawable);
  
  if (cwindow == NULL)
    return;

  cwindow_process_damage_notify (cwindow, event);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */



static void
handle_restacking (MetaCompositor *compositor,
		   CWindow *cwindow,
		   CWindow *above)
{
  GList *window_link, *above_link;
  MetaScreen *screen = cwindow_get_screen (cwindow);
  
  window_link = g_list_find (screen->compositor_windows, cwindow);
  above_link  = g_list_find (screen->compositor_windows, above);

  if (window_link->next != above_link)
    {
      screen->compositor_windows = g_list_delete_link (screen->compositor_windows, window_link);
      screen->compositor_windows = g_list_insert_before (screen->compositor_windows, above_link, cwindow);
    }
}

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_configure_notify (MetaCompositor  *compositor,
                          XConfigureEvent *event)
{
  CWindow *cwindow;
  
  cwindow = g_hash_table_lookup (compositor->window_hash,
                                 &event->window);
  if (cwindow == NULL)
    return;
  
  cwindow_process_configure_notify (cwindow, event);

  handle_restacking (compositor, cwindow,
		     g_hash_table_lookup (compositor->window_hash, &event->above));
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
  
  meta_compositor_invalidate_region (compositor,
				     screen,
				     region);

  XFixesDestroyRegion (compositor->display->xdisplay, region);
}

#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_map (MetaCompositor     *compositor,
             XMapEvent          *event)
{
  CWindow *cwindow;
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
  
  cwindow = g_hash_table_lookup (compositor->window_hash,
                                 &event->window);
  if (cwindow == NULL)
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
          meta_topic (META_DEBUG_COMPOSITOR,
                      "Map window 0x%lx\n", event->window);
          meta_compositor_add_window (compositor,
                                      event->window, &attrs);
        }
    }
  else
    {
      cwindow_set_viewable (cwindow, TRUE);
    }

  /* We don't actually need to invalidate anything, because we will
   * get damage events as the server fills the background and the client
   * draws
   */
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
static void
process_unmap (MetaCompositor     *compositor,
               XUnmapEvent        *event)
{
  CWindow *cwindow;
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
  
  cwindow = g_hash_table_lookup (compositor->window_hash,
                                 &event->window);
  if (cwindow != NULL)
    {
      cwindow_set_viewable (cwindow, FALSE);
      
      if (cwindow_get_last_painted_extents (cwindow))
        {
          meta_compositor_invalidate_region (compositor,
					   screen,
					   cwindow_get_last_painted_extents (cwindow));
          cwindow_set_last_painted_extents (cwindow, None);
        }
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
  
  g_print ("hello\n");
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
  CWindow *cwindow;
  XWindowAttributes attrs;
  
  event_screen = meta_display_screen_for_root (compositor->display,
                                               event->event);
  
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
  
  cwindow = g_hash_table_lookup (compositor->window_hash,
                                 &event->window);
  if (cwindow != NULL)
    {
      meta_topic (META_DEBUG_COMPOSITOR,
                  "Window reparented to new screen at %d,%d\n",
                  event->x, event->y);
      cwindow_set_x (cwindow, event->x);
      cwindow_set_y (cwindow, event->y);
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
                  "Reparent window 0x%lx into screen 0x%lx, adding\n",
                  event->window, event->parent);
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
      process_damage_notify (compositor,
                             (XDamageNotifyEvent*) event);
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
  CWindow *cwindow;
  MetaScreen *screen;
  
  XserverRegion region;
  
  if (!compositor->enabled)
    return; /* no extension */
  
  screen = meta_screen_for_x_screen (attrs->screen);
  g_assert (screen != NULL);
  
  cwindow = g_hash_table_lookup (compositor->window_hash,
                                 &xwindow);
  
  if (cwindow != NULL)
    {
      meta_topic (META_DEBUG_COMPOSITOR,
                  "Window 0x%lx already added\n", xwindow);
      return;
    }
  
  meta_topic (META_DEBUG_COMPOSITOR,
              "Adding window 0x%lx (%s) to compositor\n",
              xwindow,
              attrs->map_state == IsViewable ?
              "mapped" : "unmapped");
  
  cwindow = cwindow_new (compositor, xwindow, attrs);
  
  /* FIXME this assertion can fail somehow... */
  g_assert (attrs->map_state != IsUnviewable);
#if 0
  if (attrs->map_state == Mapped)
    g_print ("mapped\n");

  if (attrs->map_state == Viewable)
    g_pritn ("viewable\n");
#endif
  
  g_hash_table_insert (compositor->window_hash,
                       cwindow_get_xid_address (cwindow), cwindow);
  
  /* assume cwindow is at the top of the stack as it was either just
   * created or just reparented to the root window
   */
  screen->compositor_windows = g_list_prepend (screen->compositor_windows,
                                               cwindow);
  
  /* schedule paint of the new window */
  region = cwindow_extents (cwindow);

  meta_compositor_invalidate_region (compositor, screen, region);

  XFixesDestroyRegion (compositor->display->xdisplay, region);
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

void
meta_compositor_remove_window (MetaCompositor    *compositor,
                               Window             xwindow)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  CWindow *cwindow;
  MetaScreen *screen;
  
  if (!compositor->enabled)
    return; /* no extension */
  
  cwindow = g_hash_table_lookup (compositor->window_hash,
                                 &xwindow);
  
  if (cwindow == NULL)
    {
      meta_topic (META_DEBUG_COMPOSITOR,
                  "Window 0x%lx already removed\n", xwindow);
      return;
    }
  
  meta_topic (META_DEBUG_COMPOSITOR,
              "Removing window 0x%lx (%s) from compositor\n",
              xwindow,
              cwindow_get_viewable (cwindow) ? "mapped" : "unmapped");
  
  screen = cwindow_get_screen (cwindow);
  
  if (cwindow_get_last_painted_extents (cwindow))
    {
      meta_compositor_invalidate_region (compositor,
				       screen,
				       cwindow_get_last_painted_extents (cwindow));
      cwindow_set_last_painted_extents (cwindow, None);
    }
  
  screen->compositor_windows = g_list_remove (screen->compositor_windows,
                                              cwindow);
  
  /* Frees cwindow as side effect */
  g_hash_table_remove (compositor->window_hash,
                       &xwindow);
  
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

void
meta_compositor_manage_screen (MetaCompositor *compositor,
                               MetaScreen     *screen)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  XRenderPictureAttributes pa;
  XRectangle r;
  XRenderColor c;
  XserverRegion region;
  
  if (!compositor->enabled)
    return; /* no extension */
  
  /* FIXME we need to handle root window resize by recreating the
   * root_picture
   */
  
  g_assert (screen->root_picture == None);
  
  /* FIXME add flag for whether we're composite-managing each
   * screen and detect failure here
   */
  
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
  
  meta_topic (META_DEBUG_COMPOSITOR, "Subwindows redirected, we are now the compositing manager\n");
  
  pa.subwindow_mode = IncludeInferiors;
  
  screen->root_picture =
    XRenderCreatePicture (compositor->display->xdisplay,
                          screen->xroot, 
                          XRenderFindVisualFormat (compositor->display->xdisplay,
                                                   DefaultVisual (compositor->display->xdisplay,
                                                                  screen->number)),
                          CPSubwindowMode,
                          &pa);
  
  g_assert (screen->root_picture != None);
  
  screen->trans_pixmap = XCreatePixmap (compositor->display->xdisplay,
                                        screen->xroot, 1, 1, 8);
  
  pa.repeat = True;
  screen->trans_picture =
    XRenderCreatePicture (compositor->display->xdisplay,
                          screen->trans_pixmap,
                          XRenderFindStandardFormat (compositor->display->xdisplay,
                                                     PictStandardA8),
                          CPRepeat,
                          &pa);
  
  c.red = c.green = c.blue = 0;
  c.alpha = 0xb0b0;
  XRenderFillRectangle (compositor->display->xdisplay,
                        PictOpSrc,
                        screen->trans_picture, &c, 0, 0, 1, 1);
  
  /* Damage the whole screen */
  r.x = 0;
  r.y = 0;
  r.width = screen->width;
  r.height = screen->height;

  region = XFixesCreateRegion (compositor->display->xdisplay, &r, 1);

  
#endif /* HAVE_COMPOSITE_EXTENSIONS */
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
      CWindow *cwindow = screen->compositor_windows->data;
      
      meta_topic (META_DEBUG_COMPOSITOR,
                  "Unmanage screen for 0x%lx\n", cwindow_get_xwindow (cwindow));
      meta_compositor_remove_window (compositor, cwindow_get_xwindow (cwindow));
    }
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

static CWindow *
window_to_cwindow (MetaCompositor *compositor,
		   MetaWindow *window)
{
  Window xwindow;
  CWindow *cwindow;
  
  if (window->frame)
    xwindow = window->frame->xwindow;
  else
    xwindow = window->xwindow;
  
  cwindow = g_hash_table_lookup (compositor->window_hash,
                                 &xwindow);
  
  return cwindow;
}

void
meta_compositor_damage_window (MetaCompositor *compositor,
                               MetaWindow     *window)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  XserverRegion region;
  CWindow *cwindow;
  
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
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

void
meta_compositor_stop_compositing (MetaCompositor *compositor,
				  MetaWindow     *window)
{
#if 0
  CWindow *cwindow = window_to_cwindow (compositor, window);
  
  if (cwindow)
    {
      if (cwindow->pixmap)
	XFreePixmap (compositor->display->xdisplay, cwindow->pixmap);
      
      cwindow->pixmap = XCompositeNameWindowPixmap (compositor->display->xdisplay,
						    cwindow->xwindow);
      
      cwindow->pending_x = cwindow->x;
      cwindow->pending_y = cwindow->y;
      cwindow->pending_width = cwindow->width;
      cwindow->pending_height = cwindow->height;
      cwindow->pending_border_width = cwindow->border_width;
    }
#endif
}

void
meta_compositor_start_compositing (MetaCompositor *compositor,
				   MetaWindow     *window)
{
#if 0
  CWindow *cwindow = window_to_cwindow (compositor, window);
  
  if (cwindow)
    {
      if (cwindow->pixmap)
	{
	  XFreePixmap (compositor->display->xdisplay, cwindow->pixmap);
	  cwindow->pixmap = None;
	}
      
      cwindow->x = cwindow->pending_x;
      cwindow->y = cwindow->pending_y;
      cwindow->width = cwindow->pending_width;
      cwindow->height = cwindow->pending_height;
      cwindow->border_width = cwindow->pending_border_width;
      
      meta_compositor_damage_window (compositor, window);
    }
#endif
}

static void
quad_to_quad_interpolate (Quad *start, Quad *end, Quad *dest, gdouble t)
{
  int i;
  
  for (i = 0; i < 4; ++i)
    {
      if (i == 3)
	{
	  dest->points[i].x =
	    start->points[i].x + (end->points[i].x - start->points[i].x) * t;
	}
      else
	{
	  dest->points[i].x =
	    start->points[i].x + (end->points[i].x - start->points[i].x) * pow (t, 1.5);
	}
      if (i == 1)
	{
	  dest->points[i].y =
	    start->points[i].y + (end->points[i].y - start->points[i].y) * pow (t, 1.5);
	}
      else
	{
	  dest->points[i].y =
	    start->points[i].y + (end->points[i].y - start->points[i].y) * t;
	}
    }
}

#if 0
void
meta_compositor_old_genie (MetaCompositor *compositor,
			   MetaWindow       *window)
{
#if 0
  int i;
  int x, y, w, h;
#if 0
  int dx, dy, dw, dh;
#endif
  
  Quad start;
  Quad end;
  Quad dest;
  
  CWindow *cwindow = window_to_cwindow (compositor, window);
  
  if (!cwindow)
    return;
  
  x = cwindow_get_x (cwindow);
  y = cwindow_get_y (cwindow);
  w = cwindow_get_width (cwindow) + cwindow_get_border_width (cwindow) * 2;
  h = cwindow_get_height (cwindow) + cwindow_get_border_width (cwindow) * 2;
  
  start.points[0].x = x;
  start.points[0].y = y;
  start.points[1].x = x + w - 1;
  start.points[1].y = y;
  start.points[2].x = x;
  start.points[2].y = y + h - 1;
  start.points[3].x = x + w - 1;
  start.points[3].y = y + h - 1;
  
  end.points[0].x = 0;
  end.points[0].y = 1200 - 30;
  end.points[1].x = 100;
  end.points[1].y = 1200 - 30;
  end.points[2].x = 0;
  end.points[2].y = 1200 - 1;
  end.points[3].x = 100;
  end.points[3].y = 1200 - 1;

#define N_STEPS 5
  
  for  (i = 0; i < N_STEPS; ++i)
    {
#if 0
      int j;
#endif
#if 0
      XTransform trans = {
	{{ XDoubleToFixed(1.0), XDoubleToFixed(0),   XDoubleToFixed(0) },
	 { XDoubleToFixed(0),   XDoubleToFixed(1.0), XDoubleToFixed(0) },
	 { XDoubleToFixed(0),   XDoubleToFixed(0),   XDoubleToFixed((49 - i)/49.0)}}
      };	    
#endif
#if 0
      XRenderPictFormat *format;
#endif
      Picture buffer;
      
#if 0
      wpicture = XRenderCreatePicture (compositor->display->xdisplay,
				       cwindow_get_drawable (cwindow),
				       format,
				       CPSubwindowMode,
				       &pa);
      
      g_assert (compositor->display);
#endif
      
      quad_to_quad_interpolate (&start, &end, &dest, i/((double)N_STEPS - 1));
      
      buffer = create_root_buffer (window->screen);
      
      XFixesSetPictureClipRegion (compositor->display->xdisplay,
				  buffer, 0, 0,
				  None);
      
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

      cwindow_draw_warped (cwindow, window->screen, buffer, &dest);
      
#if 0
      XRenderColor c = { 0x1000, 0x1000, 0x1000, 0x70c0 };
#endif
      
      XFixesSetPictureClipRegion (compositor->display->xdisplay, window->screen->root_picture, 0, 0, None);
      
#if 0
      XRenderFillRectangle (compositor->display->xdisplay, PictOpOver, buffer, &c, 0, 0, 100, 100);
#endif
      
      paint_buffer (window->screen, buffer);
      
      XRenderFreePicture (compositor->display->xdisplay, buffer);
      
      XSync (compositor->display->xdisplay, False);
      /* Copy buffer to root window */
#if 0
      meta_topic (META_DEBUG_COMPOSITOR, "Copying buffer to root window 0x%lx picture 0x%lx\n",
		  screen->xroot, screen->root_picture);
#endif
#if 0
      g_print ("%d %d %d %d  %d %d %d %d\n",
	       dest.points[0].x, dest.points[0].y,
	       dest.points[1].x, dest.points[1].y,
	       dest.points[2].x, dest.points[2].y,
	       dest.points[3].x, dest.points[3].y);
#endif
#if 0
      g_usleep (30000);
#endif
    }
#endif
}
#endif

void
meta_compositor_genie (MetaCompositor *compositor,
		       MetaWindow       *window)
{
  int i;
  int x, y, w, h;
#if 0
  int dx, dy, dw, dh;
#endif
  
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
      
      buffer = create_root_buffer (window->screen);
      
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
      
      cwindow_new_draw (cwindow, buffer);
      
      XFixesSetPictureClipRegion (compositor->display->xdisplay,
				  window->screen->root_picture, 0, 0, None);
      /* Copy buffer to root window */
      paint_buffer (window->screen, buffer);
      
      XRenderFreePicture (compositor->display->xdisplay, buffer);
      
      XSync (compositor->display->xdisplay, False);
#if 0
      g_usleep (500000);
#endif
  }
}

MetaDisplay *
meta_compositor_get_display (MetaCompositor *compositor)
{
  return compositor->display;
}
