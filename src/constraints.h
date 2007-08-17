/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity size/position constraints */

/* 
 * Copyright (C) 2002 Red Hat, Inc.
 * Copyright (C) 2005 Elijah Newren
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef META_CONSTRAINTS_H
#define META_CONSTRAINTS_H

#include "util.h"
#include "window.h"
#include "frame.h"

typedef enum
{
  META_IS_CONFIGURE_REQUEST = 1 << 0,
  META_DO_GRAVITY_ADJUST    = 1 << 1,
  META_IS_USER_ACTION       = 1 << 2,
  META_IS_MOVE_ACTION       = 1 << 3,
  META_IS_RESIZE_ACTION     = 1 << 4
} MetaMoveResizeFlags;

void meta_window_constrain (MetaWindow          *window,
                            MetaFrameGeometry   *orig_fgeom,
                            MetaMoveResizeFlags  flags,
                            int                  resize_gravity,
                            const MetaRectangle *orig,
                            MetaRectangle       *new,
			    MetaDevInfo         *dev);

#endif /* META_CONSTRAINTS_H */
