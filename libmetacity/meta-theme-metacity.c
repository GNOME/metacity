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

#include "config.h"

#include <glib/gi18n-lib.h>

#include "meta-draw-op.h"
#include "meta-theme.h"
#include "meta-theme-metacity.h"

struct _MetaThemeMetacity
{
  MetaThemeImpl  parent;

  GHashTable    *integers;
  GHashTable    *floats;
  GHashTable    *colors;

  GHashTable    *draw_op_lists;
};

G_DEFINE_TYPE (MetaThemeMetacity, meta_theme_metacity, META_TYPE_THEME_IMPL)

static gboolean
first_uppercase (const gchar *str)
{
  return g_ascii_isupper (*str);
}

static void
meta_theme_metacity_dispose (GObject *object)
{
  MetaThemeMetacity *metacity;

  metacity = META_THEME_METACITY (object);

  g_clear_pointer (&metacity->integers, g_hash_table_destroy);
  g_clear_pointer (&metacity->floats, g_hash_table_destroy);
  g_clear_pointer (&metacity->colors, g_hash_table_destroy);

  g_clear_pointer (&metacity->draw_op_lists, g_hash_table_destroy);

  G_OBJECT_CLASS (meta_theme_metacity_parent_class)->dispose (object);
}

static void
meta_theme_metacity_class_init (MetaThemeMetacityClass *metacity_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (metacity_class);

  object_class->dispose = meta_theme_metacity_dispose;
}

static void
meta_theme_metacity_init (MetaThemeMetacity *metacity)
{
  metacity->draw_op_lists = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                                   (GDestroyNotify) meta_draw_op_list_unref);
}

gboolean
meta_theme_metacity_define_int (MetaThemeMetacity  *metacity,
                                const gchar        *name,
                                gint                value,
                                GError            **error)
{
  if (metacity->integers == NULL)
    {
      metacity->integers = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  g_free, NULL);
    }

  if (!first_uppercase (name))
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("User-defined constants must begin with a capital letter; '%s' does not"),
                   name);

      return FALSE;
    }

  if (g_hash_table_lookup_extended (metacity->integers, name, NULL, NULL))
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("Constant '%s' has already been defined"), name);

      return FALSE;
    }

  g_hash_table_insert (metacity->integers, g_strdup (name),
                       GINT_TO_POINTER (value));

  return TRUE;
}

gboolean
meta_theme_metacity_lookup_int (MetaThemeMetacity *metacity,
                                const gchar       *name,
                                gint              *value)
{
  gpointer tmp;

  *value = 0;

  if (metacity->integers == NULL)
    return FALSE;

  if (!g_hash_table_lookup_extended (metacity->integers, name, NULL, &tmp))
    return FALSE;

  *value = GPOINTER_TO_INT (tmp);

  return TRUE;
}

gboolean
meta_theme_metacity_define_float (MetaThemeMetacity  *metacity,
                                  const gchar        *name,
                                  gdouble             value,
                                  GError            **error)
{
  gdouble *d;

  if (metacity->floats == NULL)
    {
      metacity->floats = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                g_free, g_free);
    }

  if (!first_uppercase (name))
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("User-defined constants must begin with a capital letter; '%s' does not"),
                   name);

      return FALSE;
    }

  if (g_hash_table_lookup_extended (metacity->floats, name, NULL, NULL))
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("Constant '%s' has already been defined"), name);

      return FALSE;
    }

  d = g_new (gdouble, 1);

  *d = value;

  g_hash_table_insert (metacity->floats, g_strdup (name), d);

  return TRUE;
}

gboolean
meta_theme_metacity_lookup_float (MetaThemeMetacity *metacity,
                                  const gchar       *name,
                                  gdouble           *value)
{
  gdouble *d;

  *value = 0.0;

  if (metacity->floats == NULL)
    return FALSE;

  d = g_hash_table_lookup (metacity->floats, name);

  if (!d)
    return FALSE;

  *value = *d;

  return TRUE;
}

gboolean
meta_theme_metacity_define_color (MetaThemeMetacity  *metacity,
                                  const gchar        *name,
                                  const gchar        *value,
                                  GError            **error)
{
  if (metacity->colors == NULL)
    {
      metacity->colors = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                g_free, g_free);
    }

  if (!first_uppercase (name))
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("User-defined constants must begin with a capital letter; '%s' does not"),
                   name);

      return FALSE;
    }

  if (g_hash_table_lookup_extended (metacity->colors, name, NULL, NULL))
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("Constant '%s' has already been defined"), name);

      return FALSE;
    }

  g_hash_table_insert (metacity->colors, g_strdup (name), g_strdup (value));

  return TRUE;
}

gboolean
meta_theme_metacity_lookup_color (MetaThemeMetacity  *metacity,
                                  const gchar        *name,
                                  gchar             **value)
{
  gchar *result;

  *value = NULL;

  if (metacity->colors == NULL)
    return FALSE;

  result = g_hash_table_lookup (metacity->colors, name);

  if (!result)
    return FALSE;

  *value = result;

  return TRUE;
}

MetaDrawOpList *
meta_theme_metacity_lookup_draw_op_list (MetaThemeMetacity *metacity,
                                         const gchar       *name)
{
  return g_hash_table_lookup (metacity->draw_op_lists, name);
}

void
meta_theme_metacity_insert_draw_op_list (MetaThemeMetacity *metacity,
                                         const gchar       *name,
                                         MetaDrawOpList    *op_list)
{
  meta_draw_op_list_ref (op_list);
  g_hash_table_replace (metacity->draw_op_lists, g_strdup (name), op_list);
}
