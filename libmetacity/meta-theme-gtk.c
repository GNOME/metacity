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

#include "meta-rectangle-private.h"
#include "meta-frame-layout-private.h"
#include "meta-frame-style-private.h"
#include "meta-theme-gtk-private.h"
#include "meta-theme.h"

struct _MetaThemeGtk
{
  MetaThemeImpl parent;
};

G_DEFINE_TYPE (MetaThemeGtk, meta_theme_gtk, META_TYPE_THEME_IMPL)

static gboolean
meta_theme_gtk_load (MetaThemeImpl  *impl,
                     const gchar    *name,
                     GError        **error)
{
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return TRUE;
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
get_shadow_extents (GtkStyleContext *style,
                    GtkBorder       *border)
{
  GdkRectangle clip;

  gtk_render_background_get_clip (style, 0, 0, 0, 0, &clip);

  border->left = abs (clip.x);
  border->top = abs (clip.y);
  border->right = clip.width - border->left;
  border->bottom = clip.height - border->top;
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

  style = meta_style_info_get_style (style_info, META_STYLE_ELEMENT_DECORATION);
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

  style = meta_style_info_get_style (style_info, META_STYLE_ELEMENT_TITLEBAR);
  get_min_size (style, &layout->gtk.titlebar_min_size);
  get_padding_and_border (style, &layout->gtk.titlebar_border);
  scale_border (&layout->gtk.titlebar_border, layout->title_scale);

  style = meta_style_info_get_style (style_info, META_STYLE_ELEMENT_TITLE);
  get_margin (style, &layout->gtk.title_margin);
  scale_border (&layout->gtk.title_margin, layout->title_scale);

  style = meta_style_info_get_style (style_info, META_STYLE_ELEMENT_BUTTON);
  get_min_size (style, &layout->gtk.button_min_size);
  get_padding_and_border (style, &layout->button_border);
  scale_border (&layout->button_border, layout->title_scale);

  get_margin (style, &layout->gtk.button_margin);
  scale_border (&layout->gtk.button_margin, layout->title_scale);

  style = meta_style_info_get_style (style_info, META_STYLE_ELEMENT_IMAGE);
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
                                  gint              text_height,
                                  MetaFrameFlags    flags,
                                  MetaFrameType     type,
                                  MetaFrameBorders *borders)
{
  gboolean composited;
  gint scale;
  gint title_height;
  gint buttons_height;
  gint content_height;

  composited = meta_theme_impl_get_composited (impl);
  frame_layout_sync_with_style (layout, style_info, composited, flags);

  meta_frame_borders_clear (borders);

  /* For a full-screen window, we don't have any borders, visible or not. */
  if (flags & META_FRAME_FULLSCREEN)
    return;

  g_return_if_fail (layout != NULL);

  if (!layout->has_title)
    text_height = 0;

  /* Scale geometry for HiDPI, see comment in meta_theme_gtk_draw_frame () */
  scale = get_window_scaling_factor ();

  title_height = layout->gtk.title_margin.top +
                 text_height / scale +
                 layout->gtk.title_margin.bottom;

  buttons_height = MAX ((gint) layout->gtk.icon_size, layout->gtk.button_min_size.height) +
                   layout->gtk.button_margin.top + layout->button_border.top +
                   layout->gtk.button_margin.bottom + layout->button_border.bottom;

  content_height = MAX (title_height, buttons_height);
  content_height = MAX (content_height, layout->gtk.titlebar_min_size.height) +
                   layout->gtk.titlebar_border.top +
                   layout->gtk.titlebar_border.bottom;

  borders->visible.top = layout->gtk.frame_border.top + content_height;
  borders->visible.left = layout->gtk.frame_border.left;
  borders->visible.right = layout->gtk.frame_border.right;
  borders->visible.bottom = layout->gtk.frame_border.bottom;

  borders->shadow = layout->gtk.shadow_border;

  if (flags & META_FRAME_ALLOWS_HORIZONTAL_RESIZE)
    {
      borders->resize.left = layout->invisible_resize_border.left;
      borders->resize.right = layout->invisible_resize_border.right;
    }

  if (flags & META_FRAME_ALLOWS_VERTICAL_RESIZE)
    {
      borders->resize.bottom = layout->invisible_resize_border.bottom;

      if (type != META_FRAME_TYPE_ATTACHED)
        borders->resize.top = layout->invisible_resize_border.top;
    }

  borders->invisible.left = MAX (borders->shadow.left, borders->resize.left);
  borders->invisible.right = MAX (borders->shadow.right, borders->resize.right);
  borders->invisible.bottom = MAX (borders->shadow.bottom, borders->resize.bottom);
  borders->invisible.top = MAX (borders->shadow.top, borders->resize.top);

  borders->total.left = borders->invisible.left + borders->visible.left;
  borders->total.right = borders->invisible.right + borders->visible.right;
  borders->total.bottom = borders->invisible.bottom + borders->visible.bottom;
  borders->total.top = borders->invisible.top + borders->visible.top;

  scale_border (&borders->visible, scale);
  scale_border (&borders->shadow, scale);
  scale_border (&borders->resize, scale);
  scale_border (&borders->invisible, scale);
  scale_border (&borders->total, scale);
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
  int scale;
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
                                                       style_info, text_height,
                                                       flags, type, &borders);

  fgeom->borders = borders;

  width = client_width + borders.total.left + borders.total.right;

  height = borders.total.top + borders.total.bottom;
  if (!(flags & META_FRAME_SHADED))
    height += client_height;

  fgeom->width = width;
  fgeom->height = height;

  /* Scale geometry for HiDPI, see comment in meta_theme_gtk_draw_frame () */
  scale = get_window_scaling_factor ();

  content_width = width -
                  borders.invisible.left - layout->gtk.frame_border.left * scale -
                  borders.invisible.right - layout->gtk.frame_border.right * scale;
  content_height = borders.visible.top - layout->gtk.frame_border.top * scale;

  button_width = MAX ((gint) layout->gtk.icon_size, layout->gtk.button_min_size.width) +
                 layout->button_border.left + layout->button_border.right;
  button_height = MAX ((gint) layout->gtk.icon_size, layout->gtk.button_min_size.height) +
                  layout->button_border.top + layout->button_border.bottom;

  button_width *= scale;
  button_height *= scale;

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

      space_used_by_buttons += layout->gtk.button_margin.left * scale * n_left;
      space_used_by_buttons += button_width * n_left;
      space_used_by_buttons += layout->gtk.button_margin.right * scale * n_left;
      space_used_by_buttons += (button_width * 0.75) * n_left_spacers;
      space_used_by_buttons += layout->gtk.titlebar_spacing * scale * MAX (n_left - 1, 0);

      space_used_by_buttons += layout->gtk.button_margin.left * scale * n_right;
      space_used_by_buttons += button_width * n_right;
      space_used_by_buttons += layout->gtk.button_margin.right * scale * n_right;
      space_used_by_buttons += (button_width * 0.75) * n_right_spacers;
      space_used_by_buttons += layout->gtk.titlebar_spacing * scale * MAX (n_right - 1, 0);

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
  button_y = borders.invisible.top + layout->gtk.frame_border.top * scale +
             (content_height - button_height) / 2;

  /* right edge of farthest-right button */
  x = width - borders.invisible.right - layout->gtk.frame_border.right * scale -
      layout->gtk.titlebar_border.right * scale;

  i = n_right - 1;
  while (i >= 0)
    {
      MetaButtonSpace *rect;

      if (x < 0) /* if we go negative, leave the buttons we don't get to as 0-width */
        break;

      x -= layout->gtk.button_margin.right * scale;

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
            rect->clickable.width += layout->gtk.frame_border.right * scale +
                                     layout->gtk.titlebar_border.right * scale;
        }
      else
        g_memmove (&(rect->clickable), &(rect->visible), sizeof(rect->clickable));

      x = rect->visible.x - layout->gtk.button_margin.left * scale;

      if (i > 0)
        x -= layout->gtk.titlebar_spacing * scale;

      --i;
    }

  /* save right edge of titlebar for later use */
  title_right_edge = x;

  /* Now x changes to be position from the left and we go through
   * the left-side buttons
   */
  x = borders.invisible.left + layout->gtk.frame_border.left * scale +
      layout->gtk.titlebar_border.left * scale;

  for (i = 0; i < n_left; i++)
    {
      MetaButtonSpace *rect;

      x += layout->gtk.button_margin.left * scale;

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

      x = rect->visible.x + rect->visible.width + layout->gtk.button_margin.right * scale;
      if (i < n_left - 1)
        x += layout->gtk.titlebar_spacing * scale;
      if (left_buttons_has_spacer[i])
        x += (button_width * 0.75);
    }

  /* Center vertically in the available content area */
  fgeom->title_rect.x = x;
  fgeom->title_rect.y = borders.invisible.top + layout->gtk.frame_border.top * scale +
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
    min_size_for_rounding = 5 * scale;

  fgeom->top_left_corner_rounded_radius = 0;
  fgeom->top_right_corner_rounded_radius = 0;
  fgeom->bottom_left_corner_rounded_radius = 0;
  fgeom->bottom_right_corner_rounded_radius = 0;

  if (borders.visible.top + borders.visible.left >= min_size_for_rounding)
    fgeom->top_left_corner_rounded_radius = layout->top_left_corner_rounded_radius * scale;
  if (borders.visible.top + borders.visible.right >= min_size_for_rounding)
    fgeom->top_right_corner_rounded_radius = layout->top_right_corner_rounded_radius * scale;

  if (borders.visible.bottom + borders.visible.left >= min_size_for_rounding)
    fgeom->bottom_left_corner_rounded_radius = layout->bottom_left_corner_rounded_radius * scale;
  if (borders.visible.bottom + borders.visible.right >= min_size_for_rounding)
    fgeom->bottom_right_corner_rounded_radius = layout->bottom_right_corner_rounded_radius * scale;
}

static const char *
get_class_from_button_function (MetaButtonFunction function)
{
  if (function == META_BUTTON_FUNCTION_CLOSE)
    return "close";
  else if (function == META_BUTTON_FUNCTION_MAXIMIZE)
    return "maximize";
  else if (function == META_BUTTON_FUNCTION_MINIMIZE)
    return "minimize";

  return NULL;
}

static void
get_button_rect (MetaButtonFunction       function,
                 const MetaFrameGeometry *fgeom,
                 GdkRectangle            *rect)
{
  switch (function)
    {
    case META_BUTTON_FUNCTION_CLOSE:
      *rect = fgeom->close_rect.visible;
      break;

    case META_BUTTON_FUNCTION_SHADE:
      *rect = fgeom->shade_rect.visible;
      break;

    case META_BUTTON_FUNCTION_UNSHADE:
      *rect = fgeom->unshade_rect.visible;
      break;

    case META_BUTTON_FUNCTION_ABOVE:
      *rect = fgeom->above_rect.visible;
      break;

    case META_BUTTON_FUNCTION_UNABOVE:
      *rect = fgeom->unabove_rect.visible;
      break;

    case META_BUTTON_FUNCTION_STICK:
      *rect = fgeom->stick_rect.visible;
      break;

    case META_BUTTON_FUNCTION_UNSTICK:
      *rect = fgeom->unstick_rect.visible;
      break;

    case META_BUTTON_FUNCTION_MAXIMIZE:
      *rect = fgeom->max_rect.visible;
      break;

    case META_BUTTON_FUNCTION_MINIMIZE:
      *rect = fgeom->min_rect.visible;
      break;

    case META_BUTTON_FUNCTION_MENU:
      *rect = fgeom->menu_rect.visible;
      break;

    case META_BUTTON_FUNCTION_APPMENU:
      *rect = fgeom->appmenu_rect.visible;
      break;

    case META_BUTTON_FUNCTION_LAST:
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
meta_theme_gtk_draw_frame (MetaThemeImpl           *impl,
                           MetaFrameStyle          *style,
                           MetaStyleInfo           *style_info,
                           cairo_t                 *cr,
                           const MetaFrameGeometry *fgeom,
                           PangoLayout             *title_layout,
                           MetaFrameFlags           flags,
                           MetaButtonState          button_states[META_BUTTON_FUNCTION_LAST],
                           GdkPixbuf               *mini_icon,
                           GdkPixbuf               *icon)
{
  gdouble scale;
  GtkStyleContext *context;
  GtkStateFlags state;
  MetaButtonFunction button_function;
  MetaRectangleDouble visible_rect;
  MetaRectangleDouble titlebar_rect;
  const MetaFrameBorders *borders;

  /* We opt out of GTK+ HiDPI handling, so we have to do the scaling
   * ourselves; the nitty-gritty is a bit confusing, so here is an overview:
   *  - the values in MetaFrameLayout are always as they appear in the theme,
   *    i.e. unscaled
   *  - calculated values (borders, MetaFrameGeometry) include the scale - as
   *    the geometry is comprised of scaled decorations and the client size
   *    which we must not scale, we don't have another option
   *  - for drawing, we scale the canvas to have GTK+ render elements (borders,
   *    radii, ...) at the correct scale - as a result, we have to "unscale"
   *    the geometry again to not apply the scaling twice
   */
  scale = get_window_scaling_factor ();
  cairo_scale (cr, scale, scale);

  borders = &fgeom->borders;

  visible_rect.x = borders->invisible.left / scale;
  visible_rect.y = borders->invisible.top / scale;
  visible_rect.width = (fgeom->width - borders->invisible.left - borders->invisible.right) / scale;
  visible_rect.height = (fgeom->height - borders->invisible.top - borders->invisible.bottom) / scale;

  meta_style_info_set_flags (style_info, flags);

  context = meta_style_info_get_style (style_info, META_STYLE_ELEMENT_DECORATION);
  gtk_render_background (context, cr,
                         visible_rect.x, visible_rect.y,
                         visible_rect.width, visible_rect.height);
  gtk_render_frame (context, cr,
                    visible_rect.x, visible_rect.y,
                    visible_rect.width, visible_rect.height);

  if (flags & META_FRAME_FULLSCREEN)
    return;

  titlebar_rect.x = visible_rect.x + borders->visible.left / scale;
  titlebar_rect.y = visible_rect.y + style->layout->gtk.frame_border.top;
  titlebar_rect.width = visible_rect.width - (borders->visible.left + borders->visible.right) / scale;
  titlebar_rect.height = (borders->visible.top / scale) - style->layout->gtk.frame_border.top;

  context = meta_style_info_get_style (style_info, META_STYLE_ELEMENT_TITLEBAR);
  gtk_render_background (context, cr,
                         titlebar_rect.x, titlebar_rect.y,
                         titlebar_rect.width, titlebar_rect.height);
  gtk_render_frame (context, cr,
                    titlebar_rect.x, titlebar_rect.y,
                    titlebar_rect.width, titlebar_rect.height);

  if (style->layout->has_title && title_layout)
    {
      PangoRectangle logical;
      gdouble text_width, x, y;

      pango_layout_set_width (title_layout, -1);
      pango_layout_get_pixel_extents (title_layout, NULL, &logical);

      text_width = MIN(fgeom->title_rect.width / scale, logical.width);

      if (text_width < logical.width)
        pango_layout_set_width (title_layout, PANGO_SCALE * text_width);

      /* Center within the frame if possible */
      x = titlebar_rect.x + (titlebar_rect.width - text_width) / 2;
      y = titlebar_rect.y + (titlebar_rect.height - logical.height) / 2;

      if (x < fgeom->title_rect.x / scale)
        x = fgeom->title_rect.x / scale;
      else if (x + text_width > (fgeom->title_rect.x + fgeom->title_rect.width) / scale)
        x = (fgeom->title_rect.x + fgeom->title_rect.width) / scale - text_width;

      context = meta_style_info_get_style (style_info, META_STYLE_ELEMENT_TITLE);
      gtk_render_layout (context, cr, x, y, title_layout);
    }

  context = meta_style_info_get_style (style_info, META_STYLE_ELEMENT_BUTTON);
  state = gtk_style_context_get_state (context);
  for (button_function = 0; button_function < META_BUTTON_FUNCTION_LAST; button_function++)
    {
      MetaButtonState button_state;
      const char *button_class;
      GdkRectangle tmp_rect;
      MetaRectangleDouble button_rect;

      button_class = get_class_from_button_function (button_function);

      if (button_class)
        gtk_style_context_add_class (context, button_class);

      button_state = button_states [button_function];

      if (button_state == META_BUTTON_STATE_PRELIGHT)
        gtk_style_context_set_state (context, state | GTK_STATE_PRELIGHT);
      else if (button_state == META_BUTTON_STATE_PRESSED)
        gtk_style_context_set_state (context, state | GTK_STATE_ACTIVE);
      else
        gtk_style_context_set_state (context, state);

      cairo_save (cr);

      get_button_rect (button_function, fgeom, &tmp_rect);

      button_rect.x = tmp_rect.x / scale;
      button_rect.y = tmp_rect.y / scale;
      button_rect.width = tmp_rect.width / scale;
      button_rect.height = tmp_rect.height / scale;

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

          switch (button_function)
            {
              case META_BUTTON_FUNCTION_CLOSE:
                icon_name = "window-close-symbolic";
                break;
              case META_BUTTON_FUNCTION_MAXIMIZE:
                if (flags & META_FRAME_MAXIMIZED)
                  icon_name = "window-restore-symbolic";
                else
                  icon_name = "window-maximize-symbolic";
                break;
              case META_BUTTON_FUNCTION_MINIMIZE:
                icon_name = "window-minimize-symbolic";
                break;
              case META_BUTTON_FUNCTION_MENU:
                icon_name = "open-menu-symbolic";
                break;
              case META_BUTTON_FUNCTION_APPMENU:
                pixbuf = g_object_ref (mini_icon);
                break;
              case META_BUTTON_FUNCTION_SHADE:
              case META_BUTTON_FUNCTION_ABOVE:
              case META_BUTTON_FUNCTION_STICK:
              case META_BUTTON_FUNCTION_UNSHADE:
              case META_BUTTON_FUNCTION_UNABOVE:
              case META_BUTTON_FUNCTION_UNSTICK:
              case META_BUTTON_FUNCTION_LAST:
              default:
                icon_name = NULL;
                break;
            }

          if (icon_name)
            {
              GtkIconTheme *theme = gtk_icon_theme_get_default ();
              GtkIconInfo *info;

              info = gtk_icon_theme_lookup_icon_for_scale (theme, icon_name,
                                                           style->layout->gtk.icon_size,
                                                           scale, 0);

              pixbuf = gtk_icon_info_load_symbolic_for_context (info, context, NULL, NULL);
            }

          if (pixbuf)
            {
              float width, height;
              gdouble x, y;

              width = gdk_pixbuf_get_width (pixbuf) / scale;
              height = gdk_pixbuf_get_height (pixbuf) / scale;
              x = button_rect.x + (button_rect.width - width) / 2;
              y = button_rect.y + (button_rect.height - height) / 2;

              cairo_translate (cr, x, y);
              cairo_scale (cr,
                           width / style->layout->gtk.icon_size / scale,
                           height / style->layout->gtk.icon_size / scale);
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
  MetaThemeImplClass *impl_class;

  impl_class = META_THEME_IMPL_CLASS (gtk_class);

  impl_class->load = meta_theme_gtk_load;
  impl_class->get_frame_borders = meta_theme_gtk_get_frame_borders;
  impl_class->calc_geometry = meta_theme_gtk_calc_geometry;
  impl_class->draw_frame = meta_theme_gtk_draw_frame;
}

static void
meta_theme_gtk_init (MetaThemeGtk *gtk)
{
  MetaThemeImpl *impl;
  MetaFrameType type;

  impl = META_THEME_IMPL (gtk);

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
          case META_FRAME_TYPE_DIALOG:
          case META_FRAME_TYPE_MODAL_DIALOG:
          case META_FRAME_TYPE_ATTACHED:
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
}
