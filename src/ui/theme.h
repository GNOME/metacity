/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity Theme Rendering */

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

#ifndef THEME_H
#define THEME_H

#include <gtk/gtk.h>
#include <libmetacity/meta-button-enums.h>
#include <libmetacity/meta-button-layout.h>
#include <libmetacity/meta-frame-borders.h>
#include <libmetacity/meta-frame-enums.h>
#include <libmetacity/meta-theme.h>
#include <libmetacity/meta-style-info.h>

typedef struct _MetaButtonSpace MetaButtonSpace;
typedef struct _MetaFrameGeometry MetaFrameGeometry;

/**
 * The computed size of a button (really just a way of tying its
 * visible and clickable areas together).
 * The reason for two different rectangles here is Fitts' law & maximized
 * windows; see bug #97703 for more details.
 */
struct _MetaButtonSpace
{
  /** The screen area where the button's image is drawn */
  GdkRectangle visible;
  /** The screen area where the button can be activated by clicking */
  GdkRectangle clickable;
};

/**
 * Calculated actual geometry of the frame
 */
struct _MetaFrameGeometry
{
  MetaFrameBorders borders;
  int              top_height;

  int width;
  int height;

  GdkRectangle title_rect;

  int left_titlebar_edge;
  int right_titlebar_edge;
  int top_titlebar_edge;
  int bottom_titlebar_edge;

  /* used for a memset hack */
#define ADDRESS_OF_BUTTON_RECTS(fgeom) (((char*)(fgeom)) + G_STRUCT_OFFSET (MetaFrameGeometry, close_rect))
#define LENGTH_OF_BUTTON_RECTS (G_STRUCT_OFFSET (MetaFrameGeometry, right_single_background) + sizeof (GdkRectangle) - G_STRUCT_OFFSET (MetaFrameGeometry, close_rect))

  /* The button rects (if changed adjust memset hack) */
  MetaButtonSpace close_rect;
  MetaButtonSpace max_rect;
  MetaButtonSpace min_rect;
  MetaButtonSpace menu_rect;
  MetaButtonSpace appmenu_rect;
  MetaButtonSpace shade_rect;
  MetaButtonSpace above_rect;
  MetaButtonSpace stick_rect;
  MetaButtonSpace unshade_rect;
  MetaButtonSpace unabove_rect;
  MetaButtonSpace unstick_rect;

#define MAX_MIDDLE_BACKGROUNDS (META_BUTTON_FUNCTION_LAST - 2)
  GdkRectangle left_left_background;
  GdkRectangle left_middle_backgrounds[MAX_MIDDLE_BACKGROUNDS];
  GdkRectangle left_right_background;
  GdkRectangle left_single_background;
  GdkRectangle right_left_background;
  GdkRectangle right_middle_backgrounds[MAX_MIDDLE_BACKGROUNDS];
  GdkRectangle right_right_background;
  GdkRectangle right_single_background;
  /* End of button rects (if changed adjust memset hack) */

  /* Saved button layout */
  MetaButtonLayout button_layout;
  int n_left_buttons;
  int n_right_buttons;

  /* Round corners */
  guint top_left_corner_rounded_radius;
  guint top_right_corner_rounded_radius;
  guint bottom_left_corner_rounded_radius;
  guint bottom_right_corner_rounded_radius;
};

MetaTheme* meta_theme_get_current (void);
void       meta_theme_set_current (const char                 *name,
                                   gboolean                    force_reload,
                                   gboolean                    composited,
                                   const PangoFontDescription *titlebar_font);

double meta_theme_get_title_scale (MetaTheme     *theme,
                                   MetaFrameType  type,
                                   MetaFrameFlags flags);

void meta_theme_draw_frame (MetaTheme              *theme,
                            const gchar            *variant,
                            cairo_t                *cr,
                            MetaFrameType           type,
                            MetaFrameFlags          flags,
                            int                     client_width,
                            int                     client_height,
                            PangoLayout            *title_layout,
                            int                     text_height,
                            const MetaButtonLayout *button_layout,
                            MetaButtonState         button_states[META_BUTTON_TYPE_LAST],
                            GdkPixbuf              *mini_icon,
                            GdkPixbuf              *icon);

void meta_theme_get_frame_borders (MetaTheme         *theme,
                                   const gchar       *variant,
                                   MetaFrameType      type,
                                   int                text_height,
                                   MetaFrameFlags     flags,
                                   MetaFrameBorders  *borders);
void meta_theme_calc_geometry (MetaTheme              *theme,
                               const gchar            *variant,
                               MetaFrameType           type,
                               int                     text_height,
                               MetaFrameFlags          flags,
                               int                     client_width,
                               int                     client_height,
                               const MetaButtonLayout *button_layout,
                               MetaFrameGeometry      *fgeom);

MetaFrameType         meta_frame_type_from_string      (const char            *str);

#endif
