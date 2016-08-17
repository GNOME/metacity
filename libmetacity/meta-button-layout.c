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
meta_button_type_from_string (const gchar *str)
{
  if (g_strcmp0 (str, "menu") == 0)
    return META_BUTTON_TYPE_MENU;
  else if (g_strcmp0 (str, "appmenu") == 0)
    return META_BUTTON_TYPE_APPMENU;
  else if (g_strcmp0 (str, "minimize") == 0)
    return META_BUTTON_TYPE_MINIMIZE;
  else if (g_strcmp0 (str, "maximize") == 0)
    return META_BUTTON_TYPE_MAXIMIZE;
  else if (g_strcmp0 (str, "close") == 0)
    return META_BUTTON_TYPE_CLOSE;
  else if (g_strcmp0 (str, "shade") == 0)
    return META_BUTTON_TYPE_SHADE;
  else if (g_strcmp0 (str, "unshade") == 0)
    return META_BUTTON_TYPE_UNSHADE;
  else if (g_strcmp0 (str, "above") == 0)
    return META_BUTTON_TYPE_ABOVE;
  else if (g_strcmp0 (str, "unabove") == 0)
    return META_BUTTON_TYPE_UNABOVE;
  else if (g_strcmp0 (str, "stick") == 0)
    return META_BUTTON_TYPE_STICK;
  else if (g_strcmp0 (str, "unstick") == 0)
    return META_BUTTON_TYPE_UNSTICK;

  return META_BUTTON_TYPE_LAST;
}

static MetaButtonType
meta_button_type_get_opposite (MetaButtonType type)
{
  switch (type)
    {
      case META_BUTTON_TYPE_SHADE:
        return META_BUTTON_TYPE_UNSHADE;
      case META_BUTTON_TYPE_UNSHADE:
        return META_BUTTON_TYPE_SHADE;

      case META_BUTTON_TYPE_ABOVE:
        return META_BUTTON_TYPE_UNABOVE;
      case META_BUTTON_TYPE_UNABOVE:
        return META_BUTTON_TYPE_ABOVE;

      case META_BUTTON_TYPE_STICK:
        return META_BUTTON_TYPE_UNSTICK;
      case META_BUTTON_TYPE_UNSTICK:
        return META_BUTTON_TYPE_STICK;

      case META_BUTTON_TYPE_MENU:
      case META_BUTTON_TYPE_APPMENU:
      case META_BUTTON_TYPE_MINIMIZE:
      case META_BUTTON_TYPE_MAXIMIZE:
      case META_BUTTON_TYPE_CLOSE:
      case META_BUTTON_TYPE_LAST:
        return META_BUTTON_TYPE_LAST;

      default:
        return META_BUTTON_TYPE_LAST;
    }
}

static void
meta_button_layout_init (MetaButtonLayout *layout)
{
  gint i;

  for (i = 0; i < META_BUTTON_TYPE_LAST; i++)
    {
      layout->left_buttons[i] = META_BUTTON_TYPE_LAST;
      layout->left_buttons_has_spacer[i] = FALSE;

      layout->right_buttons[i] = META_BUTTON_TYPE_LAST;
      layout->right_buttons_has_spacer[i] = FALSE;
    }
}

static void
string_to_buttons (const gchar    *str,
                   MetaButtonType  side_buttons[META_BUTTON_TYPE_LAST],
                   gboolean        side_has_spacer[META_BUTTON_TYPE_LAST])
{
  gint i;
  gint b;
  gboolean used[META_BUTTON_TYPE_LAST];
  gchar **buttons;

  i = 0;
  while (i < META_BUTTON_TYPE_LAST)
    used[i++] = FALSE;

  buttons = g_strsplit (str, ",", -1);

  i = b = 0;
  while (buttons[b] != NULL)
    {
      MetaButtonType type;

      type = meta_button_type_from_string (buttons[b]);

      if (i > 0 && g_strcmp0 ("spacer", buttons[b]) == 0)
        {
          side_has_spacer[i - 1] = TRUE;

          type = meta_button_type_get_opposite (type);
          if (type != META_BUTTON_TYPE_LAST)
            side_has_spacer[i - 2] = TRUE;
        }
      else
        {
          if (type != META_BUTTON_TYPE_LAST && !used[type])
            {
              side_buttons[i] = type;
              used[type] = TRUE;
              i++;

              type = meta_button_type_get_opposite (type);
              if (type != META_BUTTON_TYPE_LAST)
                side_buttons[i++] = type;
            }
          else
            {
              g_debug ("Ignoring unknown or already-used button name - '%s'",
                       buttons[b]);
            }
        }

      b++;
    }

  g_strfreev (buttons);
}

MetaButtonLayout *
meta_button_layout_new (const gchar *str,
                        gboolean     invert)
{
  gchar **sides;
  MetaButtonLayout *layout;
  MetaButtonLayout *rtl_layout;
  gint i;
  gint j;

  layout = g_new0 (MetaButtonLayout, 1);
  meta_button_layout_init (layout);

  sides = g_strsplit (str, ":", 2);

  if (sides[0] != NULL)
    {
      string_to_buttons (sides[0], layout->left_buttons,
                         layout->left_buttons_has_spacer);
    }

  if (sides[0] != NULL && sides[1] != NULL)
    {
      string_to_buttons (sides[1], layout->right_buttons,
                         layout->right_buttons_has_spacer);
    }

  g_strfreev (sides);

  if (!invert)
    return layout;

  rtl_layout = g_new0 (MetaButtonLayout, 1);
  meta_button_layout_init (rtl_layout);

  i = 0;
  while (rtl_layout->left_buttons[i] != META_BUTTON_TYPE_LAST)
    i++;

  for (j = 0; j < i; j++)
    {
      rtl_layout->right_buttons[j] = layout->left_buttons[i - j - 1];

      if (j == 0)
        rtl_layout->right_buttons_has_spacer[i - 1] = layout->left_buttons_has_spacer[i - j - 1];
      else
        rtl_layout->right_buttons_has_spacer[j - 1] = layout->left_buttons_has_spacer[i - j - 1];
    }

  i = 0;
  while (rtl_layout->left_buttons[i] != META_BUTTON_TYPE_LAST)
    i++;

  for (j = 0; j < i; j++)
    {
      rtl_layout->left_buttons[j] = layout->right_buttons[i - j - 1];

      if (j == 0)
        rtl_layout->left_buttons_has_spacer[i - 1] = layout->right_buttons_has_spacer[i - j - 1];
      else
        rtl_layout->left_buttons_has_spacer[j - 1] = layout->right_buttons_has_spacer[i - j - 1];
    }

  meta_button_layout_free (layout);

  return rtl_layout;
}

void
meta_button_layout_free (MetaButtonLayout *layout)
{
  g_free (layout);
}
