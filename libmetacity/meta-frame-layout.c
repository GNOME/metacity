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

#include "meta-frame-layout-private.h"
#include "meta-theme.h"

/**
 * Sets all the fields of a border to dummy values.
 *
 * \param border The border whose fields should be reset.
 */
static void
init_border (GtkBorder *border)
{
  border->top = -1;
  border->bottom = -1;
  border->left = -1;
  border->right = -1;
}

static gboolean
validate_border (const GtkBorder  *border,
                 const char      **bad)
{
  *bad = NULL;

  if (border->top < 0)
    *bad = _("top");
  else if (border->bottom < 0)
    *bad = _("bottom");
  else if (border->left < 0)
    *bad = _("left");
  else if (border->right < 0)
    *bad = _("right");

  return *bad == NULL;
}


/**
 * Ensures that the theme supplied a particular dimension. When a
 * MetaFrameLayout is created, all its integer fields are set to -1
 * by meta_frame_layout_new(). After an instance of this type
 * should have been initialised, this function checks that
 * a given field is not still at -1.
 *
 * \param      val    The value to check
 * \param      name   The name to use in the error message
 * \param[out] error  Set to an error if val was not initialised
 */
static gboolean
validate_geometry_value (int         val,
                         const char *name,
                         GError    **error)
{
  if (val < 0)
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FRAME_GEOMETRY,
                   _("frame geometry does not specify '%s' dimension"),
                   name);

      return FALSE;
    }

  return TRUE;
}

static gboolean
validate_geometry_border (const GtkBorder *border,
                          const char      *name,
                          GError         **error)
{
  const char *bad;

  if (!validate_border (border, &bad))
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FRAME_GEOMETRY,
                   _("frame geometry does not specify dimension '%s' for border '%s'"),
                   bad, name);

      return FALSE;
    }

  return TRUE;
}

/**
 * Creates a new, empty MetaFrameLayout. The fields will be set to dummy
 * values.
 *
 * \return The newly created MetaFrameLayout.
 */
MetaFrameLayout*
meta_frame_layout_new  (void)
{
  MetaFrameLayout *layout;

  layout = g_new0 (MetaFrameLayout, 1);

  layout->refcount = 1;

  /* Spacing as hardcoded in GTK+:
   * https://git.gnome.org/browse/gtk+/tree/gtk/gtkheaderbar.c?h=gtk-3-14#n53
   */
  layout->gtk.titlebar_spacing = 6;
  layout->gtk.icon_size = 16;

  /* Fill with -1 values to detect invalid themes */
  layout->metacity.left_width = -1;
  layout->metacity.right_width = -1;
  layout->metacity.bottom_height = -1;

  init_border (&layout->metacity.title_border);

  layout->metacity.title_vertical_pad = -1;

  layout->metacity.right_titlebar_edge = -1;
  layout->metacity.left_titlebar_edge = -1;

  layout->metacity.button_sizing = META_BUTTON_SIZING_LAST;
  layout->metacity.button_aspect = 1.0;
  layout->metacity.button_width = -1;
  layout->metacity.button_height = -1;

  layout->invisible_resize_border.left = 10;
  layout->invisible_resize_border.right = 10;
  layout->invisible_resize_border.bottom = 10;
  layout->invisible_resize_border.top = 10;

  init_border (&layout->button_border);

  layout->has_title = TRUE;
  layout->title_scale = PANGO_SCALE_MEDIUM;

  return layout;
}

MetaFrameLayout *
meta_frame_layout_copy (const MetaFrameLayout *src)
{
  MetaFrameLayout *layout;

  layout = g_new0 (MetaFrameLayout, 1);

  *layout = *src;

  layout->refcount = 1;

  return layout;
}

void
meta_frame_layout_ref (MetaFrameLayout *layout)
{
  g_return_if_fail (layout != NULL);

  layout->refcount += 1;
}

void
meta_frame_layout_unref (MetaFrameLayout *layout)
{
  g_return_if_fail (layout != NULL);
  g_return_if_fail (layout->refcount > 0);

  layout->refcount -= 1;

  if (layout->refcount == 0)
    g_free (layout);
}

gboolean
meta_frame_layout_validate (const MetaFrameLayout *layout,
                            GError               **error)
{
  g_return_val_if_fail (layout != NULL, FALSE);

  if (!validate_geometry_value (layout->metacity.left_width,
      "left_width", error))
    return FALSE;

  if (!validate_geometry_value (layout->metacity.right_width,
      "right_width", error))
    return FALSE;

  if (!validate_geometry_value (layout->metacity.bottom_height,
      "bottom_height", error))
    return FALSE;

  if (!validate_geometry_border (&layout->metacity.title_border,
                                 "title_border", error))
    return FALSE;

  if (!validate_geometry_value (layout->metacity.title_vertical_pad,
      "title_vertical_pad", error))
    return FALSE;

  if (!validate_geometry_value (layout->metacity.right_titlebar_edge,
      "right_titlebar_edge", error))
    return FALSE;

  if (!validate_geometry_value (layout->metacity.left_titlebar_edge,
      "left_titlebar_edge", error))
    return FALSE;

  switch (layout->metacity.button_sizing)
    {
      case META_BUTTON_SIZING_ASPECT:
        if (layout->metacity.button_aspect < (0.1) ||
            layout->metacity.button_aspect > (15.0))
          {
            g_set_error (error, META_THEME_ERROR,
                         META_THEME_ERROR_FRAME_GEOMETRY,
                         _("Button aspect ratio %g is not reasonable"),
                         layout->metacity.button_aspect);

            return FALSE;
          }
        break;
      case META_BUTTON_SIZING_FIXED:
        if (!validate_geometry_value (layout->metacity.button_width,
                                      "button_width", error))
          return FALSE;

        if (!validate_geometry_value (layout->metacity.button_height,
                                      "button_height", error))
          return FALSE;
        break;
      case META_BUTTON_SIZING_LAST:
      default:
        g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FRAME_GEOMETRY,
                     _("Frame geometry does not specify size of buttons"));

        return FALSE;
    }

  if (!validate_geometry_border (&layout->metacity.title_border,
                                 "title_border", error))
    return FALSE;

  if (!validate_geometry_border (&layout->button_border,
                                 "button_border", error))
    return FALSE;

  return TRUE;
}
