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

#include "config.h"
#include "meta-surface-vulkan.h"

struct _MetaSurfaceVulkan
{
  MetaSurface parent;
};

G_DEFINE_TYPE (MetaSurfaceVulkan, meta_surface_vulkan, META_TYPE_SURFACE)

static void
meta_surface_vulkan_pre_paint (MetaSurface *surface)
{
}

static void
meta_surface_vulkan_class_init (MetaSurfaceVulkanClass *self_class)
{
  MetaSurfaceClass *surface_class;

  surface_class = META_SURFACE_CLASS (self_class);

  surface_class->pre_paint = meta_surface_vulkan_pre_paint;
}

static void
meta_surface_vulkan_init (MetaSurfaceVulkan *self)
{
}