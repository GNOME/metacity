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

#ifndef META_SURFACE_VULKAN_H
#define META_SURFACE_VULKAN_H

#include "meta-surface-private.h"

G_BEGIN_DECLS

#define META_TYPE_SURFACE_VULKAN (meta_surface_vulkan_get_type ())
G_DECLARE_FINAL_TYPE (MetaSurfaceVulkan, meta_surface_vulkan,
                      META, SURFACE_VULKAN, MetaSurface)

G_END_DECLS

#endif
