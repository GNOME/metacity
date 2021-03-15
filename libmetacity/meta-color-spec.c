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

#include "meta-color.h"
#include "meta-color-private.h"
#include "meta-color-spec-private.h"
#include "meta-theme.h"

enum _MetaColorSpecType
{
  META_COLOR_SPEC_BASIC,
  META_COLOR_SPEC_GTK,
  META_COLOR_SPEC_GTK_CUSTOM,
  META_COLOR_SPEC_BLEND,
  META_COLOR_SPEC_SHADE
};

enum _MetaGtkColorComponent
{
  META_GTK_COLOR_FG,
  META_GTK_COLOR_BG,
  META_GTK_COLOR_LIGHT,
  META_GTK_COLOR_DARK,
  META_GTK_COLOR_MID,
  META_GTK_COLOR_TEXT,
  META_GTK_COLOR_BASE,
  META_GTK_COLOR_TEXT_AA,
  META_GTK_COLOR_LAST
};

struct _MetaColorSpec
{
  MetaColorSpecType type;
  union
  {
    struct {
      GdkRGBA color;
    } basic;
    struct {
      MetaGtkColorComponent component;
      GtkStateFlags         state;
    } gtk;
    struct {
      gchar         *color_name;
      MetaColorSpec *fallback;
    } gtkcustom;
    struct {
      MetaColorSpec *foreground;
      MetaColorSpec *background;
      gdouble        alpha;

      GdkRGBA        color;
    } blend;
    struct {
      MetaColorSpec *base;
      gdouble        factor;

      GdkRGBA        color;
    } shade;
  } data;
};

static void
set_color_from_style (GdkRGBA               *color,
                      GtkStyleContext       *context,
                      GtkStateFlags          state,
                      MetaGtkColorComponent  component)
{
  GdkRGBA other;

  gtk_style_context_set_state (context, state);

  switch (component)
    {
      case META_GTK_COLOR_BG:
      case META_GTK_COLOR_BASE:
        meta_color_get_background_color (context, state, color);
        break;

      case META_GTK_COLOR_FG:
      case META_GTK_COLOR_TEXT:
        gtk_style_context_get_color (context, state, color);
        break;

      case META_GTK_COLOR_TEXT_AA:
        gtk_style_context_get_color (context, state, color);
        set_color_from_style (&other, context, state, META_GTK_COLOR_BASE);

        color->red = (color->red + other.red) / 2;
        color->green = (color->green + other.green) / 2;
        color->blue = (color->blue + other.blue) / 2;
        break;

      case META_GTK_COLOR_MID:
        meta_color_get_light_color (context, state, color);
        meta_color_get_dark_color (context, state, &other);

        color->red = (color->red + other.red) / 2;
        color->green = (color->green + other.green) / 2;
        color->blue = (color->blue + other.blue) / 2;
        break;

      case META_GTK_COLOR_LIGHT:
        meta_color_get_light_color (context, state, color);
        break;

      case META_GTK_COLOR_DARK:
        meta_color_get_dark_color (context, state, color);
        break;

      case META_GTK_COLOR_LAST:
      default:
        g_assert_not_reached ();
        break;
    }
}

static void
color_composite (const GdkRGBA *bg,
                 const GdkRGBA *fg,
                 gdouble        alpha,
                 GdkRGBA       *color)
{
  *color = *bg;

  color->red = color->red + (fg->red - color->red) * alpha;
  color->green = color->green + (fg->green - color->green) * alpha;
  color->blue = color->blue + (fg->blue - color->blue) * alpha;
  color->alpha = color->alpha + (fg->alpha - color->alpha) * alpha;
}

static void
set_custom_color_from_style (GdkRGBA         *color,
                             GtkStyleContext *context,
                             const gchar     *color_name,
                             MetaColorSpec   *fallback)
{
  if (!gtk_style_context_lookup_color (context, color_name, color))
    meta_color_spec_render (fallback, context, color);
}

static MetaGtkColorComponent
meta_color_component_from_string (const gchar *str)
{
  if (strcmp ("fg", str) == 0)
    return META_GTK_COLOR_FG;
  else if (strcmp ("bg", str) == 0)
    return META_GTK_COLOR_BG;
  else if (strcmp ("light", str) == 0)
    return META_GTK_COLOR_LIGHT;
  else if (strcmp ("dark", str) == 0)
    return META_GTK_COLOR_DARK;
  else if (strcmp ("mid", str) == 0)
    return META_GTK_COLOR_MID;
  else if (strcmp ("text", str) == 0)
    return META_GTK_COLOR_TEXT;
  else if (strcmp ("base", str) == 0)
    return META_GTK_COLOR_BASE;
  else if (strcmp ("text_aa", str) == 0)
    return META_GTK_COLOR_TEXT_AA;
  else
    return META_GTK_COLOR_LAST;
}

MetaColorSpec *
meta_color_spec_new (MetaColorSpecType type)
{
  MetaColorSpec *spec;
  MetaColorSpec dummy;
  gint size;

  size = G_STRUCT_OFFSET (MetaColorSpec, data);

  switch (type)
    {
      case META_COLOR_SPEC_BASIC:
        size += sizeof (dummy.data.basic);
        break;

      case META_COLOR_SPEC_GTK:
        size += sizeof (dummy.data.gtk);
        break;

      case META_COLOR_SPEC_GTK_CUSTOM:
        size += sizeof (dummy.data.gtkcustom);
        break;

      case META_COLOR_SPEC_BLEND:
        size += sizeof (dummy.data.blend);
        break;

      case META_COLOR_SPEC_SHADE:
        size += sizeof (dummy.data.shade);
        break;

      default:
        break;
    }

  spec = g_malloc0 (size);

  spec->type = type;

  return spec;
}

MetaColorSpec *
meta_color_spec_new_from_string (const gchar  *str,
                                 GError      **error)
{
  MetaColorSpec *spec;

  spec = NULL;

  if (strncmp (str, "gtk:custom", 10) == 0)
    {
      const gchar *color_name_start;
      const gchar *fallback_str_start;
      const gchar *end;
      gchar *fallback_str;
      MetaColorSpec *fallback;
      gchar *color_name;

      if (str[10] != '(')
        {
          g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                       _("GTK custom color specification must have color name and fallback in parentheses, e.g. gtk:custom(foo,bar); could not parse '%s'"),
                       str);
          return NULL;
        }

      color_name_start = str + 11;

      fallback_str_start = color_name_start;
      while (*fallback_str_start && *fallback_str_start != ',')
        {
          if (!(g_ascii_isalnum (*fallback_str_start)
                || *fallback_str_start == '-' || *fallback_str_start == '_'))
            {
              g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                           _("Invalid character '%c' in color_name parameter of gtk:custom, only A-Za-z0-9-_ are valid"),
                           *fallback_str_start);
              return NULL;
            }
          fallback_str_start++;
        }

      if (*fallback_str_start != '\0')
        fallback_str_start++;

      end = strrchr (str, ')');

      if (*color_name_start == '\0' ||
          *fallback_str_start == '\0' ||
          end == NULL)
        {
          g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                       _("Gtk:custom format is 'gtk:custom(color_name,fallback)', '%s' does not fit the format"),
                       str);
          return NULL;
        }

      fallback_str = g_strndup (fallback_str_start, end - fallback_str_start);
      fallback = meta_color_spec_new_from_string (fallback_str, error);
      g_free (fallback_str);

      if (fallback == NULL)
        return NULL;

      color_name = g_strndup (color_name_start, fallback_str_start - color_name_start - 1);

      spec = meta_color_spec_new (META_COLOR_SPEC_GTK_CUSTOM);
      spec->data.gtkcustom.color_name = color_name;
      spec->data.gtkcustom.fallback = fallback;
    }
  else if (strncmp (str, "gtk:", 4) == 0)
    {
      const gchar *bracket;
      const gchar *end_bracket;
      gchar *tmp;
      GtkStateFlags state;
      MetaGtkColorComponent component;

      bracket = str;
      while (*bracket && *bracket != '[')
        ++bracket;

      if (*bracket == '\0')
        {
          g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                       _("GTK color specification must have the state in brackets, e.g. gtk:fg[NORMAL] where NORMAL is the state; could not parse '%s'"),
                       str);
          return NULL;
        }

      end_bracket = bracket;
      ++end_bracket;
      while (*end_bracket && *end_bracket != ']')
        ++end_bracket;

      if (*end_bracket == '\0')
        {
          g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                       _("GTK color specification must have a close bracket after the state, e.g. gtk:fg[NORMAL] where NORMAL is the state; could not parse '%s'"),
                       str);
          return NULL;
        }

      tmp = g_strndup (bracket + 1, end_bracket - bracket - 1);
      if (!meta_gtk_state_from_string (tmp, &state))
        {
          g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                       _("Did not understand state '%s' in color specification"),
                       tmp);
          g_free (tmp);
          return NULL;
        }
      g_free (tmp);

      tmp = g_strndup (str + 4, bracket - str - 4);
      component = meta_color_component_from_string (tmp);
      if (component == META_GTK_COLOR_LAST)
        {
          g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                       _("Did not understand color component '%s' in color specification"),
                       tmp);
          g_free (tmp);
          return NULL;
        }
      g_free (tmp);

      spec = meta_color_spec_new (META_COLOR_SPEC_GTK);
      spec->data.gtk.state = state;
      spec->data.gtk.component = component;
      g_assert (spec->data.gtk.component < META_GTK_COLOR_LAST);
    }
  else if (strncmp (str, "blend/", 6) == 0)
    {
      gchar **split;
      gdouble alpha;
      gchar *end;
      MetaColorSpec *fg;
      MetaColorSpec *bg;

      split = g_strsplit (str, "/", 4);

      if (split[0] == NULL || split[1] == NULL ||
          split[2] == NULL || split[3] == NULL)
        {
          g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                       _("Blend format is 'blend/bg_color/fg_color/alpha', '%s' does not fit the format"),
                       str);
          g_strfreev (split);
          return NULL;
        }

      alpha = g_ascii_strtod (split[3], &end);
      if (end == split[3])
        {
          g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                       _("Could not parse alpha value '%s' in blended color"),
                       split[3]);
          g_strfreev (split);
          return NULL;
        }

      if (alpha < (0.0 - 1e6) || alpha > (1.0 + 1e6))
        {
          g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                       _("Alpha value '%s' in blended color is not between 0.0 and 1.0"),
                       split[3]);
          g_strfreev (split);
          return NULL;
        }

      fg = NULL;
      bg = NULL;

      bg = meta_color_spec_new_from_string (split[1], error);
      if (bg == NULL)
        {
          g_strfreev (split);
          return NULL;
        }

      fg = meta_color_spec_new_from_string (split[2], error);
      if (fg == NULL)
        {
          meta_color_spec_free (bg);
          g_strfreev (split);
          return NULL;
        }

      g_strfreev (split);

      spec = meta_color_spec_new (META_COLOR_SPEC_BLEND);
      spec->data.blend.alpha = alpha;
      spec->data.blend.background = bg;
      spec->data.blend.foreground = fg;
    }
  else if (strncmp (str, "shade/", 6) == 0)
    {
      gchar **split;
      gdouble factor;
      gchar *end;
      MetaColorSpec *base;

      split = g_strsplit (str, "/", 3);

      if (split[0] == NULL || split[1] == NULL || split[2] == NULL)
        {
          g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                       _("Shade format is 'shade/base_color/factor', '%s' does not fit the format"),
                       str);
          g_strfreev (split);
          return NULL;
        }

      factor = g_ascii_strtod (split[2], &end);
      if (end == split[2])
        {
          g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                       _("Could not parse shade factor '%s' in shaded color"),
                       split[2]);
          g_strfreev (split);
          return NULL;
        }

      if (factor < (0.0 - 1e6))
        {
          g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                       _("Shade factor '%s' in shaded color is negative"),
                       split[2]);
          g_strfreev (split);
          return NULL;
        }

      base = meta_color_spec_new_from_string (split[1], error);
      if (base == NULL)
        {
          g_strfreev (split);
          return NULL;
        }

      g_strfreev (split);

      spec = meta_color_spec_new (META_COLOR_SPEC_SHADE);
      spec->data.shade.factor = factor;
      spec->data.shade.base = base;
    }
  else
    {
      spec = meta_color_spec_new (META_COLOR_SPEC_BASIC);

      if (!gdk_rgba_parse (&spec->data.basic.color, str))
        {
          g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                       _("Could not parse color '%s'"), str);
          meta_color_spec_free (spec);
          return NULL;
        }
    }

  g_assert (spec);

  return spec;
}

MetaColorSpec *
meta_color_spec_new_gtk (MetaGtkColorComponent component,
                         GtkStateFlags         state)
{
  MetaColorSpec *spec;

  spec = meta_color_spec_new (META_COLOR_SPEC_GTK);

  spec->data.gtk.component = component;
  spec->data.gtk.state = state;

  return spec;
}

void
meta_color_spec_free (MetaColorSpec *spec)
{
  g_return_if_fail (spec != NULL);

  switch (spec->type)
    {
      case META_COLOR_SPEC_BASIC:
        break;

      case META_COLOR_SPEC_GTK:
        break;

      case META_COLOR_SPEC_GTK_CUSTOM:
        if (spec->data.gtkcustom.color_name)
          g_free (spec->data.gtkcustom.color_name);
        if (spec->data.gtkcustom.fallback)
          meta_color_spec_free (spec->data.gtkcustom.fallback);
        break;

      case META_COLOR_SPEC_BLEND:
        if (spec->data.blend.foreground)
          meta_color_spec_free (spec->data.blend.foreground);
        if (spec->data.blend.background)
          meta_color_spec_free (spec->data.blend.background);
        break;

      case META_COLOR_SPEC_SHADE:
        if (spec->data.shade.base)
          meta_color_spec_free (spec->data.shade.base);
        break;

      default:
        break;
    }

  g_free (spec);
}

void
meta_color_spec_render (MetaColorSpec   *spec,
                        GtkStyleContext *context,
                        GdkRGBA         *color)
{
  g_return_if_fail (spec != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  switch (spec->type)
    {
      case META_COLOR_SPEC_BASIC:
        *color = spec->data.basic.color;
        break;

      case META_COLOR_SPEC_GTK:
        set_color_from_style (color, context, spec->data.gtk.state,
                              spec->data.gtk.component);
        break;

      case META_COLOR_SPEC_GTK_CUSTOM:
        set_custom_color_from_style (color, context,
                                     spec->data.gtkcustom.color_name,
                                     spec->data.gtkcustom.fallback);
        break;

      case META_COLOR_SPEC_BLEND:
        {
          GdkRGBA bg;
          GdkRGBA fg;

          meta_color_spec_render (spec->data.blend.background, context, &bg);
          meta_color_spec_render (spec->data.blend.foreground, context, &fg);

          color_composite (&bg, &fg, spec->data.blend.alpha,
                           &spec->data.blend.color);

          *color = spec->data.blend.color;
        }
        break;

      case META_COLOR_SPEC_SHADE:
        {
          meta_color_spec_render (spec->data.shade.base, context,
                                  &spec->data.shade.color);

          meta_color_shade (&spec->data.shade.color, spec->data.shade.factor,
                            &spec->data.shade.color);

          *color = spec->data.shade.color;
        }
        break;

      default:
        break;
    }
}

/**
 * meta_gtk_state_from_string:
 * @str: state string
 * @state: (out): location to store #GtkStateFlags
 *
 * Convert string to #GtkStateFlags
 *
 * Returns: %TRUE if state string was valid, %FALSE otherwise
 */
gboolean
meta_gtk_state_from_string (const gchar   *str,
                            GtkStateFlags *state)
{
  if (g_ascii_strcasecmp ("normal", str) == 0)
    *state = GTK_STATE_FLAG_NORMAL;
  else if (g_ascii_strcasecmp ("prelight", str) == 0)
    *state = GTK_STATE_FLAG_PRELIGHT;
  else if (g_ascii_strcasecmp ("active", str) == 0)
    *state = GTK_STATE_FLAG_ACTIVE;
  else if (g_ascii_strcasecmp ("selected", str) == 0)
    *state = GTK_STATE_FLAG_SELECTED;
  else if (g_ascii_strcasecmp ("insensitive", str) == 0)
    *state = GTK_STATE_FLAG_INSENSITIVE;
  else if (g_ascii_strcasecmp ("inconsistent", str) == 0)
    *state = GTK_STATE_FLAG_INCONSISTENT;
  else if (g_ascii_strcasecmp ("focused", str) == 0)
    *state = GTK_STATE_FLAG_FOCUSED;
  else if (g_ascii_strcasecmp ("backdrop", str) == 0)
    *state = GTK_STATE_FLAG_BACKDROP;
  else
    return FALSE;

  return TRUE;
}
