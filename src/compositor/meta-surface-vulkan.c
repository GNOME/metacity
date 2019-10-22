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

static cairo_surface_t *
meta_surface_vulkan_get_image (MetaSurface *surface)
{
  return NULL;
}

static gboolean
meta_surface_vulkan_is_visible (MetaSurface *surface)
{
  return FALSE;
}

static void
meta_surface_vulkan_show (MetaSurface *surface)
{
}

static void
meta_surface_vulkan_hide (MetaSurface *surface)
{
}

static void
meta_surface_vulkan_opacity_changed (MetaSurface *surface)
{
}

static void
meta_surface_vulkan_sync_geometry (MetaSurface   *surface,
                                   MetaRectangle  old_geometry,
                                   gboolean       position_changed,
                                   gboolean       size_changed)
{
}

static void
meta_surface_vulkan_free_pixmap (MetaSurface *surface)
{
}

static void
meta_surface_vulkan_pre_paint (MetaSurface   *surface,
                               XserverRegion  damage)
{
}

static void
meta_surface_vulkan_class_init (MetaSurfaceVulkanClass *self_class)
{
  MetaSurfaceClass *surface_class;

  surface_class = META_SURFACE_CLASS (self_class);

  surface_class->get_image = meta_surface_vulkan_get_image;
  surface_class->is_visible = meta_surface_vulkan_is_visible;
  surface_class->show = meta_surface_vulkan_show;
  surface_class->hide = meta_surface_vulkan_hide;
  surface_class->opacity_changed = meta_surface_vulkan_opacity_changed;
  surface_class->sync_geometry = meta_surface_vulkan_sync_geometry;
  surface_class->free_pixmap = meta_surface_vulkan_free_pixmap;
  surface_class->pre_paint = meta_surface_vulkan_pre_paint;
}

static void
meta_surface_vulkan_init (MetaSurfaceVulkan *self)
{
}
