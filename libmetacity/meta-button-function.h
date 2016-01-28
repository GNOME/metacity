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

#ifndef META_BUTTON_FUNCTION_H
#define META_BUTTON_FUNCTION_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * MetaButtonFunction:
 * @META_BUTTON_FUNCTION_MENU:
 * @META_BUTTON_FUNCTION_APPMENU:
 * @META_BUTTON_FUNCTION_MINIMIZE:
 * @META_BUTTON_FUNCTION_MAXIMIZE:
 * @META_BUTTON_FUNCTION_CLOSE:
 * @META_BUTTON_FUNCTION_SHADE:
 * @META_BUTTON_FUNCTION_UNSHADE:
 * @META_BUTTON_FUNCTION_ABOVE:
 * @META_BUTTON_FUNCTION_UNABOVE:
 * @META_BUTTON_FUNCTION_STICK:
 * @META_BUTTON_FUNCTION_UNSTICK:
 * @META_BUTTON_FUNCTION_LAST:
 *
 */
typedef enum
{
  META_BUTTON_FUNCTION_MENU,
  META_BUTTON_FUNCTION_APPMENU,
  META_BUTTON_FUNCTION_MINIMIZE,
  META_BUTTON_FUNCTION_MAXIMIZE,
  META_BUTTON_FUNCTION_CLOSE,
  META_BUTTON_FUNCTION_SHADE,
  META_BUTTON_FUNCTION_UNSHADE,
  META_BUTTON_FUNCTION_ABOVE,
  META_BUTTON_FUNCTION_UNABOVE,
  META_BUTTON_FUNCTION_STICK,
  META_BUTTON_FUNCTION_UNSTICK,
  META_BUTTON_FUNCTION_LAST
} MetaButtonFunction;

MetaButtonFunction meta_button_function_from_string  (const gchar        *str);

MetaButtonFunction meta_button_function_get_opposite (MetaButtonFunction  function);

G_END_DECLS

#endif
