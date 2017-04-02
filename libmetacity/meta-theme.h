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

#ifndef META_THEME_H
#define META_THEME_H

#include <gtk/gtk.h>
#include <libmetacity/meta-button.h>
#include <libmetacity/meta-frame-borders.h>
#include <libmetacity/meta-frame-enums.h>

G_BEGIN_DECLS

typedef struct _MetaFrameGeometry MetaFrameGeometry;

typedef MetaButtonState (* MetaButtonStateFunc) (MetaButtonType type,
                                                 GdkRectangle   rect,
                                                 gpointer       user_data);

#define META_TYPE_THEME meta_theme_get_type ()
G_DECLARE_FINAL_TYPE (MetaTheme, meta_theme, META, THEME, GObject)

/**
 * META_THEME_ERROR:
 *
 * Domain for #MetaThemeError errors.
 */
#define META_THEME_ERROR (meta_theme_error_quark ())

/**
 * MetaThemeError:
 * @META_THEME_ERROR_TOO_OLD:
 * @META_THEME_ERROR_FRAME_GEOMETRY:
 * @META_THEME_ERROR_BAD_CHARACTER:
 * @META_THEME_ERROR_BAD_PARENS:
 * @META_THEME_ERROR_UNKNOWN_VARIABLE:
 * @META_THEME_ERROR_DIVIDE_BY_ZERO:
 * @META_THEME_ERROR_MOD_ON_FLOAT:
 * @META_THEME_ERROR_FAILED:
 *
 * Error codes for %META_THEME_ERROR.
 */
typedef enum
{
  META_THEME_ERROR_TOO_OLD,
  META_THEME_ERROR_FRAME_GEOMETRY,
  META_THEME_ERROR_BAD_CHARACTER,
  META_THEME_ERROR_BAD_PARENS,
  META_THEME_ERROR_UNKNOWN_VARIABLE,
  META_THEME_ERROR_DIVIDE_BY_ZERO,
  META_THEME_ERROR_MOD_ON_FLOAT,
  META_THEME_ERROR_FAILED
} MetaThemeError;

/**
 * MetaThemeType:
 * @META_THEME_TYPE_GTK:
 * @META_THEME_TYPE_METACITY:
 *
 * Theme types.
 */
typedef enum
{
  META_THEME_TYPE_GTK,
  META_THEME_TYPE_METACITY,
} MetaThemeType;

/**
 * Calculated actual geometry of the frame
 */
struct _MetaFrameGeometry
{
  MetaFrameBorders borders;

  int width;
  int height;

  GdkRectangle title_rect;

  /* Round corners */
  guint top_left_corner_rounded_radius;
  guint top_right_corner_rounded_radius;
  guint bottom_left_corner_rounded_radius;
  guint bottom_right_corner_rounded_radius;
};

GQuark         meta_theme_error_quark       (void);

MetaTheme     *meta_theme_new               (MetaThemeType                type);

gboolean       meta_theme_load              (MetaTheme                   *theme,
                                             const gchar                 *theme_name,
                                             GError                     **error);

void           meta_theme_invalidate        (MetaTheme                   *theme);

void           meta_theme_set_button_layout (MetaTheme                   *theme,
                                             const gchar                 *button_layout,
                                             gboolean                     invert);

MetaButton    *meta_theme_get_button        (MetaTheme                   *theme,
                                             gint                         x,
                                             gint                         y);

MetaButton   **meta_theme_get_buttons       (MetaTheme                   *theme);

void           meta_theme_set_composited    (MetaTheme                   *theme,
                                             gboolean                     composited);

void           meta_theme_set_scale         (MetaTheme                   *theme,
                                             gint                         scale);

void           meta_theme_set_dpi           (MetaTheme                   *theme,
                                             gdouble                      dpi);

void           meta_theme_set_titlebar_font (MetaTheme                   *theme,
                                             const PangoFontDescription  *titlebar_font);

void           meta_theme_get_frame_borders (MetaTheme                   *theme,
                                             const gchar                 *variant,
                                             MetaFrameType                type,
                                             MetaFrameFlags               flags,
                                             MetaFrameBorders            *borders);

void           meta_theme_calc_geometry     (MetaTheme                   *theme,
                                             const gchar                 *variant,
                                             MetaFrameType                type,
                                             MetaFrameFlags               flags,
                                             gint                         client_width,
                                             gint                         client_height,
                                             MetaFrameGeometry           *fgeom);

void           meta_theme_draw_frame        (MetaTheme                   *theme,
                                             const gchar                 *variant,
                                             cairo_t                     *cr,
                                             MetaFrameType                type,
                                             MetaFrameFlags               flags,
                                             gint                         client_width,
                                             gint                         client_height,
                                             const gchar                 *title,
                                             MetaButtonStateFunc          func,
                                             gpointer                     user_data,
                                             GdkPixbuf                   *mini_icon,
                                             GdkPixbuf                   *icon);

G_END_DECLS

#endif
