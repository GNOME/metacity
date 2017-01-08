/*
 * Copyright (C) 2017 Alberts Muktupāvels
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "compositor-none.h"
#include "compositor-private.h"

typedef struct
{
  MetaCompositor  compositor;

  MetaDisplay    *display;
} MetaCompositorNone;

static void
meta_compositor_none_destroy (MetaCompositor *compositor)
{
  g_free (compositor);
}

static void
meta_compositor_none_manage_screen (MetaCompositor *compositor,
                                    MetaScreen     *screen)
{
}

static void
meta_compositor_none_unmanage_screen (MetaCompositor *compositor,
                                      MetaScreen     *screen)
{
}

static void
meta_compositor_none_add_window (MetaCompositor    *compositor,
                                 MetaWindow        *window,
                                 Window             xwindow,
                                 XWindowAttributes *attrs)
{
}

static void
meta_compositor_none_remove_window (MetaCompositor *compositor,
                                    Window          xwindow)
{
}

static void
meta_compositor_none_set_updates (MetaCompositor *compositor,
                                  MetaWindow     *window,
                                  gboolean        updates)
{
}

static void
meta_compositor_none_process_event (MetaCompositor *compositor,
                                    XEvent         *event,
                                    MetaWindow     *window)
{
}

static cairo_surface_t *
meta_compositor_none_get_window_surface (MetaCompositor *compositor,
                                         MetaWindow     *window)
{
  return NULL;
}

static void
meta_compositor_none_set_active_window (MetaCompositor *compositor,
                                        MetaScreen     *screen,
                                        MetaWindow     *window)
{
}

static void
meta_compositor_none_begin_move (MetaCompositor *compositor,
                                 MetaWindow     *window,
                                 MetaRectangle  *initial,
                                 gint            grab_x,
                                 gint            grab_y)
{
}

static void
meta_compositor_none_update_move (MetaCompositor *compositor,
                                  MetaWindow     *window,
                                  gint            x,
                                  gint            y)
{
}

static void
meta_compositor_none_end_move (MetaCompositor *compositor,
                               MetaWindow     *window)
{
}

static void
meta_compositor_none_free_window (MetaCompositor *compositor,
                                  MetaWindow     *window)
{
}

static void
meta_compositor_none_maximize_window (MetaCompositor *compositor,
                                      MetaWindow     *window)
{
}

static void
meta_compositor_none_unmaximize_window (MetaCompositor *compositor,
                                        MetaWindow     *window)
{
}

static MetaCompositor comp_info = {
  meta_compositor_none_destroy,
  meta_compositor_none_manage_screen,
  meta_compositor_none_unmanage_screen,
  meta_compositor_none_add_window,
  meta_compositor_none_remove_window,
  meta_compositor_none_set_updates,
  meta_compositor_none_process_event,
  meta_compositor_none_get_window_surface,
  meta_compositor_none_set_active_window,
  meta_compositor_none_begin_move,
  meta_compositor_none_update_move,
  meta_compositor_none_end_move,
  meta_compositor_none_free_window,
  meta_compositor_none_maximize_window,
  meta_compositor_none_unmaximize_window,
};

MetaCompositor *
meta_compositor_none_new (MetaDisplay *display)
{
  MetaCompositorNone *none;

  none = g_new (MetaCompositorNone, 1);

  none->compositor = comp_info;
  none->display = display;

  return (MetaCompositor *) none;
}
