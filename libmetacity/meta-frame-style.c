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

#include "config.h"

#include <glib/gi18n-lib.h>

#include "meta-color-spec-private.h"
#include "meta-draw-op-private.h"
#include "meta-frame-layout-private.h"
#include "meta-frame-style-private.h"
#include "meta-theme.h"
#include "meta-theme-metacity-private.h"

static const char*
meta_frame_state_to_string (MetaFrameState state)
{
  switch (state)
    {
    case META_FRAME_STATE_NORMAL:
      return "normal";
    case META_FRAME_STATE_MAXIMIZED:
      return "maximized";
    case META_FRAME_STATE_TILED_LEFT:
      return "tiled_left";
    case META_FRAME_STATE_TILED_RIGHT:
      return "tiled_right";
    case META_FRAME_STATE_SHADED:
      return "shaded";
    case META_FRAME_STATE_MAXIMIZED_AND_SHADED:
      return "maximized_and_shaded";
    case META_FRAME_STATE_TILED_LEFT_AND_SHADED:
      return "tiled_left_and_shaded";
    case META_FRAME_STATE_TILED_RIGHT_AND_SHADED:
      return "tiled_right_and_shaded";
    case META_FRAME_STATE_LAST:
      break;
    default:
      break;
    }

  return "<unknown>";
}

static const char*
meta_frame_resize_to_string (MetaFrameResize resize)
{
  switch (resize)
    {
    case META_FRAME_RESIZE_NONE:
      return "none";
    case META_FRAME_RESIZE_VERTICAL:
      return "vertical";
    case META_FRAME_RESIZE_HORIZONTAL:
      return "horizontal";
    case META_FRAME_RESIZE_BOTH:
      return "both";
    case META_FRAME_RESIZE_LAST:
      break;
    default:
      break;
    }

  return "<unknown>";
}

static const char*
meta_frame_focus_to_string (MetaFrameFocus focus)
{
  switch (focus)
    {
    case META_FRAME_FOCUS_NO:
      return "no";
    case META_FRAME_FOCUS_YES:
      return "yes";
    case META_FRAME_FOCUS_LAST:
      break;
    default:
      break;
    }

  return "<unknown>";
}

static void
free_button_ops (MetaDrawOpList *op_lists[META_BUTTON_FUNCTION_LAST][META_BUTTON_STATE_LAST])
{
  int i, j;

  for (i = 0; i < META_BUTTON_FUNCTION_LAST; i++)
    {
      for (j = 0; j < META_BUTTON_STATE_LAST; j++)
        {
          if (op_lists[i][j])
            meta_draw_op_list_unref (op_lists[i][j]);
        }
    }
}

static void
free_focus_styles (MetaFrameStyle *focus_styles[META_FRAME_FOCUS_LAST])
{
  int i;

  for (i = 0; i < META_FRAME_FOCUS_LAST; i++)
    {
      if (focus_styles[i])
        meta_frame_style_unref (focus_styles[i]);
    }
}

static gboolean
check_state  (MetaFrameStyleSet  *style_set,
              MetaFrameState      state,
              GError            **error)
{
  int i;

  for (i = 0; i < META_FRAME_FOCUS_LAST; i++)
    {
      if (meta_frame_style_set_get_style (style_set, state,
                                          META_FRAME_RESIZE_NONE, i) == NULL)
        {
          g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                       /* Translators: This error occurs when a <frame> tag is missing
                        * in theme XML.  The "<frame ...>" is intended as a noun phrase,
                        * and the "missing" qualifies it.  You should translate "whatever".
                        */
                       _("Missing <frame state='%s' resize='%s' focus='%s' style='whatever' />"),
                       meta_frame_state_to_string (state),
                       meta_frame_resize_to_string (META_FRAME_RESIZE_NONE),
                       meta_frame_focus_to_string (i));
          return FALSE;
        }
    }

  return TRUE;
}

static const char*
meta_button_function_to_string (MetaButtonFunction function)
{
  switch (function)
    {
      case META_BUTTON_FUNCTION_CLOSE:
        return "close";
      case META_BUTTON_FUNCTION_MAXIMIZE:
        return "maximize";
      case META_BUTTON_FUNCTION_MINIMIZE:
        return "minimize";
      case META_BUTTON_FUNCTION_SHADE:
       return "shade";
      case META_BUTTON_FUNCTION_ABOVE:
        return "above";
      case META_BUTTON_FUNCTION_STICK:
        return "stick";
      case META_BUTTON_FUNCTION_UNSHADE:
        return "unshade";
      case META_BUTTON_FUNCTION_UNABOVE:
        return "unabove";
      case META_BUTTON_FUNCTION_UNSTICK:
        return "unstick";
      case META_BUTTON_FUNCTION_MENU:
        return "menu";
      case META_BUTTON_FUNCTION_LEFT_LEFT_BACKGROUND:
        return "left_left_background";
      case META_BUTTON_FUNCTION_LEFT_MIDDLE_BACKGROUND:
        return "left_middle_background";
      case META_BUTTON_FUNCTION_LEFT_RIGHT_BACKGROUND:
        return "left_right_background";
      case META_BUTTON_FUNCTION_LEFT_SINGLE_BACKGROUND:
        return "left_single_background";
      case META_BUTTON_FUNCTION_RIGHT_LEFT_BACKGROUND:
        return "right_left_background";
      case META_BUTTON_FUNCTION_RIGHT_MIDDLE_BACKGROUND:
        return "right_middle_background";
      case META_BUTTON_FUNCTION_RIGHT_RIGHT_BACKGROUND:
        return "right_right_background";
      case META_BUTTON_FUNCTION_RIGHT_SINGLE_BACKGROUND:
        return "right_single_background";
      case META_BUTTON_FUNCTION_LAST:
        break;
      default:
        break;
    }

  return "<unknown>";
}

static const char*
meta_button_state_to_string (MetaButtonState state)
{
  switch (state)
    {
      case META_BUTTON_STATE_NORMAL:
        return "normal";
      case META_BUTTON_STATE_PRESSED:
        return "pressed";
      case META_BUTTON_STATE_PRELIGHT:
        return "prelight";
      case META_BUTTON_STATE_LAST:
        break;
      default:
        break;
    }

  return "<unknown>";
}

/**
 * Constructor for a MetaFrameStyle.
 *
 * \param parent  The parent style. Data not filled in here will be
 *                looked for in the parent style, and in its parent
 *                style, and so on.
 *
 * \return The newly-constructed style.
 */
MetaFrameStyle *
meta_frame_style_new (MetaFrameStyle *parent)
{
  MetaFrameStyle *style;

  style = g_new0 (MetaFrameStyle, 1);

  style->refcount = 1;

  /* Default alpha is fully opaque */
  style->window_background_alpha = 255;

  style->parent = parent;
  if (parent)
    meta_frame_style_ref (parent);

  return style;
}

/**
 * Increases the reference count of a frame style.
 * If the style is NULL, this is a no-op.
 *
 * \param style  The style.
 */
void
meta_frame_style_ref (MetaFrameStyle *style)
{
  g_return_if_fail (style != NULL);

  style->refcount += 1;
}

void
meta_frame_style_unref (MetaFrameStyle *style)
{
  g_return_if_fail (style != NULL);
  g_return_if_fail (style->refcount > 0);

  style->refcount -= 1;

  if (style->refcount == 0)
    {
      int i;

      free_button_ops (style->buttons);

      for (i = 0; i < META_FRAME_PIECE_LAST; i++)
        if (style->pieces[i])
          meta_draw_op_list_unref (style->pieces[i]);

      if (style->layout)
        meta_frame_layout_unref (style->layout);

      if (style->window_background_color)
        meta_color_spec_free (style->window_background_color);

      /* we hold a reference to any parent style */
      if (style->parent)
        meta_frame_style_unref (style->parent);

      g_free (style);
    }
}

gboolean
meta_frame_style_validate (MetaFrameStyle  *style,
                           guint            current_theme_version,
                           GError         **error)
{
  int i, j;

  g_return_val_if_fail (style != NULL, FALSE);
  g_return_val_if_fail (style->layout != NULL, FALSE);

  for (i = 0; i < META_BUTTON_FUNCTION_LAST; i++)
    {
      /* for now the "positional" buttons are optional */
      if (i >= META_BUTTON_FUNCTION_CLOSE)
        {
          for (j = 0; j < META_BUTTON_STATE_LAST; j++)
            {
              guint earliest_version;

              earliest_version = meta_theme_metacity_earliest_version_with_button (i);

              if (meta_frame_style_get_button (style, i, j) == NULL &&
                  earliest_version <= current_theme_version)
                {
                  g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                               _("<button function='%s' state='%s' draw_ops='whatever'/> must be specified for this frame style"),
                               meta_button_function_to_string (i),
                               meta_button_state_to_string (j));
                  return FALSE;
                }
            }
        }
    }

  return TRUE;
}

MetaDrawOpList *
meta_frame_style_get_button (MetaFrameStyle     *style,
                             MetaButtonFunction  function,
                             MetaButtonState     state)
{
  MetaDrawOpList *op_list;
  MetaFrameStyle *parent;

  parent = style;
  op_list = NULL;

  while (parent && op_list == NULL)
    {
      op_list = parent->buttons[function][state];
      parent = parent->parent;
    }

  /* We fall back to the side buttons if we don't have
   * single button backgrounds, and to middle button
   * backgrounds if we don't have the ones on the sides
   */

  if (op_list == NULL && function == META_BUTTON_FUNCTION_LEFT_SINGLE_BACKGROUND)
    {
      return meta_frame_style_get_button (style,
                                          META_BUTTON_FUNCTION_LEFT_LEFT_BACKGROUND,
                                          state);
    }

  if (op_list == NULL && function == META_BUTTON_FUNCTION_RIGHT_SINGLE_BACKGROUND)
    {
      return meta_frame_style_get_button (style,
                                          META_BUTTON_FUNCTION_RIGHT_RIGHT_BACKGROUND,
                                          state);
    }

  if (op_list == NULL &&
      (function == META_BUTTON_FUNCTION_LEFT_LEFT_BACKGROUND ||
       function == META_BUTTON_FUNCTION_LEFT_RIGHT_BACKGROUND))
    {
      return meta_frame_style_get_button (style,
                                          META_BUTTON_FUNCTION_LEFT_MIDDLE_BACKGROUND,
                                          state);
    }

  if (op_list == NULL &&
      (function == META_BUTTON_FUNCTION_RIGHT_LEFT_BACKGROUND ||
       function == META_BUTTON_FUNCTION_RIGHT_RIGHT_BACKGROUND))
    {
      return meta_frame_style_get_button (style,
                                          META_BUTTON_FUNCTION_RIGHT_MIDDLE_BACKGROUND,
                                          state);
    }

  /* We fall back to normal if no prelight */
  if (op_list == NULL && state == META_BUTTON_STATE_PRELIGHT)
    return meta_frame_style_get_button (style, function, META_BUTTON_STATE_NORMAL);

  return op_list;
}

MetaFrameStyleSet *
meta_frame_style_set_new (MetaFrameStyleSet *parent)
{
  MetaFrameStyleSet *style_set;

  style_set = g_new0 (MetaFrameStyleSet, 1);

  style_set->parent = parent;
  if (parent)
    meta_frame_style_set_ref (parent);

  style_set->refcount = 1;

  return style_set;
}

void
meta_frame_style_set_ref (MetaFrameStyleSet *style_set)
{
  g_return_if_fail (style_set != NULL);

  style_set->refcount += 1;
}

void
meta_frame_style_set_unref (MetaFrameStyleSet *style_set)
{
  g_return_if_fail (style_set != NULL);
  g_return_if_fail (style_set->refcount > 0);

  style_set->refcount -= 1;

  if (style_set->refcount == 0)
    {
      int i;

      for (i = 0; i < META_FRAME_RESIZE_LAST; i++)
        {
          free_focus_styles (style_set->normal_styles[i]);
          free_focus_styles (style_set->shaded_styles[i]);
        }

      free_focus_styles (style_set->maximized_styles);
      free_focus_styles (style_set->tiled_left_styles);
      free_focus_styles (style_set->tiled_right_styles);
      free_focus_styles (style_set->maximized_and_shaded_styles);
      free_focus_styles (style_set->tiled_left_and_shaded_styles);
      free_focus_styles (style_set->tiled_right_and_shaded_styles);

      if (style_set->parent)
        meta_frame_style_set_unref (style_set->parent);

      g_free (style_set);
    }
}

gboolean
meta_frame_style_set_validate (MetaFrameStyleSet  *style_set,
                               GError            **error)
{
  int i, j;

  g_return_val_if_fail (style_set != NULL, FALSE);

  for (i = 0; i < META_FRAME_RESIZE_LAST; i++)
    for (j = 0; j < META_FRAME_FOCUS_LAST; j++)
      if (meta_frame_style_set_get_style (style_set,META_FRAME_STATE_NORMAL,
                                          i, j) == NULL)
        {
          g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                       _("Missing <frame state='%s' resize='%s' focus='%s' style='whatever' />"),
                       meta_frame_state_to_string (META_FRAME_STATE_NORMAL),
                       meta_frame_resize_to_string (i),
                       meta_frame_focus_to_string (j));

          return FALSE;
        }

  if (!check_state (style_set, META_FRAME_STATE_SHADED, error))
    return FALSE;

  if (!check_state (style_set, META_FRAME_STATE_MAXIMIZED, error))
    return FALSE;

  if (!check_state (style_set, META_FRAME_STATE_MAXIMIZED_AND_SHADED, error))
    return FALSE;

  return TRUE;
}

MetaFrameStyle *
meta_frame_style_set_get_style (MetaFrameStyleSet *style_set,
                                MetaFrameState     state,
                                MetaFrameResize    resize,
                                MetaFrameFocus     focus)
{
  MetaFrameStyle *style;

  style = NULL;

  if (state == META_FRAME_STATE_NORMAL || state == META_FRAME_STATE_SHADED)
    {
      if (state == META_FRAME_STATE_SHADED)
        style = style_set->shaded_styles[resize][focus];
      else
        style = style_set->normal_styles[resize][focus];

      /* Try parent if we failed here */
      if (style == NULL && style_set->parent)
        style = meta_frame_style_set_get_style (style_set->parent, state, resize, focus);

      /* Allow people to omit the vert/horz/none resize modes */
      if (style == NULL && resize != META_FRAME_RESIZE_BOTH)
        style = meta_frame_style_set_get_style (style_set, state, META_FRAME_RESIZE_BOTH, focus);
    }
  else
    {
      MetaFrameStyle **styles;

      styles = NULL;

      switch (state)
        {
        case META_FRAME_STATE_MAXIMIZED:
          styles = style_set->maximized_styles;
          break;
        case META_FRAME_STATE_TILED_LEFT:
          styles = style_set->tiled_left_styles;
          break;
        case META_FRAME_STATE_TILED_RIGHT:
          styles = style_set->tiled_right_styles;
          break;
        case META_FRAME_STATE_MAXIMIZED_AND_SHADED:
          styles = style_set->maximized_and_shaded_styles;
          break;
        case META_FRAME_STATE_TILED_LEFT_AND_SHADED:
          styles = style_set->tiled_left_and_shaded_styles;
          break;
        case META_FRAME_STATE_TILED_RIGHT_AND_SHADED:
          styles = style_set->tiled_right_and_shaded_styles;
          break;

        case META_FRAME_STATE_NORMAL:
        case META_FRAME_STATE_SHADED:
          g_assert_not_reached ();
          break;

        case META_FRAME_STATE_LAST:
        default:
          g_assert_not_reached ();
          break;
        }

      style = styles[focus];

      /* Tiled states are optional, try falling back to non-tiled states */
      if (style == NULL)
        {
          if (state == META_FRAME_STATE_TILED_LEFT ||
              state == META_FRAME_STATE_TILED_RIGHT)
            style = meta_frame_style_set_get_style (style_set, META_FRAME_STATE_NORMAL, resize, focus);
          else if (state == META_FRAME_STATE_TILED_LEFT_AND_SHADED ||
                   state == META_FRAME_STATE_TILED_RIGHT_AND_SHADED)
            style = meta_frame_style_set_get_style (style_set, META_FRAME_STATE_SHADED, resize, focus);
        }

      /* Try parent if we failed here */
      if (style == NULL && style_set->parent)
        style = meta_frame_style_set_get_style (style_set->parent, state, resize, focus);
    }

  return style;
}
