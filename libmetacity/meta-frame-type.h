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

#ifndef META_FRAME_TYPE_H
#define META_FRAME_TYPE_H

#include <glib.h>

G_BEGIN_DECLS

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
