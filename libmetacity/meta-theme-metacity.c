/*
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

#include "meta-theme-metacity.h"

struct _MetaThemeMetacity
{
  MetaThemeImpl parent;
};

G_DEFINE_TYPE (MetaThemeMetacity, meta_theme_metacity, META_TYPE_THEME_IMPL)

static void
meta_theme_metacity_class_init (MetaThemeMetacityClass *metacity_class)
{
}

static void
meta_theme_metacity_init (MetaThemeMetacity *metacity)
{
}
