/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity main */

/*
 * Copyright (C) 2001 Havoc Pennington
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

#ifndef META_MAIN_H
#define META_MAIN_H

#include <glib.h>

typedef enum
{
  META_EXIT_SUCCESS,
  META_EXIT_ERROR
} MetaExitCode;

/* exit immediately */
void meta_exit (MetaExitCode code) __attribute__((noreturn));

/* g_main_loop_quit() then fall out of main() */
void meta_quit (MetaExitCode code);

void meta_restart (void);

#endif
