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

#include "config.h"

#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

#include "meta-frame-style.h"
#include "meta-theme-gtk-private.h"
#include "meta-theme.h"

struct _MetaThemeGtk
{
  MetaThemeImpl  parent;

  gchar         *name;
};

G_DEFINE_TYPE (MetaThemeGtk, meta_theme_gtk, META_TYPE_THEME_IMPL)

static void
meta_theme_gtk_finalize (GObject *object)
{
  MetaThemeGtk *gtk;

  gtk = META_THEME_GTK (object);

  g_free (gtk->name);

  G_OBJECT_CLASS (meta_theme_gtk_parent_class)->finalize (object);
}

static gboolean
meta_theme_gtk_load (MetaThemeImpl  *impl,
                     const gchar    *name,
                     GError        **error)
{
  MetaThemeGtk *gtk;
  MetaFrameType type;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  gtk = META_THEME_GTK (impl);

  g_free (gtk->name);
  gtk->name = g_strdup (name);

  for (type = 0; type < META_FRAME_TYPE_LAST; type++)
    {
      MetaFrameStyleSet *style_set;
      MetaFrameStyle *style;
      gint i;
      gint j;

      style_set = meta_frame_style_set_new (NULL);

      style = meta_frame_style_new (NULL);
      style->layout = meta_frame_layout_new ();

      switch (type)
        {
          case META_FRAME_TYPE_NORMAL:
            break;

          case META_FRAME_TYPE_DIALOG:
          case META_FRAME_TYPE_MODAL_DIALOG:
          case META_FRAME_TYPE_ATTACHED:
            style->layout->hide_buttons = TRUE;
            break;

          case META_FRAME_TYPE_MENU:
          case META_FRAME_TYPE_UTILITY:
            style->layout->title_scale = PANGO_SCALE_SMALL;
            break;

          case META_FRAME_TYPE_BORDER:
            style->layout->has_title = FALSE;
            style->layout->hide_buttons = TRUE;
            break;

          case META_FRAME_TYPE_LAST:
          default:
            g_assert_not_reached ();
        }

      for (i = 0; i < META_FRAME_FOCUS_LAST; i++)
        {
          for (j = 0; j < META_FRAME_RESIZE_LAST; j++)
            {
              meta_frame_style_ref (style);
              style_set->normal_styles[j][i] = style;

              meta_frame_style_ref (style);
              style_set->shaded_styles[j][i] = style;
            }

          meta_frame_style_ref (style);
          style_set->maximized_styles[i] = style;

          meta_frame_style_ref (style);
          style_set->tiled_left_styles[i] = style;

          meta_frame_style_ref (style);
          style_set->tiled_right_styles[i] = style;

          meta_frame_style_ref (style);
          style_set->maximized_and_shaded_styles[i] = style;

          meta_frame_style_ref (style);
          style_set->tiled_left_and_shaded_styles[i] = style;

          meta_frame_style_ref (style);
          style_set->tiled_right_and_shaded_styles[i] = style;
        }

      meta_frame_style_unref (style);
      meta_theme_impl_add_style_set (impl, type, style_set);
    }

  return TRUE;
}

static gchar *
meta_theme_gtk_get_name (MetaThemeImpl *impl)
{
  MetaThemeGtk *gtk;

  gtk = META_THEME_GTK (impl);

  return gtk->name;
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
get_min_size (GtkStyleContext *style,
              GtkRequisition  *requisition)
{
  gtk_style_context_get (style, gtk_style_context_get_state (style),
                         "min-width", &requisition->width,
                         "min-height", &requisition->height,
                         NULL);
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
get_shadow_extents (GtkStyleContext *style,
                    GtkBorder       *border)
{
  GdkRectangle clip;

  gtk_render_background_get_clip (style, 0, 0, 0, 0, &clip);

  border->left = abs (clip.x);
  border->top = abs (clip.y);
  border->right = clip.width - border->left;
  border->bottom = clip.height - border->bottom;
}

static void
frame_layout_sync_with_style (MetaFrameLayout *layout,
                              MetaStyleInfo   *style_info,
                              gboolean         composited,
                              MetaFrameFlags   flags)
{
  GtkStyleContext *style;
  GtkBorder border;
  GtkRequisition requisition;
  int border_radius, max_radius;

  meta_style_info_set_flags (style_info, flags);

  style = style_info->styles[META_STYLE_ELEMENT_DECORATION];
  get_padding_and_border (style, &layout->gtk.frame_border);
  scale_border (&layout->gtk.frame_border, layout->title_scale);

  if (composited)
    {
      /* With compositing manager: margin is resize area */
      get_margin (style, &layout->invisible_resize_border);
      get_shadow_extents (style, &layout->gtk.shadow_border);

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
      max_radius = MIN (layout->gtk.frame_border.bottom, layout->gtk.frame_border.left);
      layout->bottom_left_corner_rounded_radius = MAX (border_radius, max_radius);
      max_radius = MIN (layout->gtk.frame_border.bottom, layout->gtk.frame_border.left);
      layout->bottom_right_corner_rounded_radius = MAX (border_radius, max_radius);
    }
  else
    {
      /* Without compositing manager we can not have invisible border */
      layout->invisible_resize_border.top = 0;
      layout->invisible_resize_border.bottom = 0;
      layout->invisible_resize_border.left = 0;
      layout->invisible_resize_border.right = 0;

      layout->gtk.shadow_border.top = 0;
      layout->gtk.shadow_border.bottom = 0;
      layout->gtk.shadow_border.left = 0;
      layout->gtk.shadow_border.right = 0;

      layout->top_left_corner_rounded_radius = 0;
      layout->top_right_corner_rounded_radius = 0;
      layout->bottom_left_corner_rounded_radius = 0;
      layout->bottom_right_corner_rounded_radius = 0;

      /* Without compositing manager: margin is part of border */
      get_margin (style, &border);

      layout->gtk.frame_border.left += border.left;
      layout->gtk.frame_border.right += border.right;
      layout->gtk.frame_border.top += border.top;
      layout->gtk.frame_border.bottom += border.bottom;
    }

  if (layout->hide_buttons)
    layout->gtk.icon_size = 0;

  if (!layout->has_title && layout->hide_buttons)
    return; /* border-only - be done */

  style = style_info->styles[META_STYLE_ELEMENT_TITLEBAR];
  get_min_size (style, &layout->gtk.titlebar_min_size);
  get_padding_and_border (style, &layout->gtk.titlebar_border);
  scale_border (&layout->gtk.titlebar_border, layout->title_scale);

  style = style_info->styles[META_STYLE_ELEMENT_TITLE];
  get_margin (style, &layout->gtk.title_margin);
  scale_border (&layout->gtk.title_margin, layout->title_scale);

  style = style_info->styles[META_STYLE_ELEMENT_BUTTON];
  get_min_size (style, &layout->gtk.button_min_size);
  get_padding_and_border (style, &layout->button_border);
  scale_border (&layout->button_border, layout->title_scale);

  get_margin (style, &layout->gtk.button_margin);
  scale_border (&layout->gtk.button_margin, layout->title_scale);

  style = style_info->styles[META_STYLE_ELEMENT_IMAGE];
  get_min_size (style, &requisition);
  get_padding_and_border (style, &border);
  scale_border (&border, layout->title_scale);

  layout->button_border.left += border.left;
  layout->button_border.right += border.right;
  layout->button_border.top += border.top;
  layout->button_border.bottom += border.bottom;

  get_margin (style, &border);
  layout->button_border.left += border.left;
  layout->button_border.right += border.right;
  layout->button_border.top += border.top;
  layout->button_border.bottom += border.bottom;

  layout->gtk.button_min_size.width = MAX (layout->gtk.button_min_size.width,
                                           requisition.width);
  layout->gtk.button_min_size.height = MAX (layout->gtk.button_min_size.height,
                                            requisition.height);
}

static void
meta_theme_gtk_get_frame_borders (MetaThemeImpl    *impl,
                                  MetaFrameLayout  *layout,
                                  MetaStyleInfo    *style_info,
                                  gboolean          composited,
                                  gint              text_height,
                                  MetaFrameFlags    flags,
                                  MetaFrameType     type,
                                  MetaFrameBorders *borders)
{
  gint buttons_height;
  gint content_height;

  frame_layout_sync_with_style (layout, style_info, composited, flags);

  meta_frame_borders_clear (borders);

  /* For a full-screen window, we don't have any borders, visible or not. */
  if (flags & META_FRAME_FULLSCREEN)
    return;

  g_return_if_fail (layout != NULL);

  if (!layout->has_title)
    text_height = 0;
  else
    text_height = layout->gtk.title_margin.top +
                   text_height +
                  layout->gtk.title_margin.bottom;

  buttons_height = MAX ((gint) layout->gtk.icon_size, layout->gtk.button_min_size.height) +
                   layout->gtk.button_margin.top + layout->button_border.top +
                   layout->gtk.button_margin.bottom + layout->button_border.bottom;

  content_height = MAX (buttons_height, text_height);
  content_height = MAX (content_height, layout->gtk.titlebar_min_size.height) +
                   layout->gtk.titlebar_border.top +
                   layout->gtk.titlebar_border.bottom;

  borders->visible.top = layout->gtk.frame_border.top + content_height;
  borders->visible.left = layout->gtk.frame_border.left;
  borders->visible.right = layout->gtk.frame_border.right;
  borders->visible.bottom = layout->gtk.frame_border.bottom;

  borders->invisible = layout->gtk.shadow_border;

  if (flags & META_FRAME_ALLOWS_HORIZONTAL_RESIZE)
    {
      borders->invisible.left = MAX (borders->invisible.left,
                                     layout->invisible_resize_border.left);

      borders->invisible.right = MAX (borders->invisible.right,
                                      layout->invisible_resize_border.right);
    }

  if (flags & META_FRAME_ALLOWS_VERTICAL_RESIZE)
    {
      borders->invisible.bottom = MAX (borders->invisible.bottom,
                                       layout->invisible_resize_border.bottom);

      if (type != META_FRAME_TYPE_ATTACHED)
        borders->invisible.top = MAX (borders->invisible.top,
                                      layout->invisible_resize_border.top);
    }

  borders->total.left = borders->invisible.left + borders->visible.left;
  borders->total.right = borders->invisible.right + borders->visible.right;
  borders->total.bottom = borders->invisible.bottom + borders->visible.bottom;
  borders->total.top = borders->invisible.top + borders->visible.top;
}

static MetaButtonSpace *
rect_for_function (MetaFrameGeometry  *fgeom,
                   MetaFrameFlags      flags,
                   MetaButtonFunction  function)
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

  return NULL;
}

static gboolean
strip_button (MetaButtonSpace *func_rects[META_BUTTON_FUNCTION_LAST],
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

              ++i;
            }

          func_rects[i] = NULL;

          return TRUE;
        }

      ++i;
    }

  return FALSE; /* did not strip anything */
}

static void
meta_theme_gtk_calc_geometry (MetaThemeImpl          *impl,
                              MetaFrameLayout        *layout,
                              MetaStyleInfo          *style_info,
                              gboolean                composited,
                              gint                    text_height,
                              MetaFrameFlags          flags,
                              gint                    client_width,
                              gint                    client_height,
                              const MetaButtonLayout *button_layout,
                              MetaFrameType           type,
                              MetaFrameGeometry      *fgeom)
{
  MetaFrameBorders borders;
  int i, n_left, n_right, n_left_spacers, n_right_spacers;
  int x;
  int button_y;
  int title_right_edge;
  int width, height;
  int content_width, content_height;
  int button_width, button_height;
  int min_size_for_rounding;

  /* the left/right rects in order; the max # of rects
   * is the number of button functions
   */
  MetaButtonSpace *left_func_rects[META_BUTTON_FUNCTION_LAST];
  MetaButtonSpace *right_func_rects[META_BUTTON_FUNCTION_LAST];
  gboolean left_buttons_has_spacer[META_BUTTON_FUNCTION_LAST];
  gboolean right_buttons_has_spacer[META_BUTTON_FUNCTION_LAST];

  META_THEME_IMPL_GET_CLASS (impl)->get_frame_borders (impl, layout,
                                                       style_info, composited,
                                                       text_height, flags,
                                                       type, &borders);

  fgeom->borders = borders;

  fgeom->content_border = layout->gtk.frame_border;
  fgeom->content_border.left += layout->gtk.titlebar_border.left;
  fgeom->content_border.right += layout->gtk.titlebar_border.right;
  fgeom->content_border.top += layout->gtk.titlebar_border.top;
  fgeom->content_border.bottom += layout->gtk.titlebar_border.bottom;

  width = client_width + borders.total.left + borders.total.right;

  height = ((flags & META_FRAME_SHADED) ? 0: client_height) +
    borders.total.top + borders.total.bottom;

  fgeom->width = width;
  fgeom->height = height;

  content_width = width -
                  (fgeom->content_border.left + borders.invisible.left) -
                  (fgeom->content_border.right + borders.invisible.right);
  content_height = borders.visible.top - fgeom->content_border.top - fgeom->content_border.bottom;

  button_width = MAX ((gint) layout->gtk.icon_size, layout->gtk.button_min_size.width) +
                 layout->button_border.left + layout->button_border.right;
  button_height = MAX ((gint) layout->gtk.icon_size, layout->gtk.button_min_size.height) +
                  layout->button_border.top + layout->button_border.bottom;

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
                                                       button_layout->left_buttons[i]);
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
                                                         button_layout->right_buttons[i]);
          if (right_func_rects[n_right] != NULL)
            {
              right_buttons_has_spacer[n_right] = button_layout->right_buttons_has_spacer[i];
              if (button_layout->right_buttons_has_spacer[i])
                ++n_right_spacers;

              ++n_right;
            }
        }
    }

  /* Be sure buttons fit */
  while (n_left > 0 || n_right > 0)
    {
      int space_used_by_buttons;

      space_used_by_buttons = 0;

      space_used_by_buttons += layout->gtk.button_margin.left /** scale*/ * n_left;
      space_used_by_buttons += button_width * n_left;
      space_used_by_buttons += layout->gtk.button_margin.right /** scale*/ * n_left;
      space_used_by_buttons += (button_width * 0.75) * n_left_spacers;
      space_used_by_buttons += layout->gtk.titlebar_spacing * MAX (n_left - 1, 0);

      space_used_by_buttons += layout->gtk.button_margin.left /** scale*/ * n_right;
      space_used_by_buttons += button_width * n_right;
      space_used_by_buttons += layout->gtk.button_margin.right /** scale*/ * n_right;
      space_used_by_buttons += (button_width * 0.75) * n_right_spacers;
      space_used_by_buttons += layout->gtk.titlebar_spacing * MAX (n_right - 1, 0);

      if (space_used_by_buttons <= content_width)
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
      if (strip_button (left_func_rects, &n_left, &fgeom->above_rect))
        continue;
      else if (strip_button (right_func_rects, &n_right, &fgeom->above_rect))
        continue;
      else if (strip_button (left_func_rects, &n_left, &fgeom->stick_rect))
        continue;
      else if (strip_button (right_func_rects, &n_right, &fgeom->stick_rect))
        continue;
      else if (strip_button (left_func_rects, &n_left, &fgeom->shade_rect))
        continue;
      else if (strip_button (right_func_rects, &n_right, &fgeom->shade_rect))
        continue;
      else if (strip_button (left_func_rects, &n_left, &fgeom->min_rect))
        continue;
      else if (strip_button (right_func_rects, &n_right, &fgeom->min_rect))
        continue;
      else if (strip_button (left_func_rects, &n_left, &fgeom->max_rect))
        continue;
      else if (strip_button (right_func_rects, &n_right, &fgeom->max_rect))
        continue;
      else if (strip_button (left_func_rects, &n_left, &fgeom->close_rect))
        continue;
      else if (strip_button (right_func_rects, &n_right, &fgeom->close_rect))
        continue;
      else if (strip_button (right_func_rects, &n_right, &fgeom->menu_rect))
        continue;
      else if (strip_button (left_func_rects, &n_left, &fgeom->menu_rect))
        continue;
      else if (strip_button (right_func_rects, &n_right, &fgeom->appmenu_rect))
        continue;
      else if (strip_button (left_func_rects, &n_left, &fgeom->appmenu_rect))
        continue;
      else
        {
          g_error ("Could not find a button to strip. n_left = %d n_right = %d",
                   n_left, n_right);
        }
    }

  /* Save the button layout */
  fgeom->button_layout = *button_layout;
  fgeom->n_left_buttons = n_left;
  fgeom->n_right_buttons = n_right;

  /* center buttons vertically */
  button_y = fgeom->content_border.top + borders.invisible.top +
             MAX(content_height - button_height, 0) / 2;

  /* right edge of farthest-right button */
  x = width - fgeom->content_border.right - borders.invisible.right;

  i = n_right - 1;
  while (i >= 0)
    {
      MetaButtonSpace *rect;

      if (x < 0) /* if we go negative, leave the buttons we don't get to as 0-width */
        break;

      x -= layout->gtk.button_margin.right /** scale*/;

      rect = right_func_rects[i];

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
            rect->clickable.width += fgeom->content_border.right;
        }
      else
        g_memmove (&(rect->clickable), &(rect->visible), sizeof(rect->clickable));

      x = rect->visible.x - layout->gtk.button_margin.left /** scale*/;

      if (i > 0)
        x -= layout->gtk.titlebar_spacing;

      --i;
    }

  /* save right edge of titlebar for later use */
  title_right_edge = x;

  /* Now x changes to be position from the left and we go through
   * the left-side buttons
   */
  x = fgeom->content_border.left + borders.invisible.left;
  for (i = 0; i < n_left; i++)
    {
      MetaButtonSpace *rect;

      x += layout->gtk.button_margin.left /** scale*/;

      rect = left_func_rects[i];

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

      x = rect->visible.x + rect->visible.width + layout->gtk.button_margin.right /** scale*/;
      if (i < n_left - 1)
        x += layout->gtk.titlebar_spacing;
      if (left_buttons_has_spacer[i])
        x += (button_width * 0.75);
    }

  /* Center vertically in the available content area */
  fgeom->title_rect.x = x;
  fgeom->title_rect.y = fgeom->content_border.top + borders.invisible.top +
                        (content_height - text_height) / 2;
  fgeom->title_rect.width = title_right_edge - fgeom->title_rect.x;
  fgeom->title_rect.height = text_height;

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

static void
meta_theme_gtk_draw_frame (MetaThemeImpl           *impl,
                           MetaFrameStyle          *style,
                           MetaStyleInfo           *style_info,
                           cairo_t                 *cr,
                           const MetaFrameGeometry *fgeom,
                           PangoLayout             *title_layout,
                           MetaFrameFlags           flags,
                           MetaButtonState          button_states[META_BUTTON_TYPE_LAST],
                           GdkPixbuf               *mini_icon,
                           GdkPixbuf               *icon)
{
  GtkStyleContext *context;
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

  context = style_info->styles[META_STYLE_ELEMENT_DECORATION];
  gtk_render_background (context, cr,
                         visible_rect.x, visible_rect.y,
                         visible_rect.width, visible_rect.height);
  gtk_render_frame (context, cr,
                    visible_rect.x, visible_rect.y,
                    visible_rect.width, visible_rect.height);

  titlebar_rect.x = visible_rect.x + borders->visible.left;
  titlebar_rect.y = visible_rect.y + fgeom->content_border.top;
  titlebar_rect.width = visible_rect.width - borders->visible.left - borders->visible.right;
  titlebar_rect.height = borders->visible.top - fgeom->content_border.top;

  context = style_info->styles[META_STYLE_ELEMENT_TITLEBAR];
  gtk_render_background (context, cr,
                         titlebar_rect.x, titlebar_rect.y,
                         titlebar_rect.width, titlebar_rect.height);
  gtk_render_frame (context, cr,
                    titlebar_rect.x, titlebar_rect.y,
                    titlebar_rect.width, titlebar_rect.height);

  if (style->layout->has_title && title_layout)
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

      context = style_info->styles[META_STYLE_ELEMENT_TITLE];
      gtk_render_layout (context, cr, x, y, title_layout);
    }

  context = style_info->styles[META_STYLE_ELEMENT_BUTTON];
  state = gtk_style_context_get_state (context);
  for (button_type = META_BUTTON_TYPE_CLOSE; button_type < META_BUTTON_TYPE_LAST; button_type++)
    {
      MetaButtonState button_state;
      const char *button_class;

      button_class = get_class_from_button_type (button_type);

      if (button_class)
        gtk_style_context_add_class (context, button_class);

      get_button_rect (button_type, fgeom, 0, &button_rect);

      button_state = map_button_state (button_type, fgeom, 0, button_states);

      if (button_state == META_BUTTON_STATE_PRELIGHT)
        gtk_style_context_set_state (context, state | GTK_STATE_PRELIGHT);
      else if (button_state == META_BUTTON_STATE_PRESSED)
        gtk_style_context_set_state (context, state | GTK_STATE_ACTIVE);
      else
        gtk_style_context_set_state (context, state);

      cairo_save (cr);

      if (button_rect.width > 0 && button_rect.height > 0)
        {
          GdkPixbuf *pixbuf = NULL;
          const char *icon_name = NULL;

          gtk_render_background (context, cr,
                                 button_rect.x, button_rect.y,
                                 button_rect.width, button_rect.height);
          gtk_render_frame (context, cr,
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

              info = gtk_icon_theme_lookup_icon (theme, icon_name, style->layout->gtk.icon_size, 0);
              pixbuf = gtk_icon_info_load_symbolic_for_context (info, context, NULL, NULL);
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
                           width / style->layout->gtk.icon_size,
                           height / style->layout->gtk.icon_size);
              gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
                                           cairo_paint (cr);

              g_object_unref (pixbuf);
            }
        }

      cairo_restore (cr);

      if (button_class)
        gtk_style_context_remove_class (context, button_class);
    }
}

static void
meta_theme_gtk_class_init (MetaThemeGtkClass *gtk_class)
{
  GObjectClass *object_class;
  MetaThemeImplClass *impl_class;

  object_class = G_OBJECT_CLASS (gtk_class);
  impl_class = META_THEME_IMPL_CLASS (gtk_class);

  object_class->finalize = meta_theme_gtk_finalize;

  impl_class->load = meta_theme_gtk_load;
  impl_class->get_name = meta_theme_gtk_get_name;
  impl_class->get_frame_borders = meta_theme_gtk_get_frame_borders;
  impl_class->calc_geometry = meta_theme_gtk_calc_geometry;
  impl_class->draw_frame = meta_theme_gtk_draw_frame;
}

static void
meta_theme_gtk_init (MetaThemeGtk *gtk)
{
}
