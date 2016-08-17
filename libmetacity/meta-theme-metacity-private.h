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

#ifndef META_THEME_METACITY_PRIVATE_H
#define META_THEME_METACITY_PRIVATE_H

#include "meta-button-private.h"
#include "meta-theme-impl-private.h"

G_BEGIN_DECLS

typedef struct _MetaDrawOpList MetaDrawOpList;
typedef struct _MetaFrameLayout MetaFrameLayout;
typedef struct _MetaFrameStyle MetaFrameStyle;
typedef struct _MetaFrameStyleSet MetaFrameStyleSet;

G_GNUC_INTERNAL
#define META_TYPE_THEME_METACITY meta_theme_metacity_get_type ()
G_DECLARE_FINAL_TYPE (MetaThemeMetacity, meta_theme_metacity,
                      META, THEME_METACITY, MetaThemeImpl)

G_GNUC_INTERNAL
gboolean           meta_theme_metacity_lookup_int          (MetaThemeMetacity  *metacity,
                                                            const gchar        *name,
                                                            gint               *value);

G_GNUC_INTERNAL
gboolean           meta_theme_metacity_lookup_float        (MetaThemeMetacity  *metacity,
                                                            const gchar        *name,
                                                            gdouble            *value);

G_GNUC_INTERNAL
MetaDrawOpList    *meta_theme_metacity_lookup_draw_op_list (MetaThemeMetacity  *metacity,
                                                            const gchar        *name);

G_GNUC_INTERNAL
MetaFrameLayout   *meta_theme_metacity_lookup_layout       (MetaThemeMetacity  *metacity,
                                                            const gchar        *name);

G_GNUC_INTERNAL
MetaFrameStyle    *meta_theme_metacity_lookup_style        (MetaThemeMetacity  *metacity,
                                                            const gchar        *name);

G_GNUC_INTERNAL
MetaFrameStyleSet *meta_theme_metacity_lookup_style_set    (MetaThemeMetacity  *metacity,
                                                            const gchar        *name);

G_GNUC_INTERNAL
guint              meta_theme_metacity_earliest_version_with_button (MetaButtonFunction function);

G_END_DECLS

#endif
