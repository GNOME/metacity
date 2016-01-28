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

#ifndef META_FRAME_BORDERS_H
#define META_FRAME_BORDERS_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct
{
  /* The frame border is made up of two pieces - an inner visible portion
   * and an outer portion that is invisible but responds to events.
   */
  GtkBorder visible;
  GtkBorder invisible;

  /* For convenience, we have a "total" border which is equal to the sum
   * of the two borders above. */
  GtkBorder total;
} MetaFrameBorders;

void meta_frame_borders_clear (MetaFrameBorders *self);

G_END_DECLS

#endif
