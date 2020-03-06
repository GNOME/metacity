/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001 Anders Carlsson, Havoc Pennington
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

#include <config.h>
#include "effects.h"
#include "display-private.h"
#include "ui.h"
#include "window-private.h"
#include "prefs.h"

#include <X11/extensions/shape.h>

#define META_MINIMIZE_ANIMATION_LENGTH 0.25

#include <string.h>

typedef struct
{
  MetaScreen *screen;

  double millisecs_duration;
  gint64 start_time;

  /** For wireframe window */
  Window wireframe_xwindow;

  MetaRectangle start_rect;
  MetaRectangle end_rect;

} BoxAnimationContext;

static void draw_box_animation (MetaScreen    *screen,
                                MetaRectangle *initial_rect,
                                MetaRectangle *destination_rect,
                                double         seconds_duration);

void
meta_effect_run_minimize (MetaWindow    *window,
                          MetaRectangle *window_rect,
                          MetaRectangle *icon_rect)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (icon_rect != NULL);

  if (meta_prefs_get_gnome_animations ())
    {
      draw_box_animation (window->screen, window_rect, icon_rect,
                          META_MINIMIZE_ANIMATION_LENGTH);
    }
}

/* old ugly minimization effect */

static void
update_wireframe_window (MetaDisplay         *display,
                         Window               xwindow,
                         const MetaRectangle *rect)
{
  XMoveResizeWindow (display->xdisplay,
                     xwindow,
                     rect->x, rect->y,
                     rect->width, rect->height);

#define OUTLINE_WIDTH 3

  if (rect->width > OUTLINE_WIDTH * 2 &&
      rect->height > OUTLINE_WIDTH * 2)
    {
      XRectangle xrect;
      Region inner_xregion;
      Region outer_xregion;

      inner_xregion = XCreateRegion ();
      outer_xregion = XCreateRegion ();

      xrect.x = 0;
      xrect.y = 0;
      xrect.width = rect->width;
      xrect.height = rect->height;

      XUnionRectWithRegion (&xrect, outer_xregion, outer_xregion);

      xrect.x += OUTLINE_WIDTH;
      xrect.y += OUTLINE_WIDTH;
      xrect.width -= OUTLINE_WIDTH * 2;
      xrect.height -= OUTLINE_WIDTH * 2;

      XUnionRectWithRegion (&xrect, inner_xregion, inner_xregion);

      XSubtractRegion (outer_xregion, inner_xregion, outer_xregion);

      XShapeCombineRegion (display->xdisplay, xwindow,
                           ShapeBounding, 0, 0, outer_xregion, ShapeSet);

      XDestroyRegion (outer_xregion);
      XDestroyRegion (inner_xregion);
    }
  else
    {
      /* Unset the shape */
      XShapeCombineMask (display->xdisplay, xwindow,
                         ShapeBounding, 0, 0, None, ShapeSet);
    }
}

static gboolean
effects_draw_box_animation_timeout (BoxAnimationContext *context)
{
  double elapsed;
  gint64 current_time;
  MetaRectangle draw_rect;
  double fraction;

  current_time = g_get_real_time ();

  /* We use milliseconds for all times */
  elapsed = (current_time - context->start_time) / 1000.0;

  if (elapsed < 0)
    {
      /* Probably the system clock was set backwards? */
      g_warning ("System clock seemed to go backwards?");
      elapsed = G_MAXDOUBLE; /* definitely done. */
    }

  if (elapsed > context->millisecs_duration)
    {
      /* All done */
        XDestroyWindow (context->screen->display->xdisplay,
                          context->wireframe_xwindow);

      g_free (context);
      return FALSE;
    }

  g_assert (context->millisecs_duration > 0.0);
  fraction = elapsed / context->millisecs_duration;

  draw_rect = context->start_rect;

  /* Now add a delta proportional to elapsed time. */
  draw_rect.x += (context->end_rect.x - context->start_rect.x) * fraction;
  draw_rect.y += (context->end_rect.y - context->start_rect.y) * fraction;
  draw_rect.width += (context->end_rect.width - context->start_rect.width) * fraction;
  draw_rect.height += (context->end_rect.height - context->start_rect.height) * fraction;

  /* don't confuse X or gdk-pixbuf with bogus rectangles */
  if (draw_rect.width < 1)
    draw_rect.width = 1;
  if (draw_rect.height < 1)
    draw_rect.height = 1;

  update_wireframe_window (context->screen->display,
                           context->wireframe_xwindow,
                           &draw_rect);

  /* kick changes onto the server */
  XFlush (context->screen->display->xdisplay);

  return TRUE;
}

static void
draw_box_animation (MetaScreen     *screen,
                    MetaRectangle  *initial_rect,
                    MetaRectangle  *destination_rect,
                    double          seconds_duration)
{
  BoxAnimationContext *context;
  XSetWindowAttributes attrs;

  g_return_if_fail (seconds_duration > 0.0);

  if (g_getenv ("METACITY_DEBUG_EFFECTS"))
    seconds_duration *= 10; /* slow things down */

  /* Create the animation context */
  context = g_new0 (BoxAnimationContext, 1);

  context->screen = screen;

  context->millisecs_duration = seconds_duration * 1000.0;

  context->start_rect = *initial_rect;
  context->end_rect = *destination_rect;

  attrs.override_redirect = True;
  attrs.background_pixel = BlackPixel (screen->display->xdisplay,
                                       screen->number);

  context->wireframe_xwindow = XCreateWindow (screen->display->xdisplay,
                                              screen->xroot,
                                              initial_rect->x,
                                              initial_rect->y,
                                              initial_rect->width,
                                              initial_rect->height,
                                              0,
                                              CopyFromParent,
                                              CopyFromParent,
                                              (Visual *)CopyFromParent,
                                              CWOverrideRedirect | CWBackPixel,
                                              &attrs);

  update_wireframe_window (screen->display,
                           context->wireframe_xwindow,
                           initial_rect);

  XMapWindow (screen->display->xdisplay,
              context->wireframe_xwindow);

  /* Do this only after we get the pixbuf from the server,
   * so that the animation doesn't get truncated.
   */
  context->start_time = g_get_real_time ();

  /* Add the timeout - a short one, could even use an idle,
   * but this is maybe more CPU-friendly.
   */
  g_timeout_add (15,
                 (GSourceFunc)effects_draw_box_animation_timeout,
                 context);

  /* kick changes onto the server */
  XFlush (context->screen->display->xdisplay);
}
