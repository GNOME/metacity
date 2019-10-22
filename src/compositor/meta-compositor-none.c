/*
 * Copyright (C) 2017-2019 Alberts Muktupāvels
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

static MetaSurface *
meta_compositor_none_add_window (MetaCompositor *compositor,
                                 MetaWindow     *window)
{
  return NULL;
}

static void
meta_compositor_none_process_event (MetaCompositor *compositor,
                                    XEvent         *event,
                                    MetaWindow     *window)
{
}

static void
meta_compositor_none_sync_screen_size (MetaCompositor *compositor)
{
}

static void
meta_compositor_none_redraw (MetaCompositor *compositor,
                             XserverRegion   all_damage)
{
}

static void
meta_compositor_none_class_init (MetaCompositorNoneClass *none_class)
{
  MetaCompositorClass *compositor_class;

  compositor_class = META_COMPOSITOR_CLASS (none_class);

  compositor_class->manage = meta_compositor_none_manage;
  compositor_class->add_window = meta_compositor_none_add_window;
  compositor_class->process_event = meta_compositor_none_process_event;
  compositor_class->sync_screen_size = meta_compositor_none_sync_screen_size;
  compositor_class->redraw = meta_compositor_none_redraw;
}

static void
meta_compositor_none_init (MetaCompositorNone *none)
{
  meta_compositor_set_composited (META_COMPOSITOR (none), FALSE);
}

MetaCompositor *
meta_compositor_none_new (MetaDisplay  *display,
                          GError      **error)
{
  return g_initable_new (META_TYPE_COMPOSITOR_NONE, NULL, error,
                         "display", display,
                         NULL);
}
