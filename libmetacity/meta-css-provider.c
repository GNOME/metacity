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

struct _MetaCssProvider
{
  GtkCssProvider  parent;

  gchar          *name;
  gchar          *variant;

  GResource      *resource;
};

enum
{
  PROP_0,

  PROP_NAME,
  PROP_VARIANT,

  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL };

G_DEFINE_TYPE (MetaCssProvider, meta_css_provider, GTK_TYPE_CSS_PROVIDER)

static gchar *
get_theme_dir (void)
{
  const gchar *prefix;

  prefix = g_getenv ("GTK_DATA_PREFIX");

  if (prefix == NULL)
    prefix = GTK_DATA_PREFIX;

  return g_build_filename (prefix, "share", "themes", NULL);
}

static gint
get_minor_version (void)
{
  if (GTK_MINOR_VERSION % 2)
    return GTK_MINOR_VERSION + 1;

  return GTK_MINOR_VERSION;
}

static gchar *
find_theme_dir (const gchar *dir,
                const gchar *subdir,
                const gchar *name,
                const gchar *variant)
{
  gchar *file;
  gchar *base;
  gchar *subsubdir;
  gint i;
  gchar *path;

  if (variant)
    file = g_strconcat ("gtk-", variant, ".css", NULL);
  else
    file = g_strdup ("gtk.css");

  if (subdir)
    base = g_build_filename (dir, subdir, name, NULL);
  else
    base = g_build_filename (dir, name, NULL);

  for (i = get_minor_version (); i >= 0; i = i - 2)
    {
      if (i < 14)
        i = 0;

      subsubdir = g_strdup_printf ("gtk-3.%d", i);
      path = g_build_filename (base, subsubdir, file, NULL);
      g_free (subsubdir);

      if (g_file_test (path, G_FILE_TEST_EXISTS))
        break;

      g_free (path);
      path = NULL;
    }

  g_free (file);
  g_free (base);

  return path;
}

static gchar *
find_theme (const gchar *name,
            const gchar *variant)
{
  gchar *path;
  const gchar *const *dirs;
  gint i;
  gchar *dir;

  /* First look in the user's data directory */
  path = find_theme_dir (g_get_user_data_dir (), "themes", name, variant);
  if (path)
    return path;

  /* Next look in the user's home directory */
  path = find_theme_dir (g_get_home_dir (), ".themes", name, variant);
  if (path)
    return path;

  /* Look in system data directories */
  dirs = g_get_system_data_dirs ();
  for (i = 0; dirs[i]; i++)
    {
      path = find_theme_dir (dirs[i], "themes", name, variant);
      if (path)
        return path;
    }

  /* Finally, try in the default theme directory */
  dir = get_theme_dir ();
  path = find_theme_dir (dir, NULL, name, variant);
  g_free (dir);

  return path;
}

static gboolean
load_builtin_theme (GtkCssProvider *provider,
                    const gchar    *name,
                    const gchar    *variant)
{
  gchar *path;

  if (variant != NULL)
    path = g_strdup_printf ("/org/gtk/libgtk/theme/%s/gtk-%s.css", name, variant);
  else
    path = g_strdup_printf ("/org/gtk/libgtk/theme/%s/gtk.css", name);

  if (g_resources_get_info (path, 0, NULL, NULL, NULL))
    {
      gtk_css_provider_load_from_resource (provider, path);
      g_free (path);

      return TRUE;
    }

  g_free (path);

  return FALSE;
}

static void
provider_load_named (MetaCssProvider *provider,
                     const gchar     *name,
                     const gchar     *variant)
{
  GtkCssProvider *css_provider;
  gchar *path;

  if (name == NULL)
    return;

  css_provider = GTK_CSS_PROVIDER (provider);

  if (load_builtin_theme (css_provider, name, variant))
    return;

  path = find_theme (name, variant);

  if (path != NULL)
    {
      gchar *dir;
      gchar *file;

      dir = g_path_get_dirname (path);
      file = g_build_filename (dir, "gtk.gresource", NULL);
      g_free (dir);

      provider->resource = g_resource_load (file, NULL);
      g_free (file);

      if (provider->resource != NULL)
        g_resources_register (provider->resource);

      gtk_css_provider_load_from_path (css_provider, path, NULL);
      g_free (path);
    }
  else if (variant != NULL)
    {
      provider_load_named (provider, name, NULL);
    }
}

static void
meta_css_provider_constructed (GObject *object)
{
  MetaCssProvider *provider;

  G_OBJECT_CLASS (meta_css_provider_parent_class)->constructed (object);

  provider = META_CSS_PROVIDER (object);

  provider_load_named (provider, provider->name, provider->variant);
}

static void
meta_css_provider_finalize (GObject *object)
{
  MetaCssProvider *provider;

  provider = META_CSS_PROVIDER (object);

  g_clear_pointer (&provider->name, g_free);
  g_clear_pointer (&provider->variant, g_free);

  if (provider->resource)
    {
      g_resources_unregister (provider->resource);
      g_resource_unref (provider->resource);
      provider->resource = NULL;
    }

  G_OBJECT_CLASS (meta_css_provider_parent_class)->finalize (object);
}

static void
meta_css_provider_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  MetaCssProvider *provider;

  provider = META_CSS_PROVIDER (object);

  switch (property_id)
    {
      case PROP_NAME:
        provider->name = g_value_dup_string (value);
        break;

      case PROP_VARIANT:
        provider->variant = g_value_dup_string (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
meta_css_provider_class_init (MetaCssProviderClass *provider_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (provider_class);

  object_class->constructed = meta_css_provider_constructed;
  object_class->finalize = meta_css_provider_finalize;
  object_class->set_property = meta_css_provider_set_property;

  properties[PROP_NAME] =
    g_param_spec_string ("name",
                         "GTK+ Theme Name",
                         "GTK+ Theme Name",
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_VARIANT] =
    g_param_spec_string ("variant",
                         "GTK+ Theme Variant",
                         "GTK+ Theme Variant",
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
meta_css_provider_init (MetaCssProvider *provider)
{
}

GtkCssProvider *
meta_css_provider_new (const gchar *name,
                       const gchar *variant)
{
  return g_object_new (META_TYPE_CSS_PROVIDER,
                       "name", name, "variant", variant,
                       NULL);
}
