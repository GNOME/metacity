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

#ifndef META_THEME_H
#define META_THEME_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * META_THEME_ERROR:
 *
 * Domain for #MetaThemeError errors.
 */
#define META_THEME_ERROR (meta_theme_error_quark ())

/**
 * MetaThemeError:
 * @META_THEME_ERROR_TOO_OLD:
 * @META_THEME_ERROR_FRAME_GEOMETRY:
 * @META_THEME_ERROR_BAD_CHARACTER:
 * @META_THEME_ERROR_BAD_PARENS:
 * @META_THEME_ERROR_UNKNOWN_VARIABLE:
 * @META_THEME_ERROR_DIVIDE_BY_ZERO:
 * @META_THEME_ERROR_MOD_ON_FLOAT:
 * @META_THEME_ERROR_FAILED:
 *
 * Error codes for %META_THEME_ERROR.
 */
typedef enum
{
  META_THEME_ERROR_TOO_OLD,
  META_THEME_ERROR_FRAME_GEOMETRY,
  META_THEME_ERROR_BAD_CHARACTER,
  META_THEME_ERROR_BAD_PARENS,
  META_THEME_ERROR_UNKNOWN_VARIABLE,
  META_THEME_ERROR_DIVIDE_BY_ZERO,
  META_THEME_ERROR_MOD_ON_FLOAT,
  META_THEME_ERROR_FAILED
} MetaThemeError;

/**
 * MetaThemeType:
 * @META_THEME_TYPE_GTK:
 * @META_THEME_TYPE_METACITY:
 *
 * Theme types.
 */
typedef enum
{
  META_THEME_TYPE_GTK,
  META_THEME_TYPE_METACITY,
} MetaThemeType;

GQuark meta_theme_error_quark (void);

G_END_DECLS

#endif
