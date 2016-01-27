/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2016 Alberts Muktupāvels
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

#ifndef META_HSLA_PRIVATE_H
#define META_HSLA_PRIVATE_H

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct
{
  gdouble hue;
  gdouble saturation;
  gdouble lightness;
  gdouble alpha;
} MetaHSLA;

G_GNUC_INTERNAL
void meta_hsla_from_rgba (MetaHSLA       *hsla,
                          const GdkRGBA  *rgba);

G_GNUC_INTERNAL
void meta_hsla_to_rgba   (const MetaHSLA *hsla,
                          GdkRGBA        *rgba);

G_GNUC_INTERNAL
void meta_hsla_shade     (const MetaHSLA *source,
                          const gdouble   factor,
                          MetaHSLA       *destination);

G_END_DECLS

#endif
