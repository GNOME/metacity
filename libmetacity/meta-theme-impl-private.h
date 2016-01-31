/*
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

#ifndef META_THEME_IMPL_PRIVATE_H
#define META_THEME_IMPL_PRIVATE_H

#include <glib-object.h>

#include "meta-frame-enums.h"
#include "meta-frame-style.h"

G_BEGIN_DECLS

#define META_TYPE_THEME_IMPL meta_theme_impl_get_type ()
G_DECLARE_DERIVABLE_TYPE (MetaThemeImpl, meta_theme_impl,
                          META, THEME_IMPL, GObject)

struct _MetaThemeImplClass
{
  GObjectClass parent_class;

  gboolean   (* load)     (MetaThemeImpl  *impl,
                           const gchar    *name,
                           GError        **error);

  gchar    * (* get_name) (MetaThemeImpl  *impl);
};

void               meta_theme_impl_add_style_set (MetaThemeImpl      *impl,
                                                  MetaFrameType       type,
                                                  MetaFrameStyleSet  *style_set);

MetaFrameStyleSet *meta_theme_impl_get_style_set (MetaThemeImpl      *impl,
                                                  MetaFrameType       type);

G_END_DECLS

#endif
