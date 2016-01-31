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

#include "meta-frame-style.h"
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
  GtkSettings *settings;
  MetaFrameType type;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  settings = gtk_settings_get_default ();

  if (settings == NULL)
    return FALSE;

  g_object_set (settings, "gtk-theme-name", name, NULL);

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
  GtkSettings *settings;
  gchar *name;

  settings = gtk_settings_get_default ();

  if (settings == NULL)
    return NULL;

  g_object_get (settings, "gtk-theme-name", &name, NULL);

  return name;
}

static void
meta_theme_gtk_class_init (MetaThemeGtkClass *gtk_class)
{
  MetaThemeImplClass *impl_class;

  impl_class = META_THEME_IMPL_CLASS (gtk_class);

  impl_class->load = meta_theme_gtk_load;
  impl_class->get_name = meta_theme_gtk_get_name;
}

static void
meta_theme_gtk_init (MetaThemeGtk *gtk)
{
}
