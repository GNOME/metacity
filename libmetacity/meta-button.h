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

#ifndef META_BUTTON_H
#define META_BUTTON_H

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef enum
{
  META_BUTTON_TYPE_MENU,
  META_BUTTON_TYPE_APPMENU,
  META_BUTTON_TYPE_MINIMIZE,
  META_BUTTON_TYPE_MAXIMIZE,
  META_BUTTON_TYPE_CLOSE,
  META_BUTTON_TYPE_SHADE,
  META_BUTTON_TYPE_UNSHADE,
  META_BUTTON_TYPE_ABOVE,
  META_BUTTON_TYPE_UNABOVE,
  META_BUTTON_TYPE_STICK,
  META_BUTTON_TYPE_UNSTICK,
  META_BUTTON_TYPE_SPACER,
  META_BUTTON_TYPE_LAST
} MetaButtonType;

typedef enum
{
  META_BUTTON_STATE_NORMAL,
  META_BUTTON_STATE_PRESSED,
  META_BUTTON_STATE_PRELIGHT,
  META_BUTTON_STATE_LAST
} MetaButtonState;

typedef struct
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
} MetaButton;

G_END_DECLS

#endif
