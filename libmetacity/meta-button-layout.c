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

#include "meta-button-layout-private.h"

static MetaButtonType
type_from_string (const gchar *str)
{
  if (g_strcmp0 (str, "menu") == 0)
    return META_BUTTON_TYPE_MENU;
  else if (g_strcmp0 (str, "minimize") == 0)
    return META_BUTTON_TYPE_MINIMIZE;
  else if (g_strcmp0 (str, "maximize") == 0)
    return META_BUTTON_TYPE_MAXIMIZE;
  else if (g_strcmp0 (str, "close") == 0)
    return META_BUTTON_TYPE_CLOSE;
  else if (g_strcmp0 (str, "spacer") == 0)
    return META_BUTTON_TYPE_SPACER;

  return META_BUTTON_TYPE_LAST;
}

static MetaButton *
string_to_buttons (const gchar *str,
                   gint        *n_buttons)
{
  gchar **buttons;
  MetaButton *retval;
  gint index;
  gint i;

  *n_buttons = 0;

  if (str == NULL)
    return NULL;

  buttons = g_strsplit (str, ",", -1);

  for (i = 0; buttons[i] != NULL; i++)
    {
      MetaButtonType type;

      type = type_from_string (buttons[i]);

      if (type != META_BUTTON_TYPE_LAST)
        {
          *n_buttons += 1;
        }
      else
        {
          g_debug ("Ignoring unknown button name - '%s'", buttons[i]);
        }
    }

  retval = g_new0 (MetaButton, *n_buttons);
  index = 0;

  for (i = 0; buttons[i] != NULL; i++)
    {
      MetaButtonType type;

      type = type_from_string (buttons[i]);

      if (type != META_BUTTON_TYPE_LAST)
        {
          GdkRectangle empty;
          MetaButton tmp;

          empty.x = 0;
          empty.y = 0;
          empty.width = 0;
          empty.height = 0;

          tmp.type = type;
          tmp.state = META_BUTTON_STATE_NORMAL;
          tmp.rect.visible = empty;
          tmp.rect.clickable = empty;
          tmp.visible = TRUE;

          retval[index++] = tmp;
        }
    }

  g_strfreev (buttons);

  return retval;
}

MetaButtonLayout *
meta_button_layout_new (const gchar *str,
                        gboolean     invert)
{
  MetaButtonLayout *layout;
  gchar **sides;
  const gchar *buttons;
  gint n_buttons;

  layout = g_new0 (MetaButtonLayout, 1);
  sides = g_strsplit (str, ":", 2);

  buttons = sides[0];
  layout->left_buttons = string_to_buttons (buttons, &n_buttons);
  layout->n_left_buttons = n_buttons;

  buttons = sides[0] != NULL ? sides[1] : NULL;
  layout->right_buttons = string_to_buttons (buttons, &n_buttons);
  layout->n_right_buttons = n_buttons;

  g_strfreev (sides);

  if (invert)
    {
      MetaButtonLayout *rtl_layout;
      gint i;

      rtl_layout = g_new0 (MetaButtonLayout, 1);

      rtl_layout->left_buttons = g_new0 (MetaButton, layout->n_right_buttons);
      for (i = 0; i < layout->n_right_buttons; i++)
        rtl_layout->left_buttons[i] = layout->right_buttons[layout->n_right_buttons - i - 1];
      rtl_layout->n_left_buttons = layout->n_right_buttons;

      rtl_layout->right_buttons = g_new0 (MetaButton, layout->n_left_buttons);
      for (i = 0; i < layout->n_left_buttons; i++)
        rtl_layout->right_buttons[i] = layout->left_buttons[layout->n_left_buttons - i - 1];
      rtl_layout->n_right_buttons = layout->n_left_buttons;

      meta_button_layout_free (layout);

      return rtl_layout;
    }

  return layout;
}

void
meta_button_layout_free (MetaButtonLayout *layout)
{
  g_free (layout->left_buttons);
  g_free (layout->right_buttons);

  g_free (layout);
}
