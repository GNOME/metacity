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

#ifndef META_GRADIENT_SPEC_PRIVATE_H
#define META_GRADIENT_SPEC_PRIVATE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _MetaAlphaGradientSpec MetaAlphaGradientSpec;
typedef struct _MetaGradientSpec MetaGradientSpec;

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

G_GNUC_INTERNAL
MetaGradientSpec      *meta_gradient_spec_new               (MetaGradientType         type);

G_GNUC_INTERNAL
void                   meta_gradient_spec_free              (MetaGradientSpec        *spec);

G_GNUC_INTERNAL
void                   meta_gradient_spec_add_color_spec    (MetaGradientSpec        *spec,
                                                             MetaColorSpec           *color_spec);

G_GNUC_INTERNAL
void                   meta_gradient_spec_render            (const MetaGradientSpec       *spec,
                                                             const MetaAlphaGradientSpec  *alpha_spec,
                                                             cairo_t                      *cr,
                                                             GtkStyleContext              *context,
                                                             gdouble                       x,
                                                             gdouble                       y,
                                                             gdouble                       width,
                                                             gdouble                       height);

G_GNUC_INTERNAL
gboolean               meta_gradient_spec_validate          (MetaGradientSpec        *spec,
                                                             GError                 **error);

G_GNUC_INTERNAL
MetaAlphaGradientSpec *meta_alpha_gradient_spec_new         (MetaGradientType         type,
                                                             gint                     n_alphas);

G_GNUC_INTERNAL
void                   meta_alpha_gradient_spec_free        (MetaAlphaGradientSpec   *spec);

G_GNUC_INTERNAL
void                   meta_alpha_gradient_spec_add_alpha   (MetaAlphaGradientSpec   *spec,
                                                             gint                     n_alpha,
                                                             gdouble                  alpha);

G_GNUC_INTERNAL
guchar                 meta_alpha_gradient_spec_get_alpha   (MetaAlphaGradientSpec   *spec,
                                                             gint                     n_alpha);

G_GNUC_INTERNAL
void                   meta_alpha_gradient_spec_render      (MetaAlphaGradientSpec   *spec,
                                                             GdkRGBA                  color,
                                                             cairo_t                 *cr,
                                                             gdouble                  x,
                                                             gdouble                  y,
                                                             gdouble                  width,
                                                             gdouble                  height);

G_GNUC_INTERNAL
cairo_pattern_t       *meta_alpha_gradient_spec_get_mask    (const MetaAlphaGradientSpec  *spec);

G_END_DECLS

#endif
