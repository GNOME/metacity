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

#ifndef META_GRADIENT_SPEC_H
#define META_GRADIENT_SPEC_H

#include <gtk/gtk.h>
#include <libmetacity/meta-gradient.h>

G_BEGIN_DECLS

typedef struct _MetaAlphaGradientSpec MetaAlphaGradientSpec;
typedef struct _MetaGradientSpec MetaGradientSpec;

MetaGradientSpec      *meta_gradient_spec_new               (MetaGradientType         type);

void                   meta_gradient_spec_free              (MetaGradientSpec        *spec);

void                   meta_gradient_spec_add_color_spec    (MetaGradientSpec        *spec,
                                                             MetaColorSpec           *color_spec);

GdkPixbuf             *meta_gradient_spec_render            (const MetaGradientSpec  *spec,
                                                             GtkStyleContext         *context,
                                                             gint                     width,
                                                             gint                     height);

gboolean               meta_gradient_spec_validate          (MetaGradientSpec        *spec,
                                                             GError                 **error);

MetaAlphaGradientSpec *meta_alpha_gradient_spec_new         (MetaGradientType         type,
                                                             gint                     n_alphas);

void                   meta_alpha_gradient_spec_free        (MetaAlphaGradientSpec   *spec);

void                   meta_alpha_gradient_spec_add_alpha   (MetaAlphaGradientSpec   *spec,
                                                             gint                     n_alpha,
                                                             gdouble                  alpha);

guchar                 meta_alpha_gradient_spec_get_alpha   (MetaAlphaGradientSpec   *spec,
                                                             gint                     n_alpha);

gboolean               meta_alpha_gradient_spec_needs_alpha (MetaAlphaGradientSpec   *spec);

GdkPixbuf             *meta_alpha_gradient_spec_apply_alpha (MetaAlphaGradientSpec   *spec,
                                                             GdkPixbuf               *pixbuf,
                                                             gboolean                 force_copy);

GdkPixbuf             *meta_alpha_gradient_spec_render      (MetaAlphaGradientSpec   *spec,
                                                             gint                     width,
                                                             gint                     height,
                                                             GdkRGBA                  color);

G_END_DECLS

#endif
