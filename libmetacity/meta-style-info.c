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
#include "meta-css-provider-private.h"
#include "meta-frame-enums.h"
#include "meta-style-info-private.h"
#include "meta-theme-impl-private.h"

struct _MetaStyleInfo
{
  GObject          parent;

  gchar           *gtk_theme_name;
  gchar           *gtk_theme_variant;
  gboolean         composited;
  gint             scale;

  GtkCssProvider  *theme_provider;
  GtkCssProvider  *user_provider;

  GtkStyleContext *styles[META_STYLE_ELEMENT_LAST];
};

enum
{
  PROP_0,

  PROP_GTK_THEME_NAME,
  PROP_GTK_THEME_VARIANT,
  PROP_COMPOSITED,
  PROP_SCALE,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (MetaStyleInfo, meta_style_info, G_TYPE_OBJECT)

static void
add_toplevel_class (GtkStyleContext *style,
                    const gchar     *class_name)
{
  if (gtk_style_context_get_parent (style))
    {
      GtkWidgetPath *path;

      path = gtk_widget_path_copy (gtk_style_context_get_path (style));
      gtk_widget_path_iter_add_class (path, 0, class_name);
      gtk_style_context_set_path (style, path);
      gtk_widget_path_unref (path);
    }
  else
    gtk_style_context_add_class (style, class_name);
}

static void
remove_toplevel_class (GtkStyleContext *style,
                       const gchar     *class_name)
{
  if (gtk_style_context_get_parent (style))
    {
      GtkWidgetPath *path;

      path = gtk_widget_path_copy (gtk_style_context_get_path (style));
      gtk_widget_path_iter_remove_class (path, 0, class_name);
      gtk_style_context_set_path (style, path);
      gtk_widget_path_unref (path);
    }
  else
    gtk_style_context_remove_class (style, class_name);
}

static GtkStyleContext *
create_style_context (MetaStyleInfo   *style_info,
                      GtkStyleContext *parent,
                      const gchar     *object_name,
                      const gchar     *first_class,
                      ...)
{
  GtkWidgetPath *path;
  GtkStyleContext *context;
  const gchar *name;
  va_list ap;
  GtkStyleProvider *provider;

  if (parent)
    path = gtk_widget_path_copy (gtk_style_context_get_path (parent));
  else
    path = gtk_widget_path_new ();

  gtk_widget_path_append_type (path, G_TYPE_NONE);
  gtk_widget_path_iter_set_object_name (path, -1, object_name);

  va_start (ap, first_class);
  for (name = first_class; name; name = va_arg (ap, const gchar *))
    gtk_widget_path_iter_add_class (path, -1, name);
  va_end (ap);

  context = gtk_style_context_new ();
  gtk_style_context_set_path (context, path);
  gtk_style_context_set_parent (context, parent);
  gtk_style_context_set_scale (context, style_info->scale);
  gtk_widget_path_unref (path);

  provider = GTK_STYLE_PROVIDER (style_info->theme_provider);
  gtk_style_context_add_provider (context, provider,
                                  GTK_STYLE_PROVIDER_PRIORITY_SETTINGS);

  provider = GTK_STYLE_PROVIDER (style_info->user_provider);
  gtk_style_context_add_provider (context, provider,
                                  GTK_STYLE_PROVIDER_PRIORITY_USER);

  return context;
}

static void
load_user_provider (GtkCssProvider *provider)
{
  gchar *path;

  path = g_build_filename (g_get_user_config_dir (),
                           "gtk-3.0", "gtk.css",
                           NULL);

  if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
    gtk_css_provider_load_from_path (provider, path, NULL);

  g_free (path);
}

static void
meta_style_info_constructed (GObject *object)
{
  MetaStyleInfo *style_info;

  G_OBJECT_CLASS (meta_style_info_parent_class)->constructed (object);

  style_info = META_STYLE_INFO (object);

  style_info->theme_provider = meta_css_provider_new (style_info->gtk_theme_name,
                                                      style_info->gtk_theme_variant);

  style_info->user_provider = gtk_css_provider_new ();
  load_user_provider (style_info->user_provider);

  style_info->styles[META_STYLE_ELEMENT_WINDOW] =
    create_style_context (style_info,
                          NULL,
                          "window",
                          GTK_STYLE_CLASS_BACKGROUND,
                          style_info->composited == TRUE ? "csd" : "solid-csd",
                          "metacity",
                          NULL);

  style_info->styles[META_STYLE_ELEMENT_DECORATION] =
    create_style_context (style_info,
                          style_info->styles[META_STYLE_ELEMENT_WINDOW],
                          "decoration",
                          NULL);

  style_info->styles[META_STYLE_ELEMENT_TITLEBAR] =
    create_style_context (style_info,
                          style_info->styles[META_STYLE_ELEMENT_WINDOW],
                          "headerbar",
                          GTK_STYLE_CLASS_TITLEBAR,
                          GTK_STYLE_CLASS_HORIZONTAL,
                          "default-decoration",
                          NULL);

  style_info->styles[META_STYLE_ELEMENT_TITLE] =
    create_style_context (style_info,
                          style_info->styles[META_STYLE_ELEMENT_TITLEBAR],
                          "label",
                          GTK_STYLE_CLASS_TITLE,
                          NULL);

  style_info->styles[META_STYLE_ELEMENT_BUTTON] =
    create_style_context (style_info,
                          style_info->styles[META_STYLE_ELEMENT_TITLEBAR],
                          "button",
                          "titlebutton",
                          NULL);

  style_info->styles[META_STYLE_ELEMENT_IMAGE] =
    create_style_context (style_info,
                          style_info->styles[META_STYLE_ELEMENT_BUTTON],
                          "image",
                          NULL);
}

static void
meta_style_info_dispose (GObject *object)
{
  MetaStyleInfo *style_info;
  gint i;

  style_info = META_STYLE_INFO (object);

  g_clear_object (&style_info->theme_provider);
  g_clear_object (&style_info->user_provider);

  for (i = 0; i < META_STYLE_ELEMENT_LAST; i++)
    g_clear_object (&style_info->styles[i]);

  G_OBJECT_CLASS (meta_style_info_parent_class)->dispose (object);
}

static void
meta_style_info_finalize (GObject *object)
{
  MetaStyleInfo *style_info;

  style_info = META_STYLE_INFO (object);

  g_free (style_info->gtk_theme_name);
  g_free (style_info->gtk_theme_variant);

  G_OBJECT_CLASS (meta_style_info_parent_class)->finalize (object);
}

static void
meta_style_info_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  MetaStyleInfo *style_info;

  style_info = META_STYLE_INFO (object);

  switch (property_id)
    {
      case PROP_GTK_THEME_NAME:
        style_info->gtk_theme_name = g_value_dup_string (value);
        break;

      case PROP_GTK_THEME_VARIANT:
        style_info->gtk_theme_variant = g_value_dup_string (value);
        break;

      case PROP_COMPOSITED:
        style_info->composited = g_value_get_boolean (value);
        break;

      case PROP_SCALE:
        style_info->scale = g_value_get_int (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
meta_style_info_class_init (MetaStyleInfoClass *style_info_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (style_info_class);

  object_class->constructed = meta_style_info_constructed;
  object_class->dispose = meta_style_info_dispose;
  object_class->finalize = meta_style_info_finalize;
  object_class->set_property = meta_style_info_set_property;

  properties[PROP_GTK_THEME_NAME] =
    g_param_spec_string ("gtk-theme-name",
                         "GTK+ Theme Name",
                         "GTK+ Theme Name",
                         "Adwaita",
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_GTK_THEME_VARIANT] =
    g_param_spec_string ("gtk-theme-variant",
                         "GTK+ Theme Variant",
                         "GTK+ Theme Variant",
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_COMPOSITED] =
    g_param_spec_boolean ("composited",
                          "Composited",
                          "Composited",
                          TRUE,
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                          G_PARAM_STATIC_STRINGS);

  properties[PROP_SCALE] =
    g_param_spec_int ("scale",
                      "Scale",
                      "Scale",
                      1, G_MAXINT, 1,
                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                      G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
meta_style_info_init (MetaStyleInfo *style_info)
{
}

MetaStyleInfo *
meta_style_info_new (const gchar *gtk_theme_name,
                     const gchar *gtk_theme_variant,
                     gboolean     composited,
                     gint         scale)
{
  return g_object_new (META_TYPE_STYLE_INFO,
                       "gtk-theme-name", gtk_theme_name,
                       "gtk-theme-variant", gtk_theme_variant,
                       "composited", composited,
                       "scale", scale,
                       NULL);
}

GtkStyleContext *
meta_style_info_get_style (MetaStyleInfo    *style_info,
                           MetaStyleElement  element)
{
  return style_info->styles[element];
}

void
meta_style_info_set_composited (MetaStyleInfo *style_info,
                                gboolean       composited)
{
  gint i;

  if (style_info->composited == composited)
    return;

  style_info->composited = composited;

  for (i = 0; i < META_STYLE_ELEMENT_LAST; i++)
    {
      GtkStyleContext *style;

      style = style_info->styles[i];

      if (composited)
        {
          remove_toplevel_class (style, "solid-csd");
          add_toplevel_class (style, "csd");
        }
      else
        {
          remove_toplevel_class (style, "csd");
          add_toplevel_class (style, "solid-csd");
        }
    }
}

void
meta_style_info_set_scale (MetaStyleInfo *style_info,
                           gint           scale)
{
  gint i;

  if (style_info->scale == scale)
    return;

  style_info->scale = scale;

  for (i = 0; i < META_STYLE_ELEMENT_LAST; i++)
    gtk_style_context_set_scale (style_info->styles[i], scale);
}

void
meta_style_info_set_flags (MetaStyleInfo  *style_info,
                           MetaFrameFlags  flags)
{
  GtkStyleContext *style;
  gboolean backdrop;
  gint i;

  backdrop = !(flags & META_FRAME_HAS_FOCUS);
  if (flags & META_FRAME_IS_FLASHING)
    backdrop = !backdrop;

  for (i = 0; i < META_STYLE_ELEMENT_LAST; i++)
    {
      GtkStateFlags state;

      style = style_info->styles[i];

      state = gtk_style_context_get_state (style);
      if (backdrop)
        gtk_style_context_set_state (style, state | GTK_STATE_FLAG_BACKDROP);
      else
        gtk_style_context_set_state (style, state & ~GTK_STATE_FLAG_BACKDROP);

      if (flags & META_FRAME_TILED_LEFT || flags & META_FRAME_TILED_RIGHT)
        add_toplevel_class (style, "tiled");
      else
        remove_toplevel_class (style, "tiled");

      if (flags & META_FRAME_MAXIMIZED)
        add_toplevel_class (style, "maximized");
      else
        remove_toplevel_class (style, "maximized");

      if (flags & META_FRAME_FULLSCREEN)
        add_toplevel_class (style, "fullscreen");
      else
        remove_toplevel_class (style, "fullscreen");
    }
}
