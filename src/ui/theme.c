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

/**
 * \file theme.c    Making Metacity look pretty
 *
 * The window decorations drawn by Metacity are described by files on disk
 * known internally as "themes" (externally as "window border themes" on
 * http://art.gnome.org/themes/metacity/ or "Metacity themes"). This file
 * contains most of the code necessary to support themes; it does not
 * contain the XML parser, which is in theme-parser.c.
 *
 * \bug This is a big file with lots of different subsystems, which might
 * be better split out into separate files.
 */

/**
 * \defgroup tokenizer   The theme expression tokenizer
 *
 * Themes can use a simple expression language to represent the values of
 * things. This is the tokeniser used for that language.
 *
 * \bug We could remove almost all this code by using GScanner instead,
 * but we would also have to find every expression in every existing theme
 * we could and make sure the parse trees were the same.
 */

/**
 * \defgroup parser  The theme expression parser
 *
 * Themes can use a simple expression language to represent the values of
 * things. This is the parser used for that language.
 */

#include <config.h>
#include "theme.h"
#include "util.h"
#include <gtk/gtk.h>
#include <libmetacity/meta-color.h>
#include <string.h>
#include <stdlib.h>
#define __USE_XOPEN
#include <stdarg.h>
#include <math.h>

/**
 * The current theme. (Themes are singleton.)
 */
static MetaTheme *meta_current_theme = NULL;

static void
meta_frame_layout_get_borders (MetaTheme             *theme,
                               const MetaFrameLayout *layout,
                               int                    text_height,
                               MetaFrameFlags         flags,
                               MetaFrameType          type,
                               MetaFrameBorders      *borders)
{
  int buttons_height, title_height;

  meta_frame_borders_clear (borders);

  /* For a full-screen window, we don't have any borders, visible or not. */
  if (flags & META_FRAME_FULLSCREEN)
    return;

  g_return_if_fail (layout != NULL);

  if (!layout->has_title)
    text_height = 0;

  buttons_height = layout->button_height +
    layout->button_border.top + layout->button_border.bottom;
  title_height = text_height +
    layout->title_vertical_pad +
    layout->title_border.top + layout->title_border.bottom;

  borders->visible.top = layout->top_height + MAX (buttons_height, title_height);
  borders->visible.left = layout->left_width;
  borders->visible.right = layout->right_width;
  borders->visible.bottom = layout->bottom_height;

  if (meta_theme_get_theme_type (theme) != META_THEME_TYPE_METACITY)
    {
      borders->invisible.left = layout->invisible_border.left;
      borders->invisible.right = layout->invisible_border.right;
      borders->invisible.bottom = layout->invisible_border.bottom;
      borders->invisible.top = layout->invisible_border.top;
    }
  else
    {
      if (flags & META_FRAME_ALLOWS_HORIZONTAL_RESIZE)
        {
          borders->invisible.left = layout->invisible_border.left;
          borders->invisible.right = layout->invisible_border.right;
        }

      if (flags & META_FRAME_ALLOWS_VERTICAL_RESIZE)
        {
          borders->invisible.bottom = layout->invisible_border.bottom;

          if (type != META_FRAME_TYPE_ATTACHED)
            borders->invisible.top = layout->invisible_border.top;
        }
    }

  borders->total.left = borders->invisible.left + borders->visible.left;
  borders->total.right = borders->invisible.right + borders->visible.right;
  borders->total.bottom = borders->invisible.bottom + borders->visible.bottom;
  borders->total.top = borders->invisible.top + borders->visible.top;
}

static MetaButtonType
map_button_function_to_type (MetaButtonFunction  function)
{
  switch (function)
    {
    case META_BUTTON_FUNCTION_SHADE:
      return META_BUTTON_TYPE_SHADE;
    case META_BUTTON_FUNCTION_ABOVE:
      return META_BUTTON_TYPE_ABOVE;
    case META_BUTTON_FUNCTION_STICK:
      return META_BUTTON_TYPE_STICK;
    case META_BUTTON_FUNCTION_UNSHADE:
      return META_BUTTON_TYPE_UNSHADE;
    case META_BUTTON_FUNCTION_UNABOVE:
      return META_BUTTON_TYPE_UNABOVE;
    case META_BUTTON_FUNCTION_UNSTICK:
      return META_BUTTON_TYPE_UNSTICK;
    case META_BUTTON_FUNCTION_MENU:
      return META_BUTTON_TYPE_MENU;
    case META_BUTTON_FUNCTION_APPMENU:
      return META_BUTTON_TYPE_APPMENU;
    case META_BUTTON_FUNCTION_MINIMIZE:
      return META_BUTTON_TYPE_MINIMIZE;
    case META_BUTTON_FUNCTION_MAXIMIZE:
      return META_BUTTON_TYPE_MAXIMIZE;
    case META_BUTTON_FUNCTION_CLOSE:
      return META_BUTTON_TYPE_CLOSE;
    case META_BUTTON_FUNCTION_LAST:
      return META_BUTTON_TYPE_LAST;
    default:
      break;
    }

  return META_BUTTON_TYPE_LAST;
}

static MetaButtonSpace*
rect_for_function (MetaFrameGeometry *fgeom,
                   MetaFrameFlags     flags,
                   MetaButtonFunction function,
                   MetaTheme         *theme)
{
  if (meta_theme_get_theme_type (theme) == META_THEME_TYPE_METACITY)
    {
      if (meta_theme_allows_shade_stick_above_buttons (theme))
        {
          switch (function)
            {
              case META_BUTTON_FUNCTION_SHADE:
                if ((flags & META_FRAME_ALLOWS_SHADE) && !(flags & META_FRAME_SHADED))
                  return &fgeom->shade_rect;
                else
                  return NULL;

              case META_BUTTON_FUNCTION_ABOVE:
                if (!(flags & META_FRAME_ABOVE))
                  return &fgeom->above_rect;
                else
                  return NULL;

              case META_BUTTON_FUNCTION_STICK:
                if (!(flags & META_FRAME_STUCK))
                  return &fgeom->stick_rect;
                else
                  return NULL;

              case META_BUTTON_FUNCTION_UNSHADE:
                if ((flags & META_FRAME_ALLOWS_SHADE) && (flags & META_FRAME_SHADED))
                  return &fgeom->unshade_rect;
                else
                  return NULL;

              case META_BUTTON_FUNCTION_UNABOVE:
                if (flags & META_FRAME_ABOVE)
                  return &fgeom->unabove_rect;
                else
                  return NULL;

              case META_BUTTON_FUNCTION_UNSTICK:
                if (flags & META_FRAME_STUCK)
                  return &fgeom->unstick_rect;
                else
                  return NULL;

              case META_BUTTON_FUNCTION_MENU:
              case META_BUTTON_FUNCTION_APPMENU:
              case META_BUTTON_FUNCTION_MINIMIZE:
              case META_BUTTON_FUNCTION_MAXIMIZE:
              case META_BUTTON_FUNCTION_CLOSE:
              case META_BUTTON_FUNCTION_LAST:
              default:
                break;
            }

          /* now consider the buttons which exist in all versions */
          switch (function)
            {
              case META_BUTTON_FUNCTION_MENU:
                if (flags & META_FRAME_ALLOWS_MENU)
                  return &fgeom->menu_rect;
                else
                  return NULL;

              case META_BUTTON_FUNCTION_APPMENU:
                if (flags & META_FRAME_ALLOWS_APPMENU)
                  return &fgeom->appmenu_rect;
                else
                  return NULL;

              case META_BUTTON_FUNCTION_MINIMIZE:
                if (flags & META_FRAME_ALLOWS_MINIMIZE)
                  return &fgeom->min_rect;
                else
                  return NULL;

              case META_BUTTON_FUNCTION_MAXIMIZE:
                if (flags & META_FRAME_ALLOWS_MAXIMIZE)
                  return &fgeom->max_rect;
                else
                  return NULL;

              case META_BUTTON_FUNCTION_CLOSE:
                if (flags & META_FRAME_ALLOWS_DELETE)
                  return &fgeom->close_rect;
                else
                  return NULL;

              case META_BUTTON_FUNCTION_STICK:
              case META_BUTTON_FUNCTION_SHADE:
              case META_BUTTON_FUNCTION_ABOVE:
              case META_BUTTON_FUNCTION_UNSTICK:
              case META_BUTTON_FUNCTION_UNSHADE:
              case META_BUTTON_FUNCTION_UNABOVE:
                /* we are being asked for a >v1 button which hasn't been handled yet,
                 * so obviously we're not in a theme which supports that version.
                 * therefore, we don't show the button. return NULL and all will
                 * be well.
                 */
                return NULL;

              case META_BUTTON_FUNCTION_LAST:
              default:
                break;
            }
        }
    }
  else
    {
      switch (function)
        {
          case META_BUTTON_FUNCTION_MENU:
            if (flags & META_FRAME_ALLOWS_MENU)
              return &fgeom->menu_rect;
            else
              return NULL;

          case META_BUTTON_FUNCTION_APPMENU:
            if (flags & META_FRAME_ALLOWS_APPMENU)
              return &fgeom->appmenu_rect;
            else
              return NULL;

          case META_BUTTON_FUNCTION_MINIMIZE:
            if (flags & META_FRAME_ALLOWS_MINIMIZE)
              return &fgeom->min_rect;
            else
              return NULL;

          case META_BUTTON_FUNCTION_MAXIMIZE:
            if (flags & META_FRAME_ALLOWS_MAXIMIZE)
              return &fgeom->max_rect;
            else
              return NULL;

          case META_BUTTON_FUNCTION_CLOSE:
            if (flags & META_FRAME_ALLOWS_DELETE)
              return &fgeom->close_rect;
            else
              return NULL;

          case META_BUTTON_FUNCTION_SHADE:
            if ((flags & META_FRAME_ALLOWS_SHADE) && !(flags & META_FRAME_SHADED))
              return &fgeom->shade_rect;
            else
              return NULL;

          case META_BUTTON_FUNCTION_ABOVE:
            if (!(flags & META_FRAME_ABOVE))
              return &fgeom->above_rect;
            else
              return NULL;

          case META_BUTTON_FUNCTION_STICK:
            if (!(flags & META_FRAME_STUCK))
              return &fgeom->stick_rect;
            else
              return NULL;

          case META_BUTTON_FUNCTION_UNSHADE:
            if ((flags & META_FRAME_ALLOWS_SHADE) && (flags & META_FRAME_SHADED))
              return &fgeom->unshade_rect;
            else
              return NULL;

          case META_BUTTON_FUNCTION_UNABOVE:
            if (flags & META_FRAME_ABOVE)
              return &fgeom->unabove_rect;
            else
              return NULL;

          case META_BUTTON_FUNCTION_UNSTICK:
            if (flags & META_FRAME_STUCK)
              return &fgeom->unstick_rect;
            else
              return NULL;

          case META_BUTTON_FUNCTION_LAST:
          default:
            break;
        }
    }

  return NULL;
}

static gboolean
strip_button (MetaButtonSpace *func_rects[META_BUTTON_FUNCTION_LAST],
              GdkRectangle    *bg_rects[META_BUTTON_FUNCTION_LAST],
              int             *n_rects,
              MetaButtonSpace *to_strip)
{
  int i;

  i = 0;
  while (i < *n_rects)
    {
      if (func_rects[i] == to_strip)
        {
          *n_rects -= 1;

          /* shift the other rects back in the array */
          while (i < *n_rects)
            {
              func_rects[i] = func_rects[i+1];
              bg_rects[i] = bg_rects[i+1];

              ++i;
            }

          func_rects[i] = NULL;
          bg_rects[i] = NULL;

          return TRUE;
        }

      ++i;
    }

  return FALSE; /* did not strip anything */
}

static void
get_margin (GtkStyleContext *style,
            GtkBorder       *border)
{
  GtkStateFlags state;

  state = gtk_style_context_get_state (style);

  gtk_style_context_get_margin (style, state, border);
}

static void
get_padding_and_border (GtkStyleContext *style,
                        GtkBorder       *border)
{
  GtkBorder tmp;
  GtkStateFlags state = gtk_style_context_get_state (style);

  gtk_style_context_get_border (style, state, border);
  gtk_style_context_get_padding (style, state, &tmp);

  border->left += tmp.left;
  border->top += tmp.top;
  border->right += tmp.right;
  border->bottom += tmp.bottom;
}

static void
scale_border (GtkBorder *border,
              double     factor)
{
  border->left *= factor;
  border->right *= factor;
  border->top *= factor;
  border->bottom *= factor;
}

static void
meta_frame_layout_sync_with_style (MetaFrameLayout *layout,
                                   MetaStyleInfo   *style_info,
                                   MetaFrameFlags   flags,
                                   MetaTheme       *theme)
{
  GtkStyleContext *style;
  GtkBorder border;
  int border_radius, max_radius;

  /* We don't want GTK+ info for metacity theme */
  if (meta_theme_get_theme_type (theme) == META_THEME_TYPE_METACITY)
    return;

  meta_style_info_set_flags (style_info, flags);

  layout->button_sizing = META_BUTTON_SIZING_FIXED;

  style = style_info->styles[META_STYLE_ELEMENT_DECORATION];
  get_padding_and_border (style, &border);
  scale_border (&border, layout->title_scale);

  layout->left_width = border.left;
  layout->right_width = border.right;
  layout->top_height = border.top;
  layout->bottom_height = border.bottom;

  if (meta_theme_get_composited (theme))
    get_margin (style, &layout->invisible_border);
  else
    {
      get_margin (style, &border);

      layout->left_width += border.left;
      layout->right_width += border.right;
      layout->top_height += border.top;
      layout->bottom_height += border.bottom;
    }

  if (layout->hide_buttons)
    layout->icon_size = 0;

  if (!layout->has_title && layout->hide_buttons)
    return; /* border-only - be done */

  style = style_info->styles[META_STYLE_ELEMENT_TITLEBAR];

  if (meta_theme_get_composited (theme))
    {
      gtk_style_context_get (style, gtk_style_context_get_state (style),
                             "border-radius", &border_radius,
                             NULL);
      /* GTK+ currently does not allow us to look up radii of individual
       * corners; however we don't clip the client area, so with the
       * current trend of using small/no visible frame borders, most
       * themes should work fine with this.
       */
      layout->top_left_corner_rounded_radius = border_radius;
      layout->top_right_corner_rounded_radius = border_radius;
      max_radius = MIN (layout->bottom_height, layout->left_width);
      layout->bottom_left_corner_rounded_radius = MAX (border_radius, max_radius);
      max_radius = MIN (layout->bottom_height, layout->right_width);
      layout->bottom_right_corner_rounded_radius = MAX (border_radius, max_radius);
    }

  get_padding_and_border (style, &border);
  scale_border (&border, layout->title_scale);
  layout->left_titlebar_edge = layout->left_width + border.left;
  layout->right_titlebar_edge = layout->right_width + border.right;
  layout->title_vertical_pad = border.top;

  layout->button_border.top = border.top;
  layout->button_border.bottom = border.bottom;
  layout->button_border.left = 0;
  layout->button_border.right = 0;

  layout->button_width = layout->icon_size;
  layout->button_height = layout->icon_size;

  style = style_info->styles[META_STYLE_ELEMENT_BUTTON];
  get_padding_and_border (style, &border);
  scale_border (&border, layout->title_scale);
  layout->button_width += border.left + border.right;
  layout->button_height += border.top + border.bottom;

  style = style_info->styles[META_STYLE_ELEMENT_IMAGE];
  get_padding_and_border (style, &border);
  scale_border (&border, layout->title_scale);
  layout->button_width += border.left + border.right;
  layout->button_height += border.top + border.bottom;
}

static void
meta_frame_layout_calc_geometry (MetaFrameLayout        *layout,
                                 MetaStyleInfo          *style_info,
                                 int                     text_height,
                                 MetaFrameFlags          flags,
                                 int                     client_width,
                                 int                     client_height,
                                 const MetaButtonLayout *button_layout,
                                 MetaFrameType           type,
                                 MetaFrameGeometry      *fgeom,
                                 MetaTheme              *theme)
{
  MetaFrameBorders borders;
  int i, n_left, n_right, n_left_spacers, n_right_spacers;
  int x;
  int button_y;
  int title_right_edge;
  int width, height;
  int button_width, button_height;
  int min_size_for_rounding;

  /* the left/right rects in order; the max # of rects
   * is the number of button functions
   */
  MetaButtonSpace *left_func_rects[META_BUTTON_FUNCTION_LAST];
  MetaButtonSpace *right_func_rects[META_BUTTON_FUNCTION_LAST];
  GdkRectangle *left_bg_rects[META_BUTTON_FUNCTION_LAST];
  gboolean left_buttons_has_spacer[META_BUTTON_FUNCTION_LAST];
  GdkRectangle *right_bg_rects[META_BUTTON_FUNCTION_LAST];
  gboolean right_buttons_has_spacer[META_BUTTON_FUNCTION_LAST];

  meta_frame_layout_sync_with_style (layout, style_info, flags, theme);

  meta_frame_layout_get_borders (theme, layout, text_height,
                                 flags, type, &borders);

  fgeom->borders = borders;
  fgeom->top_height = layout->top_height;

  width = client_width + borders.total.left + borders.total.right;

  height = ((flags & META_FRAME_SHADED) ? 0: client_height) +
    borders.total.top + borders.total.bottom;

  fgeom->width = width;
  fgeom->height = height;

  fgeom->top_titlebar_edge = layout->title_border.top;
  fgeom->bottom_titlebar_edge = layout->title_border.bottom;
  fgeom->left_titlebar_edge = layout->left_titlebar_edge;
  fgeom->right_titlebar_edge = layout->right_titlebar_edge;

  /* gcc warnings */
  button_width = -1;
  button_height = -1;

  switch (layout->button_sizing)
    {
    case META_BUTTON_SIZING_ASPECT:
      button_height = borders.visible.top - layout->button_border.top - layout->button_border.bottom;
      button_width = button_height / layout->button_aspect;
      break;
    case META_BUTTON_SIZING_FIXED:
      button_width = layout->button_width;
      button_height = layout->button_height;
      break;
    case META_BUTTON_SIZING_LAST:
    default:
      g_assert_not_reached ();
      break;
    }

  /* FIXME all this code sort of pretends that duplicate buttons
   * with the same function are allowed, but that breaks the
   * code in frames.c, so isn't really allowed right now.
   * Would need left_close_rect, right_close_rect, etc.
   */

  /* Init all button rects to 0, lame hack */
  memset (ADDRESS_OF_BUTTON_RECTS (fgeom), '\0',
          LENGTH_OF_BUTTON_RECTS);

  n_left = 0;
  n_right = 0;
  n_left_spacers = 0;
  n_right_spacers = 0;

  if (!layout->hide_buttons)
    {
      /* Try to fill in rects */
      for (i = 0; i < META_BUTTON_FUNCTION_LAST && button_layout->left_buttons[i] != META_BUTTON_FUNCTION_LAST; i++)
        {
          left_func_rects[n_left] = rect_for_function (fgeom, flags,
                                                       button_layout->left_buttons[i],
                                                       theme);
          if (left_func_rects[n_left] != NULL)
            {
              left_buttons_has_spacer[n_left] = button_layout->left_buttons_has_spacer[i];
              if (button_layout->left_buttons_has_spacer[i])
                ++n_left_spacers;

              ++n_left;
            }
        }

      for (i = 0; i < META_BUTTON_FUNCTION_LAST && button_layout->right_buttons[i] != META_BUTTON_FUNCTION_LAST; i++)
        {
          right_func_rects[n_right] = rect_for_function (fgeom, flags,
                                                         button_layout->right_buttons[i],
                                                         theme);
          if (right_func_rects[n_right] != NULL)
            {
              right_buttons_has_spacer[n_right] = button_layout->right_buttons_has_spacer[i];
              if (button_layout->right_buttons_has_spacer[i])
                ++n_right_spacers;

              ++n_right;
            }
        }
    }

  for (i = 0; i < META_BUTTON_FUNCTION_LAST; i++)
    {
      left_bg_rects[i] = NULL;
      right_bg_rects[i] = NULL;
    }

  for (i = 0; i < n_left; i++)
    {
      if (n_left == 1)
        left_bg_rects[i] = &fgeom->left_single_background;
      else if (i == 0)
        left_bg_rects[i] = &fgeom->left_left_background;
      else if (i == (n_left - 1))
        left_bg_rects[i] = &fgeom->left_right_background;
      else
        left_bg_rects[i] = &fgeom->left_middle_backgrounds[i - 1];
    }

  for (i = 0; i < n_right; i++)
    {
      if (n_right == 1)
        right_bg_rects[i] = &fgeom->right_single_background;
      else if (i == (n_right - 1))
        right_bg_rects[i] = &fgeom->right_right_background;
      else if (i == 0)
        right_bg_rects[i] = &fgeom->right_left_background;
      else
        right_bg_rects[i] = &fgeom->right_middle_backgrounds[i - 1];
    }

  /* Be sure buttons fit */
  while (n_left > 0 || n_right > 0)
    {
      int space_used_by_buttons;
      int space_available;

      space_available = fgeom->width - layout->left_titlebar_edge - layout->right_titlebar_edge;

      space_used_by_buttons = 0;

      space_used_by_buttons += button_width * n_left;
      space_used_by_buttons += (button_width * 0.75) * n_left_spacers;
      if (meta_theme_get_theme_type (theme) == META_THEME_TYPE_METACITY)
        {
          space_used_by_buttons += layout->button_border.left * n_left;
          space_used_by_buttons += layout->button_border.right * n_left;
        }
      else
        space_used_by_buttons += layout->titlebar_spacing * MAX (n_left - 1, 0);

      space_used_by_buttons += button_width * n_right;
      space_used_by_buttons += (button_width * 0.75) * n_right_spacers;
      if (meta_theme_get_theme_type (theme) == META_THEME_TYPE_METACITY)
        {
          space_used_by_buttons += layout->button_border.left * n_right;
          space_used_by_buttons += layout->button_border.right * n_right;
        }
      else
        space_used_by_buttons += layout->titlebar_spacing * MAX (n_right - 1, 0);

      if (space_used_by_buttons <= space_available)
        break; /* Everything fits, bail out */

      /* First try to remove separators */
      if (n_left_spacers > 0)
        {
          left_buttons_has_spacer[--n_left_spacers] = FALSE;
          continue;
        }
      else if (n_right_spacers > 0)
        {
          right_buttons_has_spacer[--n_right_spacers] = FALSE;
          continue;
        }

      /* Otherwise we need to shave out a button. Shave
       * above, stick, shade, min, max, close, then menu (menu is most useful);
       * prefer the default button locations.
       */
      if (strip_button (left_func_rects, left_bg_rects,
                        &n_left, &fgeom->above_rect))
        continue;
      else if (strip_button (right_func_rects, right_bg_rects,
                             &n_right, &fgeom->above_rect))
        continue;
      else if (strip_button (left_func_rects, left_bg_rects,
                        &n_left, &fgeom->stick_rect))
        continue;
      else if (strip_button (right_func_rects, right_bg_rects,
                             &n_right, &fgeom->stick_rect))
        continue;
      else if (strip_button (left_func_rects, left_bg_rects,
                        &n_left, &fgeom->shade_rect))
        continue;
      else if (strip_button (right_func_rects, right_bg_rects,
                             &n_right, &fgeom->shade_rect))
        continue;
      else if (strip_button (left_func_rects, left_bg_rects,
                        &n_left, &fgeom->min_rect))
        continue;
      else if (strip_button (right_func_rects, right_bg_rects,
                             &n_right, &fgeom->min_rect))
        continue;
      else if (strip_button (left_func_rects, left_bg_rects,
                             &n_left, &fgeom->max_rect))
        continue;
      else if (strip_button (right_func_rects, right_bg_rects,
                             &n_right, &fgeom->max_rect))
        continue;
      else if (strip_button (left_func_rects, left_bg_rects,
                             &n_left, &fgeom->close_rect))
        continue;
      else if (strip_button (right_func_rects, right_bg_rects,
                             &n_right, &fgeom->close_rect))
        continue;
      else if (strip_button (right_func_rects, right_bg_rects,
                             &n_right, &fgeom->menu_rect))
        continue;
      else if (strip_button (left_func_rects, left_bg_rects,
                             &n_left, &fgeom->menu_rect))
        continue;
      else if (strip_button (right_func_rects, right_bg_rects,
                             &n_right, &fgeom->appmenu_rect))
        continue;
      else if (strip_button (left_func_rects, left_bg_rects,
                             &n_left, &fgeom->appmenu_rect))
        continue;
      else
        {
          meta_bug ("Could not find a button to strip. n_left = %d n_right = %d\n",
                    n_left, n_right);
        }
    }

  /* Save the button layout */
  fgeom->button_layout = *button_layout;
  fgeom->n_left_buttons = n_left;
  fgeom->n_right_buttons = n_right;

  /* center buttons vertically */
  button_y = (borders.visible.top - fgeom->top_height -
              (button_height + layout->button_border.top + layout->button_border.bottom)) / 2 + layout->button_border.top + fgeom->top_height + borders.invisible.top;

  /* right edge of farthest-right button */
  x = width - layout->right_titlebar_edge - borders.invisible.right;

  i = n_right - 1;
  while (i >= 0)
    {
      MetaButtonSpace *rect;

      if (x < 0) /* if we go negative, leave the buttons we don't get to as 0-width */
        break;

      rect = right_func_rects[i];
      if (meta_theme_get_theme_type (theme) == META_THEME_TYPE_METACITY)
        rect->visible.x = x - layout->button_border.right - button_width;
      else
        rect->visible.x = x - button_width;
      if (right_buttons_has_spacer[i])
        rect->visible.x -= (button_width * 0.75);

      rect->visible.y = button_y;
      rect->visible.width = button_width;
      rect->visible.height = button_height;

      if (flags & META_FRAME_MAXIMIZED ||
          flags & META_FRAME_TILED_LEFT ||
          flags & META_FRAME_TILED_RIGHT)
        {
          rect->clickable.x = rect->visible.x;
          rect->clickable.y = rect->visible.y;
          rect->clickable.width = button_width;
          rect->clickable.height = button_height;

          if (i == n_right - 1)
            {
              if (meta_theme_get_theme_type (theme) == META_THEME_TYPE_METACITY)
                rect->clickable.width += layout->right_titlebar_edge + layout->right_width + layout->button_border.right;
              else
                rect->clickable.width += layout->right_titlebar_edge + layout->right_width;
            }

        }
      else
        g_memmove (&(rect->clickable), &(rect->visible), sizeof(rect->clickable));

      *(right_bg_rects[i]) = rect->visible;

      if (meta_theme_get_theme_type (theme) == META_THEME_TYPE_METACITY)
        x = rect->visible.x - layout->button_border.left;
      else
        {
          x = rect->visible.x;

          if (i > 0)
            x -= layout->titlebar_spacing;
        }

      --i;
    }

  /* save right edge of titlebar for later use */
  title_right_edge = x - layout->title_border.right;

  /* Now x changes to be position from the left and we go through
   * the left-side buttons
   */
  x = layout->left_titlebar_edge + borders.invisible.left;
  for (i = 0; i < n_left; i++)
    {
      MetaButtonSpace *rect;

      rect = left_func_rects[i];

      if (meta_theme_get_theme_type (theme) == META_THEME_TYPE_METACITY)
        rect->visible.x = x + layout->button_border.left;
      else
        rect->visible.x = x;
      rect->visible.y = button_y;
      rect->visible.width = button_width;
      rect->visible.height = button_height;

      if (flags & META_FRAME_MAXIMIZED)
        {
          rect->clickable.x = rect->visible.x;
          rect->clickable.y = rect->visible.y;
          rect->clickable.width = button_width;
          rect->clickable.height = button_height;
        }
      else
        g_memmove (&(rect->clickable), &(rect->visible), sizeof(rect->clickable));

      if (meta_theme_get_theme_type (theme) == META_THEME_TYPE_METACITY)
        x = rect->visible.x + rect->visible.width + layout->button_border.right;
      else
        {
          x = rect->visible.x + rect->visible.width;
          if (i < n_left - 1)
            x += layout->titlebar_spacing;
        }
      if (left_buttons_has_spacer[i])
        x += (button_width * 0.75);

      *(left_bg_rects[i]) = rect->visible;
    }

  /* We always fill as much vertical space as possible with title rect,
   * rather than centering it like the buttons
   */
  fgeom->title_rect.x = x + layout->title_border.left;
  fgeom->title_rect.y = layout->title_border.top + borders.invisible.top;
  fgeom->title_rect.width = title_right_edge - fgeom->title_rect.x;
  fgeom->title_rect.height = borders.visible.top - layout->title_border.top - layout->title_border.bottom;

  /* Nuke title if it won't fit */
  if (fgeom->title_rect.width < 0 ||
      fgeom->title_rect.height < 0)
    {
      fgeom->title_rect.width = 0;
      fgeom->title_rect.height = 0;
    }

  if (flags & META_FRAME_SHADED)
    min_size_for_rounding = 0;
  else
    min_size_for_rounding = 5;

  fgeom->top_left_corner_rounded_radius = 0;
  fgeom->top_right_corner_rounded_radius = 0;
  fgeom->bottom_left_corner_rounded_radius = 0;
  fgeom->bottom_right_corner_rounded_radius = 0;

  if (borders.visible.top + borders.visible.left >= min_size_for_rounding)
    fgeom->top_left_corner_rounded_radius = layout->top_left_corner_rounded_radius;
  if (borders.visible.top + borders.visible.right >= min_size_for_rounding)
    fgeom->top_right_corner_rounded_radius = layout->top_right_corner_rounded_radius;

  if (borders.visible.bottom + borders.visible.left >= min_size_for_rounding)
    fgeom->bottom_left_corner_rounded_radius = layout->bottom_left_corner_rounded_radius;
  if (borders.visible.bottom + borders.visible.right >= min_size_for_rounding)
    fgeom->bottom_right_corner_rounded_radius = layout->bottom_right_corner_rounded_radius;
}

static MetaButtonState
map_button_state (MetaButtonType           button_type,
                  const MetaFrameGeometry *fgeom,
                  int                      middle_bg_offset,
                  MetaButtonState          button_states[META_BUTTON_TYPE_LAST])
{
  MetaButtonFunction function = META_BUTTON_FUNCTION_LAST;

  switch (button_type)
    {
    /* First handle functions, which map directly */
    case META_BUTTON_TYPE_SHADE:
    case META_BUTTON_TYPE_ABOVE:
    case META_BUTTON_TYPE_STICK:
    case META_BUTTON_TYPE_UNSHADE:
    case META_BUTTON_TYPE_UNABOVE:
    case META_BUTTON_TYPE_UNSTICK:
    case META_BUTTON_TYPE_MENU:
    case META_BUTTON_TYPE_APPMENU:
    case META_BUTTON_TYPE_MINIMIZE:
    case META_BUTTON_TYPE_MAXIMIZE:
    case META_BUTTON_TYPE_CLOSE:
      return button_states[button_type];

    /* Map position buttons to the corresponding function */
    case META_BUTTON_TYPE_RIGHT_LEFT_BACKGROUND:
    case META_BUTTON_TYPE_RIGHT_SINGLE_BACKGROUND:
      if (fgeom->n_right_buttons > 0)
        function = fgeom->button_layout.right_buttons[0];
      break;
    case META_BUTTON_TYPE_RIGHT_RIGHT_BACKGROUND:
      if (fgeom->n_right_buttons > 0)
        function = fgeom->button_layout.right_buttons[fgeom->n_right_buttons - 1];
      break;
    case META_BUTTON_TYPE_RIGHT_MIDDLE_BACKGROUND:
      if (middle_bg_offset + 1 < fgeom->n_right_buttons)
        function = fgeom->button_layout.right_buttons[middle_bg_offset + 1];
      break;
    case META_BUTTON_TYPE_LEFT_LEFT_BACKGROUND:
    case META_BUTTON_TYPE_LEFT_SINGLE_BACKGROUND:
      if (fgeom->n_left_buttons > 0)
        function = fgeom->button_layout.left_buttons[0];
      break;
    case META_BUTTON_TYPE_LEFT_RIGHT_BACKGROUND:
      if (fgeom->n_left_buttons > 0)
        function = fgeom->button_layout.left_buttons[fgeom->n_left_buttons - 1];
      break;
    case META_BUTTON_TYPE_LEFT_MIDDLE_BACKGROUND:
      if (middle_bg_offset + 1 < fgeom->n_left_buttons)
        function = fgeom->button_layout.left_buttons[middle_bg_offset + 1];
      break;
    case META_BUTTON_TYPE_LAST:
      break;
    default:
      break;
    }

  if (function != META_BUTTON_FUNCTION_LAST)
    return button_states[map_button_function_to_type (function)];

  return META_BUTTON_STATE_LAST;
}

static void
get_button_rect (MetaButtonType           type,
                 const MetaFrameGeometry *fgeom,
                 int                      middle_background_offset,
                 GdkRectangle            *rect)
{
  switch (type)
    {
    case META_BUTTON_TYPE_LEFT_LEFT_BACKGROUND:
      *rect = fgeom->left_left_background;
      break;

    case META_BUTTON_TYPE_LEFT_MIDDLE_BACKGROUND:
      *rect = fgeom->left_middle_backgrounds[middle_background_offset];
      break;

    case META_BUTTON_TYPE_LEFT_RIGHT_BACKGROUND:
      *rect = fgeom->left_right_background;
      break;

    case META_BUTTON_TYPE_LEFT_SINGLE_BACKGROUND:
      *rect = fgeom->left_single_background;
      break;

    case META_BUTTON_TYPE_RIGHT_LEFT_BACKGROUND:
      *rect = fgeom->right_left_background;
      break;

    case META_BUTTON_TYPE_RIGHT_MIDDLE_BACKGROUND:
      *rect = fgeom->right_middle_backgrounds[middle_background_offset];
      break;

    case META_BUTTON_TYPE_RIGHT_RIGHT_BACKGROUND:
      *rect = fgeom->right_right_background;
      break;

    case META_BUTTON_TYPE_RIGHT_SINGLE_BACKGROUND:
      *rect = fgeom->right_single_background;
      break;

    case META_BUTTON_TYPE_CLOSE:
      *rect = fgeom->close_rect.visible;
      break;

    case META_BUTTON_TYPE_SHADE:
      *rect = fgeom->shade_rect.visible;
      break;

    case META_BUTTON_TYPE_UNSHADE:
      *rect = fgeom->unshade_rect.visible;
      break;

    case META_BUTTON_TYPE_ABOVE:
      *rect = fgeom->above_rect.visible;
      break;

    case META_BUTTON_TYPE_UNABOVE:
      *rect = fgeom->unabove_rect.visible;
      break;

    case META_BUTTON_TYPE_STICK:
      *rect = fgeom->stick_rect.visible;
      break;

    case META_BUTTON_TYPE_UNSTICK:
      *rect = fgeom->unstick_rect.visible;
      break;

    case META_BUTTON_TYPE_MAXIMIZE:
      *rect = fgeom->max_rect.visible;
      break;

    case META_BUTTON_TYPE_MINIMIZE:
      *rect = fgeom->min_rect.visible;
      break;

    case META_BUTTON_TYPE_MENU:
      *rect = fgeom->menu_rect.visible;
      break;

    case META_BUTTON_TYPE_APPMENU:
      *rect = fgeom->appmenu_rect.visible;
      break;

    case META_BUTTON_TYPE_LAST:
    default:
      g_assert_not_reached ();
      break;
    }
}

/* Used for metacity theme */
static void
meta_frame_style_draw_with_style (MetaFrameStyle          *style,
                                  MetaStyleInfo           *style_info,
                                  cairo_t                 *cr,
                                  const MetaFrameGeometry *fgeom,
                                  PangoLayout             *title_layout,
                                  MetaButtonState          button_states[META_BUTTON_TYPE_LAST],
                                  GdkPixbuf               *mini_icon,
                                  GdkPixbuf               *icon)
{
  int i, j;
  GdkRectangle visible_rect;
  GdkRectangle titlebar_rect;
  GdkRectangle left_titlebar_edge;
  GdkRectangle right_titlebar_edge;
  GdkRectangle bottom_titlebar_edge;
  GdkRectangle top_titlebar_edge;
  GdkRectangle left_edge, right_edge, bottom_edge;
  PangoRectangle extents;
  MetaDrawInfo draw_info;
  const MetaFrameBorders *borders;

  borders = &fgeom->borders;

  visible_rect.x = borders->invisible.left;
  visible_rect.y = borders->invisible.top;
  visible_rect.width = fgeom->width - borders->invisible.left - borders->invisible.right;
  visible_rect.height = fgeom->height - borders->invisible.top - borders->invisible.bottom;

  titlebar_rect.x = visible_rect.x;
  titlebar_rect.y = visible_rect.y;
  titlebar_rect.width = visible_rect.width;
  titlebar_rect.height = borders->visible.top;

  left_titlebar_edge.x = titlebar_rect.x;
  left_titlebar_edge.y = titlebar_rect.y + fgeom->top_titlebar_edge;
  left_titlebar_edge.width = fgeom->left_titlebar_edge;
  left_titlebar_edge.height = titlebar_rect.height - fgeom->top_titlebar_edge - fgeom->bottom_titlebar_edge;

  right_titlebar_edge.y = left_titlebar_edge.y;
  right_titlebar_edge.height = left_titlebar_edge.height;
  right_titlebar_edge.width = fgeom->right_titlebar_edge;
  right_titlebar_edge.x = titlebar_rect.x + titlebar_rect.width - right_titlebar_edge.width;

  top_titlebar_edge.x = titlebar_rect.x;
  top_titlebar_edge.y = titlebar_rect.y;
  top_titlebar_edge.width = titlebar_rect.width;
  top_titlebar_edge.height = fgeom->top_titlebar_edge;

  bottom_titlebar_edge.x = titlebar_rect.x;
  bottom_titlebar_edge.width = titlebar_rect.width;
  bottom_titlebar_edge.height = fgeom->bottom_titlebar_edge;
  bottom_titlebar_edge.y = titlebar_rect.y + titlebar_rect.height - bottom_titlebar_edge.height;

  left_edge.x = visible_rect.x;
  left_edge.y = visible_rect.y + borders->visible.top;
  left_edge.width = borders->visible.left;
  left_edge.height = visible_rect.height - borders->visible.top - borders->visible.bottom;

  right_edge.x = visible_rect.x + visible_rect.width - borders->visible.right;
  right_edge.y = visible_rect.y + borders->visible.top;
  right_edge.width = borders->visible.right;
  right_edge.height = visible_rect.height - borders->visible.top - borders->visible.bottom;

  bottom_edge.x = visible_rect.x;
  bottom_edge.y = visible_rect.y + visible_rect.height - borders->visible.bottom;
  bottom_edge.width = visible_rect.width;
  bottom_edge.height = borders->visible.bottom;

  if (title_layout)
    pango_layout_get_pixel_extents (title_layout,
                                    NULL, &extents);

  draw_info.mini_icon = mini_icon;
  draw_info.icon = icon;
  draw_info.title_layout = title_layout;
  draw_info.title_layout_width = title_layout ? extents.width : 0;
  draw_info.title_layout_height = title_layout ? extents.height : 0;

  draw_info.borders = fgeom->borders;
  draw_info.width = fgeom->width;
  draw_info.height = fgeom->height;

  /* The enum is in the order the pieces should be rendered. */
  i = 0;
  while (i < META_FRAME_PIECE_LAST)
    {
      GdkRectangle rect;

      switch ((MetaFramePiece) i)
        {
        case META_FRAME_PIECE_ENTIRE_BACKGROUND:
          rect = visible_rect;
          break;

        case META_FRAME_PIECE_TITLEBAR:
          rect = titlebar_rect;
          break;

        case META_FRAME_PIECE_LEFT_TITLEBAR_EDGE:
          rect = left_titlebar_edge;
          break;

        case META_FRAME_PIECE_RIGHT_TITLEBAR_EDGE:
          rect = right_titlebar_edge;
          break;

        case META_FRAME_PIECE_TOP_TITLEBAR_EDGE:
          rect = top_titlebar_edge;
          break;

        case META_FRAME_PIECE_BOTTOM_TITLEBAR_EDGE:
          rect = bottom_titlebar_edge;
          break;

        case META_FRAME_PIECE_TITLEBAR_MIDDLE:
          rect.x = left_titlebar_edge.x + left_titlebar_edge.width;
          rect.y = top_titlebar_edge.y + top_titlebar_edge.height;
          rect.width = titlebar_rect.width - left_titlebar_edge.width -
            right_titlebar_edge.width;
          rect.height = titlebar_rect.height - top_titlebar_edge.height - bottom_titlebar_edge.height;
          break;

        case META_FRAME_PIECE_TITLE:
          rect = fgeom->title_rect;
          break;

        case META_FRAME_PIECE_LEFT_EDGE:
          rect = left_edge;
          break;

        case META_FRAME_PIECE_RIGHT_EDGE:
          rect = right_edge;
          break;

        case META_FRAME_PIECE_BOTTOM_EDGE:
          rect = bottom_edge;
          break;

        case META_FRAME_PIECE_OVERLAY:
          rect = visible_rect;
          break;

        case META_FRAME_PIECE_LAST:
        default:
          g_assert_not_reached ();
          break;
        }

      cairo_save (cr);

      gdk_cairo_rectangle (cr, &rect);
      cairo_clip (cr);

      if (gdk_cairo_get_clip_rectangle (cr, NULL))
        {
          MetaDrawOpList *op_list;
          MetaFrameStyle *parent;

          parent = style;
          op_list = NULL;
          while (parent && op_list == NULL)
            {
              op_list = parent->pieces[i];
              parent = parent->parent;
            }

          if (op_list)
            {
              meta_draw_op_list_draw_with_style (op_list,
                                                 style_info->styles[META_STYLE_ELEMENT_WINDOW],
                                                 cr,
                                                 &draw_info,
                                                 rect);
            }
        }

      cairo_restore (cr);

      /* Draw buttons just before overlay */
      if ((i + 1) == META_FRAME_PIECE_OVERLAY)
        {
          MetaDrawOpList *op_list;
          int middle_bg_offset;

          middle_bg_offset = 0;
          j = 0;
          while (j < META_BUTTON_TYPE_LAST)
            {
              MetaButtonState button_state;

              get_button_rect (j, fgeom, middle_bg_offset, &rect);

              button_state = map_button_state (j, fgeom, middle_bg_offset, button_states);
              op_list = meta_frame_style_get_button (style, j, button_state);

              if (op_list)
                {
                  cairo_save (cr);
                  gdk_cairo_rectangle (cr, &rect);
                  cairo_clip (cr);

                  if (gdk_cairo_get_clip_rectangle (cr, NULL))
                    {
                      meta_draw_op_list_draw_with_style (op_list,
                                                         style_info->styles[META_STYLE_ELEMENT_WINDOW],
                                                         cr,
                                                         &draw_info,
                                                         rect);
                    }
                  cairo_restore (cr);
                }

              /* MIDDLE_BACKGROUND type may get drawn more than once */
              if ((j == META_BUTTON_TYPE_RIGHT_MIDDLE_BACKGROUND ||
                   j == META_BUTTON_TYPE_LEFT_MIDDLE_BACKGROUND) &&
                  (middle_bg_offset < (MAX_MIDDLE_BACKGROUNDS - 1)))
                {
                  ++middle_bg_offset;
                }
              else
                {
                  middle_bg_offset = 0;
                  ++j;
                }
            }
        }

      ++i;
    }
}

static const char *
get_class_from_button_type (MetaButtonType type)
{
  if (type == META_BUTTON_TYPE_CLOSE)
    return "close";
  else if (type == META_BUTTON_TYPE_MAXIMIZE)
    return "maximize";
  else if (type == META_BUTTON_TYPE_MINIMIZE)
    return "minimize";

  return NULL;
}

/* Used for GTK+ theme */
static void
meta_frame_style_draw_with_style_gtk (MetaFrameStyle          *frame_style,
                                      MetaStyleInfo           *style_info,
                                      cairo_t                 *cr,
                                      const MetaFrameGeometry *fgeom,
                                      PangoLayout             *title_layout,
                                      MetaFrameFlags           flags,
                                      MetaButtonState          button_states[META_BUTTON_TYPE_LAST],
                                      GdkPixbuf               *mini_icon)
{
  GtkStyleContext *style;
  GtkStateFlags state;
  MetaButtonType button_type;
  GdkRectangle visible_rect;
  GdkRectangle titlebar_rect;
  GdkRectangle button_rect;
  const MetaFrameBorders *borders;

  borders = &fgeom->borders;

  visible_rect.x = borders->invisible.left;
  visible_rect.y = borders->invisible.top;
  visible_rect.width = fgeom->width - borders->invisible.left - borders->invisible.right;
  visible_rect.height = fgeom->height - borders->invisible.top - borders->invisible.bottom;

  meta_style_info_set_flags (style_info, flags);

  style = style_info->styles[META_STYLE_ELEMENT_DECORATION];
  gtk_render_background (style, cr,
                         visible_rect.x, visible_rect.y,
                         visible_rect.width, visible_rect.height);
  gtk_render_frame (style, cr,
                    visible_rect.x, visible_rect.y,
                    visible_rect.width, visible_rect.height);

  titlebar_rect.x = visible_rect.x + borders->visible.left;
  titlebar_rect.y = visible_rect.y + fgeom->top_height;
  titlebar_rect.width = visible_rect.width - borders->visible.left - borders->visible.right;
  titlebar_rect.height = borders->visible.top - fgeom->top_height;

  style = style_info->styles[META_STYLE_ELEMENT_TITLEBAR];
  gtk_render_background (style, cr,
                         titlebar_rect.x, titlebar_rect.y,
                         titlebar_rect.width, titlebar_rect.height);
  gtk_render_frame (style, cr,
                    titlebar_rect.x, titlebar_rect.y,
                    titlebar_rect.width, titlebar_rect.height);

  if (frame_style->layout->has_title && title_layout)
    {
      PangoRectangle logical;
      int text_width, x, y;

      pango_layout_set_width (title_layout, -1);
      pango_layout_get_pixel_extents (title_layout, NULL, &logical);

      text_width = MIN(fgeom->title_rect.width, logical.width);

      if (text_width < logical.width)
        pango_layout_set_width (title_layout, PANGO_SCALE * text_width);

      /* Center within the frame if possible */
      x = titlebar_rect.x + (titlebar_rect.width - text_width) / 2;
      y = titlebar_rect.y + (titlebar_rect.height - logical.height) / 2;

      if (x < fgeom->title_rect.x)
        x = fgeom->title_rect.x;
      else if (x + text_width > fgeom->title_rect.x + fgeom->title_rect.width)
        x = fgeom->title_rect.x + fgeom->title_rect.width - text_width;

      style = style_info->styles[META_STYLE_ELEMENT_TITLE];
      gtk_render_layout (style, cr, x, y, title_layout);
    }

  style = style_info->styles[META_STYLE_ELEMENT_BUTTON];
  state = gtk_style_context_get_state (style);
  for (button_type = META_BUTTON_TYPE_CLOSE; button_type < META_BUTTON_TYPE_LAST; button_type++)
    {
      MetaButtonState button_state;
      const char *button_class;

      button_class = get_class_from_button_type (button_type);

      if (button_class)
        gtk_style_context_add_class (style, button_class);

      get_button_rect (button_type, fgeom, 0, &button_rect);

      button_state = map_button_state (button_type, fgeom, 0, button_states);

      if (button_state == META_BUTTON_STATE_PRELIGHT)
        gtk_style_context_set_state (style, state | GTK_STATE_PRELIGHT);
      else if (button_state == META_BUTTON_STATE_PRESSED)
        gtk_style_context_set_state (style, state | GTK_STATE_ACTIVE);
      else
        gtk_style_context_set_state (style, state);

      cairo_save (cr);

      if (button_rect.width > 0 && button_rect.height > 0)
        {
          GdkPixbuf *pixbuf = NULL;
          const char *icon_name = NULL;

          gtk_render_background (style, cr,
                                 button_rect.x, button_rect.y,
                                 button_rect.width, button_rect.height);
          gtk_render_frame (style, cr,
                            button_rect.x, button_rect.y,
                            button_rect.width, button_rect.height);

          switch (button_type)
            {
              case META_BUTTON_TYPE_CLOSE:
                icon_name = "window-close-symbolic";
                break;
              case META_BUTTON_TYPE_MAXIMIZE:
                if (flags & META_FRAME_MAXIMIZED)
                  icon_name = "window-restore-symbolic";
                else
                  icon_name = "window-maximize-symbolic";
                break;
              case META_BUTTON_TYPE_MINIMIZE:
                icon_name = "window-minimize-symbolic";
                break;
              case META_BUTTON_TYPE_MENU:
                icon_name = "open-menu-symbolic";
                break;
              case META_BUTTON_TYPE_APPMENU:
                pixbuf = g_object_ref (mini_icon);
                break;
              case META_BUTTON_TYPE_LEFT_LEFT_BACKGROUND:
              case META_BUTTON_TYPE_LEFT_MIDDLE_BACKGROUND:
              case META_BUTTON_TYPE_LEFT_RIGHT_BACKGROUND:
              case META_BUTTON_TYPE_LEFT_SINGLE_BACKGROUND:
              case META_BUTTON_TYPE_RIGHT_LEFT_BACKGROUND:
              case META_BUTTON_TYPE_RIGHT_MIDDLE_BACKGROUND:
              case META_BUTTON_TYPE_RIGHT_RIGHT_BACKGROUND:
              case META_BUTTON_TYPE_RIGHT_SINGLE_BACKGROUND:
              case META_BUTTON_TYPE_SHADE:
              case META_BUTTON_TYPE_ABOVE:
              case META_BUTTON_TYPE_STICK:
              case META_BUTTON_TYPE_UNSHADE:
              case META_BUTTON_TYPE_UNABOVE:
              case META_BUTTON_TYPE_UNSTICK:
              case META_BUTTON_TYPE_LAST:
              default:
                icon_name = NULL;
                break;
            }

          if (icon_name)
            {
              GtkIconTheme *theme = gtk_icon_theme_get_default ();
              GtkIconInfo *info;

              info = gtk_icon_theme_lookup_icon (theme, icon_name, frame_style->layout->icon_size, 0);
              pixbuf = gtk_icon_info_load_symbolic_for_context (info, style, NULL, NULL);
            }

          if (pixbuf)
            {
              float width, height;
              int x, y;

              width = gdk_pixbuf_get_width (pixbuf);
              height = gdk_pixbuf_get_height (pixbuf);
              x = button_rect.x + (button_rect.width - width) / 2;
              y = button_rect.y + (button_rect.height - height) / 2;

              cairo_translate (cr, x, y);
              cairo_scale (cr,
                           width / frame_style->layout->icon_size,
                           height / frame_style->layout->icon_size);
              gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
                                           cairo_paint (cr);

              g_object_unref (pixbuf);
            }
        }

      cairo_restore (cr);

      if (button_class)
        gtk_style_context_remove_class (style, button_class);
    }
}

MetaTheme*
meta_theme_get_current (void)
{
  return meta_current_theme;
}

void
meta_theme_set_current (const gchar                *name,
                        gboolean                    force_reload,
                        gboolean                    composited,
                        const PangoFontDescription *titlebar_font)
{
  MetaTheme *new_theme;
  GError *error;

  g_debug ("Setting current theme to '%s'", name);

  if (!force_reload && meta_current_theme)
    {
      gchar *theme_name;

      theme_name = meta_theme_get_name (meta_current_theme);
      if (g_strcmp0 (name, theme_name) == 0)
        {
          g_free (theme_name);
          return;
        }

      g_free (theme_name);
    }

  if (name != NULL && strcmp (name, "") != 0)
    new_theme = meta_theme_new (META_THEME_TYPE_METACITY);
  else
    new_theme = meta_theme_new (META_THEME_TYPE_GTK);

  meta_theme_set_composited (new_theme, composited);
  meta_theme_set_titlebar_font (new_theme, titlebar_font);

  error = NULL;
  if (!meta_theme_load (new_theme, name, &error))
    {
      g_warning (_("Failed to load theme '%s': %s"), name, error->message);
      g_error_free (error);

      g_object_unref (new_theme);
    }
  else
    {
      if (meta_current_theme)
        g_object_unref (meta_current_theme);
      meta_current_theme = new_theme;

      g_debug ("New theme is '%s'", name);
    }
}

double
meta_theme_get_title_scale (MetaTheme     *theme,
                            MetaFrameType  type,
                            MetaFrameFlags flags)
{
  MetaFrameStyle *style;

  g_return_val_if_fail (type < META_FRAME_TYPE_LAST, 1.0);

  style = meta_theme_get_frame_style (theme, type, flags);

  /* Parser is not supposed to allow this currently */
  if (style == NULL)
    return 1.0;

  return style->layout->title_scale;
}

void
meta_theme_draw_frame (MetaTheme              *theme,
                       const gchar            *theme_variant,
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
                       GdkPixbuf              *icon)
{
  MetaFrameGeometry fgeom;
  MetaFrameStyle *style;
  MetaStyleInfo *style_info;

  g_return_if_fail (type < META_FRAME_TYPE_LAST);

  style = meta_theme_get_frame_style (theme, type, flags);

  /* Parser is not supposed to allow this currently */
  if (style == NULL)
    return;

  style_info = meta_theme_get_style_info (theme, theme_variant);

  meta_frame_layout_calc_geometry (style->layout,
                                   style_info,
                                   text_height,
                                   flags,
                                   client_width, client_height,
                                   button_layout,
                                   type,
                                   &fgeom,
                                   theme);

  if (meta_theme_get_theme_type (theme) == META_THEME_TYPE_METACITY)
    {
      meta_frame_style_draw_with_style (style,
                                        style_info,
                                        cr,
                                        &fgeom,
                                        title_layout,
                                        button_states,
                                        mini_icon,
                                        icon);
    }
  else
    {
      meta_frame_style_draw_with_style_gtk (style,
                                            style_info,
                                            cr,
                                            &fgeom,
                                            title_layout,
                                            flags,
                                            button_states,
                                            mini_icon);
    }
}

void
meta_theme_get_frame_borders (MetaTheme        *theme,
                              const gchar      *theme_variant,
                              MetaFrameType     type,
                              int               text_height,
                              MetaFrameFlags    flags,
                              MetaFrameBorders *borders)
{
  MetaFrameStyle *style;
  MetaStyleInfo *style_info;

  g_return_if_fail (type < META_FRAME_TYPE_LAST);

  style = meta_theme_get_frame_style (theme, type, flags);

  meta_frame_borders_clear (borders);

  /* Parser is not supposed to allow this currently */
  if (style == NULL)
    return;

  style_info = meta_theme_get_style_info (theme, theme_variant);

  meta_frame_layout_sync_with_style (style->layout, style_info, flags, theme);

  meta_frame_layout_get_borders (theme, style->layout, text_height,
                                 flags, type, borders);
}

void
meta_theme_calc_geometry (MetaTheme              *theme,
                          const gchar            *theme_variant,
                          MetaFrameType           type,
                          int                     text_height,
                          MetaFrameFlags          flags,
                          int                     client_width,
                          int                     client_height,
                          const MetaButtonLayout *button_layout,
                          MetaFrameGeometry      *fgeom)
{
  MetaFrameStyle *style;
  MetaStyleInfo *style_info;

  g_return_if_fail (type < META_FRAME_TYPE_LAST);

  style = meta_theme_get_frame_style (theme, type, flags);

  /* Parser is not supposed to allow this currently */
  if (style == NULL)
    return;

  style_info = meta_theme_get_style_info (theme, theme_variant);

  meta_frame_layout_calc_geometry (style->layout,
                                   style_info,
                                   text_height,
                                   flags,
                                   client_width, client_height,
                                   button_layout,
                                   type,
                                   fgeom,
                                   theme);
}

MetaFrameType
meta_frame_type_from_string (const char *str)
{
  if (strcmp ("normal", str) == 0)
    return META_FRAME_TYPE_NORMAL;
  else if (strcmp ("dialog", str) == 0)
    return META_FRAME_TYPE_DIALOG;
  else if (strcmp ("modal_dialog", str) == 0)
    return META_FRAME_TYPE_MODAL_DIALOG;
  else if (strcmp ("utility", str) == 0)
    return META_FRAME_TYPE_UTILITY;
  else if (strcmp ("menu", str) == 0)
    return META_FRAME_TYPE_MENU;
  else if (strcmp ("border", str) == 0)
    return META_FRAME_TYPE_BORDER;
  else if (strcmp ("attached", str) == 0)
    return META_FRAME_TYPE_ATTACHED;
  else
    return META_FRAME_TYPE_LAST;
}
