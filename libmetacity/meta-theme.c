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

  GHashTable           *variants;
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

  g_clear_pointer (&theme->variants, g_hash_table_destroy);

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
  theme->composited = TRUE;

  theme->variants = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                           (GDestroyNotify) meta_style_info_unref);
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
meta_theme_style_invalidate (MetaTheme *theme)
{
  GList *variants;
  GList *l;

  variants = g_hash_table_get_keys (theme->variants);

  for (l = variants; l != NULL; l = g_list_next (l))
    {
      gchar *variant;
      MetaStyleInfo *style_info;

      variant = g_strdup ((gchar *) l->data);

      if (g_strcmp0 (variant, "default") == 0)
        style_info = meta_style_info_new (NULL, theme->composited);
      else
        style_info = meta_style_info_new (variant, theme->composited);

      g_hash_table_insert (theme->variants, variant, style_info);
    }

  g_list_free (variants);
}

MetaStyleInfo *
meta_theme_get_style_info (MetaTheme   *theme,
                           const gchar *variant)
{
  MetaStyleInfo *style_info;

  if (variant == NULL)
    variant = "default";

  style_info = g_hash_table_lookup (theme->variants, variant);

  if (style_info == NULL)
    {
      style_info = meta_style_info_new (variant, theme->composited);

      g_hash_table_insert (theme->variants, g_strdup (variant), style_info);
    }

  return style_info;
}

void
meta_theme_set_composited (MetaTheme *theme,
                           gboolean   composited)
{
  if (theme->composited == composited)
    return;

  theme->composited = composited;

  meta_theme_style_invalidate (theme);
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

MetaFrameStyle *
meta_theme_get_frame_style (MetaTheme      *theme,
                            MetaFrameType   type,
                            MetaFrameFlags  flags)
{
  MetaFrameState state;
  MetaFrameResize resize;
  MetaFrameFocus focus;
  MetaFrameStyle *style;
  MetaFrameStyleSet *style_set;

  g_return_val_if_fail (type < META_FRAME_TYPE_LAST, NULL);

  style_set = meta_theme_impl_get_style_set (theme->impl, type);

  if (style_set == NULL && type == META_FRAME_TYPE_ATTACHED)
    style_set = meta_theme_impl_get_style_set (theme->impl, META_FRAME_TYPE_BORDER);

  /* Right now the parser forces a style set for all other types,
   * but this fallback code is here in case I take that out.
   */
  if (style_set == NULL)
    style_set = meta_theme_impl_get_style_set (theme->impl, META_FRAME_TYPE_NORMAL);

  if (style_set == NULL)
    return NULL;

  switch (flags & (META_FRAME_MAXIMIZED | META_FRAME_SHADED | META_FRAME_TILED_LEFT | META_FRAME_TILED_RIGHT))
    {
    case 0:
      state = META_FRAME_STATE_NORMAL;
      break;
    case META_FRAME_MAXIMIZED:
      state = META_FRAME_STATE_MAXIMIZED;
      break;
    case META_FRAME_TILED_LEFT:
      state = META_FRAME_STATE_TILED_LEFT;
      break;
    case META_FRAME_TILED_RIGHT:
      state = META_FRAME_STATE_TILED_RIGHT;
      break;
    case META_FRAME_SHADED:
      state = META_FRAME_STATE_SHADED;
      break;
    case (META_FRAME_MAXIMIZED | META_FRAME_SHADED):
      state = META_FRAME_STATE_MAXIMIZED_AND_SHADED;
      break;
    case (META_FRAME_TILED_LEFT | META_FRAME_SHADED):
      state = META_FRAME_STATE_TILED_LEFT_AND_SHADED;
      break;
    case (META_FRAME_TILED_RIGHT | META_FRAME_SHADED):
      state = META_FRAME_STATE_TILED_RIGHT_AND_SHADED;
      break;
    default:
      g_assert_not_reached ();
      state = META_FRAME_STATE_LAST; /* compiler */
      break;
    }

  switch (flags & (META_FRAME_ALLOWS_VERTICAL_RESIZE | META_FRAME_ALLOWS_HORIZONTAL_RESIZE))
    {
    case 0:
      resize = META_FRAME_RESIZE_NONE;
      break;
    case META_FRAME_ALLOWS_VERTICAL_RESIZE:
      resize = META_FRAME_RESIZE_VERTICAL;
      break;
    case META_FRAME_ALLOWS_HORIZONTAL_RESIZE:
      resize = META_FRAME_RESIZE_HORIZONTAL;
      break;
    case (META_FRAME_ALLOWS_VERTICAL_RESIZE | META_FRAME_ALLOWS_HORIZONTAL_RESIZE):
      resize = META_FRAME_RESIZE_BOTH;
      break;
    default:
      g_assert_not_reached ();
      resize = META_FRAME_RESIZE_LAST; /* compiler */
      break;
    }

  /* re invert the styles used for focus/unfocussed while flashing a frame */
  if (((flags & META_FRAME_HAS_FOCUS) && !(flags & META_FRAME_IS_FLASHING))
      || (!(flags & META_FRAME_HAS_FOCUS) && (flags & META_FRAME_IS_FLASHING)))
    focus = META_FRAME_FOCUS_YES;
  else
    focus = META_FRAME_FOCUS_NO;

  style = meta_frame_style_set_get_style (style_set, state, resize, focus);

  return style;
}

PangoFontDescription*
meta_style_info_create_font_desc (MetaTheme     *theme,
                                  MetaStyleInfo *style_info)
{
  GtkStyleContext *context;
  PangoFontDescription *font_desc;

  context = style_info->styles[META_STYLE_ELEMENT_TITLE];

  gtk_style_context_save (context);
  gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);

  gtk_style_context_get (context, GTK_STATE_FLAG_NORMAL,
                         "font", &font_desc, NULL);

  gtk_style_context_restore (context);

  if (theme->titlebar_font)
    pango_font_description_merge (font_desc, theme->titlebar_font, TRUE);

  return font_desc;
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
