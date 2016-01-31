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

#include "meta-enum-types.h"
#include "meta-theme.h"
#include "meta-theme-gtk-private.h"
#include "meta-theme-impl-private.h"
#include "meta-theme-metacity-private.h"

struct _MetaTheme
{
  GObject               parent;

  MetaThemeType         type;
  MetaThemeImpl        *impl;

  gboolean              composited;

  PangoFontDescription *titlebar_font;
};

enum
{
  PROP_0,

  PROP_TYPE,

  LAST_PROP
};

static GParamSpec *theme_properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (MetaTheme, meta_theme, G_TYPE_OBJECT)

static void
meta_theme_constructed (GObject *object)
{
  MetaTheme *theme;

  G_OBJECT_CLASS (meta_theme_parent_class)->constructed (object);

  theme = META_THEME (object);

  if (theme->type == META_THEME_TYPE_GTK)
    theme->impl = g_object_new (META_TYPE_THEME_GTK, NULL);
  else if (theme->type == META_THEME_TYPE_METACITY)
    theme->impl = g_object_new (META_TYPE_THEME_METACITY, NULL);
  else
    g_assert_not_reached ();
}

static void
meta_theme_dispose (GObject *object)
{
  MetaTheme *theme;

  theme = META_THEME (object);

  g_clear_object (&theme->impl);

  G_OBJECT_CLASS (meta_theme_parent_class)->dispose (object);
}

static void
meta_theme_finalize (GObject *object)
{
  MetaTheme *theme;

  theme = META_THEME (object);

  if (theme->titlebar_font)
    {
      pango_font_description_free (theme->titlebar_font);
      theme->titlebar_font = NULL;
    }

  G_OBJECT_CLASS (meta_theme_parent_class)->finalize (object);
}

static void
meta_theme_get_property (GObject    *object,
                         guint       property_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  MetaTheme *theme;

  theme = META_THEME (object);

  switch (property_id)
    {
      case PROP_TYPE:
        g_value_set_enum (value, theme->type);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
meta_theme_set_property (GObject      *object,
                         guint         property_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  MetaTheme *theme;

  theme = META_THEME (object);

  switch (property_id)
    {
      case PROP_TYPE:
        theme->type = g_value_get_enum (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
meta_theme_install_properties (GObjectClass *object_class)
{
  theme_properties[PROP_TYPE] =
    g_param_spec_enum ("type", "type", "type",
                        META_TYPE_THEME_TYPE, META_THEME_TYPE_GTK,
                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                        G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP,
                                     theme_properties);
}

static void
meta_theme_class_init (MetaThemeClass *theme_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (theme_class);

  object_class->constructed = meta_theme_constructed;
  object_class->dispose = meta_theme_dispose;
  object_class->finalize = meta_theme_finalize;
  object_class->get_property = meta_theme_get_property;
  object_class->set_property = meta_theme_set_property;

  meta_theme_install_properties (object_class);
}

static void
meta_theme_init (MetaTheme *theme)
{
}

/**
 * meta_theme_error_quark:
 *
 * Domain for #MetaThemeError errors.
 *
 * Returns: the #GQuark identifying the #MetaThemeError domain.
 */
GQuark
meta_theme_error_quark (void)
{
  return g_quark_from_static_string ("meta-theme-error-quark");
}

MetaTheme *
meta_theme_new (MetaThemeType type)
{
  return g_object_new (META_TYPE_THEME, "type", type, NULL);
}

gboolean
meta_theme_load (MetaTheme    *theme,
                 const gchar  *name,
                 GError      **error)
{
  return META_THEME_IMPL_GET_CLASS (theme->impl)->load (theme->impl, name,
                                                        error);
}

void
meta_theme_set_composited (MetaTheme *theme,
                           gboolean   composited)
{
  theme->composited = composited;
}

gboolean
meta_theme_get_composited (MetaTheme *theme)
{
  return theme->composited;
}

void
meta_theme_set_titlebar_font (MetaTheme                  *theme,
                              const PangoFontDescription *titlebar_font)
{
  pango_font_description_free (theme->titlebar_font);
  theme->titlebar_font = pango_font_description_copy (titlebar_font);
}

const PangoFontDescription *
meta_theme_get_titlebar_font (MetaTheme *theme)
{
  return theme->titlebar_font;
}

MetaThemeType
meta_theme_get_theme_type (MetaTheme *theme)
{
  return theme->type;
}

gchar *
meta_theme_get_name (MetaTheme *theme)
{
  return META_THEME_IMPL_GET_CLASS (theme->impl)->get_name (theme->impl);
}

MetaFrameStyleSet *
meta_theme_get_style_set (MetaTheme     *theme,
                          MetaFrameType  type)
{
  return meta_theme_impl_get_style_set (theme->impl, type);
}

gboolean
meta_theme_allows_shade_stick_above_buttons (MetaTheme *theme)
{
  MetaThemeMetacity *metacity;

  if (theme->type != META_THEME_TYPE_METACITY)
    return TRUE;

  metacity = META_THEME_METACITY (theme->impl);

  return meta_theme_metacity_allows_shade_stick_above_buttons (metacity);
}
