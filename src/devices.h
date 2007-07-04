/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity device structures */

/* 
 * Copyright (C) 2007 Paulo Ricardo Zanoni
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

#ifndef META_DEVICES_H
#define META_DEVICES_H

#ifdef MPX

#include <X11/extensions/XInput.h>

#include "common.h"

/* By default, the MetaDevInfo lists have size 8. Almost no client has more
 * than 8 mice or keyboards... */
/* FIXME setting this define to 1 or 2 causes memory corruption!!!! */
#define DEFAULT_INPUT_ARRAY_SIZE 8

typedef struct _MetaDevices MetaDevices;
/* typedef struct _MetaDevInfo MetaDevInfo; This guy was declared at common.h */

struct _MetaDevInfo
{
  XDevice *xdev;
  gchar *name;
};

struct _MetaDevices
{
  MetaDevInfo *mice;
  int miceUsed;
  int miceSize;

  MetaDevInfo *keyboards;
  int keybsUsed;
  int keybsSize;
  MetaDevInfo *pairedPointers;
};

#endif

#endif
