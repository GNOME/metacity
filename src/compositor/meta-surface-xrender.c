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
#include "meta-surface-xrender.h"

struct _MetaSurfaceXRender
{
  MetaSurface parent;
};

G_DEFINE_TYPE (MetaSurfaceXRender, meta_surface_xrender, META_TYPE_SURFACE)

static void
meta_surface_xrender_pre_paint (MetaSurface *surface)
{
}

static void
meta_surface_xrender_class_init (MetaSurfaceXRenderClass *self_class)
{
  MetaSurfaceClass *surface_class;

  surface_class = META_SURFACE_CLASS (self_class);

  surface_class->pre_paint = meta_surface_xrender_pre_paint;
}

static void
meta_surface_xrender_init (MetaSurfaceXRender *self)
{
}
