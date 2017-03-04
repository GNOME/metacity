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

#include "meta-compositor-none.h"

struct _MetaCompositorNone
{
  MetaCompositor parent;
};

G_DEFINE_TYPE (MetaCompositorNone, meta_compositor_none, META_TYPE_COMPOSITOR)

static gboolean
meta_compositor_none_manage (MetaCompositor  *compositor,
                             GError         **error)
{
  return TRUE;
}

static void
meta_compositor_none_unmanage (MetaCompositor *compositor)
{
}

static void
meta_compositor_none_add_window (MetaCompositor *compositor,
                                 MetaWindow     *window)
{
}

static void
meta_compositor_none_remove_window (MetaCompositor *compositor,
                                    MetaWindow     *window)
{
}

static void
meta_compositor_none_show_window (MetaCompositor *compositor,
                                  MetaWindow     *window,
                                  MetaEffectType  effect)
{
}

static void
meta_compositor_none_hide_window (MetaCompositor *compositor,
                                  MetaWindow     *window,
                                  MetaEffectType  effect)
{
}

static void
meta_compositor_none_set_updates_frozen (MetaCompositor *compositor,
                                         MetaWindow     *window,
                                         gboolean        updates_frozen)
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
meta_compositor_none_maximize_window (MetaCompositor *compositor,
                                      MetaWindow     *window)
{
}

static void
meta_compositor_none_unmaximize_window (MetaCompositor *compositor,
                                        MetaWindow     *window)
{
}

static void
meta_compositor_none_sync_stack (MetaCompositor *compositor,
                                 GList          *stack)
{
}

static gboolean
meta_compositor_none_is_our_xwindow (MetaCompositor *compositor,
                                     Window          xwindow)
{
  return FALSE;
}

static void
meta_compositor_none_class_init (MetaCompositorNoneClass *none_class)
{
  MetaCompositorClass *compositor_class;

  compositor_class = META_COMPOSITOR_CLASS (none_class);

  compositor_class->manage = meta_compositor_none_manage;
  compositor_class->unmanage = meta_compositor_none_unmanage;
  compositor_class->add_window = meta_compositor_none_add_window;
  compositor_class->remove_window = meta_compositor_none_remove_window;
  compositor_class->show_window = meta_compositor_none_show_window;
  compositor_class->hide_window = meta_compositor_none_hide_window;
  compositor_class->set_updates_frozen = meta_compositor_none_set_updates_frozen;
  compositor_class->process_event = meta_compositor_none_process_event;
  compositor_class->get_window_surface = meta_compositor_none_get_window_surface;
  compositor_class->set_active_window = meta_compositor_none_set_active_window;
  compositor_class->begin_move = meta_compositor_none_begin_move;
  compositor_class->update_move = meta_compositor_none_update_move;
  compositor_class->end_move = meta_compositor_none_end_move;
  compositor_class->maximize_window = meta_compositor_none_maximize_window;
  compositor_class->unmaximize_window = meta_compositor_none_unmaximize_window;
  compositor_class->sync_stack = meta_compositor_none_sync_stack;
  compositor_class->is_our_xwindow = meta_compositor_none_is_our_xwindow;
}

static void
meta_compositor_none_init (MetaCompositorNone *none)
{
}
