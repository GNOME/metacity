/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2008 Iain Holmes
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef META_SCREEN_H
#define META_SCREEN_H

#include <X11/Xlib.h>
#include <glib.h>
#include "types.h"

int meta_screen_get_screen_number (MetaScreen *screen);
MetaDisplay *meta_screen_get_display (MetaScreen *screen);

Window meta_screen_get_xroot (MetaScreen *screen);
void meta_screen_get_size (MetaScreen *screen,
                           int        *width,
                           int        *height);

MetaScreen *meta_screen_for_x_screen (Screen *xscreen);

#endif
