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

#include "meta-button-function.h"

/**
 * meta_button_function_from_string:
 * @str:
 *
 * Returns:
 */
MetaButtonFunction
meta_button_function_from_string (const gchar *str)
{
  if (g_strcmp0 (str, "menu") == 0)
    return META_BUTTON_FUNCTION_MENU;
  else if (g_strcmp0 (str, "appmenu") == 0)
    return META_BUTTON_FUNCTION_APPMENU;
  else if (g_strcmp0 (str, "minimize") == 0)
    return META_BUTTON_FUNCTION_MINIMIZE;
  else if (g_strcmp0 (str, "maximize") == 0)
    return META_BUTTON_FUNCTION_MAXIMIZE;
  else if (g_strcmp0 (str, "close") == 0)
    return META_BUTTON_FUNCTION_CLOSE;
  else if (g_strcmp0 (str, "shade") == 0)
    return META_BUTTON_FUNCTION_SHADE;
  else if (g_strcmp0 (str, "unshade") == 0)
    return META_BUTTON_FUNCTION_UNSHADE;
  else if (g_strcmp0 (str, "above") == 0)
    return META_BUTTON_FUNCTION_ABOVE;
  else if (g_strcmp0 (str, "unabove") == 0)
    return META_BUTTON_FUNCTION_UNABOVE;
  else if (g_strcmp0 (str, "stick") == 0)
    return META_BUTTON_FUNCTION_STICK;
  else if (g_strcmp0 (str, "unstick") == 0)
    return META_BUTTON_FUNCTION_UNSTICK;

  return META_BUTTON_FUNCTION_LAST;
}

/**
 * meta_button_function_get_opposite:
 * @function:
 *
 * Returns:
 */
MetaButtonFunction
meta_button_function_get_opposite (MetaButtonFunction function)
{
  switch (function)
    {
      case META_BUTTON_FUNCTION_SHADE:
        return META_BUTTON_FUNCTION_UNSHADE;
      case META_BUTTON_FUNCTION_UNSHADE:
        return META_BUTTON_FUNCTION_SHADE;

      case META_BUTTON_FUNCTION_ABOVE:
        return META_BUTTON_FUNCTION_UNABOVE;
      case META_BUTTON_FUNCTION_UNABOVE:
        return META_BUTTON_FUNCTION_ABOVE;

      case META_BUTTON_FUNCTION_STICK:
        return META_BUTTON_FUNCTION_UNSTICK;
      case META_BUTTON_FUNCTION_UNSTICK:
        return META_BUTTON_FUNCTION_STICK;

      case META_BUTTON_FUNCTION_MENU:
      case META_BUTTON_FUNCTION_APPMENU:
      case META_BUTTON_FUNCTION_MINIMIZE:
      case META_BUTTON_FUNCTION_MAXIMIZE:
      case META_BUTTON_FUNCTION_CLOSE:
      case META_BUTTON_FUNCTION_LAST:
        return META_BUTTON_FUNCTION_LAST;

      default:
        return META_BUTTON_FUNCTION_LAST;
    }
}
