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

#include <glib/gi18n-lib.h>
#include <string.h>

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

  gchar                *theme_name;
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

  g_free (theme->theme_name);

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
  g_free (theme->theme_name);

  if (theme->type == META_THEME_TYPE_GTK)
    {
      theme->theme_name = g_strdup (name);
    }
  else if (theme->type == META_THEME_TYPE_METACITY)
    {
      GtkSettings *settings;

      settings = gtk_settings_get_default ();

      g_object_get (settings, "gtk-theme-name", &theme->theme_name, NULL);
    }
  else
    {
      g_assert_not_reached ();
    }

  return META_THEME_IMPL_GET_CLASS (theme->impl)->load (theme->impl, name,
                                                        error);
}

void
meta_theme_invalidate (MetaTheme *theme)
{
  g_hash_table_remove_all (theme->variants);
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
      style_info = meta_style_info_new (theme->theme_name, variant,
                                        theme->composited);

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

  meta_theme_invalidate (theme);
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

/**
 * Returns the height of the letters in a particular font.
 *
 * \param font_desc  the font
 * \param context  the context of the font
 * \return  the height of the letters
 */
gint
meta_pango_font_desc_get_text_height (const PangoFontDescription *font_desc,
                                      PangoContext               *context)
{
  PangoFontMetrics *metrics;
  PangoLanguage *lang;
  int retval;

  lang = pango_context_get_language (context);
  metrics = pango_context_get_metrics (context, font_desc, lang);

  retval = PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) +
                         pango_font_metrics_get_descent (metrics));

  pango_font_metrics_unref (metrics);

  return retval;
}

MetaFrameType
meta_frame_type_from_string (const gchar *str)
{
  if (strcmp ("normal", str) == 0)
    return META_FRAME_TYPE_NORMAL;
  else if (strcmp ("dialog", str) == 0)
    return META_FRAME_TYPE_DIALOG;
  else if (strcmp ("modal_dialog", str) == 0)
    return META_FRAME_TYPE_MODAL_DIALOG;
  else if (strcmp ("utility", str) == 0)
    return META_FRAME_TYPE_UTILITY;
  else if (strcmp ("menu", str) == 0)
    return META_FRAME_TYPE_MENU;
  else if (strcmp ("border", str) == 0)
    return META_FRAME_TYPE_BORDER;
  else if (strcmp ("attached", str) == 0)
    return META_FRAME_TYPE_ATTACHED;
  else
    return META_FRAME_TYPE_LAST;
}

gdouble
meta_theme_get_title_scale (MetaTheme     *theme,
                            MetaFrameType  type,
                            MetaFrameFlags flags)
{
  MetaFrameStyle *style;

  g_return_val_if_fail (type < META_FRAME_TYPE_LAST, 1.0);

  style = meta_theme_get_frame_style (theme, type, flags);

  /* Parser is not supposed to allow this currently */
  if (style == NULL)
    return 1.0;

  return style->layout->title_scale;
}

void
meta_theme_get_frame_borders (MetaTheme        *theme,
                              const gchar      *theme_variant,
                              MetaFrameType     type,
                              int               text_height,
                              MetaFrameFlags    flags,
                              MetaFrameBorders *borders)
{
  MetaFrameStyle *style;
  MetaStyleInfo *style_info;

  g_return_if_fail (type < META_FRAME_TYPE_LAST);

  meta_frame_borders_clear (borders);

  style = meta_theme_get_frame_style (theme, type, flags);

  /* Parser is not supposed to allow this currently */
  if (style == NULL)
    return;

  style_info = meta_theme_get_style_info (theme, theme_variant);

  META_THEME_IMPL_GET_CLASS (theme->impl)->get_frame_borders (theme->impl,
                                                              style->layout,
                                                              style_info,
                                                              theme->composited,
                                                              text_height,
                                                              flags,
                                                              type,
                                                              borders);
}

void
meta_theme_calc_geometry (MetaTheme              *theme,
                          const gchar            *theme_variant,
                          MetaFrameType           type,
                          gint                    text_height,
                          MetaFrameFlags          flags,
                          gint                    client_width,
                          gint                    client_height,
                          const MetaButtonLayout *button_layout,
                          MetaFrameGeometry      *fgeom)
{
  MetaFrameStyle *style;
  MetaStyleInfo *style_info;

  g_return_if_fail (type < META_FRAME_TYPE_LAST);

  style = meta_theme_get_frame_style (theme, type, flags);

  /* Parser is not supposed to allow this currently */
  if (style == NULL)
    return;

  style_info = meta_theme_get_style_info (theme, theme_variant);

  META_THEME_IMPL_GET_CLASS (theme->impl)->calc_geometry (theme->impl,
                                                          style->layout,
                                                          style_info,
                                                          theme->composited,
                                                          text_height,
                                                          flags,
                                                          client_width,
                                                          client_height,
                                                          button_layout,
                                                          type,
                                                          fgeom);
}

void
meta_theme_draw_frame (MetaTheme              *theme,
                       const gchar            *theme_variant,
                       cairo_t                *cr,
                       MetaFrameType           type,
                       MetaFrameFlags          flags,
                       int                     client_width,
                       int                     client_height,
                       PangoLayout            *title_layout,
                       int                     text_height,
                       const MetaButtonLayout *button_layout,
                       MetaButtonState         button_states[META_BUTTON_TYPE_LAST],
                       GdkPixbuf              *mini_icon,
                       GdkPixbuf              *icon)
{
  MetaFrameStyle *style;
  MetaStyleInfo *style_info;
  MetaFrameGeometry fgeom;

  g_return_if_fail (type < META_FRAME_TYPE_LAST);

  style = meta_theme_get_frame_style (theme, type, flags);

  /* Parser is not supposed to allow this currently */
  if (style == NULL)
    return;

  style_info = meta_theme_get_style_info (theme, theme_variant);

  META_THEME_IMPL_GET_CLASS (theme->impl)->calc_geometry (theme->impl,
                                                          style->layout,
                                                          style_info,
                                                          theme->composited,
                                                          text_height,
                                                          flags,
                                                          client_width,
                                                          client_height,
                                                          button_layout,
                                                          type,
                                                          &fgeom);

  META_THEME_IMPL_GET_CLASS (theme->impl)->draw_frame (theme->impl,
                                                       style,
                                                       style_info,
                                                       cr,
                                                       &fgeom,
                                                       title_layout,
                                                       flags,
                                                       button_states,
                                                       mini_icon,
                                                       icon);
}

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
