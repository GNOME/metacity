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

#ifndef META_THEME_METACITY_H
#define META_THEME_METACITY_H

#include "meta-theme-impl.h"

G_BEGIN_DECLS

typedef struct _MetaDrawOpList MetaDrawOpList;
typedef struct _MetaFrameLayout MetaFrameLayout;
typedef struct _MetaFrameStyle MetaFrameStyle;
typedef struct _MetaFrameStyleSet MetaFrameStyleSet;

#define META_TYPE_THEME_METACITY meta_theme_metacity_get_type ()
G_DECLARE_FINAL_TYPE (MetaThemeMetacity, meta_theme_metacity,
                      META, THEME_METACITY, MetaThemeImpl)

gboolean           meta_theme_metacity_define_int          (MetaThemeMetacity  *metacity,
                                                            const gchar        *name,
                                                            gint                value,
                                                            GError            **error);

gboolean           meta_theme_metacity_lookup_int          (MetaThemeMetacity  *metacity,
                                                            const gchar        *name,
                                                            gint               *value);

gboolean           meta_theme_metacity_define_float        (MetaThemeMetacity  *metacity,
                                                            const gchar        *name,
                                                            gdouble             value,
                                                            GError            **error);

gboolean           meta_theme_metacity_lookup_float        (MetaThemeMetacity  *metacity,
                                                            const gchar        *name,
                                                            gdouble            *value);

gboolean           meta_theme_metacity_define_color        (MetaThemeMetacity  *metacity,
                                                            const gchar        *name,
                                                            const gchar        *value,
                                                            GError            **error);

gboolean           meta_theme_metacity_lookup_color        (MetaThemeMetacity  *metacity,
                                                            const gchar        *name,
                                                            gchar             **value);

MetaDrawOpList    *meta_theme_metacity_lookup_draw_op_list (MetaThemeMetacity  *metacity,
                                                            const gchar        *name);

void               meta_theme_metacity_insert_draw_op_list (MetaThemeMetacity  *metacity,
                                                            const gchar        *name,
                                                            MetaDrawOpList     *op_list);

MetaFrameLayout   *meta_theme_metacity_lookup_layout       (MetaThemeMetacity  *metacity,
                                                            const gchar        *name);

void               meta_theme_metacity_insert_layout       (MetaThemeMetacity  *metacity,
                                                            const gchar        *name,
                                                            MetaFrameLayout    *layout);

MetaFrameStyle    *meta_theme_metacity_lookup_style        (MetaThemeMetacity  *metacity,
                                                            const gchar        *name);

void               meta_theme_metacity_insert_style        (MetaThemeMetacity  *metacity,
                                                            const gchar        *name,
                                                            MetaFrameStyle     *style);

MetaFrameStyleSet *meta_theme_metacity_lookup_style_set    (MetaThemeMetacity  *metacity,
                                                            const gchar        *name);

void               meta_theme_metacity_insert_style_set    (MetaThemeMetacity  *metacity,
                                                            const gchar        *name,
                                                            MetaFrameStyleSet  *style_set);

G_END_DECLS

#endif
