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

#include "meta-frame-layout.h"
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
 * a given field is not still at -1. It is never called directly, but
 * rather via the CHECK_GEOMETRY_VALUE and CHECK_GEOMETRY_BORDER
 * macros.
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

  /* Fill with -1 values to detect invalid themes */
  layout->left_width = -1;
  layout->right_width = -1;
  layout->top_height = 0; /* only used by GTK+ theme */
  layout->bottom_height = -1;

  layout->invisible_border.left = 10;
  layout->invisible_border.right = 10;
  layout->invisible_border.bottom = 10;
  layout->invisible_border.top = 10;

  init_border (&layout->title_border);

  layout->title_vertical_pad = -1;

  layout->right_titlebar_edge = -1;
  layout->left_titlebar_edge = -1;

  layout->button_sizing = META_BUTTON_SIZING_LAST;
  layout->button_aspect = 1.0;
  layout->button_width = -1;
  layout->button_height = -1;

  /* Spacing as hardcoded in GTK+:
   * https://git.gnome.org/browse/gtk+/tree/gtk/gtkheaderbar.c?h=gtk-3-14#n53
   */
  layout->titlebar_spacing = 6;
  layout->has_title = TRUE;
  layout->title_scale = 1.0;
  layout->icon_size = 16; /* was META_MINI_ICON_WIDTH from common.h */

  init_border (&layout->button_border);

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

#define CHECK_GEOMETRY_VALUE(vname) if (!validate_geometry_value (layout->vname, #vname, error)) return FALSE

#define CHECK_GEOMETRY_BORDER(bname) if (!validate_geometry_border (&layout->bname, #bname, error)) return FALSE

  CHECK_GEOMETRY_VALUE (left_width);
  CHECK_GEOMETRY_VALUE (right_width);
  CHECK_GEOMETRY_VALUE (bottom_height);

  CHECK_GEOMETRY_BORDER (title_border);

  CHECK_GEOMETRY_VALUE (title_vertical_pad);

  CHECK_GEOMETRY_VALUE (right_titlebar_edge);
  CHECK_GEOMETRY_VALUE (left_titlebar_edge);

  switch (layout->button_sizing)
    {
      case META_BUTTON_SIZING_ASPECT:
        if (layout->button_aspect < (0.1) || layout->button_aspect > (15.0))
          {
            g_set_error (error, META_THEME_ERROR,
                         META_THEME_ERROR_FRAME_GEOMETRY,
                         _("Button aspect ratio %g is not reasonable"),
                         layout->button_aspect);

            return FALSE;
          }
        break;
      case META_BUTTON_SIZING_FIXED:
        CHECK_GEOMETRY_VALUE (button_width);
        CHECK_GEOMETRY_VALUE (button_height);
        break;
      case META_BUTTON_SIZING_LAST:
      default:
        g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FRAME_GEOMETRY,
                     _("Frame geometry does not specify size of buttons"));

        return FALSE;
    }

  CHECK_GEOMETRY_BORDER (button_border);

  return TRUE;
}
