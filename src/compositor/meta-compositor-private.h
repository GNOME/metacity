/*
 * Copyright (C) 2008 Iain Holmes
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

#ifndef META_COMPOSITOR_PRIVATE_H
#define META_COMPOSITOR_PRIVATE_H

#include <X11/extensions/Xfixes.h>
#include "meta-compositor.h"
#include "meta-surface.h"

G_BEGIN_DECLS

struct _MetaCompositorClass
{
  GObjectClass parent_class;

  gboolean      (* manage)                 (MetaCompositor  *compositor,
                                            GError         **error);

  MetaSurface * (* add_window)             (MetaCompositor  *compositor,
                                            MetaWindow      *window);

  void          (* process_event)          (MetaCompositor  *compositor,
                                            XEvent          *event,
                                            MetaWindow      *window);

  void          (* sync_screen_size)       (MetaCompositor  *compositor);

  void          (* sync_window_geometry)   (MetaCompositor  *compositor,
                                            MetaSurface     *surface);

  gboolean      (* ready_to_redraw)        (MetaCompositor  *compositor);

  void          (* pre_paint)              (MetaCompositor  *compositor);

  void          (* redraw)                 (MetaCompositor  *compositor,
                                            XserverRegion    all_damage);
};

void         meta_compositor_set_composited          (MetaCompositor  *compositor,
                                                      gboolean         composited);

gboolean     meta_compositor_check_common_extensions (MetaCompositor  *compositor,
                                                      GError         **error);

gboolean     meta_compositor_set_selection           (MetaCompositor  *compositor,
                                                      GError         **error);

Window       meta_compositor_get_overlay_window      (MetaCompositor  *compositor);

gboolean     meta_compositor_redirect_windows        (MetaCompositor  *compositor,
                                                      GError         **error);

MetaDisplay *meta_compositor_get_display             (MetaCompositor  *compositor);

GList       *meta_compositor_get_stack               (MetaCompositor  *compositor);

void         meta_compositor_add_damage              (MetaCompositor  *compositor,
                                                      const gchar     *name,
                                                      XserverRegion    damage);

void         meta_compositor_damage_screen           (MetaCompositor  *compositor);

void         meta_compositor_queue_redraw            (MetaCompositor  *compositor);

G_END_DECLS

#endif
