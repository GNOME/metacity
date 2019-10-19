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

#ifndef META_SURFACE_H
#define META_SURFACE_H

#include <X11/extensions/Xdamage.h>
#include "meta-compositor.h"
#include "window.h"

G_BEGIN_DECLS

#define META_TYPE_SURFACE (meta_surface_get_type ())
G_DECLARE_DERIVABLE_TYPE (MetaSurface, meta_surface, META, SURFACE, GObject)

MetaCompositor  *meta_surface_get_compositor        (MetaSurface        *self);

MetaWindow      *meta_surface_get_window            (MetaSurface        *self);

Pixmap           meta_surface_get_pixmap            (MetaSurface        *self);

int              meta_surface_get_x                 (MetaSurface        *self);

int              meta_surface_get_y                 (MetaSurface        *self);

int              meta_surface_get_width             (MetaSurface        *self);

int              meta_surface_get_height            (MetaSurface        *self);

XserverRegion    meta_surface_get_opaque_region     (MetaSurface        *self);

XserverRegion    meta_surface_get_shape_region      (MetaSurface        *self);

cairo_surface_t *meta_surface_get_image             (MetaSurface        *self);

gboolean         meta_surface_has_shadow            (MetaSurface        *self);

gboolean         meta_surface_is_opaque             (MetaSurface        *self);

gboolean         meta_surface_is_visible            (MetaSurface        *self);

void             meta_surface_show                  (MetaSurface        *self);

void             meta_surface_hide                  (MetaSurface        *self);

void             meta_surface_process_damage        (MetaSurface        *self,
                                                     XDamageNotifyEvent *event);

void             meta_surface_opacity_changed       (MetaSurface        *self);

void             meta_surface_opaque_region_changed (MetaSurface        *self);

void             meta_surface_shape_region_changed  (MetaSurface        *self);

void             meta_surface_sync_geometry         (MetaSurface        *self);

void             meta_surface_pre_paint             (MetaSurface        *self);

G_END_DECLS

#endif
