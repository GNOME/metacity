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

#include "meta-frame-enums.h"
#include "meta-style-info-private.h"

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
create_style_context (GtkStyleContext *parent,
                      GtkCssProvider  *provider,
                      const char      *object_name,
                      const char      *first_class,
                      ...)
{
  GtkWidgetPath *path;
  GtkStyleContext *context;
  const char *name;
  va_list ap;

  if (parent)
    path = gtk_widget_path_copy (gtk_style_context_get_path (parent));
  else
    path = gtk_widget_path_new ();

  gtk_widget_path_append_type (path, G_TYPE_NONE);
  gtk_widget_path_iter_set_object_name (path, -1, object_name);

  va_start (ap, first_class);
  for (name = first_class; name; name = va_arg (ap, const char *))
    gtk_widget_path_iter_add_class (path, -1, name);
  va_end (ap);

  context = gtk_style_context_new ();
  gtk_style_context_set_path (context, path);
  gtk_style_context_set_parent (context, parent);
  gtk_widget_path_unref (path);

  gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_SETTINGS);

  return context;
}

MetaStyleInfo *
meta_style_info_new (const gchar *theme_name,
                     const gchar *variant,
                     gboolean     composited)
{
  MetaStyleInfo *style_info;
  GtkCssProvider *provider;

  if (theme_name && *theme_name)
    provider = gtk_css_provider_get_named (theme_name, variant);
  else
    provider = gtk_css_provider_get_default ();

  style_info = g_new0 (MetaStyleInfo, 1);
  style_info->refcount = 1;

  style_info->styles[META_STYLE_ELEMENT_WINDOW] =
    create_style_context (NULL,
                          provider,
                          "window",
                          GTK_STYLE_CLASS_BACKGROUND,
                          composited == TRUE ? "ssd" : "solid-csd",
                          NULL);
  style_info->styles[META_STYLE_ELEMENT_DECORATION] =
    create_style_context (style_info->styles[META_STYLE_ELEMENT_WINDOW],
                          provider,
                          "decoration",
                          NULL);
  style_info->styles[META_STYLE_ELEMENT_TITLEBAR] =
    create_style_context (style_info->styles[META_STYLE_ELEMENT_DECORATION],
                          provider,
                          "headerbar",
                          GTK_STYLE_CLASS_TITLEBAR,
                          GTK_STYLE_CLASS_HORIZONTAL,
                          "default-decoration",
                          NULL);
  style_info->styles[META_STYLE_ELEMENT_TITLE] =
    create_style_context (style_info->styles[META_STYLE_ELEMENT_TITLEBAR],
                          provider,
                          "label",
                          GTK_STYLE_CLASS_TITLE,
                          NULL);
  style_info->styles[META_STYLE_ELEMENT_BUTTON] =
    create_style_context (style_info->styles[META_STYLE_ELEMENT_TITLEBAR],
                          provider,
                          "button",
                          "titlebutton",
                          NULL);
  style_info->styles[META_STYLE_ELEMENT_IMAGE] =
    create_style_context (style_info->styles[META_STYLE_ELEMENT_BUTTON],
                          provider,
                          "image",
                          NULL);

  return style_info;
}

MetaStyleInfo *
meta_style_info_ref (MetaStyleInfo *style_info)
{
  g_return_val_if_fail (style_info != NULL, NULL);
  g_return_val_if_fail (style_info->refcount > 0, NULL);

  g_atomic_int_inc ((volatile int *)&style_info->refcount);
  return style_info;
}

void
meta_style_info_unref (MetaStyleInfo *style_info)
{
  g_return_if_fail (style_info != NULL);
  g_return_if_fail (style_info->refcount > 0);

  if (g_atomic_int_dec_and_test ((volatile int *)&style_info->refcount))
    {
      int i;
      for (i = 0; i < META_STYLE_ELEMENT_LAST; i++)
        g_object_unref (style_info->styles[i]);
      g_free (style_info);
    }
}

void
meta_style_info_set_flags (MetaStyleInfo  *style_info,
                           MetaFrameFlags  flags)
{
  GtkStyleContext *style;
  gboolean backdrop;
  int i;

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
