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

#ifndef META_BUTTON_PRIVATE_H
#define META_BUTTON_PRIVATE_H

#include "meta-button.h"

G_BEGIN_DECLS

typedef enum
{
  /* Ordered so that background is drawn first */
  META_BUTTON_FUNCTION_LEFT_LEFT_BACKGROUND,
  META_BUTTON_FUNCTION_LEFT_MIDDLE_BACKGROUND,
  META_BUTTON_FUNCTION_LEFT_RIGHT_BACKGROUND,
  META_BUTTON_FUNCTION_LEFT_SINGLE_BACKGROUND,
  META_BUTTON_FUNCTION_RIGHT_LEFT_BACKGROUND,
  META_BUTTON_FUNCTION_RIGHT_MIDDLE_BACKGROUND,
  META_BUTTON_FUNCTION_RIGHT_RIGHT_BACKGROUND,
  META_BUTTON_FUNCTION_RIGHT_SINGLE_BACKGROUND,
  META_BUTTON_FUNCTION_CLOSE,
  META_BUTTON_FUNCTION_MAXIMIZE,
  META_BUTTON_FUNCTION_MINIMIZE,
  META_BUTTON_FUNCTION_MENU,
  META_BUTTON_FUNCTION_SHADE,
  META_BUTTON_FUNCTION_ABOVE,
  META_BUTTON_FUNCTION_STICK,
  META_BUTTON_FUNCTION_UNSHADE,
  META_BUTTON_FUNCTION_UNABOVE,
  META_BUTTON_FUNCTION_UNSTICK,
  META_BUTTON_FUNCTION_LAST
} MetaButtonFunction;

struct _MetaButton
{
  MetaButtonType  type;
  MetaButtonState state;

  /* The computed size of a button (really just a way of tying its visible
   * and clickable areas together). The reason for two different rectangles
   * here is Fitts' law & maximized windows; See bug #97703 for more details.
   */
  struct {
    /* The area where the button's image is drawn. */
    GdkRectangle  visible;

    /* The area where the button can be activated by clicking */
    GdkRectangle clickable;
  } rect;

  gboolean       visible;
};

G_END_DECLS

#endif
