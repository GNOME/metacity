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

#include <libmetacity/meta-color-spec.h>
#include <libmetacity/meta-draw-op.h>
#include <libmetacity/meta-frame-layout.h>
#include <libmetacity/meta-theme-impl.h>

#include "theme.h"

G_BEGIN_DECLS

typedef struct _MetaFrameStyle MetaFrameStyle;
typedef struct _MetaFrameStyleSet MetaFrameStyleSet;

typedef enum
{
  /* Listed in the order in which the textures are drawn.
   * (though this only matters for overlaps of course.)
   * Buttons are drawn after the frame textures.
   *
   * On the corners, horizontal pieces are arbitrarily given the
   * corner area:
   *
   *   =====                 |====
   *   |                     |
   *   |       rather than   |
   *
   */

  /* entire frame */
  META_FRAME_PIECE_ENTIRE_BACKGROUND,
  /* entire titlebar background */
  META_FRAME_PIECE_TITLEBAR,
  /* portion of the titlebar background inside the titlebar
   * background edges
   */
  META_FRAME_PIECE_TITLEBAR_MIDDLE,
  /* left end of titlebar */
  META_FRAME_PIECE_LEFT_TITLEBAR_EDGE,
  /* right end of titlebar */
  META_FRAME_PIECE_RIGHT_TITLEBAR_EDGE,
  /* top edge of titlebar */
  META_FRAME_PIECE_TOP_TITLEBAR_EDGE,
  /* bottom edge of titlebar */
  META_FRAME_PIECE_BOTTOM_TITLEBAR_EDGE,
  /* render over title background (text area) */
  META_FRAME_PIECE_TITLE,
  /* left edge of the frame */
  META_FRAME_PIECE_LEFT_EDGE,
  /* right edge of the frame */
  META_FRAME_PIECE_RIGHT_EDGE,
  /* bottom edge of the frame */
  META_FRAME_PIECE_BOTTOM_EDGE,
  /* place over entire frame, after drawing everything else */
  META_FRAME_PIECE_OVERLAY,
  /* Used to get size of the enum */
  META_FRAME_PIECE_LAST
} MetaFramePiece;

/* Kinds of frame...
 *
 *  normal ->   noresize / vert only / horz only / both
 *              focused / unfocused
 *  max    ->   focused / unfocused
 *  shaded ->   focused / unfocused
 *  max/shaded -> focused / unfocused
 *
 *  so 4 states with 8 sub-states in one, 2 sub-states in the other 3,
 *  meaning 14 total
 *
 * 14 window states times 7 or 8 window types. Except some
 * window types never get a frame so that narrows it down a bit.
 *
 */
typedef enum
{
  META_FRAME_STATE_NORMAL,
  META_FRAME_STATE_MAXIMIZED,
  META_FRAME_STATE_TILED_LEFT,
  META_FRAME_STATE_TILED_RIGHT,
  META_FRAME_STATE_SHADED,
  META_FRAME_STATE_MAXIMIZED_AND_SHADED,
  META_FRAME_STATE_TILED_LEFT_AND_SHADED,
  META_FRAME_STATE_TILED_RIGHT_AND_SHADED,
  META_FRAME_STATE_LAST
} MetaFrameState;

typedef enum
{
  META_FRAME_RESIZE_NONE,
  META_FRAME_RESIZE_VERTICAL,
  META_FRAME_RESIZE_HORIZONTAL,
  META_FRAME_RESIZE_BOTH,
  META_FRAME_RESIZE_LAST
} MetaFrameResize;

typedef enum
{
  META_FRAME_FOCUS_NO,
  META_FRAME_FOCUS_YES,
  META_FRAME_FOCUS_LAST
} MetaFrameFocus;

/**
 * How to draw a frame in a particular state (say, a focussed, non-maximised,
 * resizable frame). This corresponds closely to the <frame_style> tag
 * in a theme file.
 */
struct _MetaFrameStyle
{
  /** Reference count. */
  int refcount;
  /**
   * Parent style.
   * Settings which are unspecified here will be taken from there.
   */
  MetaFrameStyle *parent;
  /** Operations for drawing each kind of button in each state. */
  MetaDrawOpList *buttons[META_BUTTON_TYPE_LAST][META_BUTTON_STATE_LAST];
  /** Operations for drawing each piece of the frame. */
  MetaDrawOpList *pieces[META_FRAME_PIECE_LAST];
  /**
   * Details such as the height and width of each edge, the corner rounding,
   * and the aspect ratio of the buttons.
   */
  MetaFrameLayout *layout;
  /**
   * Background colour of the window. Only present in theme formats
   * 2 and above. Can be NULL to use the standard GTK theme engine.
   */
  MetaColorSpec *window_background_color;
  /**
   * Transparency of the window background. 0=transparent; 255=opaque.
   */
  guint8 window_background_alpha;
};

/**
 * How to draw frames at different times: when it's maximised or not, shaded
 * or not, when it's focussed or not, and (for non-maximised windows), when
 * it can be horizontally or vertically resized, both, or neither.
 * Not all window types actually get a frame.
 *
 * A theme contains one of these objects for each type of window (each
 * MetaFrameType), that is, normal, dialogue (modal and non-modal), etc.
 *
 * This corresponds closely to the <frame_style_set> tag in a theme file.
 */
struct _MetaFrameStyleSet
{
  int refcount;
  MetaFrameStyleSet *parent;
  MetaFrameStyle *normal_styles[META_FRAME_RESIZE_LAST][META_FRAME_FOCUS_LAST];
  MetaFrameStyle *maximized_styles[META_FRAME_FOCUS_LAST];
  MetaFrameStyle *tiled_left_styles[META_FRAME_FOCUS_LAST];
  MetaFrameStyle *tiled_right_styles[META_FRAME_FOCUS_LAST];
  MetaFrameStyle *shaded_styles[META_FRAME_RESIZE_LAST][META_FRAME_FOCUS_LAST];
  MetaFrameStyle *maximized_and_shaded_styles[META_FRAME_FOCUS_LAST];
  MetaFrameStyle *tiled_left_and_shaded_styles[META_FRAME_FOCUS_LAST];
  MetaFrameStyle *tiled_right_and_shaded_styles[META_FRAME_FOCUS_LAST];
};

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
  GHashTable *layouts_by_name;
  GHashTable *styles_by_name;
  GHashTable *style_sets_by_name;

  MetaFrameStyleSet *style_sets_by_type[META_FRAME_TYPE_LAST];

  MetaThemeImpl *impl;
};

MetaFrameStyle        *meta_frame_style_new                    (MetaFrameStyle              *parent);
void                   meta_frame_style_ref                    (MetaFrameStyle              *style);
void                   meta_frame_style_unref                  (MetaFrameStyle              *style);

void                   meta_frame_style_apply_scale            (const MetaFrameStyle        *style,
                                                                PangoFontDescription        *font_desc);

gboolean               meta_frame_style_validate               (MetaFrameStyle              *style,
                                                                guint                        current_theme_version,
                                                                GError                     **error);

MetaFrameStyleSet     *meta_frame_style_set_new                (MetaFrameStyleSet           *parent);
void                   meta_frame_style_set_ref                (MetaFrameStyleSet           *style_set);
void                   meta_frame_style_set_unref              (MetaFrameStyleSet           *style_set);

gboolean               meta_frame_style_set_validate           (MetaFrameStyleSet           *style_set,
                                                                GError                     **error);

MetaFrameStyle        *meta_theme_get_frame_style              (MetaTheme                   *theme,
                                                                MetaFrameType                type,
                                                                MetaFrameFlags               flags);

PangoFontDescription  *meta_style_info_create_font_desc        (MetaTheme                   *theme,
                                                                MetaStyleInfo               *style_info);

MetaFrameLayout       *meta_theme_lookup_layout                (MetaTheme                   *theme,
                                                                const char                  *name);
void                   meta_theme_insert_layout                (MetaTheme                   *theme,
                                                                const char                  *name,
                                                                MetaFrameLayout             *layout);
MetaFrameStyle        *meta_theme_lookup_style                 (MetaTheme                   *theme,
                                                                const char                  *name);
void                   meta_theme_insert_style                 (MetaTheme                   *theme,
                                                                const char                  *name,
                                                                MetaFrameStyle              *style);
MetaFrameStyleSet     *meta_theme_lookup_style_set             (MetaTheme                   *theme,
                                                                const char                  *name);
void                   meta_theme_insert_style_set             (MetaTheme                   *theme,
                                                                const char                  *name,
                                                                MetaFrameStyleSet           *style_set);

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
