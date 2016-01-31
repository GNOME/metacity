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

#include <libmetacity/meta-button-enums.h>
#include "meta-theme-impl.h"

G_BEGIN_DECLS

typedef struct _MetaDrawOpList MetaDrawOpList;
typedef struct _MetaFrameLayout MetaFrameLayout;
typedef struct _MetaFrameStyle MetaFrameStyle;
typedef struct _MetaFrameStyleSet MetaFrameStyleSet;

#define META_TYPE_THEME_METACITY meta_theme_metacity_get_type ()
G_DECLARE_FINAL_TYPE (MetaThemeMetacity, meta_theme_metacity,
                      META, THEME_METACITY, MetaThemeImpl)

gboolean           meta_theme_metacity_lookup_int          (MetaThemeMetacity  *metacity,
                                                            const gchar        *name,
                                                            gint               *value);

gboolean           meta_theme_metacity_lookup_float        (MetaThemeMetacity  *metacity,
                                                            const gchar        *name,
                                                            gdouble            *value);

MetaDrawOpList    *meta_theme_metacity_lookup_draw_op_list (MetaThemeMetacity  *metacity,
                                                            const gchar        *name);

MetaFrameLayout   *meta_theme_metacity_lookup_layout       (MetaThemeMetacity  *metacity,
                                                            const gchar        *name);

MetaFrameStyle    *meta_theme_metacity_lookup_style        (MetaThemeMetacity  *metacity,
                                                            const gchar        *name);

MetaFrameStyleSet *meta_theme_metacity_lookup_style_set    (MetaThemeMetacity  *metacity,
                                                            const gchar        *name);

const gchar       *meta_theme_metacity_get_name            (MetaThemeMetacity  *metacity);

const gchar       *meta_theme_metacity_get_readable_name   (MetaThemeMetacity  *metacity);

gboolean           meta_theme_metacity_allows_shade_stick_above_buttons (MetaThemeMetacity *metacity);

guint              meta_theme_metacity_earliest_version_with_button (MetaButtonType type);

G_END_DECLS

#endif
