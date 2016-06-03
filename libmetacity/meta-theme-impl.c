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

#include <glib/gi18n.h>

#include "meta-frame-style.h"
#include "meta-theme-impl-private.h"
#include "meta-theme.h"

typedef struct
{
  gboolean           composited;

  MetaFrameStyleSet *style_sets_by_type[META_FRAME_TYPE_LAST];
} MetaThemeImplPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaThemeImpl, meta_theme_impl, G_TYPE_OBJECT)

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

static void
meta_theme_impl_dispose (GObject *object)
{
  MetaThemeImpl *impl;
  MetaThemeImplPrivate *priv;
  gint i;

  impl = META_THEME_IMPL (object);
  priv = meta_theme_impl_get_instance_private (impl);

  for (i = 0; i < META_FRAME_TYPE_LAST; i++)
    {
      if (priv->style_sets_by_type[i])
        {
          meta_frame_style_set_unref (priv->style_sets_by_type[i]);
          priv->style_sets_by_type[i] = NULL;
        }
    }

  G_OBJECT_CLASS (meta_theme_impl_parent_class)->dispose (object);
}

static gboolean
meta_theme_impl_real_load (MetaThemeImpl  *impl,
                           const gchar    *name,
                           GError        **error)
{
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
               _("MetaThemeImplClass::load not implemented for '%s'"),
               g_type_name (G_TYPE_FROM_INSTANCE (impl)));

  return FALSE;
}

static void
meta_theme_impl_class_init (MetaThemeImplClass *impl_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (impl_class);

  object_class->dispose = meta_theme_impl_dispose;

  impl_class->load = meta_theme_impl_real_load;
}

static void
meta_theme_impl_init (MetaThemeImpl *impl)
{
}

void
meta_theme_impl_set_composited (MetaThemeImpl *impl,
                                gboolean       composited)
{
  MetaThemeImplPrivate *priv;

  priv = meta_theme_impl_get_instance_private (impl);

  priv->composited = composited;
}

gboolean
meta_theme_impl_get_composited (MetaThemeImpl *impl)
{
  MetaThemeImplPrivate *priv;

  priv = meta_theme_impl_get_instance_private (impl);

  return priv->composited;
}

void
meta_theme_impl_add_style_set (MetaThemeImpl     *impl,
                               MetaFrameType      type,
                               MetaFrameStyleSet *style_set)
{
  MetaThemeImplPrivate *priv;

  priv = meta_theme_impl_get_instance_private (impl);

  if (priv->style_sets_by_type[type])
    meta_frame_style_set_unref (priv->style_sets_by_type[type]);

  priv->style_sets_by_type[type] = style_set;
}

MetaFrameStyleSet *
meta_theme_impl_get_style_set (MetaThemeImpl *impl,
                               MetaFrameType  type)
{
  MetaThemeImplPrivate *priv;

  priv = meta_theme_impl_get_instance_private (impl);

  return priv->style_sets_by_type[type];
}

void
get_button_rect (MetaButtonType           type,
                 const MetaFrameGeometry *fgeom,
                 gint                     middle_background_offset,
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

MetaButtonState
map_button_state (MetaButtonType           button_type,
                  const MetaFrameGeometry *fgeom,
                  gint                     middle_bg_offset,
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

void
scale_border (GtkBorder *border,
              double     factor)
{
  border->left *= factor;
  border->right *= factor;
  border->top *= factor;
  border->bottom *= factor;
}

int
get_window_scaling_factor (void)
{
  GdkScreen *screen;
  GValue value = G_VALUE_INIT;

  screen = gdk_screen_get_default ();

  g_value_init (&value, G_TYPE_INT);

  if (gdk_screen_get_setting (screen, "gdk-window-scaling-factor", &value))
    return g_value_get_int (&value);
  else
    return 1;
}
