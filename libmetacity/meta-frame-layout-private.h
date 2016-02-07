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

#ifndef META_FRAME_LAYOUT_PRIVATE_H
#define META_FRAME_LAYOUT_PRIVATE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _MetaFrameLayout MetaFrameLayout;

/**
 * Whether a button's size is calculated from the area around it (aspect
 * sizing) or is given as a fixed height and width in pixels (fixed sizing).
 *
 * \bug This could be done away with; see the comment at the top of
 * MetaFrameLayout.
 */
typedef enum
{
  META_BUTTON_SIZING_ASPECT,
  META_BUTTON_SIZING_FIXED,
  META_BUTTON_SIZING_LAST
} MetaButtonSizing;

/**
 * Various parameters used to calculate the geometry of a frame.
 * They are used inside a MetaFrameStyle.
 * This corresponds closely to the <frame_geometry> tag in a theme file.
 *
 * \bug button_sizing isn't really necessary, because we could easily say
 * that if button_aspect is zero, the height and width are fixed values.
 * This would also mean that MetaButtonSizing didn't need to exist, and
 * save code.
 **/
struct _MetaFrameLayout
{
  gint refcount;

  struct {
    /** Border/padding of the entire frame */
    GtkBorder frame_border;

    /** Shadow border used in invisible resize area */
    GtkBorder shadow_border;

    /** Border/padding of the titlebar region */
    GtkBorder titlebar_border;
    /** Border/padding of titlebar buttons */

    /** Size of images in buttons */
    guint icon_size;

    /** Space between titlebar elements */
    guint titlebar_spacing;

    /** Margin of title */
    GtkBorder title_margin;
    /** Margin of titlebar buttons */
    GtkBorder button_margin;

    /** Min size of titlebar region */
    GtkRequisition titlebar_min_size;
    /** Min size of titlebar buttons */
    GtkRequisition button_min_size;
  } gtk;

  struct {
    /** Size of left side */
    gint left_width;
    /** Size of right side */
    gint right_width;
    /** Size of bottom side */
    gint bottom_height;

    /** Border of blue title region
     * \bug (blue?!)
     **/
    GtkBorder title_border;

    /** Extra height for inside of title region, above the font height */
    int title_vertical_pad;

    /** Right indent of buttons from edges of frame */
    int right_titlebar_edge;
    /** Left indent of buttons from edges of frame */
    int left_titlebar_edge;

    /**
     * Sizing rule of buttons, either META_BUTTON_SIZING_ASPECT
     * (in which case button_aspect will be honoured, and
     * button_width and button_height set from it), or
     * META_BUTTON_SIZING_FIXED (in which case we read the width
     * and height directly).
     */
    MetaButtonSizing button_sizing;

    /**
     * Ratio of height/width. Honoured only if
     * button_sizing==META_BUTTON_SIZING_ASPECT.
     * Otherwise we figure out the height from the button_border.
     */
    double button_aspect;

    /** Width of a button; set even when we are using aspect sizing */
    gint button_width;

    /** Height of a button; set even when we are using aspect sizing */
    gint button_height;
  } metacity;

  /** Invisible resize area border */
  GtkBorder invisible_resize_border;

  /** Space around buttons */
  GtkBorder button_border;

  /** scale factor for title text */
  double title_scale;

  /** Whether title text will be displayed */
  guint has_title : 1;

  /** Whether we should hide the buttons */
  guint hide_buttons : 1;

  /** Radius of the top left-hand corner; 0 if not rounded */
  guint top_left_corner_rounded_radius;
  /** Radius of the top right-hand corner; 0 if not rounded */
  guint top_right_corner_rounded_radius;
  /** Radius of the bottom left-hand corner; 0 if not rounded */
  guint bottom_left_corner_rounded_radius;
  /** Radius of the bottom right-hand corner; 0 if not rounded */
  guint bottom_right_corner_rounded_radius;
};

G_GNUC_INTERNAL
MetaFrameLayout *meta_frame_layout_new      (void);

G_GNUC_INTERNAL
MetaFrameLayout *meta_frame_layout_copy     (const MetaFrameLayout  *src);

G_GNUC_INTERNAL
void             meta_frame_layout_ref      (MetaFrameLayout        *layout);

G_GNUC_INTERNAL
void             meta_frame_layout_unref    (MetaFrameLayout        *layout);

G_GNUC_INTERNAL
gboolean         meta_frame_layout_validate (const MetaFrameLayout  *layout,
                                             GError                **error);

G_END_DECLS

#endif
