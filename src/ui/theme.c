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

/**
 * The current theme. (Themes are singleton.)
 */
static MetaTheme *meta_current_theme = NULL;

MetaTheme*
meta_theme_get_current (void)
{
  return meta_current_theme;
}

void
meta_theme_set_current (const gchar                *name,
                        gboolean                    force_reload,
                        gboolean                    composited,
                        const PangoFontDescription *titlebar_font)
{
  MetaTheme *new_theme;
  GError *error;

  g_debug ("Setting current theme to '%s'", name);

  if (!force_reload && meta_current_theme)
    {
      gchar *theme_name;

      theme_name = meta_theme_get_name (meta_current_theme);
      if (g_strcmp0 (name, theme_name) == 0)
        {
          g_free (theme_name);
          return;
        }

      g_free (theme_name);
    }

  if (name != NULL && strcmp (name, "") != 0)
    new_theme = meta_theme_new (META_THEME_TYPE_METACITY);
  else
    new_theme = meta_theme_new (META_THEME_TYPE_GTK);

  meta_theme_set_composited (new_theme, composited);
  meta_theme_set_titlebar_font (new_theme, titlebar_font);

  error = NULL;
  if (!meta_theme_load (new_theme, name, &error))
    {
      g_warning (_("Failed to load theme '%s': %s"), name, error->message);
      g_error_free (error);

      g_object_unref (new_theme);
    }
  else
    {
      if (meta_current_theme)
        g_object_unref (meta_current_theme);
      meta_current_theme = new_theme;

      g_debug ("New theme is '%s'", name);
    }
}
