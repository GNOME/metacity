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

#include "meta-frame-borders.h"

static void
clear_border (GtkBorder *border)
{
  border->left = 0;
  border->right = 0;
  border->top = 0;
  border->bottom = 0;
}

void
meta_frame_borders_clear (MetaFrameBorders *borders)
{
  clear_border (&borders->visible);
  clear_border (&borders->shadow);
  clear_border (&borders->resize);
  clear_border (&borders->invisible);
  clear_border (&borders->total);
}
