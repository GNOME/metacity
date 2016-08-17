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

#ifndef META_FRAME_STYLE_PRIVATE_H
#define META_FRAME_STYLE_PRIVATE_H

#include "meta-button.h"
#include "meta-button-private.h"
#include "meta-color-spec-private.h"
#include "meta-draw-op-private.h"
#include "meta-frame-enums.h"
#include "meta-frame-layout-private.h"

G_BEGIN_DECLS

typedef struct _MetaFrameStyle MetaFrameStyle;
typedef struct _MetaFrameStyleSet MetaFrameStyleSet;

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
  MetaDrawOpList *buttons[META_BUTTON_FUNCTION_LAST][META_BUTTON_STATE_LAST];
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

G_GNUC_INTERNAL
MetaFrameStyle    *meta_frame_style_new           (MetaFrameStyle     *parent);

G_GNUC_INTERNAL
void               meta_frame_style_ref           (MetaFrameStyle     *style);

G_GNUC_INTERNAL
void               meta_frame_style_unref         (MetaFrameStyle     *style);

G_GNUC_INTERNAL
gboolean           meta_frame_style_validate      (MetaFrameStyle     *style,
                                                   guint               current_theme_version,
                                                   GError            **error);

G_GNUC_INTERNAL
MetaDrawOpList    *meta_frame_style_get_button    (MetaFrameStyle     *style,
                                                   MetaButtonFunction  function,
                                                   MetaButtonState     state);

G_GNUC_INTERNAL
MetaFrameStyleSet *meta_frame_style_set_new       (MetaFrameStyleSet  *parent);

G_GNUC_INTERNAL
void               meta_frame_style_set_ref       (MetaFrameStyleSet  *style_set);

G_GNUC_INTERNAL
void               meta_frame_style_set_unref     (MetaFrameStyleSet  *style_set);

G_GNUC_INTERNAL
gboolean           meta_frame_style_set_validate  (MetaFrameStyleSet  *style_set,
                                                   GError            **error);

G_GNUC_INTERNAL
MetaFrameStyle    *meta_frame_style_set_get_style (MetaFrameStyleSet  *style_set,
                                                   MetaFrameState      state,
                                                   MetaFrameResize     resize,
                                                   MetaFrameFocus      focus);

G_END_DECLS

#endif
