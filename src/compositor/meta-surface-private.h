/*
 * Copyright (C) 2019 Alberts Muktupāvels
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

#ifndef META_SURFACE_PRIVATE_H
#define META_SURFACE_PRIVATE_H

#include "meta-surface.h"

G_BEGIN_DECLS

struct _MetaSurfaceClass
{
  GObjectClass parent_class;

  cairo_surface_t * (* get_image)       (MetaSurface   *self);

  gboolean          (* is_visible)      (MetaSurface   *self);

  void              (* show)            (MetaSurface   *self);

  void              (* hide)            (MetaSurface   *self);

  void              (* opacity_changed) (MetaSurface   *self);

  void              (* sync_geometry)   (MetaSurface   *self,
                                         MetaRectangle  old_geometry,
                                         gboolean       position_changed,
                                         gboolean       size_changed);

  void              (* free_pixmap)     (MetaSurface   *self);

  void              (* pre_paint)       (MetaSurface   *self,
                                         XserverRegion  damage);
};

G_END_DECLS

#endif
