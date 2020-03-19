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
  META_BUTTON_TYPE_MINIMIZE,
  META_BUTTON_TYPE_MAXIMIZE,
  META_BUTTON_TYPE_CLOSE,
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

typedef struct _MetaButton MetaButton;

MetaButtonType meta_button_get_type       (MetaButton   *button);

void           meta_button_get_event_rect (MetaButton   *button,
                                           GdkRectangle *rect);

G_END_DECLS

#endif
