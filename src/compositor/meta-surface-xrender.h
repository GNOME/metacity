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

#ifndef META_SURFACE_XRENDER_H
#define META_SURFACE_XRENDER_H

#include <X11/extensions/Xrender.h>
#include "meta-surface-private.h"

G_BEGIN_DECLS

#define META_TYPE_SURFACE_XRENDER (meta_surface_xrender_get_type ())
G_DECLARE_FINAL_TYPE (MetaSurfaceXRender, meta_surface_xrender,
                      META, SURFACE_XRENDER, MetaSurface)

void meta_surface_xrender_update_shadow (MetaSurfaceXRender *self);

void meta_surface_xrender_paint_shadow  (MetaSurfaceXRender *self,
                                         XserverRegion       paint_region,
                                         Picture             paint_buffer);

void meta_surface_xrender_paint         (MetaSurfaceXRender *self,
                                         XserverRegion       paint_region,
                                         Picture             paint_buffer,
                                         gboolean            opaque);

G_END_DECLS

#endif
