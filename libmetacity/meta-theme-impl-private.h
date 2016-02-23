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

#include "meta-button-layout.h"
#include "meta-frame-borders.h"
#include "meta-frame-enums.h"
#include "meta-frame-style.h"
#include "meta-style-info-private.h"

G_BEGIN_DECLS

typedef struct _MetaFrameGeometry MetaFrameGeometry;

#define META_TYPE_THEME_IMPL meta_theme_impl_get_type ()
G_DECLARE_DERIVABLE_TYPE (MetaThemeImpl, meta_theme_impl,
                          META, THEME_IMPL, GObject)

struct _MetaThemeImplClass
{
  GObjectClass parent_class;

  gboolean   (* load)              (MetaThemeImpl            *impl,
                                    const gchar              *name,
                                    GError                  **error);

  void       (* get_frame_borders) (MetaThemeImpl            *impl,
                                    MetaFrameLayout          *layout,
                                    MetaStyleInfo            *style_info,
                                    gboolean                  composited,
                                    gint                      text_height,
                                    MetaFrameFlags            flags,
                                    MetaFrameType             type,
                                    MetaFrameBorders         *borders);

  void       (* calc_geometry)     (MetaThemeImpl            *impl,
                                    MetaFrameLayout          *layout,
                                    MetaStyleInfo            *style_info,
                                    gboolean                  composited,
                                    gint                      text_height,
                                    MetaFrameFlags            flags,
                                    gint                      client_width,
                                    gint                      client_height,
                                    const MetaButtonLayout   *button_layout,
                                    MetaFrameType             type,
                                    MetaFrameGeometry        *fgeom);

  void       (* draw_frame)        (MetaThemeImpl            *impl,
                                    MetaFrameStyle           *style,
                                    MetaStyleInfo            *style_info,
                                    cairo_t                  *cr,
                                    const MetaFrameGeometry  *fgeom,
                                    PangoLayout              *title_layout,
                                    MetaFrameFlags            flags,
                                    MetaButtonState           button_states[META_BUTTON_TYPE_LAST],
                                    GdkPixbuf                *mini_icon,
                                    GdkPixbuf                *icon);
};

G_GNUC_INTERNAL
void               meta_theme_impl_add_style_set (MetaThemeImpl           *impl,
                                                  MetaFrameType            type,
                                                  MetaFrameStyleSet       *style_set);

G_GNUC_INTERNAL
MetaFrameStyleSet *meta_theme_impl_get_style_set (MetaThemeImpl           *impl,
                                                  MetaFrameType            type);

G_GNUC_INTERNAL
void               get_button_rect               (MetaButtonType           type,
                                                  const MetaFrameGeometry *fgeom,
                                                  gint                     middle_background_offset,
                                                  GdkRectangle            *rect);

G_GNUC_INTERNAL
MetaButtonState    map_button_state              (MetaButtonType           button_type,
                                                  const MetaFrameGeometry *fgeom,
                                                  gint                     middle_bg_offset,
                                                  MetaButtonState          button_states[META_BUTTON_TYPE_LAST]);

G_END_DECLS

#endif
