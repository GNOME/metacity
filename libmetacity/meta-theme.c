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
#include "meta-frame-layout-private.h"
#include "meta-theme.h"
#include "meta-theme-gtk-private.h"
#include "meta-theme-impl-private.h"
#include "meta-theme-metacity-private.h"
#include "meta-style-info-private.h"

struct _MetaTheme
{
  GObject               parent;

  MetaThemeType         type;
  MetaThemeImpl        *impl;

  MetaButtonLayout     *button_layout;

  gboolean              composited;

  PangoFontDescription *titlebar_font;

  gulong                gtk_theme_name_id;
  gchar                *gtk_theme_name;
  GHashTable           *variants;

  PangoContext         *context;

  GHashTable           *font_descs;
  GHashTable           *title_heights;
};

enum
{
  PROP_0,

  PROP_TYPE,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (MetaTheme, meta_theme, G_TYPE_OBJECT)

static MetaStyleInfo *
get_style_info (MetaTheme   *theme,
                const gchar *variant)
{
  const gchar *key;
  MetaStyleInfo *style_info;

  key = variant;
  if (variant == NULL)
    key = "default";

  style_info = g_hash_table_lookup (theme->variants, key);

  if (style_info == NULL)
    {
      gint window_scale;

      window_scale = get_window_scaling_factor ();
      style_info = meta_style_info_new (theme->gtk_theme_name, variant,
                                        theme->composited, window_scale);

      g_hash_table_insert (theme->variants, g_strdup (key), style_info);
    }

  return style_info;
}

static MetaFrameStyle *
get_frame_style (MetaTheme      *theme,
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

static void
font_desc_apply_scale (PangoFontDescription *font_desc,
                       MetaTheme            *theme,
                       MetaFrameType         type,
                       MetaFrameFlags        flags)
{
  gint old_size;
  MetaFrameStyle *style;
  gdouble scale;
  gint new_size;

  old_size = pango_font_description_get_size (font_desc);
  style = get_frame_style (theme, type, flags);
  scale = get_window_scaling_factor ();

  new_size = MAX (old_size * (style->layout->title_scale / scale), 1);

  pango_font_description_set_size (font_desc, new_size);
}

static PangoFontDescription *
get_title_font_desc (MetaTheme      *theme,
                     const gchar    *variant,
                     MetaFrameType   type,
                     MetaFrameFlags  flags)
{
  gchar *key;
  PangoFontDescription *font_desc;
  MetaStyleInfo *style_info;
  GtkStyleContext *context;

  key = g_strdup_printf ("%s_%d_%x", variant ? variant : "default", type, flags);
  font_desc = g_hash_table_lookup (theme->font_descs, key);

  if (font_desc != NULL)
    {
      g_free (key);
      return font_desc;
    }

  style_info = get_style_info (theme, variant);
  context = meta_style_info_get_style (style_info, META_STYLE_ELEMENT_TITLE);

  gtk_style_context_save (context);
  gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);

  gtk_style_context_get (context, GTK_STATE_FLAG_NORMAL,
                         "font", &font_desc, NULL);

  gtk_style_context_restore (context);

  if (theme->titlebar_font)
    pango_font_description_merge (font_desc, theme->titlebar_font, TRUE);

  font_desc_apply_scale (font_desc, theme, type, flags);

  g_hash_table_insert (theme->font_descs, key, font_desc);

  return font_desc;
}

static void
ensure_pango_context (MetaTheme *theme)
{
  GdkScreen *screen;
  PangoFontMap *fontmap;
  PangoContext *context;
  const cairo_font_options_t *options;
  gdouble dpi;

  if (theme->context != NULL)
    return;

  screen = gdk_screen_get_default ();

  fontmap = pango_cairo_font_map_get_default ();
  context = pango_font_map_create_context (fontmap);

  options = gdk_screen_get_font_options (screen);
  pango_cairo_context_set_font_options (context, options);

  dpi = gdk_screen_get_resolution (screen);
  pango_cairo_context_set_resolution (context, dpi);

  theme->context = context;
}

static gint
get_title_height (MetaTheme      *theme,
                  const gchar    *variant,
                  MetaFrameType   type,
                  MetaFrameFlags  flags)
{
  PangoFontDescription *description;
  gpointer size;
  gpointer height;
  gint title_height;

  description = get_title_font_desc (theme, variant, type, flags);
  g_assert (description != NULL);

  size = GINT_TO_POINTER (pango_font_description_get_size (description));
  height = g_hash_table_lookup (theme->title_heights, size);

  if (height != NULL)
    {
      title_height = GPOINTER_TO_INT (height);
    }
  else
    {
      PangoLanguage *lang;
      PangoFontMetrics *metrics;
      gint ascent;
      gint descent;
      gint scale;

      ensure_pango_context (theme);

      lang = pango_context_get_language (theme->context);
      metrics = pango_context_get_metrics (theme->context, description, lang);

      ascent = pango_font_metrics_get_ascent (metrics);
      descent = pango_font_metrics_get_descent (metrics);
      pango_font_metrics_unref (metrics);

      title_height = PANGO_PIXELS (ascent + descent);
      scale = get_window_scaling_factor ();

      title_height *= scale;

      height = GINT_TO_POINTER (title_height);
      g_hash_table_insert (theme->title_heights, size, height);
    }

  return title_height;
}

static PangoLayout *
create_title_layout (MetaTheme      *theme,
                     const gchar    *variant,
                     MetaFrameType   type,
                     MetaFrameFlags  flags,
                     const gchar    *title)
{
  PangoLayout *layout;
  PangoFontDescription *font_desc;

  ensure_pango_context (theme);

  layout = pango_layout_new (theme->context);

  if (title)
    pango_layout_set_text (layout, title, -1);

  pango_layout_set_auto_dir (layout, FALSE);
  pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);
  pango_layout_set_single_paragraph_mode (layout, TRUE);

  font_desc = get_title_font_desc (theme, variant, type, flags);
  pango_layout_set_font_description (layout, font_desc);

  return layout;
}

static void
notify_gtk_theme_name_cb (GtkSettings *settings,
                          GParamSpec  *pspec,
                          MetaTheme   *theme)
{
  g_free (theme->gtk_theme_name);
  g_object_get (settings, "gtk-theme-name", &theme->gtk_theme_name, NULL);

  meta_theme_invalidate (theme);
}

static void
meta_theme_constructed (GObject *object)
{
  MetaTheme *theme;
  const gchar *button_layout;

  G_OBJECT_CLASS (meta_theme_parent_class)->constructed (object);

  theme = META_THEME (object);

  if (theme->type == META_THEME_TYPE_GTK)
    theme->impl = g_object_new (META_TYPE_THEME_GTK, NULL);
  else if (theme->type == META_THEME_TYPE_METACITY)
    theme->impl = g_object_new (META_TYPE_THEME_METACITY, NULL);
  else
    g_assert_not_reached ();

  meta_theme_impl_set_composited (theme->impl, theme->composited);

  button_layout = "appmenu:minimize,maximize,close";
  meta_theme_set_button_layout (theme, button_layout, FALSE);
}

static void
meta_theme_dispose (GObject *object)
{
  MetaTheme *theme;

  theme = META_THEME (object);

  g_clear_object (&theme->impl);

  if (theme->gtk_theme_name_id > 0)
    {
      GtkSettings *settings;

      settings = gtk_settings_get_default ();

      g_signal_handler_disconnect (settings, theme->gtk_theme_name_id);
      theme->gtk_theme_name_id = 0;
    }

  g_clear_pointer (&theme->variants, g_hash_table_destroy);

  g_clear_object (&theme->context);

  g_clear_pointer (&theme->font_descs, g_hash_table_destroy);
  g_clear_pointer (&theme->title_heights, g_hash_table_destroy);

  G_OBJECT_CLASS (meta_theme_parent_class)->dispose (object);
}

static void
meta_theme_finalize (GObject *object)
{
  MetaTheme *theme;

  theme = META_THEME (object);

  if (theme->button_layout != NULL)
    {
      meta_button_layout_free (theme->button_layout);
      theme->button_layout = NULL;
    }

  if (theme->titlebar_font)
    {
      pango_font_description_free (theme->titlebar_font);
      theme->titlebar_font = NULL;
    }

  g_free (theme->gtk_theme_name);

  G_OBJECT_CLASS (meta_theme_parent_class)->finalize (object);
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
  properties[PROP_TYPE] =
    g_param_spec_enum ("type", "type", "type",
                        META_TYPE_THEME_TYPE, META_THEME_TYPE_GTK,
                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                        G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
meta_theme_class_init (MetaThemeClass *theme_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (theme_class);

  object_class->constructed = meta_theme_constructed;
  object_class->dispose = meta_theme_dispose;
  object_class->finalize = meta_theme_finalize;
  object_class->set_property = meta_theme_set_property;

  meta_theme_install_properties (object_class);
}

static void
meta_theme_init (MetaTheme *theme)
{
  theme->composited = TRUE;

  theme->variants = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           g_free, g_object_unref);

  theme->font_descs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                             (GDestroyNotify) pango_font_description_free);

  theme->title_heights = g_hash_table_new (NULL, NULL);
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
  if (theme->type == META_THEME_TYPE_GTK)
    {
      g_free (theme->gtk_theme_name);
      theme->gtk_theme_name = g_strdup (name);
    }
  else if (theme->type == META_THEME_TYPE_METACITY)
    {
      GtkSettings *settings;

      settings = gtk_settings_get_default ();

      g_free (theme->gtk_theme_name);
      g_object_get (settings, "gtk-theme-name", &theme->gtk_theme_name, NULL);

      if (theme->gtk_theme_name_id == 0)
        {
          theme->gtk_theme_name_id =
            g_signal_connect (settings, "notify::gtk-theme-name",
                              G_CALLBACK (notify_gtk_theme_name_cb), theme);
        }
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
  g_clear_object (&theme->context);
  g_hash_table_remove_all (theme->font_descs);
  g_hash_table_remove_all (theme->title_heights);
}

void
meta_theme_set_button_layout (MetaTheme   *theme,
                              const gchar *button_layout,
                              gboolean     invert)
{
  if (theme->button_layout != NULL)
    meta_button_layout_free (theme->button_layout);

  theme->button_layout = meta_button_layout_new (button_layout, invert);
}

MetaButton *
meta_theme_get_button (MetaTheme *theme,
                       gint       x,
                       gint       y)
{
  gint side;

  for (side = 0; side < 2; side++)
    {
      MetaButton *buttons;
      gint n_buttons;
      gint i;

      if (side == 0)
        {
          buttons = theme->button_layout->left_buttons;
          n_buttons = theme->button_layout->n_left_buttons;
        }
      else if (side == 1)
        {
          buttons = theme->button_layout->right_buttons;
          n_buttons = theme->button_layout->n_right_buttons;
        }
      else
        {
          g_assert_not_reached ();
        }

      for (i = 0; i < n_buttons; i++)
        {
          MetaButton *btn;
          GdkRectangle rect;

          btn = &buttons[i];
          rect = btn->rect.visible;

          if (!btn->visible || btn->type == META_BUTTON_TYPE_SPACER ||
              rect.width <= 0 || rect.height <= 0)
            {
              continue;
            }

          rect = btn->rect.clickable;

          if (x >= rect.x && x < (rect.x + rect.width) &&
              y >= rect.y && y < (rect.y + rect.height))
            {
              return btn;
            }
        }
    }

  return NULL;
}

void
meta_theme_set_composited (MetaTheme *theme,
                           gboolean   composited)
{
  if (theme->composited == composited)
    return;

  theme->composited = composited;

  meta_theme_impl_set_composited (theme->impl, composited);
  meta_theme_invalidate (theme);
}

void
meta_theme_set_titlebar_font (MetaTheme                  *theme,
                              const PangoFontDescription *titlebar_font)
{
  pango_font_description_free (theme->titlebar_font);
  theme->titlebar_font = pango_font_description_copy (titlebar_font);

  g_hash_table_remove_all (theme->font_descs);
  g_hash_table_remove_all (theme->title_heights);
}

void
meta_theme_get_frame_borders (MetaTheme        *theme,
                              const gchar      *variant,
                              MetaFrameType     type,
                              MetaFrameFlags    flags,
                              MetaFrameBorders *borders)
{
  MetaFrameStyle *style;
  MetaThemeImplClass *impl_class;
  MetaStyleInfo *style_info;
  gint title_height;

  g_return_if_fail (type < META_FRAME_TYPE_LAST);

  meta_frame_borders_clear (borders);

  style = get_frame_style (theme, type, flags);

  /* Parser is not supposed to allow this currently */
  if (style == NULL)
    return;

  impl_class = META_THEME_IMPL_GET_CLASS (theme->impl);
  style_info = get_style_info (theme, variant);
  title_height = get_title_height (theme, variant, type, flags);

  impl_class->get_frame_borders (theme->impl, style->layout, style_info,
                                 title_height, flags, type, borders);
}

void
meta_theme_calc_geometry (MetaTheme         *theme,
                          const gchar       *variant,
                          MetaFrameType      type,
                          MetaFrameFlags     flags,
                          gint               client_width,
                          gint               client_height,
                          MetaFrameGeometry *fgeom)
{
  MetaFrameStyle *style;
  MetaThemeImplClass *impl_class;
  MetaStyleInfo *style_info;
  gint title_height;

  g_return_if_fail (type < META_FRAME_TYPE_LAST);

  style = get_frame_style (theme, type, flags);

  /* Parser is not supposed to allow this currently */
  if (style == NULL)
    return;

  impl_class = META_THEME_IMPL_GET_CLASS (theme->impl);
  style_info = get_style_info (theme, variant);
  title_height = get_title_height (theme, variant, type, flags);

  impl_class->calc_geometry (theme->impl, style->layout, style_info,
                             title_height, flags, client_width, client_height,
                             theme->button_layout, type, fgeom);
}

void
meta_theme_draw_frame (MetaTheme           *theme,
                       const gchar         *variant,
                       cairo_t             *cr,
                       MetaFrameType        type,
                       MetaFrameFlags       flags,
                       gint                 client_width,
                       gint                 client_height,
                       const gchar         *title,
                       MetaButtonStateFunc  func,
                       gpointer             user_data,
                       GdkPixbuf           *mini_icon,
                       GdkPixbuf           *icon)
{
  MetaFrameStyle *style;
  MetaThemeImplClass *impl_class;
  MetaStyleInfo *style_info;
  gint title_height;
  PangoLayout *title_layout;
  MetaFrameGeometry fgeom;
  gint i;

  g_return_if_fail (type < META_FRAME_TYPE_LAST);

  style = get_frame_style (theme, type, flags);

  /* Parser is not supposed to allow this currently */
  if (style == NULL)
    return;

  impl_class = META_THEME_IMPL_GET_CLASS (theme->impl);
  style_info = get_style_info (theme, variant);
  title_height = get_title_height (theme, variant, type, flags);
  title_layout = create_title_layout (theme, variant, type, flags, title);

  impl_class->calc_geometry (theme->impl, style->layout, style_info,
                             title_height, flags, client_width, client_height,
                             theme->button_layout, type, &fgeom);

  for (i = 0; i < 2; i++)
    {
      MetaButton *buttons;
      gint n_buttons;
      gint j;

      if (i == 0)
        {
          buttons = theme->button_layout->left_buttons;
          n_buttons = theme->button_layout->n_left_buttons;
        }
      else if (i == 1)
        {
          buttons = theme->button_layout->right_buttons;
          n_buttons = theme->button_layout->n_right_buttons;
        }
      else
        {
          g_assert_not_reached ();
        }

      for (j = 0; j < n_buttons; j++)
        {
          MetaButton *button;
          MetaButtonState state;
          GdkRectangle rect;

          button = &buttons[j];
          state = META_BUTTON_STATE_NORMAL;
          rect = button->rect.visible;

          if (!button->visible || button->type == META_BUTTON_TYPE_SPACER ||
              rect.width <= 0 || rect.height <= 0)
            {
              button->state = state;
              continue;
            }

          if (func != NULL)
            state = (* func) (button->type, button->rect.clickable, user_data);

          g_assert (state >= META_BUTTON_STATE_NORMAL && state < META_BUTTON_STATE_LAST);
          button->state = state;
        }
    }

  impl_class->draw_frame (theme->impl, style, style_info, cr, &fgeom,
                          title_layout, flags, theme->button_layout,
                          mini_icon, icon);

  g_object_unref (title_layout);
}
