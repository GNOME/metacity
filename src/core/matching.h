/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Window matching */

/*
 * Copyright (C) 2009 Thomas Thurman
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

#ifndef META_MATCHING_H
#define META_MATCHING_H

#include "common.h"

/**
 * Represents the position of a given window on a display.
 */
typedef struct
{
  gint x;
  gint y;
  guint width;
  guint height;
  guint desktop;
} MetaMatching;

MetaMatching* meta_matching_load_from_role (gchar *role);

void meta_matching_save_to_role (gchar *role, MetaMatching *matching);

void meta_matching_save_all (void);

#endif

/* eof matching.h */
