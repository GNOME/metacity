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
#include <libmetacity/meta-theme.h>

MetaTheme* meta_theme_get_current (void);
void       meta_theme_set_current (const char                 *name,
                                   gboolean                    force_reload,
                                   gboolean                    composited,
                                   const PangoFontDescription *titlebar_font);

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

#endif
