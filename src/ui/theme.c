/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity Theme Rendering */

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

/**
 * \file theme.c    Making Metacity look pretty
 *
 * The window decorations drawn by Metacity are described by files on disk
 * known internally as "themes" (externally as "window border themes" on
 * http://art.gnome.org/themes/metacity/ or "Metacity themes"). This file
 * contains most of the code necessary to support themes; it does not
 * contain the XML parser, which is in theme-parser.c.
 *
 * \bug This is a big file with lots of different subsystems, which might
 * be better split out into separate files.
 */

/**
 * \defgroup tokenizer   The theme expression tokenizer
 *
 * Themes can use a simple expression language to represent the values of
 * things. This is the tokeniser used for that language.
 *
 * \bug We could remove almost all this code by using GScanner instead,
 * but we would also have to find every expression in every existing theme
 * we could and make sure the parse trees were the same.
 */

/**
 * \defgroup parser  The theme expression parser
 *
 * Themes can use a simple expression language to represent the values of
 * things. This is the parser used for that language.
 */

#include <config.h>
#include "theme.h"
#include "util.h"
#include <gtk/gtk.h>
#include <libmetacity/meta-color.h>
#include <string.h>
#include <stdlib.h>
#define __USE_XOPEN
#include <stdarg.h>
#include <math.h>
