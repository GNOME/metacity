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

#ifndef META_SHADOW_XRENDER_H
#define META_SHADOW_XRENDER_H

#include <glib-object.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrender.h>

G_BEGIN_DECLS

typedef struct
{
  Display       *xdisplay;

  int            dx;
  int            dy;
  int            width;
  int            height;

  Picture        black;
  Picture        shadow;

  XserverRegion  region;
} MetaShadowXRender;

void          meta_shadow_xrender_free       (MetaShadowXRender *self);

XserverRegion meta_shadow_xrender_get_region (MetaShadowXRender *self);

void          meta_shadow_xrender_paint      (MetaShadowXRender *self,
                                              XserverRegion      paint_region,
                                              Picture            paint_buffer,
                                              int                x,
                                              int                y);

G_END_DECLS

#endif
