/*
 * Copyright (C) 2001 Havoc Pennington
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef META_THEME_PRIVATE_H
#define META_THEME_PRIVATE_H

#include <libmetacity/meta-frame-style.h>
#include <libmetacity/meta-theme-impl.h>

#include "theme.h"

G_BEGIN_DECLS

/**
 * A theme. This is a singleton class which groups all settings from a theme
 * on disk together.
 *
 * \bug It is rather useless to keep the metadata fields in core, I think.
 */
struct _MetaTheme
{
  /** Name of the theme (on disk), e.g. "Crux" */
  char *name;
  /** Path to the files associated with the theme */
  char *dirname;
  /**
   * Filename of the XML theme file.
   * \bug Kept lying around for no discernable reason.
   */
  char *filename;
  /** Metadata: Human-readable name of the theme. */
  char *readable_name;
  /** Metadata: Author of the theme. */
  char *author;
  /** Metadata: Copyright holder. */
  char *copyright;
  /** Metadata: Date of the theme. */
  char *date;
  /** Metadata: Description of the theme. */
  char *description;
  /** Version of the theme format. Older versions cannot use the features
   * of newer versions even if they think they can (this is to allow forward
   * and backward compatibility.
   */
  guint format_version;

  gboolean is_gtk_theme;

  gboolean composited;

  PangoFontDescription *titlebar_font;

  GHashTable *images_by_filename;

  MetaFrameStyleSet *style_sets_by_type[META_FRAME_TYPE_LAST];

  MetaThemeImpl *impl;
};

MetaFrameStyle        *meta_theme_get_frame_style              (MetaTheme                   *theme,
                                                                MetaFrameType                type,
                                                                MetaFrameFlags               flags);

PangoFontDescription  *meta_style_info_create_font_desc        (MetaTheme                   *theme,
                                                                MetaStyleInfo               *style_info);

PangoFontDescription  *meta_gtk_widget_get_font_desc           (GtkWidget                   *widget,
                                                                double                       scale,
                                                                const PangoFontDescription  *override);

int                    meta_pango_font_desc_get_text_height    (const PangoFontDescription  *font_desc,
                                                                PangoContext                *context);

guint                  meta_theme_earliest_version_with_button (MetaButtonType               type);

#define META_THEME_ALLOWS(theme, feature) (theme->format_version >= feature)

/* What version of the theme file format were various features introduced in? */
#define META_THEME_SHADE_STICK_ABOVE_BUTTONS 2
#define META_THEME_UBIQUITOUS_CONSTANTS 2
#define META_THEME_VARIED_ROUND_CORNERS 2
#define META_THEME_IMAGES_FROM_ICON_THEMES 2
#define META_THEME_UNRESIZABLE_SHADED_STYLES 2
#define META_THEME_DEGREES_IN_ARCS 2
#define META_THEME_HIDDEN_BUTTONS 2
#define META_THEME_COLOR_CONSTANTS 2
#define META_THEME_FRAME_BACKGROUNDS 2

G_END_DECLS

#endif
