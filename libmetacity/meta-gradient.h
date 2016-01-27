/*
 * Copyright (C) 2001 Havoc Pennington, 99% copied from wrlib in
 * WindowMaker, Copyright (C) 1997-2000 Dan Pascu and Alfredo Kojima
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef META_GRADIENT_H
#define META_GRADIENT_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>

/**
 * MetaGradientType:
 * @META_GRADIENT_VERTICAL: Vertical gradient
 * @META_GRADIENT_HORIZONTAL: Horizontal gradient
 * @META_GRADIENT_DIAGONAL: Diagonal gradient
 * @META_GRADIENT_LAST: Marks the end of the #MetaGradientType enumeration
 *
 */
typedef enum
{
  META_GRADIENT_VERTICAL,
  META_GRADIENT_HORIZONTAL,
  META_GRADIENT_DIAGONAL,
  META_GRADIENT_LAST
} MetaGradientType;

GdkPixbuf *meta_gradient_create_simple     (int               width,
                                            int               height,
                                            const GdkRGBA    *from,
                                            const GdkRGBA    *to,
                                            MetaGradientType  style);

GdkPixbuf *meta_gradient_create_multi      (int               width,
                                            int               height,
                                            const GdkRGBA    *colors,
                                            int               n_colors,
                                            MetaGradientType  style);

GdkPixbuf *meta_gradient_create_interwoven (int               width,
                                            int               height,
                                            const GdkRGBA     colors1[2],
                                            int               thickness1,
                                            const GdkRGBA     colors2[2],
                                            int               thickness2);

void       meta_gradient_add_alpha         (GdkPixbuf        *pixbuf,
                                            const guchar     *alphas,
                                            int               n_alphas,
                                            MetaGradientType  type);
#endif
