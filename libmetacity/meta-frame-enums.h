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

#ifndef META_FRAME_ENUMS_H
#define META_FRAME_ENUMS_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
  META_FRAME_ALLOWS_DELETE            = 1 << 0,
  META_FRAME_ALLOWS_MENU              = 1 << 1,
  META_FRAME_ALLOWS_APPMENU           = 1 << 2,
  META_FRAME_ALLOWS_MINIMIZE          = 1 << 3,
  META_FRAME_ALLOWS_MAXIMIZE          = 1 << 4,
  META_FRAME_ALLOWS_VERTICAL_RESIZE   = 1 << 5,
  META_FRAME_ALLOWS_HORIZONTAL_RESIZE = 1 << 6,
  META_FRAME_HAS_FOCUS                = 1 << 7,
  META_FRAME_SHADED                   = 1 << 8,
  META_FRAME_STUCK                    = 1 << 9,
  META_FRAME_MAXIMIZED                = 1 << 10,
  META_FRAME_ALLOWS_SHADE             = 1 << 11,
  META_FRAME_ALLOWS_MOVE              = 1 << 12,
  META_FRAME_FULLSCREEN               = 1 << 13,
  META_FRAME_IS_FLASHING              = 1 << 14,
  META_FRAME_ABOVE                    = 1 << 15,
  META_FRAME_TILED_LEFT               = 1 << 16,
  META_FRAME_TILED_RIGHT              = 1 << 17
} MetaFrameFlags;

typedef enum
{
  META_FRAME_TYPE_NORMAL,
  META_FRAME_TYPE_DIALOG,
  META_FRAME_TYPE_MODAL_DIALOG,
  META_FRAME_TYPE_UTILITY,
  META_FRAME_TYPE_MENU,
  META_FRAME_TYPE_BORDER,
  META_FRAME_TYPE_ATTACHED,
  META_FRAME_TYPE_LAST
} MetaFrameType;

G_END_DECLS

#endif
