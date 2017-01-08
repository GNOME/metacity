/*
 * Copyright (C) 2008 Iain Holmes
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
#include "compositor-xrender.h"

MetaCompositor *
meta_compositor_new (MetaCompositorType  type,
                     MetaDisplay        *display)
{
  switch (type)
    {
      case META_COMPOSITOR_TYPE_NONE:
        return meta_compositor_none_new (display);

      case META_COMPOSITOR_TYPE_XRENDER:
        return meta_compositor_xrender_new (display);

      default:
        g_assert_not_reached ();
        break;
    }

  return NULL;
}

void
meta_compositor_destroy (MetaCompositor *compositor)
{
  if (compositor && compositor->destroy)
    compositor->destroy (compositor);
}

void
meta_compositor_manage_screen (MetaCompositor *compositor,
                               MetaScreen     *screen)
{
  if (compositor && compositor->manage_screen)
    compositor->manage_screen (compositor, screen);
}

void
meta_compositor_unmanage_screen (MetaCompositor *compositor,
                                 MetaScreen     *screen)
{
  if (compositor && compositor->unmanage_screen)
    compositor->unmanage_screen (compositor, screen);
}

void
meta_compositor_add_window (MetaCompositor    *compositor,
                            MetaWindow        *window,
                            Window             xwindow,
                            XWindowAttributes *attrs)
{
  if (compositor && compositor->add_window)
    compositor->add_window (compositor, window, xwindow, attrs);
}

void
meta_compositor_remove_window (MetaCompositor *compositor,
                               Window          xwindow)
{
  if (compositor && compositor->remove_window)
    compositor->remove_window (compositor, xwindow);
}

void
meta_compositor_set_updates (MetaCompositor *compositor,
                             MetaWindow     *window,
                             gboolean        updates)
{
  if (compositor && compositor->set_updates)
    compositor->set_updates (compositor, window, updates);
}

void
meta_compositor_process_event (MetaCompositor *compositor,
                               XEvent         *event,
                               MetaWindow     *window)
{
  if (compositor && compositor->process_event)
    compositor->process_event (compositor, event, window);
}

cairo_surface_t *
meta_compositor_get_window_surface (MetaCompositor *compositor,
                                    MetaWindow     *window)
{
  if (compositor && compositor->get_window_surface)
    return compositor->get_window_surface (compositor, window);
  else
    return NULL;
}

void
meta_compositor_set_active_window (MetaCompositor *compositor,
                                   MetaScreen     *screen,
                                   MetaWindow     *window)
{
  if (compositor && compositor->set_active_window)
    compositor->set_active_window (compositor, screen, window);
}

void
meta_compositor_begin_move (MetaCompositor *compositor,
                            MetaWindow     *window,
                            MetaRectangle  *initial,
                            gint            grab_x,
                            gint            grab_y)
{
}

void
meta_compositor_update_move (MetaCompositor *compositor,
                             MetaWindow     *window,
                             gint            x,
                             gint            y)
{
}

void
meta_compositor_end_move (MetaCompositor *compositor,
                          MetaWindow     *window)
{
}

void meta_compositor_free_window (MetaCompositor *compositor,
                                  MetaWindow     *window)
{
  if (compositor && compositor->free_window)
    compositor->free_window (compositor, window);
}

void
meta_compositor_maximize_window (MetaCompositor *compositor,
                                 MetaWindow     *window)
{
  if (compositor && compositor->maximize_window)
    compositor->maximize_window (compositor, window);
}

void
meta_compositor_unmaximize_window (MetaCompositor *compositor,
                                   MetaWindow     *window)
{
  if (compositor && compositor->unmaximize_window)
    compositor->unmaximize_window (compositor, window);
}
