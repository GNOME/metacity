/*
 * Copyright (C) 2001 Havoc Pennington
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

#ifndef META_BUTTON_TYPE_PRIVATE_H
#define META_BUTTON_TYPE_PRIVATE_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
  /* Ordered so that background is drawn first */
  META_BUTTON_TYPE_LEFT_LEFT_BACKGROUND,
  META_BUTTON_TYPE_LEFT_MIDDLE_BACKGROUND,
  META_BUTTON_TYPE_LEFT_RIGHT_BACKGROUND,
  META_BUTTON_TYPE_LEFT_SINGLE_BACKGROUND,
  META_BUTTON_TYPE_RIGHT_LEFT_BACKGROUND,
  META_BUTTON_TYPE_RIGHT_MIDDLE_BACKGROUND,
  META_BUTTON_TYPE_RIGHT_RIGHT_BACKGROUND,
  META_BUTTON_TYPE_RIGHT_SINGLE_BACKGROUND,
  META_BUTTON_TYPE_CLOSE,
  META_BUTTON_TYPE_MAXIMIZE,
  META_BUTTON_TYPE_MINIMIZE,
  META_BUTTON_TYPE_MENU,
  META_BUTTON_TYPE_APPMENU,
  META_BUTTON_TYPE_SHADE,
  META_BUTTON_TYPE_ABOVE,
  META_BUTTON_TYPE_STICK,
  META_BUTTON_TYPE_UNSHADE,
  META_BUTTON_TYPE_UNABOVE,
  META_BUTTON_TYPE_UNSTICK,
  META_BUTTON_TYPE_LAST
} MetaButtonType;

G_END_DECLS

#endif
