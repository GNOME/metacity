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

#include "meta-button-layout.h"

static void
meta_button_layout_init (MetaButtonLayout *layout)
{
  gint i;

  for (i = 0; i < META_BUTTON_FUNCTION_LAST; i++)
    {
      layout->left_buttons[i] = META_BUTTON_FUNCTION_LAST;
      layout->left_buttons_has_spacer[i] = FALSE;

      layout->right_buttons[i] = META_BUTTON_FUNCTION_LAST;
      layout->right_buttons_has_spacer[i] = FALSE;
    }
}

static void
string_to_buttons (const gchar        *str,
                   MetaButtonFunction  side_buttons[META_BUTTON_FUNCTION_LAST],
                   gboolean            side_has_spacer[META_BUTTON_FUNCTION_LAST])
{
  gint i;
  gint b;
  gboolean used[META_BUTTON_FUNCTION_LAST];
  gchar **buttons;

  i = 0;
  while (i < META_BUTTON_FUNCTION_LAST)
    used[i++] = FALSE;

  buttons = g_strsplit (str, ",", -1);

  i = b = 0;
  while (buttons[b] != NULL)
    {
      MetaButtonFunction f;

      f = meta_button_function_from_string (buttons[b]);

      if (i > 0 && g_strcmp0 ("spacer", buttons[b]) == 0)
        {
          side_has_spacer[i - 1] = TRUE;

          f = meta_button_function_get_opposite (f);
          if (f != META_BUTTON_FUNCTION_LAST)
            side_has_spacer[i - 2] = TRUE;
        }
      else
        {
          if (f != META_BUTTON_FUNCTION_LAST && !used[f])
            {
              side_buttons[i] = f;
              used[f] = TRUE;
              i++;

              f = meta_button_function_get_opposite (f);
              if (f != META_BUTTON_FUNCTION_LAST)
                side_buttons[i++] = f;
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

MetaButtonLayout
meta_button_layout_new (const gchar *str,
                        gboolean     invert)
{
  gchar **sides;
  MetaButtonLayout layout;
  MetaButtonLayout rtl_layout;
  gint i;
  gint j;

  sides = g_strsplit (str, ":", 2);
  meta_button_layout_init (&layout);

  if (sides[0] != NULL)
    {
      string_to_buttons (sides[0], layout.left_buttons,
                         layout.left_buttons_has_spacer);
    }

  if (sides[0] != NULL && sides[1] != NULL)
    {
      string_to_buttons (sides[1], layout.right_buttons,
                         layout.right_buttons_has_spacer);
    }

  g_strfreev (sides);

  if (!invert)
    return layout;

  meta_button_layout_init (&rtl_layout);

  i = 0;
  while (rtl_layout.left_buttons[i] != META_BUTTON_FUNCTION_LAST)
    i++;

  for (j = 0; j < i; j++)
    {
      rtl_layout.right_buttons[j] = layout.left_buttons[i - j - 1];

      if (j == 0)
        rtl_layout.right_buttons_has_spacer[i - 1] = layout.left_buttons_has_spacer[i - j - 1];
      else
        rtl_layout.right_buttons_has_spacer[j - 1] = layout.left_buttons_has_spacer[i - j - 1];
    }

  i = 0;
  while (rtl_layout.left_buttons[i] != META_BUTTON_FUNCTION_LAST)
    i++;

  for (j = 0; j < i; j++)
    {
      rtl_layout.left_buttons[j] = layout.right_buttons[i - j - 1];

      if (j == 0)
        rtl_layout.left_buttons_has_spacer[i - 1] = layout.right_buttons_has_spacer[i - j - 1];
      else
        rtl_layout.left_buttons_has_spacer[j - 1] = layout.right_buttons_has_spacer[i - j - 1];
    }

  return rtl_layout;
}
