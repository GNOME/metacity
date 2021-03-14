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
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "meta-draw-op-private.h"
#include "meta-frame-layout-private.h"
#include "meta-frame-style-private.h"
#include "meta-theme.h"
#include "meta-theme-metacity-private.h"

/* We were intending to put the version number
 * in the subdirectory name, but we ended up
 * using the filename instead.  The "-1" survives
 * as a fossil.
 */
#define THEME_SUBDIR "metacity-1"

#define METACITY_THEME_FILENAME_FORMAT "metacity-theme-%d.xml"

/* Highest version of the theme format to
 * look out for.
 */
#define THEME_MAJOR_VERSION 3
#define THEME_MINOR_VERSION 4
#define THEME_VERSION (1000 * THEME_MAJOR_VERSION + THEME_MINOR_VERSION)

/* Translators: This means that an attribute which should have been found
 * on an XML element was not in fact found.
 */
#define ATTRIBUTE_NOT_FOUND _("No '%s' attribute on element <%s>")

/* What version of the theme file format were feature introduced in? */
#define META_THEME_COLOR_CONSTANTS 2
#define META_THEME_DEGREES_IN_ARCS 2
#define META_THEME_FRAME_BACKGROUNDS 2
#define META_THEME_HIDDEN_BUTTONS 2
#define META_THEME_IMAGES_FROM_ICON_THEMES 2
#define META_THEME_SHADE_STICK_ABOVE_BUTTONS 2
#define META_THEME_UBIQUITOUS_CONSTANTS 2
#define META_THEME_UNRESIZABLE_SHADED_STYLES 2
#define META_THEME_VARIED_ROUND_CORNERS 2

struct _MetaThemeMetacity
{
  MetaThemeImpl      parent;

  MetaFrameStyleSet *style_sets_by_type[META_FRAME_TYPE_LAST];

  gchar             *name;
  gchar             *dirname;

  guint              format_version;

  gchar             *readable_name;
  gchar             *author;
  gchar             *copyright;
  gchar             *date;
  gchar             *description;

  GHashTable        *integers;
  GHashTable        *floats;
  GHashTable        *colors;

  GHashTable        *draw_op_lists;
  GHashTable        *frame_layouts;
  GHashTable        *styles;
  GHashTable        *style_sets;
  GHashTable        *images;
};

typedef enum
{
  STATE_START,
  STATE_THEME,
  /* info section */
  STATE_INFO,
  STATE_NAME,
  STATE_AUTHOR,
  STATE_COPYRIGHT,
  STATE_DATE,
  STATE_DESCRIPTION,
  /* constants */
  STATE_CONSTANT,
  /* geometry */
  STATE_FRAME_GEOMETRY,
  STATE_DISTANCE,
  STATE_BORDER,
  STATE_ASPECT_RATIO,
  /* draw ops */
  STATE_DRAW_OPS,
  STATE_LINE,
  STATE_RECTANGLE,
  STATE_ARC,
  STATE_CLIP,
  STATE_TINT,
  STATE_GRADIENT,
  STATE_IMAGE,
  STATE_GTK_ARROW,
  STATE_GTK_BOX,
  STATE_GTK_VLINE,
  STATE_ICON,
  STATE_TITLE,
  STATE_INCLUDE, /* include another draw op list */
  STATE_TILE,    /* tile another draw op list */
  /* sub-parts of gradient */
  STATE_COLOR,
  /* frame style */
  STATE_FRAME_STYLE,
  STATE_PIECE,
  STATE_BUTTON,
  /* style set */
  STATE_FRAME_STYLE_SET,
  STATE_FRAME,
  /* assigning style sets to windows */
  STATE_WINDOW,
  /* things we don't use any more but we can still parse: */
  STATE_MENU_ICON,
  STATE_FALLBACK
} ParseState;

typedef struct
{
  /* This two lists contain stacks of state and required version
   * (cast to pointers.) There is one list item for each currently
   * open element. */
  GSList            *states;
  GSList            *required_versions;

  MetaThemeMetacity *metacity;       /* theme being parsed */

  gchar             *name;           /* name of named thing being parsed */
  MetaFrameLayout   *layout;         /* layout being parsed if any */
  MetaDrawOpList    *op_list;        /* op list being parsed if any */
  MetaDrawOp        *op;             /* op being parsed if any */
  MetaFrameStyle    *style;          /* frame style being parsed if any */
  MetaFrameStyleSet *style_set;      /* frame style set being parsed if any */
  MetaFramePiece     piece;          /* position of piece being parsed */
  MetaButtonFunction button_function; /* type of button/menuitem being parsed */
  MetaButtonState    button_state;   /* state of button being parsed */

  gint               skip_level;     /* depth of elements that we're ignoring */
} ParseInfo;

typedef struct
{
  const gchar  *name;
  const gchar **retloc;
  gboolean      required;
} LocateAttr;

G_DEFINE_TYPE (MetaThemeMetacity, meta_theme_metacity, META_TYPE_THEME_IMPL)

static gboolean
theme_allows (MetaThemeMetacity *metacity,
              guint              feature)
{
  if (metacity->format_version >= feature)
    return TRUE;

  return FALSE;
}

static ParseInfo *
parse_info_new (MetaThemeMetacity *metacity)
{
  ParseInfo *info;

  info = g_new0 (ParseInfo, 1);

  info->states = g_slist_prepend (NULL, GINT_TO_POINTER (STATE_START));
  info->required_versions = NULL;

  info->metacity = metacity;

  info->name = NULL;
  info->layout = NULL;
  info->op_list = NULL;
  info->op = NULL;
  info->style = NULL;
  info->style_set = NULL;
  info->piece = META_FRAME_PIECE_LAST;
  info->button_function = META_BUTTON_FUNCTION_LAST;
  info->button_state = META_BUTTON_STATE_LAST;

  info->skip_level = 0;

  return info;
}

static void
parse_info_free (ParseInfo *info)
{
  g_slist_free (info->states);
  g_slist_free (info->required_versions);

  if (info->layout)
    meta_frame_layout_unref (info->layout);

  if (info->op_list)
    meta_draw_op_list_unref (info->op_list);

  if (info->op)
    meta_draw_op_free (info->op);

  if (info->style)
    meta_frame_style_unref (info->style);

  if (info->style_set)
    meta_frame_style_set_unref (info->style_set);

  g_free (info);
}

static void
push_state (ParseInfo  *info,
            ParseState  state)
{
  info->states = g_slist_prepend (info->states, GINT_TO_POINTER (state));
}

static void
pop_state (ParseInfo *info)
{
  g_return_if_fail (info->states != NULL);

  info->states = g_slist_remove (info->states, info->states->data);
}

static void
push_required_version (ParseInfo *info,
                       int        version)
{
  info->required_versions = g_slist_prepend (info->required_versions,
                                             GINT_TO_POINTER (version));
}

static void
pop_required_version (ParseInfo *info)
{
  g_return_if_fail (info->required_versions != NULL);

  info->required_versions = g_slist_delete_link (info->required_versions,
                                                 info->required_versions);
}

static ParseState
peek_state (ParseInfo *info)
{
  g_return_val_if_fail (info->states != NULL, STATE_START);

  return GPOINTER_TO_INT (info->states->data);
}

static int
peek_required_version (ParseInfo *info)
{
  if (info->required_versions)
    return GPOINTER_TO_INT (info->required_versions->data);
  else
    return info->metacity->format_version;
}

G_GNUC_PRINTF(5, 6)
static void
set_error (GError              **err,
           GMarkupParseContext  *context,
           gint                  error_domain,
           gint                  error_code,
           const gchar          *format,
           ...)
{
  gint line;
  gint ch;
  va_list args;
  gchar *str;

  g_markup_parse_context_get_position (context, &line, &ch);

  va_start (args, format);
  str = g_strdup_vprintf (format, args);
  va_end (args);

  g_set_error (err, error_domain, error_code,
               _("Line %d character %d: %s"),
               line, ch, str);

  g_free (str);
}

static void
add_context_to_error (GError              **err,
                      GMarkupParseContext  *context)
{
  gint line;
  gint ch;
  gchar *str;

  if (err == NULL || *err == NULL)
    return;

  g_markup_parse_context_get_position (context, &line, &ch);

  str = g_strdup_printf (_("Line %d character %d: %s"),
                         line, ch, (*err)->message);

  g_free ((*err)->message);
  (*err)->message = str;
}

#define MAX_REASONABLE 4096
static gboolean
parse_positive_integer (const char           *str,
                        int                  *val,
                        GMarkupParseContext  *context,
                        MetaThemeMetacity    *metacity,
                        GError              **error)
{
  char *end;
  long l;
  int j;

  *val = 0;

  end = NULL;

  /* Is str a constant? */

  if (theme_allows (metacity, META_THEME_UBIQUITOUS_CONSTANTS) &&
      meta_theme_metacity_lookup_int (metacity, str, &j))
    {
      /* Yes. */
      l = j;
    }
  else
    {
      /* No. Let's try parsing it instead. */

      l = strtol (str, &end, 10);

      if (end == NULL || end == str)
      {
        set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                   _("Could not parse '%s' as an integer"),
                   str);
        return FALSE;
      }

    if (*end != '\0')
      {
        set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                   _("Did not understand trailing characters '%s' in string '%s'"),
                   end, str);
        return FALSE;
      }
    }

  if (l < 0)
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Integer %ld must be positive"), l);
      return FALSE;
    }

  if (l > MAX_REASONABLE)
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Integer %ld is too large, current max is %d"),
                 l, MAX_REASONABLE);
      return FALSE;
    }

  *val = (int) l;

  return TRUE;
}

/* Attribute names can have a leading '!' to indicate that they are
 * required.
 */
static gboolean
locate_attributes (GMarkupParseContext  *context,
                   const gchar          *element_name,
                   const gchar         **attribute_names,
                   const gchar         **attribute_values,
                   GError              **error,
                   const gchar          *first_attribute_name,
                   const gchar         **first_attribute_retloc,
                   ...)
{
  va_list args;
  const char *name;
  const char **retloc;
  int n_attrs;
#define MAX_ATTRS 24
  LocateAttr attrs[MAX_ATTRS];
  gboolean retval;
  int i;

  g_return_val_if_fail (first_attribute_name != NULL, FALSE);
  g_return_val_if_fail (first_attribute_retloc != NULL, FALSE);

  retval = TRUE;

  /* FIXME: duplicated code; refactor loop */
  n_attrs = 1;
  attrs[0].name = first_attribute_name;
  attrs[0].retloc = first_attribute_retloc;
  attrs[0].required = attrs[0].name[0]=='!';
  if (attrs[0].required)
    attrs[0].name++; /* skip past it */
  *first_attribute_retloc = NULL;

  va_start (args, first_attribute_retloc);

  name = va_arg (args, const gchar *);
  retloc = va_arg (args, const gchar **);

  while (name != NULL)
    {
      if (retloc == NULL)
        {
          retval = FALSE;
          goto out;
        }

      g_assert (n_attrs < MAX_ATTRS);

      attrs[n_attrs].name = name;
      attrs[n_attrs].retloc = retloc;
      attrs[n_attrs].required = attrs[n_attrs].name[0]=='!';
      if (attrs[n_attrs].required)
        attrs[n_attrs].name++; /* skip past it */

      n_attrs += 1;
      *retloc = NULL;

      name = va_arg (args, const char*);
      retloc = va_arg (args, const char**);
    }

  va_end (args);

  i = 0;
  while (attribute_names[i])
    {
      int j;
      gboolean found;

      /* Can be present anywhere */
      if (strcmp (attribute_names[i], "version") == 0)
        {
          ++i;
          continue;
        }

      found = FALSE;
      j = 0;
      while (j < n_attrs)
        {
          if (strcmp (attrs[j].name, attribute_names[i]) == 0)
            {
              retloc = attrs[j].retloc;

              if (*retloc != NULL)
                {

                  set_error (error, context,
                             G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                             _("Attribute '%s' repeated twice on the same <%s> element"),
                             attrs[j].name, element_name);
                  retval = FALSE;
                  goto out;
                }

              *retloc = attribute_values[i];
              found = TRUE;
            }

          ++j;
        }

      if (!found)
        {
          j = 0;
          while (j < n_attrs)
            {
              g_warning ("It could have been %s.\n", attrs[j++].name);
            }

          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Attribute '%s' is invalid on <%s> element in this context"),
                     attribute_names[i], element_name);
          retval = FALSE;
          goto out;
        }

      ++i;
    }

    /* Did we catch them all? */
    i = 0;
    while (i < n_attrs)
      {
        if (attrs[i].required && *(attrs[i].retloc)==NULL)
          {
            set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                       ATTRIBUTE_NOT_FOUND, attrs[i].name, element_name);
            retval = FALSE;
            goto out;
          }

        ++i;
      }

 out:
  return retval;
}

static gboolean
check_no_attributes (GMarkupParseContext  *context,
                     const gchar          *element_name,
                     const gchar         **attribute_names,
                     const gchar         **attribute_values,
                     GError              **error)
{
  int i = 0;

  /* Can be present anywhere */
  if (attribute_names[0] && strcmp (attribute_names[i], "version") == 0)
    i++;

  if (attribute_names[i] != NULL)
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Attribute '%s' is invalid on <%s> element in this context"),
                 attribute_names[0], element_name);
      return FALSE;
    }

  return TRUE;
}

static gboolean
parse_double (const char           *str,
              double               *val,
              GMarkupParseContext  *context,
              GError              **error)
{
  char *end;

  *val = 0;

  end = NULL;

  *val = g_ascii_strtod (str, &end);

  if (end == NULL || end == str)
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Could not parse '%s' as a floating point number"),
                 str);
      return FALSE;
    }

  if (*end != '\0')
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Did not understand trailing characters '%s' in string '%s'"),
                 end, str);
      return FALSE;
    }

  return TRUE;
}

static gboolean
parse_alpha (const char             *str,
             MetaAlphaGradientSpec **spec_ret,
             GMarkupParseContext    *context,
             GError                **error)
{
  char **split;
  int i;
  int n_alphas;
  MetaAlphaGradientSpec *spec;

  *spec_ret = NULL;

  split = g_strsplit (str, ":", -1);

  i = 0;
  while (split[i])
    ++i;

  if (i == 0)
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Could not parse '%s' as a floating point number"),
                 str);

      g_strfreev (split);

      return FALSE;
    }

  n_alphas = i;

  /* FIXME allow specifying horizontal/vertical/diagonal in theme format,
   * once we implement vertical/diagonal in gradient.c
   */
  spec = meta_alpha_gradient_spec_new (META_GRADIENT_HORIZONTAL, n_alphas);

  i = 0;
  while (i < n_alphas)
    {
      double v;

      if (!parse_double (split[i], &v, context, error))
        {
          /* clear up, but don't set error: it was set by parse_double */
          g_strfreev (split);
          meta_alpha_gradient_spec_free (spec);

          return FALSE;
        }

      if (v < (0.0 - 1e-6) || v > (1.0 + 1e-6))
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Alpha must be between 0.0 (invisible) and 1.0 (fully opaque), was %g"),
                     v);

          g_strfreev (split);
          meta_alpha_gradient_spec_free (spec);

          return FALSE;
        }

      meta_alpha_gradient_spec_add_alpha (spec, i, v);

      ++i;
    }

  g_strfreev (split);

  *spec_ret = spec;

  return TRUE;
}

static gboolean
parse_title_scale (const char          *str,
                   double              *val,
                   GMarkupParseContext *context,
                   GError             **error)
{
  double factor;

  if (strcmp (str, "xx-small") == 0)
    factor = PANGO_SCALE_XX_SMALL;
  else if (strcmp (str, "x-small") == 0)
    factor = PANGO_SCALE_X_SMALL;
  else if (strcmp (str, "small") == 0)
    factor = PANGO_SCALE_SMALL;
  else if (strcmp (str, "medium") == 0)
    factor = PANGO_SCALE_MEDIUM;
  else if (strcmp (str, "large") == 0)
    factor = PANGO_SCALE_LARGE;
  else if (strcmp (str, "x-large") == 0)
    factor = PANGO_SCALE_X_LARGE;
  else if (strcmp (str, "xx-large") == 0)
    factor = PANGO_SCALE_XX_LARGE;
  else
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Invalid title scale '%s' (must be one of xx-small,x-small,small,medium,large,x-large,xx-large)\n"),
                 str);
      return FALSE;
    }

  *val = factor;

  return TRUE;
}

static gboolean
parse_rounding (const char           *str,
                guint                *val,
                GMarkupParseContext  *context,
                MetaThemeMetacity    *metacity,
                GError              **error)
{
  if (strcmp ("true", str) == 0)
    *val = 5; /* historical "true" value */
  else if (strcmp ("false", str) == 0)
    *val = 0;
  else
    {
      int tmp;
      gboolean result;
       if (!theme_allows (metacity, META_THEME_VARIED_ROUND_CORNERS))
         {
           /* Not known in this version, so bail. */
           set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                      _("Boolean values must be 'true' or 'false' not '%s'"),
                      str);
           return FALSE;
         }

      result = parse_positive_integer (str, &tmp, context, metacity, error);

      *val = tmp;

      return result;
    }

  return TRUE;
}

static gboolean
parse_boolean (const char          *str,
               gboolean            *val,
               GMarkupParseContext *context,
               GError             **error)
{
  if (strcmp ("true", str) == 0)
    *val = TRUE;
  else if (strcmp ("false", str) == 0)
    *val = FALSE;
  else
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Boolean values must be 'true' or 'false' not '%s'"),
                 str);
      return FALSE;
    }

  return TRUE;
}

static gboolean
first_uppercase (const gchar *str)
{
  return g_ascii_isupper (*str);
}

static gboolean
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

static gboolean
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

static gboolean
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

static void
meta_theme_metacity_insert_draw_op_list (MetaThemeMetacity *metacity,
                                         const gchar       *name,
                                         MetaDrawOpList    *op_list)
{
  meta_draw_op_list_ref (op_list);
  g_hash_table_replace (metacity->draw_op_lists, g_strdup (name), op_list);
}

static void
meta_theme_metacity_insert_layout (MetaThemeMetacity *metacity,
                                   const gchar       *name,
                                   MetaFrameLayout   *layout)
{
  meta_frame_layout_ref (layout);
  g_hash_table_replace (metacity->frame_layouts, g_strdup (name), layout);
}

static void
meta_theme_metacity_insert_style (MetaThemeMetacity *metacity,
                                  const gchar       *name,
                                  MetaFrameStyle    *style)
{
  meta_frame_style_ref (style);
  g_hash_table_replace (metacity->styles, g_strdup (name), style);
}

static void
meta_theme_metacity_insert_style_set (MetaThemeMetacity *metacity,
                                      const gchar       *name,
                                      MetaFrameStyleSet *style_set)
{
  meta_frame_style_set_ref (style_set);
  g_hash_table_replace (metacity->style_sets, g_strdup (name), style_set);
}

static MetaFrameType
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

static void
parse_toplevel_element (GMarkupParseContext  *context,
                        const gchar          *element_name,
                        const gchar         **attribute_names,
                        const gchar         **attribute_values,
                        ParseInfo            *info,
                        GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_THEME);

  if (g_strcmp0 (element_name, "info") == 0)
    {
      if (!check_no_attributes (context, element_name,
                                attribute_names, attribute_values,
                                error))
        return;

      push_state (info, STATE_INFO);
    }
  else if (g_strcmp0 (element_name, "constant") == 0)
    {
      const char *name;
      const char *value;
      int ival = 0;
      double dval = 0.0;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!name", &name, "!value", &value,
                              NULL))
        return;

      /* We don't know how a a constant is going to be used, so we have guess its
       * type from its contents:
       *
       *  - Starts like a number and contains a '.': float constant
       *  - Starts like a number and doesn't contain a '.': int constant
       *  - Starts with anything else: a color constant.
       *    (colors always start with # or a letter)
       */
      if (value[0] == '.' || value[0] == '+' || value[0] == '-' || (value[0] >= '0' && value[0] <= '9'))
        {
          if (strchr (value, '.'))
            {
              if (!parse_double (value, &dval, context, error))
                return;

              if (!meta_theme_metacity_define_float (info->metacity,
                                                     name, dval, error))
                {
                  add_context_to_error (error, context);
                  return;
                }
            }
          else
            {
              if (!parse_positive_integer (value, &ival, context, info->metacity, error))
                return;

              if (!meta_theme_metacity_define_int (info->metacity,
                                                   name, ival, error))
                {
                  add_context_to_error (error, context);
                  return;
                }
            }
        }
      else
        {
          if (!meta_theme_metacity_define_color (info->metacity,
                                                 name, value, error))
            {
              add_context_to_error (error, context);
              return;
            }
        }

      push_state (info, STATE_CONSTANT);
    }
  else if (g_strcmp0 (element_name, "frame_geometry") == 0)
    {
      const char *name = NULL;
      const char *parent = NULL;
      const char *has_title = NULL;
      const char *title_scale = NULL;
      const char *rounded_top_left = NULL;
      const char *rounded_top_right = NULL;
      const char *rounded_bottom_left = NULL;
      const char *rounded_bottom_right = NULL;
      const char *hide_buttons = NULL;
      gboolean has_title_val;
      guint rounded_top_left_val;
      guint rounded_top_right_val;
      guint rounded_bottom_left_val;
      guint rounded_bottom_right_val;
      gboolean hide_buttons_val;
      double title_scale_val;
      MetaFrameLayout *parent_layout;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!name", &name, "parent", &parent,
                              "has_title", &has_title, "title_scale", &title_scale,
                              "rounded_top_left", &rounded_top_left,
                              "rounded_top_right", &rounded_top_right,
                              "rounded_bottom_left", &rounded_bottom_left,
                              "rounded_bottom_right", &rounded_bottom_right,
                              "hide_buttons", &hide_buttons,
                              NULL))
        return;

      has_title_val = TRUE;
      if (has_title && !parse_boolean (has_title, &has_title_val, context, error))
        return;

      hide_buttons_val = FALSE;
      if (hide_buttons && !parse_boolean (hide_buttons, &hide_buttons_val, context, error))
        return;

      rounded_top_left_val = 0;
      rounded_top_right_val = 0;
      rounded_bottom_left_val = 0;
      rounded_bottom_right_val = 0;

      if (rounded_top_left && !parse_rounding (rounded_top_left, &rounded_top_left_val, context, info->metacity, error))
        return;
      if (rounded_top_right && !parse_rounding (rounded_top_right, &rounded_top_right_val, context, info->metacity, error))
        return;
      if (rounded_bottom_left && !parse_rounding (rounded_bottom_left, &rounded_bottom_left_val, context, info->metacity, error))
        return;
      if (rounded_bottom_right && !parse_rounding (rounded_bottom_right, &rounded_bottom_right_val, context, info->metacity, error))
        return;

      title_scale_val = 1.0;
      if (title_scale && !parse_title_scale (title_scale, &title_scale_val, context, error))
        return;

      if (meta_theme_metacity_lookup_layout (info->metacity, name))
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("<%s> name \"%s\" used a second time"),
                     element_name, name);
          return;
        }

      parent_layout = NULL;
      if (parent)
        {
          parent_layout = meta_theme_metacity_lookup_layout (info->metacity, parent);
          if (parent_layout == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("<%s> parent \"%s\" has not been defined"),
                         element_name, parent);
              return;
            }
        }

      g_assert (info->layout == NULL);

      if (parent_layout)
        info->layout = meta_frame_layout_copy (parent_layout);
      else
        info->layout = meta_frame_layout_new ();

      if (has_title) /* only if explicit, otherwise inherit */
        info->layout->has_title = has_title_val;

      if (theme_allows (info->metacity, META_THEME_HIDDEN_BUTTONS) && hide_buttons_val)
        info->layout->hide_buttons = hide_buttons_val;

      if (title_scale)
        info->layout->title_scale = title_scale_val;

      if (rounded_top_left)
        info->layout->top_left_corner_rounded_radius = rounded_top_left_val;

      if (rounded_top_right)
        info->layout->top_right_corner_rounded_radius = rounded_top_right_val;

      if (rounded_bottom_left)
        info->layout->bottom_left_corner_rounded_radius = rounded_bottom_left_val;

      if (rounded_bottom_right)
        info->layout->bottom_right_corner_rounded_radius = rounded_bottom_right_val;

      meta_theme_metacity_insert_layout (info->metacity, name, info->layout);

      push_state (info, STATE_FRAME_GEOMETRY);
    }
  else if (g_strcmp0 (element_name, "draw_ops") == 0)
    {
      const char *name = NULL;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!name", &name,
                              NULL))
        return;

      if (meta_theme_metacity_lookup_draw_op_list (info->metacity, name))
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("<%s> name '%s' used a second time"),
                     element_name, name);
          return;
        }

      g_assert (info->op_list == NULL);
      info->op_list = meta_draw_op_list_new (2);

      meta_theme_metacity_insert_draw_op_list (info->metacity, name, info->op_list);

      push_state (info, STATE_DRAW_OPS);
    }
  else if (g_strcmp0 (element_name, "frame_style") == 0)
    {
      const char *name = NULL;
      const char *parent = NULL;
      const char *geometry = NULL;
      const char *background = NULL;
      const char *alpha = NULL;
      MetaFrameStyle *parent_style;
      MetaFrameLayout *layout;

      if (!locate_attributes (context, element_name, attribute_names,
                              attribute_values, error,
                              "!name", &name, "parent", &parent,
                              "geometry", &geometry,
                              "background", &background,
                              "alpha", &alpha,
                              NULL))
        return;

      if (meta_theme_metacity_lookup_style (info->metacity, name))
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("<%s> name \"%s\" used a second time"),
                     element_name, name);
          return;
        }

      parent_style = NULL;
      if (parent)
        {
          parent_style = meta_theme_metacity_lookup_style (info->metacity, parent);
          if (parent_style == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("<%s> parent '%s' has not been defined"),
                         element_name, parent);
              return;
            }
        }

      layout = NULL;
      if (geometry)
        {
          layout = meta_theme_metacity_lookup_layout (info->metacity, geometry);
          if (layout == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("<%s> geometry '%s' has not been defined"),
                         element_name, geometry);
              return;
            }
        }
      else if (parent_style)
        {
          layout = parent_style->layout;
        }

      if (layout == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("<%s> must specify either a geometry or a parent that has a geometry"),
                     element_name);
          return;
        }

      g_assert (info->style == NULL);

      info->style = meta_frame_style_new (parent_style);
      g_assert (info->style->layout == NULL);
      meta_frame_layout_ref (layout);
      info->style->layout = layout;

      if (background != NULL && theme_allows (info->metacity, META_THEME_FRAME_BACKGROUNDS))
        {
          info->style->window_background_color = meta_color_spec_new_from_string (background, error);
          if (!info->style->window_background_color)
            return;

          if (alpha != NULL)
            {
               gboolean success;
               MetaAlphaGradientSpec *alpha_vector;

               g_clear_error (error);
               /* fortunately, we already have a routine to parse alpha values,
                * though it produces a vector of them, which is a superset of
                * what we want.
                */
               success = parse_alpha (alpha, &alpha_vector, context, error);
               if (!success)
                 return;

               /* alpha_vector->alphas must contain at least one element */
               info->style->window_background_alpha = meta_alpha_gradient_spec_get_alpha (alpha_vector, 0);

               meta_alpha_gradient_spec_free (alpha_vector);
            }
        }
      else if (alpha != NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("You must specify a background for an alpha value to be meaningful"));
          return;
        }

      meta_theme_metacity_insert_style (info->metacity, name, info->style);

      push_state (info, STATE_FRAME_STYLE);
    }
  else if (g_strcmp0 (element_name, "frame_style_set") == 0)
    {
      const char *name = NULL;
      const char *parent = NULL;
      MetaFrameStyleSet *parent_set;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!name", &name, "parent", &parent,
                              NULL))
        return;

      if (meta_theme_metacity_lookup_style_set (info->metacity, name))
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("<%s> name \"%s\" used a second time"),
                     element_name, name);
          return;
        }

      parent_set = NULL;
      if (parent)
        {
          parent_set = meta_theme_metacity_lookup_style_set (info->metacity, parent);
          if (parent_set == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("<%s> parent '%s' has not been defined"),
                         element_name, parent);
              return;
            }
        }

      g_assert (info->style_set == NULL);

      info->style_set = meta_frame_style_set_new (parent_set);

      meta_theme_metacity_insert_style_set (info->metacity, name, info->style_set);

      push_state (info, STATE_FRAME_STYLE_SET);
    }
  else if (g_strcmp0 (element_name, "window") == 0)
    {
      const char *type_name = NULL;
      const char *style_set_name = NULL;
      MetaFrameStyleSet *style_set;
      MetaFrameType type;

      if (!locate_attributes (context, element_name, attribute_names,
                              attribute_values, error,
                              "!type", &type_name, "!style_set", &style_set_name,
                              NULL))
        return;

      type = meta_frame_type_from_string (type_name);

      if (type == META_FRAME_TYPE_LAST ||
         (type == META_FRAME_TYPE_ATTACHED && peek_required_version (info) < 3002))
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Unknown type '%s' on <%s> element"),
                     type_name, element_name);
          return;
        }

      style_set = meta_theme_metacity_lookup_style_set (info->metacity, style_set_name);
      if (style_set == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Unknown style_set '%s' on <%s> element"),
                     style_set_name, element_name);
          return;
        }

      if (info->metacity->style_sets_by_type[type] != NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Window type '%s' has already been assigned a style set"),
                     type_name);
          return;
        }

      meta_frame_style_set_ref (style_set);
      info->metacity->style_sets_by_type[type] = style_set;

      push_state (info, STATE_WINDOW);
    }
  else if (g_strcmp0 (element_name, "menu_icon") == 0)
    {
      /* Not supported any more, but we have to parse it if they include it,
       * for backwards compatibility.
       */
      g_assert (info->op_list == NULL);

      push_state (info, STATE_MENU_ICON);
    }
  else if (g_strcmp0 (element_name, "fallback") == 0)
    {
      /* Not supported any more, but we have to parse it if they include it,
       * for backwards compatibility.
       */
      push_state (info, STATE_FALLBACK);
    }
   else
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "metacity_theme");
    }
}

static void
parse_info_element (GMarkupParseContext  *context,
                    const gchar          *element_name,
                    const gchar         **attribute_names,
                    const gchar         **attribute_values,
                    ParseInfo            *info,
                    GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_INFO);

  if (g_strcmp0 (element_name, "name") == 0)
    {
      if (!check_no_attributes (context, element_name,
                                attribute_names, attribute_values,
                                error))
        return;

      push_state (info, STATE_NAME);
    }
  else if (g_strcmp0 (element_name, "author") == 0)
    {
      if (!check_no_attributes (context, element_name,
                                attribute_names, attribute_values,
                                error))
        return;

      push_state (info, STATE_AUTHOR);
    }
  else if (g_strcmp0 (element_name, "copyright") == 0)
    {
      if (!check_no_attributes (context, element_name,
                                attribute_names, attribute_values,
                                error))
        return;

      push_state (info, STATE_COPYRIGHT);
    }
  else if (g_strcmp0 (element_name, "description") == 0)
    {
      if (!check_no_attributes (context, element_name,
                                attribute_names, attribute_values,
                                error))
        return;

      push_state (info, STATE_DESCRIPTION);
    }
  else if (g_strcmp0 (element_name, "date") == 0)
    {
      if (!check_no_attributes (context, element_name,
                                attribute_names, attribute_values,
                                error))
        return;

      push_state (info, STATE_DATE);
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "info");
    }
}

static void
parse_aspect_ratio (GMarkupParseContext  *context,
                    const gchar          *element_name,
                    const gchar         **attribute_names,
                    const gchar         **attribute_values,
                    ParseInfo            *info,
                    GError              **error)
{
  const char *name;
  const char *value;
  double val;

  if (!locate_attributes (context, element_name, attribute_names,
                          attribute_values, error,
                          "!name", &name, "!value", &value,
                          NULL))
    return;

  val = 0;
  if (!parse_double (value, &val, context, error))
    return;

  g_assert (info->layout);

  if (strcmp (name, "button") == 0)
    {
      info->layout->metacity.button_aspect = val;

      if (info->layout->metacity.button_sizing != META_BUTTON_SIZING_LAST)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Cannot specify both 'button_width'/'button_height' and 'aspect_ratio' for buttons"));
          return;
        }

      info->layout->metacity.button_sizing = META_BUTTON_SIZING_ASPECT;
    }
  else
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Aspect ratio '%s' is unknown"), name);
      return;
    }
}

static void
parse_border (GMarkupParseContext  *context,
              const gchar          *element_name,
              const gchar         **attribute_names,
              const gchar         **attribute_values,
              ParseInfo            *info,
              GError              **error)
{
  const char *name;
  const char *top;
  const char *bottom;
  const char *left;
  const char *right;
  int top_val;
  int bottom_val;
  int left_val;
  int right_val;
  GtkBorder *border;

  if (!locate_attributes (context, element_name, attribute_names,
                          attribute_values, error,
                          "!name", &name,
                          "!top", &top,
                          "!bottom", &bottom,
                          "!left", &left,
                          "!right", &right,
                          NULL))
    return;

  top_val = 0;
  if (!parse_positive_integer (top, &top_val, context, info->metacity, error))
    return;

  bottom_val = 0;
  if (!parse_positive_integer (bottom, &bottom_val, context, info->metacity, error))
    return;

  left_val = 0;
  if (!parse_positive_integer (left, &left_val, context, info->metacity, error))
    return;

  right_val = 0;
  if (!parse_positive_integer (right, &right_val, context, info->metacity, error))
    return;

  g_assert (info->layout);

  border = NULL;

  if (strcmp (name, "title_border") == 0)
    border = &info->layout->metacity.title_border;
  else if (strcmp (name, "button_border") == 0)
    border = &info->layout->button_border;

  if (border == NULL)
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Border '%s' is unknown"), name);
      return;
    }

  border->top = top_val;
  border->bottom = bottom_val;
  border->left = left_val;
  border->right = right_val;
}

static void
parse_distance (GMarkupParseContext  *context,
                const gchar          *element_name,
                const gchar         **attribute_names,
                const gchar         **attribute_values,
                ParseInfo            *info,
                GError              **error)
{
  const char *name;
  const char *value;
  int val;

  if (!locate_attributes (context, element_name, attribute_names,
                          attribute_values, error,
                          "!name", &name, "!value", &value,
                          NULL))
    return;

  val = 0;
  if (!parse_positive_integer (value, &val, context, info->metacity, error))
    return;

  g_assert (val >= 0); /* yeah, "non-negative" not "positive" get over it */
  g_assert (info->layout);

  if (strcmp (name, "left_width") == 0)
    info->layout->metacity.left_width = val;
  else if (strcmp (name, "right_width") == 0)
    info->layout->metacity.right_width = val;
  else if (strcmp (name, "bottom_height") == 0)
    info->layout->metacity.bottom_height = val;
  else if (strcmp (name, "title_vertical_pad") == 0)
    info->layout->metacity.title_vertical_pad = val;
  else if (strcmp (name, "right_titlebar_edge") == 0)
    info->layout->metacity.right_titlebar_edge = val;
  else if (strcmp (name, "left_titlebar_edge") == 0)
    info->layout->metacity.left_titlebar_edge = val;
  else if (strcmp (name, "button_width") == 0)
    {
      info->layout->metacity.button_width = val;

      if (!(info->layout->metacity.button_sizing == META_BUTTON_SIZING_LAST ||
            info->layout->metacity.button_sizing == META_BUTTON_SIZING_FIXED))
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Cannot specify both 'button_width'/'button_height' and 'aspect_ratio' for buttons"));
          return;
        }

      info->layout->metacity.button_sizing = META_BUTTON_SIZING_FIXED;
    }
  else if (strcmp (name, "button_height") == 0)
    {
      info->layout->metacity.button_height = val;

      if (!(info->layout->metacity.button_sizing == META_BUTTON_SIZING_LAST ||
            info->layout->metacity.button_sizing == META_BUTTON_SIZING_FIXED))
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Cannot specify both 'button_width'/'button_height' and 'aspect_ratio' for buttons"));
          return;
        }

      info->layout->metacity.button_sizing = META_BUTTON_SIZING_FIXED;
    }
  else
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Distance '%s' is unknown"), name);
      return;
    }
}

static void
parse_geometry_element (GMarkupParseContext  *context,
                        const gchar          *element_name,
                        const gchar         **attribute_names,
                        const gchar         **attribute_values,
                        ParseInfo            *info,
                        GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_FRAME_GEOMETRY);

  if (g_strcmp0 (element_name, "distance") == 0)
    {
      parse_distance (context, element_name,
                      attribute_names, attribute_values,
                      info, error);
      push_state (info, STATE_DISTANCE);
    }
  else if (g_strcmp0 (element_name, "border") == 0)
    {
      parse_border (context, element_name,
                    attribute_names, attribute_values,
                    info, error);
      push_state (info, STATE_BORDER);
    }
  else if (g_strcmp0 (element_name, "aspect_ratio") == 0)
    {
      parse_aspect_ratio (context, element_name,
                          attribute_names, attribute_values,
                          info, error);

      push_state (info, STATE_ASPECT_RATIO);
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "frame_geometry");
    }
}

static GtkArrowType
meta_gtk_arrow_from_string (const char *str)
{
  if (strcmp ("up", str) == 0)
    return GTK_ARROW_UP;
  else if (strcmp ("down", str) == 0)
    return GTK_ARROW_DOWN;
  else if (strcmp ("left", str) == 0)
    return GTK_ARROW_LEFT;
  else if (strcmp ("right", str) == 0)
    return GTK_ARROW_RIGHT;
  else if (strcmp ("none", str) == 0)
    return GTK_ARROW_NONE;
  else
    return -1;
}

static GtkShadowType
meta_gtk_shadow_from_string (const char *str)
{
  if (strcmp ("none", str) == 0)
    return GTK_SHADOW_NONE;
  else if (strcmp ("in", str) == 0)
    return GTK_SHADOW_IN;
  else if (strcmp ("out", str) == 0)
    return GTK_SHADOW_OUT;
  else if (strcmp ("etched_in", str) == 0)
    return GTK_SHADOW_ETCHED_IN;
  else if (strcmp ("etched_out", str) == 0)
    return GTK_SHADOW_ETCHED_OUT;
  else
    return -1;
}

static MetaGradientType
meta_gradient_type_from_string (const char *str)
{
  if (strcmp ("vertical", str) == 0)
    return META_GRADIENT_VERTICAL;
  else if (strcmp ("horizontal", str) == 0)
    return META_GRADIENT_HORIZONTAL;
  else if (strcmp ("diagonal", str) == 0)
    return META_GRADIENT_DIAGONAL;
  else
    return META_GRADIENT_LAST;
}

/**
 * Returns a fill_type from a string.  The inverse of
 * meta_image_fill_type_to_string().
 *
 * \param str  a string representing a fill_type
 * \result  the fill_type, or -1 if it represents no fill_type.
 */
static MetaImageFillType
meta_image_fill_type_from_string (const char *str)
{
  if (strcmp ("tile", str) == 0)
    return META_IMAGE_FILL_TILE;
  else if (strcmp ("scale", str) == 0)
    return META_IMAGE_FILL_SCALE;
  else
    return -1;
}

static gboolean
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

static MetaColorSpec*
parse_color (MetaThemeMetacity  *metacity,
             const gchar        *str,
             GError            **err)
{
  gchar* referent;

  if (theme_allows (metacity, META_THEME_COLOR_CONSTANTS) &&
      meta_theme_metacity_lookup_color (metacity, str, &referent))
    {
      if (referent)
        return meta_color_spec_new_from_string (referent, err);

      /* no need to free referent: it's a pointer into the actual hash table */
    }

  return meta_color_spec_new_from_string (str, err);
}

static GdkPixbuf *
meta_theme_load_image (MetaThemeMetacity  *metacity,
                       const gchar        *filename,
                       guint               size_of_theme_icons,
                       GError            **error)
{
  GdkPixbuf *pixbuf;

  pixbuf = g_hash_table_lookup (metacity->images, filename);

  if (pixbuf == NULL)
    {
      if (g_str_has_prefix (filename, "theme:") &&
          theme_allows (metacity, META_THEME_IMAGES_FROM_ICON_THEMES))
        {
          pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                             filename + 6, size_of_theme_icons,
                                             0, error);

          if (pixbuf == NULL)
            return NULL;
         }
      else
        {
          char *full_path;
          full_path = g_build_filename (metacity->dirname, filename, NULL);

          pixbuf = gdk_pixbuf_new_from_file (full_path, error);
          if (pixbuf == NULL)
            {
              g_free (full_path);
              return NULL;
            }

          g_free (full_path);
        }

      g_hash_table_replace (metacity->images, g_strdup (filename), pixbuf);
    }

  g_assert (pixbuf);

  g_object_ref (G_OBJECT (pixbuf));

  return pixbuf;
}

static gboolean
parse_angle (const char          *str,
             double              *val,
             GMarkupParseContext *context,
             GError             **error)
{
  if (!parse_double (str, val, context, error))
    return FALSE;

  if (*val < (0.0 - 1e6) || *val > (360.0 + 1e6))
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Angle must be between 0.0 and 360.0, was %g\n"),
                 *val);
      return FALSE;
    }

  return TRUE;
}

static void
parse_draw_op_element (GMarkupParseContext  *context,
                       const gchar          *element_name,
                       const gchar         **attribute_names,
                       const gchar         **attribute_values,
                       ParseInfo            *info,
                       GError              **error)
{
  MetaThemeMetacity *metacity;

  g_return_if_fail (peek_state (info) == STATE_DRAW_OPS);

  metacity = info->metacity;

  if (g_strcmp0 (element_name, "line") == 0)
    {
      MetaDrawOp *op;
      const char *color;
      const char *x1;
      const char *y1;
      const char *x2;
      const char *y2;
      const char *dash_on_length;
      const char *dash_off_length;
      const char *width;
      MetaColorSpec *color_spec;
      int dash_on_val;
      int dash_off_val;
      int width_val;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!color", &color,
                              "!x1", &x1, "!y1", &y1,
                              "!x2", &x2, "!y2", &y2,
                              "dash_on_length", &dash_on_length,
                              "dash_off_length", &dash_off_length,
                              "width", &width,
                              NULL))
        return;

      dash_on_val = 0;
      if (dash_on_length &&
          !parse_positive_integer (dash_on_length, &dash_on_val, context, info->metacity, error))
        return;

      dash_off_val = 0;
      if (dash_off_length &&
          !parse_positive_integer (dash_off_length, &dash_off_val, context, info->metacity, error))
        return;

      width_val = 0;
      if (width &&
          !parse_positive_integer (width, &width_val, context, info->metacity, error))
        return;

      /* Check last so we don't have to free it when other
       * stuff fails
       */
      color_spec = parse_color (info->metacity, color, error);
      if (color_spec == NULL)
        {
          add_context_to_error (error, context);
          return;
        }

      op = meta_draw_op_new (META_DRAW_LINE);

      op->data.line.color_spec = color_spec;

      op->data.line.x1 = meta_draw_spec_new (metacity, x1, NULL);
      op->data.line.y1 = meta_draw_spec_new (metacity, y1, NULL);

      if (strcmp(x1, x2)==0)
        op->data.line.x2 = NULL;
      else
        op->data.line.x2 = meta_draw_spec_new (metacity, x2, NULL);

      if (strcmp(y1, y2)==0)
        op->data.line.y2 = NULL;
      else
        op->data.line.y2 = meta_draw_spec_new (metacity, y2, NULL);

      op->data.line.width = width_val;
      op->data.line.dash_on_length = dash_on_val;
      op->data.line.dash_off_length = dash_off_val;

      g_assert (info->op_list);

      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_LINE);
    }
  else if (g_strcmp0 (element_name, "rectangle") == 0)
    {
      MetaDrawOp *op;
      const char *color;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *filled;
      gboolean filled_val;
      MetaColorSpec *color_spec;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!color", &color,
                              "!x", &x, "!y", &y,
                              "!width", &width, "!height", &height,
                              "filled", &filled,
                              NULL))
        return;

      filled_val = FALSE;
      if (filled && !parse_boolean (filled, &filled_val, context, error))
        return;

      /* Check last so we don't have to free it when other
       * stuff fails
       */
      color_spec = parse_color (info->metacity, color, error);
      if (color_spec == NULL)
        {
          add_context_to_error (error, context);
          return;
        }

      op = meta_draw_op_new (META_DRAW_RECTANGLE);

      op->data.rectangle.color_spec = color_spec;
      op->data.rectangle.x = meta_draw_spec_new (metacity, x, NULL);
      op->data.rectangle.y = meta_draw_spec_new (metacity, y, NULL);
      op->data.rectangle.width = meta_draw_spec_new (metacity, width, NULL);
      op->data.rectangle.height = meta_draw_spec_new (metacity,
                                                      height, NULL);

      op->data.rectangle.filled = filled_val;

      g_assert (info->op_list);

      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_RECTANGLE);
    }
  else if (g_strcmp0 (element_name, "arc") == 0)
    {
      MetaDrawOp *op;
      const char *color;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *filled;
      const char *start_angle;
      const char *extent_angle;
      const char *from;
      const char *to;
      gboolean filled_val;
      double start_angle_val;
      double extent_angle_val;
      MetaColorSpec *color_spec;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!color", &color,
                              "!x", &x, "!y", &y,
                              "!width", &width, "!height", &height,
                              "filled", &filled,
                              "start_angle", &start_angle,
                              "extent_angle", &extent_angle,
                              "from", &from,
                              "to", &to,
                              NULL))
        return;

      if (theme_allows (info->metacity, META_THEME_DEGREES_IN_ARCS))
        {
          if (start_angle == NULL && from == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("No \"start_angle\" or \"from\" attribute on element <%s>"), element_name);
              return;
            }

          if (extent_angle == NULL && to == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("No \"extent_angle\" or \"to\" attribute on element <%s>"), element_name);
              return;
            }
        }
      else
        {
          if (start_angle == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         ATTRIBUTE_NOT_FOUND, "start_angle", element_name);
              return;
            }

          if (extent_angle == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         ATTRIBUTE_NOT_FOUND, "extent_angle", element_name);
              return;
            }
        }

      if (start_angle == NULL)
        {
          if (!parse_angle (from, &start_angle_val, context, error))
            return;

          start_angle_val = (180-start_angle_val)/360.0;
        }
      else
        {
          if (!parse_angle (start_angle, &start_angle_val, context, error))
            return;
        }

      if (extent_angle == NULL)
        {
          if (!parse_angle (to, &extent_angle_val, context, error))
            return;

          extent_angle_val = ((180-extent_angle_val)/360.0) - start_angle_val;
        }
      else
        {
           if (!parse_angle (extent_angle, &extent_angle_val, context, error))
             return;
        }

      filled_val = FALSE;
      if (filled && !parse_boolean (filled, &filled_val, context, error))
        return;

      /* Check last so we don't have to free it when other
       * stuff fails
       */
      color_spec = parse_color (info->metacity, color, error);
      if (color_spec == NULL)
        {
          add_context_to_error (error, context);
          return;
        }

      op = meta_draw_op_new (META_DRAW_ARC);

      op->data.arc.color_spec = color_spec;

      op->data.arc.x = meta_draw_spec_new (metacity, x, NULL);
      op->data.arc.y = meta_draw_spec_new (metacity, y, NULL);
      op->data.arc.width = meta_draw_spec_new (metacity, width, NULL);
      op->data.arc.height = meta_draw_spec_new (metacity, height, NULL);

      op->data.arc.filled = filled_val;
      op->data.arc.start_angle = start_angle_val;
      op->data.arc.extent_angle = extent_angle_val;

      g_assert (info->op_list);

      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_ARC);
    }
  else if (g_strcmp0 (element_name, "clip") == 0)
    {
      MetaDrawOp *op;
      const char *x;
      const char *y;
      const char *width;
      const char *height;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!x", &x, "!y", &y,
                              "!width", &width, "!height", &height,
                              NULL))
        return;

      op = meta_draw_op_new (META_DRAW_CLIP);

      op->data.clip.x = meta_draw_spec_new (metacity, x, NULL);
      op->data.clip.y = meta_draw_spec_new (metacity, y, NULL);
      op->data.clip.width = meta_draw_spec_new (metacity, width, NULL);
      op->data.clip.height = meta_draw_spec_new (metacity, height, NULL);

      g_assert (info->op_list);

      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_CLIP);
    }
  else if (g_strcmp0 (element_name, "tint") == 0)
    {
      MetaDrawOp *op;
      const char *color;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *alpha;
      MetaAlphaGradientSpec *alpha_spec;
      MetaColorSpec *color_spec;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!color", &color,
                              "!x", &x, "!y", &y,
                              "!width", &width, "!height", &height,
                              "!alpha", &alpha,
                              NULL))
        return;

      alpha_spec = NULL;
      if (!parse_alpha (alpha, &alpha_spec, context, error))
        return;

      /* Check last so we don't have to free it when other
       * stuff fails
       */
      color_spec = parse_color (info->metacity, color, error);
      if (color_spec == NULL)
        {
          if (alpha_spec)
            meta_alpha_gradient_spec_free (alpha_spec);

          add_context_to_error (error, context);
          return;
        }

      op = meta_draw_op_new (META_DRAW_TINT);

      op->data.tint.color_spec = color_spec;
      op->data.tint.alpha_spec = alpha_spec;

      op->data.tint.x = meta_draw_spec_new (metacity, x, NULL);
      op->data.tint.y = meta_draw_spec_new (metacity, y, NULL);
      op->data.tint.width = meta_draw_spec_new (metacity, width, NULL);
      op->data.tint.height = meta_draw_spec_new (metacity, height, NULL);

      g_assert (info->op_list);

      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_TINT);
    }
  else if (g_strcmp0 (element_name, "gradient") == 0)
    {
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *type;
      const char *alpha;
      MetaAlphaGradientSpec *alpha_spec;
      MetaGradientType type_val;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!type", &type,
                              "!x", &x, "!y", &y,
                              "!width", &width, "!height", &height,
                              "alpha", &alpha,
                              NULL))
        return;

      type_val = meta_gradient_type_from_string (type);
      if (type_val == META_GRADIENT_LAST)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Did not understand value \"%s\" for type of gradient"),
                     type);
          return;
        }

      alpha_spec = NULL;
      if (alpha && !parse_alpha (alpha, &alpha_spec, context, error))
        return;

      g_assert (info->op == NULL);
      info->op = meta_draw_op_new (META_DRAW_GRADIENT);

      info->op->data.gradient.x = meta_draw_spec_new (metacity, x, NULL);
      info->op->data.gradient.y = meta_draw_spec_new (metacity, y, NULL);
      info->op->data.gradient.width = meta_draw_spec_new (metacity,
                                                        width, NULL);
      info->op->data.gradient.height = meta_draw_spec_new (metacity,
                                                         height, NULL);

      info->op->data.gradient.gradient_spec = meta_gradient_spec_new (type_val);

      info->op->data.gradient.alpha_spec = alpha_spec;

      push_state (info, STATE_GRADIENT);

      /* op gets appended on close tag */
    }
  else if (g_strcmp0 (element_name, "image") == 0)
    {
      MetaDrawOp *op;
      const char *filename;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *alpha;
      const char *colorize;
      const char *fill_type;
      MetaAlphaGradientSpec *alpha_spec;
      GdkPixbuf *pixbuf;
      MetaColorSpec *colorize_spec = NULL;
      MetaImageFillType fill_type_val;
      int h, w, c;
      int pixbuf_width, pixbuf_height, pixbuf_n_channels, pixbuf_rowstride;
      guchar *pixbuf_pixels;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!x", &x, "!y", &y,
                              "!width", &width, "!height", &height,
                              "alpha", &alpha, "!filename", &filename,
                              "colorize", &colorize,
                              "fill_type", &fill_type,
                              NULL))
        return;

      fill_type_val = META_IMAGE_FILL_SCALE;
      if (fill_type)
        {
          fill_type_val = meta_image_fill_type_from_string (fill_type);

          if (((int) fill_type_val) == -1)
            {
              set_error (error, context, G_MARKUP_ERROR,
                         G_MARKUP_ERROR_PARSE,
                         _("Did not understand fill type \"%s\" for <%s> element"),
                         fill_type, element_name);
            }
        }

      /* Check last so we don't have to free it when other
       * stuff fails.
       *
       * If it's a theme image, ask for it at 64px, which is
       * the largest possible. We scale it anyway.
       */
      pixbuf = meta_theme_load_image (info->metacity, filename, 64, error);

      if (pixbuf == NULL)
        {
          add_context_to_error (error, context);
          return;
        }

      if (colorize)
        {
          colorize_spec = parse_color (info->metacity, colorize, error);

          if (colorize_spec == NULL)
            {
              add_context_to_error (error, context);
              g_object_unref (G_OBJECT (pixbuf));
              return;
            }
        }

      alpha_spec = NULL;
      if (alpha && !parse_alpha (alpha, &alpha_spec, context, error))
        {
          g_object_unref (G_OBJECT (pixbuf));
          if (colorize_spec != NULL)
            meta_color_spec_free (colorize_spec);
          return;
        }

      op = meta_draw_op_new (META_DRAW_IMAGE);

      op->data.image.pixbuf = pixbuf;
      op->data.image.colorize_spec = colorize_spec;

      op->data.image.x = meta_draw_spec_new (metacity, x, NULL);
      op->data.image.y = meta_draw_spec_new (metacity, y, NULL);
      op->data.image.width = meta_draw_spec_new (metacity, width, NULL);
      op->data.image.height = meta_draw_spec_new (metacity, height, NULL);

      op->data.image.alpha_spec = alpha_spec;
      op->data.image.fill_type = fill_type_val;

      /* Check for vertical & horizontal stripes */
      pixbuf_n_channels = gdk_pixbuf_get_n_channels(pixbuf);
      pixbuf_width = gdk_pixbuf_get_width(pixbuf);
      pixbuf_height = gdk_pixbuf_get_height(pixbuf);
      pixbuf_rowstride = gdk_pixbuf_get_rowstride(pixbuf);
      pixbuf_pixels = gdk_pixbuf_get_pixels(pixbuf);

      /* Check for horizontal stripes */
      for (h = 0; h < pixbuf_height; h++)
        {
          for (w = 1; w < pixbuf_width; w++)
            {
              for (c = 0; c < pixbuf_n_channels; c++)
                {
                  if (pixbuf_pixels[(h * pixbuf_rowstride) + c] !=
                      pixbuf_pixels[(h * pixbuf_rowstride) + w + c])
                    break;
                }
              if (c < pixbuf_n_channels)
                break;
            }
          if (w < pixbuf_width)
            break;
        }

      if (h >= pixbuf_height)
        {
          op->data.image.horizontal_stripes = TRUE;
        }
      else
        {
          op->data.image.horizontal_stripes = FALSE;
        }

      /* Check for vertical stripes */
      for (w = 0; w < pixbuf_width; w++)
        {
          for (h = 1; h < pixbuf_height; h++)
            {
              for (c = 0; c < pixbuf_n_channels; c++)
                {
                  if (pixbuf_pixels[w + c] !=
                      pixbuf_pixels[(h * pixbuf_rowstride) + w + c])
                    break;
                }
              if (c < pixbuf_n_channels)
                break;
            }
          if (h < pixbuf_height)
            break;
        }

      if (w >= pixbuf_width)
        {
          op->data.image.vertical_stripes = TRUE;
        }
      else
        {
          op->data.image.vertical_stripes = FALSE;
        }

      g_assert (info->op_list);

      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_IMAGE);
    }
  else if (g_strcmp0 (element_name, "gtk_arrow") == 0)
    {
      MetaDrawOp *op;
      const char *state;
      const char *shadow;
      const char *arrow;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *filled;
      gboolean filled_val;
      GtkStateFlags state_val;
      GtkShadowType shadow_val;
      GtkArrowType arrow_val;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!state", &state,
                              "!shadow", &shadow,
                              "!arrow", &arrow,
                              "!x", &x, "!y", &y,
                              "!width", &width, "!height", &height,
                              "filled", &filled,
                              NULL))
        return;

      filled_val = TRUE;
      if (filled && !parse_boolean (filled, &filled_val, context, error))
        return;

      if (!meta_gtk_state_from_string (state, &state_val))
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Did not understand state \"%s\" for <%s> element"),
                     state, element_name);
          return;
        }

      shadow_val = meta_gtk_shadow_from_string (shadow);
      if (((int) shadow_val) == -1)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Did not understand shadow \"%s\" for <%s> element"),
                     shadow, element_name);
          return;
        }

      arrow_val = meta_gtk_arrow_from_string (arrow);
      if (((int) arrow_val) == -1)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Did not understand arrow \"%s\" for <%s> element"),
                     arrow, element_name);
          return;
        }

      op = meta_draw_op_new (META_DRAW_GTK_ARROW);

      op->data.gtk_arrow.x = meta_draw_spec_new (metacity, x, NULL);
      op->data.gtk_arrow.y = meta_draw_spec_new (metacity, y, NULL);
      op->data.gtk_arrow.width = meta_draw_spec_new (metacity, width, NULL);
      op->data.gtk_arrow.height = meta_draw_spec_new (metacity,
                                                      height, NULL);

      op->data.gtk_arrow.filled = filled_val;
      op->data.gtk_arrow.state = state_val;
      op->data.gtk_arrow.shadow = shadow_val;
      op->data.gtk_arrow.arrow = arrow_val;

      g_assert (info->op_list);

      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_GTK_ARROW);
    }
  else if (g_strcmp0 (element_name, "gtk_box") == 0)
    {
      MetaDrawOp *op;
      const char *state;
      const char *shadow;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      GtkStateFlags state_val;
      GtkShadowType shadow_val;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!state", &state,
                              "!shadow", &shadow,
                              "!x", &x, "!y", &y,
                              "!width", &width, "!height", &height,
                              NULL))
        return;

      if (!meta_gtk_state_from_string (state, &state_val))
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Did not understand state \"%s\" for <%s> element"),
                     state, element_name);
          return;
        }

      shadow_val = meta_gtk_shadow_from_string (shadow);
      if (((int) shadow_val) == -1)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Did not understand shadow \"%s\" for <%s> element"),
                     shadow, element_name);
          return;
        }

      op = meta_draw_op_new (META_DRAW_GTK_BOX);

      op->data.gtk_box.x = meta_draw_spec_new (metacity, x, NULL);
      op->data.gtk_box.y = meta_draw_spec_new (metacity, y, NULL);
      op->data.gtk_box.width = meta_draw_spec_new (metacity, width, NULL);
      op->data.gtk_box.height = meta_draw_spec_new (metacity, height, NULL);

      op->data.gtk_box.state = state_val;
      op->data.gtk_box.shadow = shadow_val;

      g_assert (info->op_list);

      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_GTK_BOX);
    }
  else if (g_strcmp0 (element_name, "gtk_vline") == 0)
    {
      MetaDrawOp *op;
      const char *state;
      const char *x;
      const char *y1;
      const char *y2;
      GtkStateFlags state_val;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!state", &state,
                              "!x", &x, "!y1", &y1, "!y2", &y2,
                              NULL))
        return;

      if (!meta_gtk_state_from_string (state, &state_val))
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Did not understand state \"%s\" for <%s> element"),
                     state, element_name);
          return;
        }

      op = meta_draw_op_new (META_DRAW_GTK_VLINE);

      op->data.gtk_vline.x = meta_draw_spec_new (metacity, x, NULL);
      op->data.gtk_vline.y1 = meta_draw_spec_new (metacity, y1, NULL);
      op->data.gtk_vline.y2 = meta_draw_spec_new (metacity, y2, NULL);

      op->data.gtk_vline.state = state_val;

      g_assert (info->op_list);

      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_GTK_VLINE);
    }
  else if (g_strcmp0 (element_name, "icon") == 0)
    {
      MetaDrawOp *op;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *alpha;
      const char *fill_type;
      MetaAlphaGradientSpec *alpha_spec;
      MetaImageFillType fill_type_val;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!x", &x, "!y", &y,
                              "!width", &width, "!height", &height,
                              "alpha", &alpha,
                              "fill_type", &fill_type,
                              NULL))
        return;

      fill_type_val = META_IMAGE_FILL_SCALE;
      if (fill_type)
        {
          fill_type_val = meta_image_fill_type_from_string (fill_type);

          if (((int) fill_type_val) == -1)
            {
              set_error (error, context, G_MARKUP_ERROR,
                         G_MARKUP_ERROR_PARSE,
                         _("Did not understand fill type \"%s\" for <%s> element"),
                         fill_type, element_name);
            }
        }

      alpha_spec = NULL;
      if (alpha && !parse_alpha (alpha, &alpha_spec, context, error))
        return;

      op = meta_draw_op_new (META_DRAW_ICON);

      op->data.icon.x = meta_draw_spec_new (metacity, x, NULL);
      op->data.icon.y = meta_draw_spec_new (metacity, y, NULL);
      op->data.icon.width = meta_draw_spec_new (metacity, width, NULL);
      op->data.icon.height = meta_draw_spec_new (metacity, height, NULL);

      op->data.icon.alpha_spec = alpha_spec;
      op->data.icon.fill_type = fill_type_val;

      g_assert (info->op_list);

      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_ICON);
    }
  else if (g_strcmp0 (element_name, "title") == 0)
    {
      MetaDrawOp *op;
      const char *color;
      const char *x;
      const char *y;
      const char *ellipsize_width;
      MetaColorSpec *color_spec;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!color", &color,
                              "!x", &x, "!y", &y,
                              "ellipsize_width", &ellipsize_width,
                              NULL))
        return;

      if (ellipsize_width && peek_required_version (info) < 3001)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     ATTRIBUTE_NOT_FOUND, "ellipsize_width", element_name);
          return;
        }

      /* Check last so we don't have to free it when other
       * stuff fails
       */
      color_spec = parse_color (info->metacity, color, error);
      if (color_spec == NULL)
        {
          add_context_to_error (error, context);
          return;
        }

      op = meta_draw_op_new (META_DRAW_TITLE);

      op->data.title.color_spec = color_spec;

      op->data.title.x = meta_draw_spec_new (metacity, x, NULL);
      op->data.title.y = meta_draw_spec_new (metacity, y, NULL);
      if (ellipsize_width)
        op->data.title.ellipsize_width = meta_draw_spec_new (metacity, ellipsize_width, NULL);

      g_assert (info->op_list);

      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_TITLE);
    }
  else if (g_strcmp0 (element_name, "include") == 0)
    {
      MetaDrawOp *op;
      const char *name;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      MetaDrawOpList *op_list;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "x", &x, "y", &y,
                              "width", &width, "height", &height,
                              "!name", &name,
                              NULL))
        return;

      /* x/y/width/height default to 0,0,width,height - should
       * probably do this for all the draw ops
       */
      op_list = meta_theme_metacity_lookup_draw_op_list (info->metacity, name);
      if (op_list == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("No <draw_ops> called \"%s\" has been defined"),
                     name);
          return;
        }

      g_assert (info->op_list);

      if (op_list == info->op_list ||
          meta_draw_op_list_contains (op_list, info->op_list))
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Including draw_ops \"%s\" here would create a circular reference"),
                     name);
          return;
        }

      op = meta_draw_op_new (META_DRAW_OP_LIST);

      meta_draw_op_list_ref (op_list);
      op->data.op_list.op_list = op_list;

      op->data.op_list.x = meta_draw_spec_new (metacity, x ? x : "0", NULL);
      op->data.op_list.y = meta_draw_spec_new (metacity, y ? y : "0", NULL);
      op->data.op_list.width = meta_draw_spec_new (metacity,
                                                   width ? width : "width",
                                                   NULL);
      op->data.op_list.height = meta_draw_spec_new (metacity,
                                                    height ? height : "height",
                                                    NULL);

      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_INCLUDE);
    }
  else if (g_strcmp0 (element_name, "tile") == 0)
    {
      MetaDrawOp *op;
      const char *name;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *tile_xoffset;
      const char *tile_yoffset;
      const char *tile_width;
      const char *tile_height;
      MetaDrawOpList *op_list;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "x", &x, "y", &y,
                              "width", &width, "height", &height,
                              "!name", &name,
                              "tile_xoffset", &tile_xoffset,
                              "tile_yoffset", &tile_yoffset,
                              "!tile_width", &tile_width,
                              "!tile_height", &tile_height,
                              NULL))
        return;

      /* These default to 0 */
      op_list = meta_theme_metacity_lookup_draw_op_list (info->metacity, name);
      if (op_list == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("No <draw_ops> called \"%s\" has been defined"),
                     name);
          return;
        }

      g_assert (info->op_list);

      if (op_list == info->op_list ||
          meta_draw_op_list_contains (op_list, info->op_list))
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Including draw_ops \"%s\" here would create a circular reference"),
                     name);
          return;
        }

      op = meta_draw_op_new (META_DRAW_TILE);

      meta_draw_op_list_ref (op_list);

      op->data.tile.x = meta_draw_spec_new (metacity, x ? x : "0", NULL);
      op->data.tile.y = meta_draw_spec_new (metacity, y ? y : "0", NULL);
      op->data.tile.width = meta_draw_spec_new (metacity,
                                                width ? width : "width",
                                                NULL);
      op->data.tile.height = meta_draw_spec_new (metacity,
                                                 height ? height : "height",
                                                 NULL);
      op->data.tile.tile_xoffset = meta_draw_spec_new (metacity,
                                                       tile_xoffset ? tile_xoffset : "0",
                                                       NULL);
      op->data.tile.tile_yoffset = meta_draw_spec_new (metacity,
                                                       tile_yoffset ? tile_yoffset : "0",
                                                       NULL);
      op->data.tile.tile_width = meta_draw_spec_new (metacity, tile_width, NULL);
      op->data.tile.tile_height = meta_draw_spec_new (metacity, tile_height, NULL);

      op->data.tile.op_list = op_list;

      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_TILE);
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "draw_ops");
    }
}

static void
parse_gradient_element (GMarkupParseContext  *context,
                        const gchar          *element_name,
                        const gchar         **attribute_names,
                        const gchar         **attribute_values,
                        ParseInfo            *info,
                        GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_GRADIENT);

  if (g_strcmp0 (element_name, "color") == 0)
    {
      const char *value = NULL;
      MetaColorSpec *color_spec;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!value", &value,
                              NULL))
        return;

      color_spec = parse_color (info->metacity, value, error);
      if (color_spec == NULL)
        {
          add_context_to_error (error, context);
          return;
        }

      g_assert (info->op);
      g_assert (info->op->type == META_DRAW_GRADIENT);
      g_assert (info->op->data.gradient.gradient_spec != NULL);

      meta_gradient_spec_add_color_spec (info->op->data.gradient.gradient_spec,
                                         color_spec);

      push_state (info, STATE_COLOR);
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "gradient");
    }
}

static MetaButtonState
meta_button_state_from_string (const char *str)
{
  if (strcmp ("normal", str) == 0)
    return META_BUTTON_STATE_NORMAL;
  else if (strcmp ("pressed", str) == 0)
    return META_BUTTON_STATE_PRESSED;
  else if (strcmp ("prelight", str) == 0)
    return META_BUTTON_STATE_PRELIGHT;
  else
    return META_BUTTON_STATE_LAST;
}

static MetaButtonFunction
meta_button_function_from_string (MetaThemeMetacity *metacity,
                                  const gchar       *str)
{
  if (theme_allows (metacity, META_THEME_SHADE_STICK_ABOVE_BUTTONS))
    {
      if (strcmp ("shade", str) == 0)
        return META_BUTTON_FUNCTION_SHADE;
      else if (strcmp ("above", str) == 0)
        return META_BUTTON_FUNCTION_ABOVE;
      else if (strcmp ("stick", str) == 0)
        return META_BUTTON_FUNCTION_STICK;
      else if (strcmp ("unshade", str) == 0)
        return META_BUTTON_FUNCTION_UNSHADE;
      else if (strcmp ("unabove", str) == 0)
        return META_BUTTON_FUNCTION_UNABOVE;
      else if (strcmp ("unstick", str) == 0)
        return META_BUTTON_FUNCTION_UNSTICK;
     }

  if (strcmp ("close", str) == 0)
    return META_BUTTON_FUNCTION_CLOSE;
  else if (strcmp ("maximize", str) == 0)
    return META_BUTTON_FUNCTION_MAXIMIZE;
  else if (strcmp ("minimize", str) == 0)
    return META_BUTTON_FUNCTION_MINIMIZE;
  else if (strcmp ("menu", str) == 0)
    return META_BUTTON_FUNCTION_MENU;
  else if (strcmp ("left_left_background", str) == 0)
    return META_BUTTON_FUNCTION_LEFT_LEFT_BACKGROUND;
  else if (strcmp ("left_middle_background", str) == 0)
    return META_BUTTON_FUNCTION_LEFT_MIDDLE_BACKGROUND;
  else if (strcmp ("left_right_background", str) == 0)
    return META_BUTTON_FUNCTION_LEFT_RIGHT_BACKGROUND;
  else if (strcmp ("left_single_background", str) == 0)
    return META_BUTTON_FUNCTION_LEFT_SINGLE_BACKGROUND;
  else if (strcmp ("right_left_background", str) == 0)
    return META_BUTTON_FUNCTION_RIGHT_LEFT_BACKGROUND;
  else if (strcmp ("right_middle_background", str) == 0)
    return META_BUTTON_FUNCTION_RIGHT_MIDDLE_BACKGROUND;
  else if (strcmp ("right_right_background", str) == 0)
    return META_BUTTON_FUNCTION_RIGHT_RIGHT_BACKGROUND;
  else if (strcmp ("right_single_background", str) == 0)
    return META_BUTTON_FUNCTION_RIGHT_SINGLE_BACKGROUND;
  else
    return META_BUTTON_FUNCTION_LAST;
}

static MetaFramePiece
meta_frame_piece_from_string (const char *str)
{
  if (strcmp ("entire_background", str) == 0)
    return META_FRAME_PIECE_ENTIRE_BACKGROUND;
  else if (strcmp ("titlebar", str) == 0)
    return META_FRAME_PIECE_TITLEBAR;
  else if (strcmp ("titlebar_middle", str) == 0)
    return META_FRAME_PIECE_TITLEBAR_MIDDLE;
  else if (strcmp ("left_titlebar_edge", str) == 0)
    return META_FRAME_PIECE_LEFT_TITLEBAR_EDGE;
  else if (strcmp ("right_titlebar_edge", str) == 0)
    return META_FRAME_PIECE_RIGHT_TITLEBAR_EDGE;
  else if (strcmp ("top_titlebar_edge", str) == 0)
    return META_FRAME_PIECE_TOP_TITLEBAR_EDGE;
  else if (strcmp ("bottom_titlebar_edge", str) == 0)
    return META_FRAME_PIECE_BOTTOM_TITLEBAR_EDGE;
  else if (strcmp ("title", str) == 0)
    return META_FRAME_PIECE_TITLE;
  else if (strcmp ("left_edge", str) == 0)
    return META_FRAME_PIECE_LEFT_EDGE;
  else if (strcmp ("right_edge", str) == 0)
    return META_FRAME_PIECE_RIGHT_EDGE;
  else if (strcmp ("bottom_edge", str) == 0)
    return META_FRAME_PIECE_BOTTOM_EDGE;
  else if (strcmp ("overlay", str) == 0)
    return META_FRAME_PIECE_OVERLAY;
  else
    return META_FRAME_PIECE_LAST;
}

static void
parse_style_element (GMarkupParseContext  *context,
                     const gchar          *element_name,
                     const gchar         **attribute_names,
                     const gchar         **attribute_values,
                     ParseInfo            *info,
                     GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_FRAME_STYLE);

  g_assert (info->style);

  if (g_strcmp0 (element_name, "piece") == 0)
    {
      const char *position = NULL;
      const char *draw_ops = NULL;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!position", &position,
                              "draw_ops", &draw_ops,
                              NULL))
        return;

      info->piece = meta_frame_piece_from_string (position);
      if (info->piece == META_FRAME_PIECE_LAST)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Unknown position \"%s\" for frame piece"),
                     position);
          return;
        }

      if (info->style->pieces[info->piece] != NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Frame style already has a piece at position %s"),
                     position);
          return;
        }

      g_assert (info->op_list == NULL);

      if (draw_ops)
        {
          MetaDrawOpList *op_list;

          op_list = meta_theme_metacity_lookup_draw_op_list (info->metacity, draw_ops);

          if (op_list == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("No <draw_ops> with the name \"%s\" has been defined"),
                         draw_ops);
              return;
            }

          meta_draw_op_list_ref (op_list);
          info->op_list = op_list;
        }

      push_state (info, STATE_PIECE);
    }
  else if (g_strcmp0 (element_name, "button") == 0)
    {
      const char *function = NULL;
      const char *state = NULL;
      const char *draw_ops = NULL;
      guint earliest_version;
      gint required_version;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!function", &function,
                              "!state", &state,
                              "draw_ops", &draw_ops,
                              NULL))
        return;

      info->button_function = meta_button_function_from_string (info->metacity, function);
      if (info->button_function == META_BUTTON_FUNCTION_LAST)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Unknown function \"%s\" for button"),
                     function);
          return;
        }

      earliest_version = meta_theme_metacity_earliest_version_with_button (info->button_function);
      required_version = peek_required_version (info);

      if (earliest_version > (guint) required_version)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Button function '%s' does not exist in this version (%d, need %d)"),
                     function, required_version, earliest_version);
          return;
        }

      info->button_state = meta_button_state_from_string (state);
      if (info->button_state == META_BUTTON_STATE_LAST)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Unknown state '%s' for button"), state);
          return;
        }

      if (info->style->buttons[info->button_function][info->button_state] != NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Frame style already has a button for function %s state %s"),
                     function, state);
          return;
        }

      g_assert (info->op_list == NULL);

      if (draw_ops)
        {
          MetaDrawOpList *op_list;

          op_list = meta_theme_metacity_lookup_draw_op_list (info->metacity,
                                                             draw_ops);

          if (op_list == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("No <draw_ops> with the name '%s' has been defined"),
                         draw_ops);
              return;
            }

          meta_draw_op_list_ref (op_list);
          info->op_list = op_list;
        }

      push_state (info, STATE_BUTTON);
    }
  else
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "frame_style");
    }
}

static void
parse_piece_element (GMarkupParseContext  *context,
                     const gchar          *element_name,
                     const gchar         **attribute_names,
                     const gchar         **attribute_values,
                     ParseInfo            *info,
                     GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_PIECE);

  if (g_strcmp0 (element_name, "draw_ops") == 0)
    {
      if (info->op_list)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Can't have a two draw_ops for a <piece> element (theme specified a draw_ops attribute and also a <draw_ops> element, or specified two elements)"));
          return;
        }

      if (!check_no_attributes (context, element_name, attribute_names,
                                attribute_values, error))
        return;

      g_assert (info->op_list == NULL);
      info->op_list = meta_draw_op_list_new (2);

      push_state (info, STATE_DRAW_OPS);
    }
  else
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "piece");
    }
}

static void
parse_button_element (GMarkupParseContext  *context,
                      const gchar          *element_name,
                      const gchar         **attribute_names,
                      const gchar         **attribute_values,
                      ParseInfo            *info,
                      GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_BUTTON);

  if (g_strcmp0 (element_name, "draw_ops") == 0)
    {
      if (info->op_list)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Can't have a two draw_ops for a <button> element (theme specified a draw_ops attribute and also a <draw_ops> element, or specified two elements)"));
          return;
        }

      if (!check_no_attributes (context, element_name, attribute_names,
                                attribute_values, error))
        return;

      g_assert (info->op_list == NULL);
      info->op_list = meta_draw_op_list_new (2);

      push_state (info, STATE_DRAW_OPS);
    }
  else
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "button");
    }
}

static void
parse_menu_icon_element (GMarkupParseContext  *context,
                         const gchar          *element_name,
                         const gchar         **attribute_names,
                         const gchar         **attribute_values,
                         ParseInfo            *info,
                         GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_MENU_ICON);

  if (g_strcmp0 (element_name, "draw_ops") == 0)
    {
      if (info->op_list)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Can't have a two draw_ops for a <menu_icon> element (theme specified a draw_ops attribute and also a <draw_ops> element, or specified two elements)"));
          return;
        }

      if (!check_no_attributes (context, element_name, attribute_names,
                                attribute_values, error))
        return;

      g_assert (info->op_list == NULL);
      info->op_list = meta_draw_op_list_new (2);

      push_state (info, STATE_DRAW_OPS);
    }
  else
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "menu_icon");
    }
}

static MetaFrameFocus
meta_frame_focus_from_string (const char *str)
{
  if (strcmp ("no", str) == 0)
    return META_FRAME_FOCUS_NO;
  else if (strcmp ("yes", str) == 0)
    return META_FRAME_FOCUS_YES;
  else
    return META_FRAME_FOCUS_LAST;
}

static MetaFrameResize
meta_frame_resize_from_string (const char *str)
{
  if (strcmp ("none", str) == 0)
    return META_FRAME_RESIZE_NONE;
  else if (strcmp ("vertical", str) == 0)
    return META_FRAME_RESIZE_VERTICAL;
  else if (strcmp ("horizontal", str) == 0)
    return META_FRAME_RESIZE_HORIZONTAL;
  else if (strcmp ("both", str) == 0)
    return META_FRAME_RESIZE_BOTH;
  else
    return META_FRAME_RESIZE_LAST;
}

static MetaFrameState
meta_frame_state_from_string (const char *str)
{
  if (strcmp ("normal", str) == 0)
    return META_FRAME_STATE_NORMAL;
  else if (strcmp ("maximized", str) == 0)
    return META_FRAME_STATE_MAXIMIZED;
  else if (strcmp ("tiled_left", str) == 0)
    return META_FRAME_STATE_TILED_LEFT;
  else if (strcmp ("tiled_right", str) == 0)
    return META_FRAME_STATE_TILED_RIGHT;
  else if (strcmp ("shaded", str) == 0)
    return META_FRAME_STATE_SHADED;
  else if (strcmp ("maximized_and_shaded", str) == 0)
    return META_FRAME_STATE_MAXIMIZED_AND_SHADED;
  else if (strcmp ("tiled_left_and_shaded", str) == 0)
    return META_FRAME_STATE_TILED_LEFT_AND_SHADED;
  else if (strcmp ("tiled_right_and_shaded", str) == 0)
    return META_FRAME_STATE_TILED_RIGHT_AND_SHADED;
  else
    return META_FRAME_STATE_LAST;
}

static void
parse_style_set_element (GMarkupParseContext  *context,
                         const gchar          *element_name,
                         const gchar         **attribute_names,
                         const gchar         **attribute_values,
                         ParseInfo            *info,
                         GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_FRAME_STYLE_SET);

  if (g_strcmp0 (element_name, "frame") == 0)
    {
      const char *focus = NULL;
      const char *state = NULL;
      const char *resize = NULL;
      const char *style = NULL;
      MetaFrameFocus frame_focus;
      MetaFrameState frame_state;
      MetaFrameResize frame_resize;
      MetaFrameStyle *frame_style;

      if (!locate_attributes (context, element_name, attribute_names,
                              attribute_values, error,
                              "!focus", &focus,
                              "!state", &state,
                              "resize", &resize,
                              "!style", &style,
                              NULL))
        return;

      frame_focus = meta_frame_focus_from_string (focus);
      if (frame_focus == META_FRAME_FOCUS_LAST)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("'%s' is not a valid value for focus attribute"),
                     focus);
          return;
        }

      frame_state = meta_frame_state_from_string (state);
      if (frame_state == META_FRAME_STATE_LAST)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("'%s' is not a valid value for state attribute"),
                     focus);
          return;
        }

      frame_style = meta_theme_metacity_lookup_style (info->metacity, style);
      if (frame_style == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("A style called '%s' has not been defined"), style);
          return;
        }

      switch (frame_state)
        {
          case META_FRAME_STATE_NORMAL:
            if (resize == NULL)
              {
                set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                           ATTRIBUTE_NOT_FOUND, "resize", element_name);
                return;
              }

            frame_resize = meta_frame_resize_from_string (resize);
            if (frame_resize == META_FRAME_RESIZE_LAST)
              {
                set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                           _("'%s' is not a valid value for resize attribute"),
                           focus);
                return;
              }

            break;

          case META_FRAME_STATE_SHADED:
            if (theme_allows (info->metacity, META_THEME_UNRESIZABLE_SHADED_STYLES))
              {
                if (resize == NULL)
                  /* In state="normal" we would complain here. But instead we accept
                   * not having a resize attribute and default to resize="both", since
                   * that most closely mimics what we did in v1, and thus people can
                   * upgrade a theme to v2 without as much hassle.
                   */
                  frame_resize = META_FRAME_RESIZE_BOTH;
                else
                  {
                    frame_resize = meta_frame_resize_from_string (resize);
                    if (frame_resize == META_FRAME_RESIZE_LAST)
                      {
                        set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                                   _("'%s' is not a valid value for resize attribute"),
                                   focus);
                        return;
                      }
                  }
              }
            else /* v1 theme */
              {
                if (resize != NULL)
                  {
                    set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("Should not have 'resize' attribute on <%s> element for maximized/shaded states"),
                        element_name);
                    return;
                  }

                /* resize="both" is equivalent to the old behaviour */
                frame_resize = META_FRAME_RESIZE_BOTH;
              }
            break;

          case META_FRAME_STATE_MAXIMIZED:
          case META_FRAME_STATE_TILED_LEFT:
          case META_FRAME_STATE_TILED_RIGHT:
          case META_FRAME_STATE_MAXIMIZED_AND_SHADED:
          case META_FRAME_STATE_TILED_LEFT_AND_SHADED:
          case META_FRAME_STATE_TILED_RIGHT_AND_SHADED:
          case META_FRAME_STATE_LAST:
          default:
            if (resize != NULL)
              {
                set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                           _("Should not have 'resize' attribute on <%s> element for maximized states"),
                           element_name);
                return;
              }

            frame_resize = META_FRAME_RESIZE_LAST;
            break;
        }

      switch (frame_state)
        {
          case META_FRAME_STATE_NORMAL:
            if (info->style_set->normal_styles[frame_resize][frame_focus])
              {
                set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                           _("Style has already been specified for state %s resize %s focus %s"),
                           state, resize, focus);
                return;
              }
            meta_frame_style_ref (frame_style);
            info->style_set->normal_styles[frame_resize][frame_focus] = frame_style;
            break;

          case META_FRAME_STATE_MAXIMIZED:
            if (info->style_set->maximized_styles[frame_focus])
              {
                set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                           _("Style has already been specified for state %s focus %s"),
                           state, focus);
                return;
              }
            meta_frame_style_ref (frame_style);
            info->style_set->maximized_styles[frame_focus] = frame_style;
            break;

          case META_FRAME_STATE_TILED_LEFT:
            if (info->style_set->tiled_left_styles[frame_focus])
              {
                set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                           _("Style has already been specified for state %s focus %s"),
                           state, focus);
                return;
              }
            meta_frame_style_ref (frame_style);
            info->style_set->tiled_left_styles[frame_focus] = frame_style;
            break;

          case META_FRAME_STATE_TILED_RIGHT:
            if (info->style_set->tiled_right_styles[frame_focus])
              {
                set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                           _("Style has already been specified for state %s focus %s"),
                           state, focus);
                return;
              }
            meta_frame_style_ref (frame_style);
            info->style_set->tiled_right_styles[frame_focus] = frame_style;
            break;

          case META_FRAME_STATE_SHADED:
            if (info->style_set->shaded_styles[frame_resize][frame_focus])
              {
                set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                           _("Style has already been specified for state %s resize %s focus %s"),
                           state, resize, focus);
                return;
              }
            meta_frame_style_ref (frame_style);
            info->style_set->shaded_styles[frame_resize][frame_focus] = frame_style;
            break;

          case META_FRAME_STATE_MAXIMIZED_AND_SHADED:
            if (info->style_set->maximized_and_shaded_styles[frame_focus])
              {
                set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                           _("Style has already been specified for state %s focus %s"),
                           state, focus);
                return;
              }
            meta_frame_style_ref (frame_style);
            info->style_set->maximized_and_shaded_styles[frame_focus] = frame_style;
            break;

          case META_FRAME_STATE_TILED_LEFT_AND_SHADED:
            if (info->style_set->tiled_left_and_shaded_styles[frame_focus])
              {
                set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                           _("Style has already been specified for state %s focus %s"),
                           state, focus);
                return;
              }
            meta_frame_style_ref (frame_style);
            info->style_set->tiled_left_and_shaded_styles[frame_focus] = frame_style;
            break;

          case META_FRAME_STATE_TILED_RIGHT_AND_SHADED:
            if (info->style_set->tiled_right_and_shaded_styles[frame_focus])
              {
                set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                           _("Style has already been specified for state %s focus %s"),
                           state, focus);
                return;
              }
            meta_frame_style_ref (frame_style);
            info->style_set->tiled_right_and_shaded_styles[frame_focus] = frame_style;
            break;

          case META_FRAME_STATE_LAST:
          default:
            g_assert_not_reached ();
            break;
        }

      push_state (info, STATE_FRAME);
    }
  else
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "frame_style_set");
    }
}

static const gchar *
find_version (const gchar **attribute_names,
              const gchar **attribute_values)
{
  int i;

  for (i = 0; attribute_names[i]; i++)
    {
      if (strcmp (attribute_names[i], "version") == 0)
        return attribute_values[i];
    }

  return NULL;
}

/* Returns whether the version element was successfully parsed.
 * If successfully parsed, then two additional items are returned:
 *
 * satisfied:        whether this version of Metacity meets the version check
 * minimum_required: minimum version of theme format required by version check
 */
static gboolean
check_version (GMarkupParseContext  *context,
               const char           *version_str,
               gboolean             *satisfied,
               guint                *minimum_required,
               GError              **error)
{
  static GRegex *version_regex;
  GMatchInfo *info;
  char *comparison_str, *major_str, *minor_str;
  guint version;

  *minimum_required = 0;

  if (!version_regex)
    version_regex = g_regex_new ("^\\s*([<>]=?)\\s*(\\d+)(\\.\\d+)?\\s*$", 0, 0, NULL);

  if (!g_regex_match (version_regex, version_str, 0, &info))
    {
      g_match_info_free (info);
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Bad version specification '%s'"), version_str);
      return FALSE;
    }

  comparison_str = g_match_info_fetch (info, 1);
  major_str = g_match_info_fetch (info, 2);
  minor_str = g_match_info_fetch (info, 3);

  version = 1000 * atoi (major_str);
  /* might get NULL, see: https://bugzilla.gnome.org/review?bug=588217 */
  if (minor_str && minor_str[0])
    version += atoi (minor_str + 1);

  if (comparison_str[0] == '<')
    {
      if (comparison_str[1] == '=')
        *satisfied = THEME_VERSION <= version;
      else
        *satisfied = THEME_VERSION < version;
   }
  else
   {
     if (comparison_str[1] == '=')
       {
         *satisfied = THEME_VERSION >= version;
         *minimum_required = version;
       }
     else
       {
         *satisfied = THEME_VERSION > version;
         *minimum_required = version + 1;
       }
   }

  g_free (comparison_str);
  g_free (major_str);
  g_free (minor_str);
  g_match_info_free (info);

  return TRUE;
}

static void
start_element_handler (GMarkupParseContext  *context,
                       const gchar          *element_name,
                       const gchar         **attribute_names,
                       const gchar         **attribute_values,
                       gpointer              user_data,
                       GError              **error)
{
  ParseInfo *info = user_data;
  const char *version;
  guint required_version = 0;

  if (info->skip_level > 0)
    {
      info->skip_level++;
      return;
    }

  required_version = peek_required_version (info);
  version = find_version (attribute_names, attribute_values);

  if (version != NULL)
    {
      gboolean satisfied;
      guint element_required;

      if (required_version < 3000)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("'version' attribute cannot be used in metacity-theme-1.xml or metacity-theme-2.xml"));
          return;
        }

      if (!check_version (context, version, &satisfied, &element_required, error))
        return;

      /* Two different ways of handling an unsatisfied version check:
       * for the toplevel element of a file, we throw an error back so
       * that the controlling code can go ahead and look for an
       * alternate metacity-theme-1.xml or metacity-theme-2.xml; for
       * other elements we just silently skip the element and children.
       */
      if (peek_state (info) == STATE_START)
        {
          if (satisfied)
            {
              if (element_required > info->metacity->format_version)
                info->metacity->format_version = element_required;
            }
          else
            {
              set_error (error, context, META_THEME_ERROR, META_THEME_ERROR_TOO_OLD,
                         _("Theme requires version %s but latest supported theme version is %d.%d"),
                         version, THEME_VERSION, THEME_MINOR_VERSION);
              return;
            }
        }
      else if (!satisfied)
        {
          info->skip_level = 1;
          return;
        }

      if (element_required > required_version)
        required_version = element_required;
    }

  push_required_version (info, required_version);

  switch (peek_state (info))
    {
      case STATE_START:
        if (strcmp (element_name, "metacity_theme") == 0)
          push_state (info, STATE_THEME);
        else
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Outermost element in theme must be <metacity_theme> not <%s>"),
                     element_name);
        break;

      case STATE_THEME:
        parse_toplevel_element (context, element_name, attribute_names,
                                attribute_values, info, error);
        break;

      case STATE_INFO:
        parse_info_element (context, element_name, attribute_names,
                            attribute_values, info, error);
        break;

      case STATE_NAME:
      case STATE_AUTHOR:
      case STATE_COPYRIGHT:
      case STATE_DATE:
      case STATE_DESCRIPTION:
        set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                   _("Element <%s> is not allowed inside a name/author/date/description element"),
                   element_name);
        break;

      case STATE_CONSTANT:
        set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                   _("Element <%s> is not allowed inside a <constant> element"),
                   element_name);
        break;

      case STATE_FRAME_GEOMETRY:
        parse_geometry_element (context, element_name, attribute_names,
                                attribute_values, info, error);
        break;

      case STATE_DISTANCE:
      case STATE_BORDER:
      case STATE_ASPECT_RATIO:
        set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                   _("Element <%s> is not allowed inside a distance/border/aspect_ratio element"),
                   element_name);
        break;

      case STATE_DRAW_OPS:
        parse_draw_op_element (context, element_name, attribute_names,
                               attribute_values, info, error);
        break;

      case STATE_LINE:
      case STATE_RECTANGLE:
      case STATE_ARC:
      case STATE_CLIP:
      case STATE_TINT:
      case STATE_IMAGE:
      case STATE_GTK_ARROW:
      case STATE_GTK_BOX:
      case STATE_GTK_VLINE:
      case STATE_ICON:
      case STATE_TITLE:
      case STATE_INCLUDE:
      case STATE_TILE:
        set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                   _("Element <%s> is not allowed inside a draw operation element"),
                   element_name);
        break;

      case STATE_GRADIENT:
        parse_gradient_element (context, element_name, attribute_names,
                                attribute_values, info, error);
        break;

      case STATE_COLOR:
        set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                   _("Element <%s> is not allowed inside a <%s> element"),
                   element_name, "color");
        break;

      case STATE_FRAME_STYLE:
        parse_style_element (context, element_name, attribute_names,
                             attribute_values, info, error);
        break;

      case STATE_PIECE:
        parse_piece_element (context, element_name, attribute_names,
                             attribute_values, info, error);
        break;

      case STATE_BUTTON:
        parse_button_element (context, element_name, attribute_names,
                              attribute_values, info, error);
        break;

      case STATE_MENU_ICON:
        parse_menu_icon_element (context, element_name, attribute_names,
                                 attribute_values, info, error);
        break;

      case STATE_FRAME_STYLE_SET:
        parse_style_set_element (context, element_name, attribute_names,
                                 attribute_values, info, error);
        break;

      case STATE_FRAME:
        set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                   _("Element <%s> is not allowed inside a <%s> element"),
                   element_name, "frame");
        break;

      case STATE_WINDOW:
        set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                   _("Element <%s> is not allowed inside a <%s> element"),
                   element_name, "window");
        break;

      case STATE_FALLBACK:
        set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                   _("Element <%s> is not allowed inside a <%s> element"),
                   element_name, "fallback");
        break;

      default:
        break;
    }
}

static const char*
meta_frame_type_to_string (MetaFrameType type)
{
  switch (type)
    {
    case META_FRAME_TYPE_NORMAL:
      return "normal";
    case META_FRAME_TYPE_DIALOG:
      return "dialog";
    case META_FRAME_TYPE_MODAL_DIALOG:
      return "modal_dialog";
    case META_FRAME_TYPE_UTILITY:
      return "utility";
    case META_FRAME_TYPE_MENU:
      return "menu";
    case META_FRAME_TYPE_BORDER:
      return "border";
    case META_FRAME_TYPE_ATTACHED:
      return "attached";
    case  META_FRAME_TYPE_LAST:
      break;
    default:
      break;
    }

  return "<unknown>";
}

static gboolean
theme_validate (MetaThemeMetacity  *metacity,
                GError            **error)
{
  guint i;

  g_return_val_if_fail (metacity != NULL, FALSE);

  g_assert (metacity->name);

  if (metacity->readable_name == NULL)
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   /* Translators: This error means that a necessary XML tag (whose name
                    * is given in angle brackets) was not found in a given theme (whose
                    * name is given second, in quotation marks).
                    */
                   _("No <%s> set for theme '%s'"), "name", metacity->name);

      return FALSE;
    }

  if (metacity->author == NULL)
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("No <%s> set for theme '%s'"), "author", metacity->name);

      return FALSE;
    }

  if (metacity->date == NULL)
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("No <%s> set for theme '%s'"), "date", metacity->name);

      return FALSE;
    }

  if (metacity->description == NULL)
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("No <%s> set for theme '%s'"), "description",
                   metacity->name);

      return FALSE;
    }

  if (metacity->copyright == NULL)
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("No <%s> set for theme '%s'"), "copyright",
                   metacity->name);

      return FALSE;
    }

  for (i = 0; i < META_FRAME_TYPE_LAST; i++)
    {
      if (i != META_FRAME_TYPE_ATTACHED && metacity->style_sets_by_type[i] == NULL)
        {
          g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                       _("No frame style set for window type '%s' in theme '%s', add a <window type='%s' style_set='whatever' /> element"),
                       meta_frame_type_to_string (i), metacity->name,
                       meta_frame_type_to_string (i));

        return FALSE;
      }
    }

  return TRUE;
}

static void
end_element_handler (GMarkupParseContext  *context,
                     const gchar          *element_name,
                     gpointer              user_data,
                     GError              **error)
{
  ParseInfo *info;

  info = (ParseInfo *) user_data;

  if (info->skip_level > 0)
    {
      info->skip_level--;
      return;
    }

  switch (peek_state (info))
    {
      case STATE_START:
        break;

      case STATE_THEME:
        g_assert (info->metacity);

        if (!theme_validate (info->metacity, error))
          add_context_to_error (error, context);

        pop_state (info);
        g_assert (peek_state (info) == STATE_START);
        break;

      case STATE_INFO:
        pop_state (info);
        g_assert (peek_state (info) == STATE_THEME);
        break;

      case STATE_NAME:
        pop_state (info);
        g_assert (peek_state (info) == STATE_INFO);
        break;

      case STATE_AUTHOR:
        pop_state (info);
        g_assert (peek_state (info) == STATE_INFO);
        break;

      case STATE_COPYRIGHT:
        pop_state (info);
        g_assert (peek_state (info) == STATE_INFO);
        break;

      case STATE_DATE:
        pop_state (info);
        g_assert (peek_state (info) == STATE_INFO);
        break;

      case STATE_DESCRIPTION:
        pop_state (info);
        g_assert (peek_state (info) == STATE_INFO);
        break;

      case STATE_CONSTANT:
        pop_state (info);
        g_assert (peek_state (info) == STATE_THEME);
        break;

      case STATE_FRAME_GEOMETRY:
        g_assert (info->layout);

        if (!meta_frame_layout_validate (info->layout, error))
          add_context_to_error (error, context);

        /* layout will already be stored in the theme under
         * its name
         */
        meta_frame_layout_unref (info->layout);
        info->layout = NULL;

        pop_state (info);
        g_assert (peek_state (info) == STATE_THEME);
        break;

      case STATE_DISTANCE:
        pop_state (info);
        g_assert (peek_state (info) == STATE_FRAME_GEOMETRY);
        break;

      case STATE_BORDER:
        pop_state (info);
        g_assert (peek_state (info) == STATE_FRAME_GEOMETRY);
        break;

      case STATE_ASPECT_RATIO:
        pop_state (info);
        g_assert (peek_state (info) == STATE_FRAME_GEOMETRY);
        break;

      case STATE_DRAW_OPS:
        {
          ParseState parse_state;

          g_assert (info->op_list);

          if (!meta_draw_op_list_validate (info->op_list, error))
            {
              add_context_to_error (error, context);
              meta_draw_op_list_unref (info->op_list);
              info->op_list = NULL;
            }

          pop_state (info);

          parse_state = peek_state (info);
          if (parse_state == STATE_BUTTON || parse_state == STATE_PIECE ||
              parse_state == STATE_MENU_ICON)
            {
              /* Leave info->op_list to be picked up
               * when these elements are closed
               */
              g_assert (info->op_list);
            }
          else if (parse_state == STATE_THEME)
            {
              g_assert (info->op_list);
              meta_draw_op_list_unref (info->op_list);
              info->op_list = NULL;
            }
          else
            {
              /* Op list can't occur in other contexts */
              g_assert_not_reached ();
            }
        }
        break;

      case STATE_LINE:
        pop_state (info);
        g_assert (peek_state (info) == STATE_DRAW_OPS);
        break;

      case STATE_RECTANGLE:
        pop_state (info);
        g_assert (peek_state (info) == STATE_DRAW_OPS);
        break;

      case STATE_ARC:
        pop_state (info);
        g_assert (peek_state (info) == STATE_DRAW_OPS);
        break;

      case STATE_CLIP:
        pop_state (info);
        g_assert (peek_state (info) == STATE_DRAW_OPS);
        break;

      case STATE_TINT:
        pop_state (info);
        g_assert (peek_state (info) == STATE_DRAW_OPS);
        break;

      case STATE_GRADIENT:
        g_assert (info->op);
        g_assert (info->op->type == META_DRAW_GRADIENT);

        if (!meta_gradient_spec_validate (info->op->data.gradient.gradient_spec,
                                          error))
          {
            add_context_to_error (error, context);
            meta_draw_op_free (info->op);
            info->op = NULL;
          }
        else
          {
            g_assert (info->op_list);
            meta_draw_op_list_append (info->op_list, info->op);
            info->op = NULL;
          }

        pop_state (info);
        g_assert (peek_state (info) == STATE_DRAW_OPS);
        break;

      case STATE_IMAGE:
        pop_state (info);
        g_assert (peek_state (info) == STATE_DRAW_OPS);
        break;

      case STATE_GTK_ARROW:
        pop_state (info);
        g_assert (peek_state (info) == STATE_DRAW_OPS);
        break;

      case STATE_GTK_BOX:
        pop_state (info);
        g_assert (peek_state (info) == STATE_DRAW_OPS);
        break;

      case STATE_GTK_VLINE:
        pop_state (info);
        g_assert (peek_state (info) == STATE_DRAW_OPS);
        break;

      case STATE_ICON:
        pop_state (info);
        g_assert (peek_state (info) == STATE_DRAW_OPS);
        break;

      case STATE_TITLE:
        pop_state (info);
        g_assert (peek_state (info) == STATE_DRAW_OPS);
        break;

      case STATE_INCLUDE:
        pop_state (info);
        g_assert (peek_state (info) == STATE_DRAW_OPS);
        break;

      case STATE_TILE:
        pop_state (info);
        g_assert (peek_state (info) == STATE_DRAW_OPS);
        break;

      case STATE_COLOR:
        pop_state (info);
        g_assert (peek_state (info) == STATE_GRADIENT);
        break;

      case STATE_FRAME_STYLE:
        g_assert (info->style);

        if (!meta_frame_style_validate (info->style,
                                        peek_required_version (info),
                                        error))
          {
            add_context_to_error (error, context);
          }

        /* Frame style is in the theme hash table and a ref
         * is held there
         */
        meta_frame_style_unref (info->style);
        info->style = NULL;

        pop_state (info);
        g_assert (peek_state (info) == STATE_THEME);
        break;

      case STATE_PIECE:
        g_assert (info->style);
        if (info->op_list == NULL)
          {
            set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                       _("No draw_ops provided for frame piece"));
          }
        else
          {
            info->style->pieces[info->piece] = info->op_list;
            info->op_list = NULL;
          }
        pop_state (info);
        g_assert (peek_state (info) == STATE_FRAME_STYLE);
        break;

      case STATE_BUTTON:
        g_assert (info->style);
        if (info->op_list == NULL)
          {
            set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                       _("No draw_ops provided for button"));
          }
        else
          {
            MetaButtonFunction function;
            MetaButtonState state;

            function = info->button_function;
            state = info->button_state;

            info->style->buttons[function][state] = info->op_list;
            info->op_list = NULL;
          }
        pop_state (info);
        break;

      case STATE_MENU_ICON:
        g_assert (info->metacity);

        if (info->op_list != NULL)
          {
            meta_draw_op_list_unref (info->op_list);
            info->op_list = NULL;
          }

        pop_state (info);
        g_assert (peek_state (info) == STATE_THEME);
        break;

      case STATE_FRAME_STYLE_SET:
        g_assert (info->style_set);

        if (!meta_frame_style_set_validate (info->style_set, error))
          add_context_to_error (error, context);

        /* Style set is in the theme hash table and a reference
         * is held there.
         */
        meta_frame_style_set_unref (info->style_set);
        info->style_set = NULL;

        pop_state (info);
        g_assert (peek_state (info) == STATE_THEME);
        break;

      case STATE_FRAME:
        pop_state (info);
        g_assert (peek_state (info) == STATE_FRAME_STYLE_SET);
        break;

      case STATE_WINDOW:
        pop_state (info);
        g_assert (peek_state (info) == STATE_THEME);
        break;

      case STATE_FALLBACK:
        pop_state (info);
        g_assert (peek_state (info) == STATE_THEME);
        break;

      default:
        break;
    }

  pop_required_version (info);
}

static gboolean
all_whitespace (const gchar *text,
                gint         text_len)
{
  const gchar *p;
  const gchar *end;

  p = text;
  end = text + text_len;

  while (p != end)
    {
      if (!g_ascii_isspace (*p))
        return FALSE;

      p = g_utf8_next_char (p);
    }

  return TRUE;
}

static void
text_handler (GMarkupParseContext  *context,
              const gchar          *text,
              gsize                 text_len,
              gpointer              user_data,
              GError              **error)
{
  ParseInfo *info;

  info = (ParseInfo *) user_data;

  if (info->skip_level > 0)
    return;

  if (all_whitespace (text, text_len))
    return;

  switch (peek_state (info))
    {
      case STATE_START:
        g_assert_not_reached (); /* gmarkup shouldn't do this */
        break;

      case STATE_NAME:
        if (info->metacity->readable_name != NULL)
          {
            set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                       _("<%s> specified twice for this theme"), "name");
            return;
          }

        info->metacity->readable_name = g_strndup (text, text_len);
        break;

      case STATE_AUTHOR:
        if (info->metacity->author != NULL)
          {
            set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                       _("<%s> specified twice for this theme"), "author");
            return;
          }

        info->metacity->author = g_strndup (text, text_len);
        break;

      case STATE_COPYRIGHT:
        if (info->metacity->copyright != NULL)
          {
            set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                       _("<%s> specified twice for this theme"), "copyright");
            return;
          }

        info->metacity->copyright = g_strndup (text, text_len);
        break;

      case STATE_DATE:
        if (info->metacity->date != NULL)
          {
            set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                       _("<%s> specified twice for this theme"), "date");
            return;
          }

        info->metacity->date = g_strndup (text, text_len);
        break;

      case STATE_DESCRIPTION:
        if (info->metacity->description != NULL)
          {
            set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                       _("<%s> specified twice for this theme"), "description");
            return;
          }

        info->metacity->description = g_strndup (text, text_len);
        break;

      case STATE_THEME:
      case STATE_INFO:
      case STATE_CONSTANT:
      case STATE_FRAME_GEOMETRY:
      case STATE_DISTANCE:
      case STATE_BORDER:
      case STATE_ASPECT_RATIO:
      case STATE_DRAW_OPS:
      case STATE_LINE:
      case STATE_RECTANGLE:
      case STATE_ARC:
      case STATE_CLIP:
      case STATE_TINT:
      case STATE_GRADIENT:
      case STATE_IMAGE:
      case STATE_GTK_ARROW:
      case STATE_GTK_BOX:
      case STATE_GTK_VLINE:
      case STATE_ICON:
      case STATE_TITLE:
      case STATE_INCLUDE:
      case STATE_TILE:
      case STATE_COLOR:
      case STATE_FRAME_STYLE:
      case STATE_PIECE:
      case STATE_BUTTON:
      case STATE_MENU_ICON:
      case STATE_FRAME_STYLE_SET:
      case STATE_FRAME:
      case STATE_WINDOW:
      case STATE_FALLBACK:
        set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                   _("No text is allowed inside element <%s>"),
                   g_markup_parse_context_get_element (context));
        break;

      default:
        break;
    }
}

/* If the theme is not-corrupt, keep looking for alternate versions
 * in other locations we might be compatible with
 */
static gboolean
theme_error_is_fatal (GError *error)
{
  return !(error->domain == G_FILE_ERROR ||
           (error->domain == META_THEME_ERROR &&
            error->code == META_THEME_ERROR_TOO_OLD));
}

static gboolean
keep_trying (GError **error)
{
  if (*error && !theme_error_is_fatal (*error))
    {
      g_clear_error (error);
      return TRUE;
    }

  return FALSE;
}

static void
clear_theme (MetaThemeMetacity *metacity)
{
  MetaFrameType type;

  g_free (metacity->name);
  metacity->name = NULL;

  g_free (metacity->dirname);
  metacity->dirname = NULL;

  g_free (metacity->readable_name);
  metacity->readable_name = NULL;

  g_free (metacity->date);
  metacity->date = NULL;

  g_free (metacity->description);
  metacity->description = NULL;

  g_free (metacity->author);
  metacity->author = NULL;

  g_free (metacity->copyright);
  metacity->copyright = NULL;

  g_clear_pointer (&metacity->integers, g_hash_table_destroy);
  g_clear_pointer (&metacity->floats, g_hash_table_destroy);
  g_clear_pointer (&metacity->colors, g_hash_table_destroy);

  g_hash_table_remove_all (metacity->draw_op_lists);
  g_hash_table_remove_all (metacity->frame_layouts);
  g_hash_table_remove_all (metacity->styles);
  g_hash_table_remove_all (metacity->style_sets);
  g_hash_table_remove_all (metacity->images);

  for (type = 0; type < META_FRAME_TYPE_LAST; type++)
    {
      if (metacity->style_sets_by_type[type] != NULL)
        {
          meta_frame_style_set_unref (metacity->style_sets_by_type[type]);
          metacity->style_sets_by_type[type] = NULL;
        }
    }
}

static GMarkupParser metacity_theme_parser =
  {
    start_element_handler,
    end_element_handler,
    text_handler,
    NULL,
    NULL
  };

static gboolean
load_theme (MetaThemeMetacity  *metacity,
            const gchar        *theme_dir,
            const gchar        *theme_name,
            guint               major_version,
            GError            **error)
{
  gchar *filename;
  gchar *file;
  gboolean retval;
  gchar *text;
  gsize length;
  ParseInfo *info;
  GMarkupParseContext *context;

  g_return_val_if_fail (error && *error == NULL, FALSE);

  clear_theme (metacity);

  metacity->name = g_strdup (theme_name);
  metacity->dirname = g_strdup (theme_dir);
  metacity->format_version = 1000 * major_version;

  filename = g_strdup_printf (METACITY_THEME_FILENAME_FORMAT, major_version);
  file =  g_build_filename (theme_dir, filename, NULL);

  retval = FALSE;
  text = NULL;
  info = NULL;
  context = NULL;

  if (!g_file_get_contents (file, &text, &length, error))
    goto out;

  g_debug ("Parsing theme file %s", file);

  info = parse_info_new (metacity);
  context = g_markup_parse_context_new (&metacity_theme_parser, 0, info, NULL);

  if (!g_markup_parse_context_parse (context, text, length, error))
    goto out;

  if (!g_markup_parse_context_end_parse (context, error))
    goto out;

  retval = TRUE;

out:

  if (*error && !theme_error_is_fatal (*error))
    g_debug ("Failed to read theme from file %s: %s", file, (*error)->message);

  if (context)
    g_markup_parse_context_free (context);

  if (info)
    parse_info_free (info);

  g_free (filename);
  g_free (file);
  g_free (text);

  return retval;
}

static gchar *
get_theme_dir (const gchar *dir,
               const gchar *theme_name)
{
  return g_build_filename (dir, "themes", theme_name, THEME_SUBDIR, NULL);
}

static void
meta_theme_metacity_dispose (GObject *object)
{
  MetaThemeMetacity *metacity;
  gint i;

  metacity = META_THEME_METACITY (object);

  for (i = 0; i < META_FRAME_TYPE_LAST; i++)
    {
      if (metacity->style_sets_by_type[i] != NULL)
        {
          meta_frame_style_set_unref (metacity->style_sets_by_type[i]);
          metacity->style_sets_by_type[i] = NULL;
        }
    }

  g_clear_pointer (&metacity->integers, g_hash_table_destroy);
  g_clear_pointer (&metacity->floats, g_hash_table_destroy);
  g_clear_pointer (&metacity->colors, g_hash_table_destroy);

  g_clear_pointer (&metacity->draw_op_lists, g_hash_table_destroy);
  g_clear_pointer (&metacity->frame_layouts, g_hash_table_destroy);
  g_clear_pointer (&metacity->styles, g_hash_table_destroy);
  g_clear_pointer (&metacity->style_sets, g_hash_table_destroy);

  G_OBJECT_CLASS (meta_theme_metacity_parent_class)->dispose (object);
}

static void
meta_theme_metacity_finalize (GObject *object)
{
  MetaThemeMetacity *metacity;

  metacity = META_THEME_METACITY (object);

  g_free (metacity->name);
  g_free (metacity->dirname);

  g_free (metacity->readable_name);
  g_free (metacity->author);
  g_free (metacity->copyright);
  g_free (metacity->date);
  g_free (metacity->description);

  G_OBJECT_CLASS (meta_theme_metacity_parent_class)->finalize (object);
}

static gboolean
meta_theme_metacity_load (MetaThemeImpl  *impl,
                          const gchar    *name,
                          GError        **err)
{
  MetaThemeMetacity *metacity;
  gboolean retval;
  GError *error;
  gint version;

  g_return_val_if_fail (err == NULL || *err == NULL, FALSE);

  metacity = META_THEME_METACITY (impl);

  retval = FALSE;
  error = NULL;

  /* We try all supported major versions from current to oldest */
  for (version = THEME_MAJOR_VERSION; version > 0; version--)
    {
      gchar *dir;
      const gchar *const *xdg_data_dirs;
      gint i;

      /* Try XDG_USER_DATA_DIR first */
      dir = get_theme_dir (g_get_user_data_dir(), name);
      retval = load_theme (metacity, dir, name, version, &error);
      g_free (dir);

      if (!keep_trying (&error))
        goto out;

      /* Try each XDG_DATA_DIRS for theme */
      xdg_data_dirs = g_get_system_data_dirs();

      for (i = 0; xdg_data_dirs[i] != NULL; i++)
        {
          dir = get_theme_dir (xdg_data_dirs[i], name);
          retval = load_theme (metacity, dir, name, version, &error);
          g_free (dir);

          if (!keep_trying (&error))
            goto out;
        }

      /* Look for themes in DATADIR */
      dir = get_theme_dir (DATADIR, name);
      retval = load_theme (metacity, dir, name, version, &error);
      g_free (dir);

      if (!keep_trying (&error))
        goto out;
    }

out:

  if (!error && !retval)
    {
      g_set_error (&error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("Failed to find a valid file for theme '%s'"), name);
    }

  if (error)
    g_propagate_error (err, error);

  return retval;
}

static MetaFrameStyle *
meta_theme_metacity_get_frame_style (MetaThemeImpl  *impl,
                                     MetaFrameType   type,
                                     MetaFrameFlags  flags)
{
  MetaThemeMetacity *metacity;
  MetaFrameState state;
  MetaFrameResize resize;
  MetaFrameFocus focus;
  MetaFrameStyle *style;
  MetaFrameStyleSet *style_set;

  g_return_val_if_fail (type < META_FRAME_TYPE_LAST, NULL);

  metacity = META_THEME_METACITY (impl);
  style_set = metacity->style_sets_by_type[type];

  if (style_set == NULL && type == META_FRAME_TYPE_ATTACHED)
    style_set = metacity->style_sets_by_type[META_FRAME_TYPE_BORDER];

  /* Right now the parser forces a style set for all other types,
   * but this fallback code is here in case I take that out.
   */
  if (style_set == NULL)
    style_set = metacity->style_sets_by_type[META_FRAME_TYPE_NORMAL];

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
meta_theme_metacity_get_frame_borders (MetaThemeImpl    *impl,
                                       MetaFrameLayout  *layout,
                                       MetaStyleInfo    *style_info,
                                       gint              text_height,
                                       MetaFrameFlags    flags,
                                       MetaFrameType     type,
                                       MetaFrameBorders *borders)
{
  gint scale;
  gint buttons_height;
  gint title_height;

  meta_frame_borders_clear (borders);

  /* For a full-screen window, we don't have any borders, visible or not. */
  if (flags & META_FRAME_FULLSCREEN)
    return;

  g_return_if_fail (layout != NULL);

  if (!layout->has_title)
    text_height = 0;

  /* Scale geometry for HiDPI, see comment in meta_theme_metacity_draw_frame () */
  scale = meta_theme_impl_get_scale (impl);

  buttons_height = layout->metacity.button_height +
                   layout->button_border.top +
                   layout->button_border.bottom;

  title_height = text_height / scale +
                 layout->metacity.title_vertical_pad +
                 layout->metacity.title_border.top +
                 layout->metacity.title_border.bottom;

  borders->visible.top = MAX (buttons_height, title_height);
  borders->visible.left = layout->metacity.left_width;
  borders->visible.right = layout->metacity.right_width;
  borders->visible.bottom = layout->metacity.bottom_height;

  borders->shadow.top = 0;
  borders->shadow.left = 0;
  borders->shadow.right = 0;
  borders->shadow.bottom = 0;

  if (flags & META_FRAME_ALLOWS_HORIZONTAL_RESIZE)
    {
      borders->resize.left = layout->invisible_resize_border.left;
      borders->resize.right = layout->invisible_resize_border.right;
    }

  if (flags & META_FRAME_ALLOWS_VERTICAL_RESIZE)
    {
      borders->resize.bottom = layout->invisible_resize_border.bottom;

      if (type != META_FRAME_TYPE_ATTACHED)
        borders->resize.top = layout->invisible_resize_border.top;
    }

  borders->invisible.left = MAX (borders->shadow.left, borders->resize.left);
  borders->invisible.right = MAX (borders->shadow.right, borders->resize.right);
  borders->invisible.bottom = MAX (borders->shadow.bottom, borders->resize.bottom);
  borders->invisible.top = MAX (borders->shadow.top, borders->resize.top);

  borders->total.left = borders->invisible.left + borders->visible.left;
  borders->total.right = borders->invisible.right + borders->visible.right;
  borders->total.bottom = borders->invisible.bottom + borders->visible.bottom;
  borders->total.top = borders->invisible.top + borders->visible.top;

  scale_border (&borders->visible, scale);
  scale_border (&borders->shadow, scale);
  scale_border (&borders->resize, scale);
  scale_border (&borders->invisible, scale);
  scale_border (&borders->total, scale);
}

static gboolean
is_button_allowed (MetaThemeMetacity *metacity,
                   MetaButtonType     type)
{
  switch (type)
    {
      case META_BUTTON_TYPE_MENU:
      case META_BUTTON_TYPE_MINIMIZE:
      case META_BUTTON_TYPE_MAXIMIZE:
      case META_BUTTON_TYPE_CLOSE:
      case META_BUTTON_TYPE_SPACER:
        return TRUE;

      case META_BUTTON_TYPE_LAST:
      default:
        break;
    }

  return FALSE;
}

static void
meta_theme_metacity_calc_geometry (MetaThemeImpl     *impl,
                                   MetaFrameLayout   *layout,
                                   MetaStyleInfo     *style_info,
                                   gint               text_height,
                                   MetaFrameFlags     flags,
                                   gint               client_width,
                                   gint               client_height,
                                   MetaButtonLayout  *button_layout,
                                   MetaFrameType      type,
                                   MetaFrameGeometry *fgeom)
{
  MetaFrameBorders borders;
  int i, n_left, n_right, n_left_spacers, n_right_spacers;
  MetaThemeMetacity *metacity;
  int x;
  int button_y;
  int title_right_edge;
  int width, height;
  int button_width, button_height;
  int min_size_for_rounding;
  int scale;

  META_THEME_IMPL_GET_CLASS (impl)->get_frame_borders (impl, layout,
                                                       style_info, text_height,
                                                       flags, type, &borders);

  fgeom->borders = borders;

  width = client_width + borders.total.left + borders.total.right;

  height = ((flags & META_FRAME_SHADED) ? 0: client_height) +
    borders.total.top + borders.total.bottom;

  fgeom->width = width;
  fgeom->height = height;

  /* gcc warnings */
  button_width = -1;
  button_height = -1;

  /* Scale geometry for HiDPI, see comment in meta_theme_metacity_draw_frame () */
  scale = meta_theme_impl_get_scale (impl);

  switch (layout->metacity.button_sizing)
    {
    case META_BUTTON_SIZING_ASPECT:
      button_height = borders.visible.top - layout->button_border.top * scale - layout->button_border.bottom * scale;
      button_width = button_height / layout->metacity.button_aspect;
      break;
    case META_BUTTON_SIZING_FIXED:
      button_width = layout->metacity.button_width * scale;
      button_height = layout->metacity.button_height * scale;
      break;
    case META_BUTTON_SIZING_LAST:
    default:
      g_assert_not_reached ();
      break;
    }

  n_left = 0;
  n_right = 0;
  n_left_spacers = 0;
  n_right_spacers = 0;

  metacity = META_THEME_METACITY (impl);

  if (!layout->hide_buttons)
    {
      MetaButton *button;

      for (i = 0; i < button_layout->n_left_buttons; i++)
        {
          button = &button_layout->left_buttons[i];
          button->visible = is_button_visible (button, flags) &&
                            is_button_allowed (metacity, button->type);

          if (button->visible)
            {
              if (button->type != META_BUTTON_TYPE_SPACER)
                n_left++;
              else
                n_left_spacers++;
            }
        }

      for (i = 0; i < button_layout->n_right_buttons; i++)
        {
          button = &button_layout->right_buttons[i];
          button->visible = is_button_visible (button, flags) &&
                            is_button_allowed (metacity, button->type);

          if (button->visible)
            {
              if (button->type != META_BUTTON_TYPE_SPACER)
                n_right++;
              else
                n_right_spacers++;
            }
        }
    }
  else
    {
      for (i = 0; i < button_layout->n_left_buttons; i++)
        button_layout->left_buttons[i].visible = FALSE;

      for (i = 0; i < button_layout->n_right_buttons; i++)
        button_layout->right_buttons[i].visible = FALSE;
    }

  /* Be sure buttons fit */
  while (n_left > 0 || n_right > 0)
    {
      int space_used_by_buttons;
      int space_available;

      space_available = fgeom->width -
                        borders.invisible.left -
                        layout->metacity.left_titlebar_edge * scale -
                        borders.invisible.right -
                        layout->metacity.right_titlebar_edge * scale;

      space_used_by_buttons = 0;

      space_used_by_buttons += button_width * n_left;
      space_used_by_buttons += (button_width * 0.75) * n_left_spacers;
      space_used_by_buttons += layout->button_border.left * scale * n_left;
      space_used_by_buttons += layout->button_border.right * scale * n_left;

      space_used_by_buttons += button_width * n_right;
      space_used_by_buttons += (button_width * 0.75) * n_right_spacers;
      space_used_by_buttons += layout->button_border.left * scale * n_right;
      space_used_by_buttons += layout->button_border.right * scale * n_right;

      if (space_used_by_buttons <= space_available)
        break; /* Everything fits, bail out */

      /* First try to remove separators */
      if (n_left_spacers > 0)
        {
          if (strip_button (button_layout->left_buttons,
                            button_layout->n_left_buttons,
                            META_BUTTON_TYPE_SPACER))
            {
              n_left_spacers--;
              continue;
            }
          else
            {
              g_assert_not_reached ();
            }
        }
      else if (n_right_spacers > 0)
        {
          if (strip_button (button_layout->right_buttons,
                            button_layout->n_right_buttons,
                            META_BUTTON_TYPE_SPACER))
            {
              n_right_spacers--;
              continue;
            }
          else
            {
              g_assert_not_reached ();
            }
        }

      /* Otherwise we need to shave out a button. Shave
       * above, stick, shade, min, max, close, then menu (menu is most useful);
       * prefer the default button locations.
       */
      if (strip_buttons (button_layout, &n_left, &n_right))
        {
          continue;
        }
      else
        {
          g_error ("Could not find a button to strip. n_left = %d n_right = %d",
                   n_left, n_right);
        }
    }

  /* center buttons vertically */
  button_y = (borders.visible.top -
              (button_height + layout->button_border.top * scale + layout->button_border.bottom * scale)) / 2 + layout->button_border.top * scale + borders.invisible.top;

  /* right edge of farthest-right button */
  x = width - layout->metacity.right_titlebar_edge * scale - borders.invisible.right;

  for (i = button_layout->n_right_buttons - 1; i >= 0; i--)
    {
      MetaButton *button;
      GdkRectangle rect;

      button = &button_layout->right_buttons[i];

      if (button->visible == FALSE)
        continue;

      /* if we go negative, leave the buttons we don't get to as 0 - width */
      if (x < 0)
        break;

      rect.y = button_y;
      rect.width = button_width;
      rect.height = button_height;

      if (button->type == META_BUTTON_TYPE_SPACER)
        {
          rect.x = x - button_width * 0.75;
          rect.width *= 0.75;
        }
      else
        {
          rect.x = x - layout->button_border.right * scale - button_width;
        }

      button->rect.visible = rect;
      button->rect.clickable = rect;

      if ((flags & META_FRAME_MAXIMIZED || flags & META_FRAME_TILED_RIGHT) &&
          i == button_layout->n_right_buttons - 1)
        {
          gint extra_width;
          gint extra_height;

          extra_width = layout->metacity.right_titlebar_edge * scale +
                        layout->metacity.right_width * scale +
                        layout->button_border.right * scale;

          /* FIXME: */
          extra_height = 0;

          button->rect.clickable.y -= extra_height;
          button->rect.clickable.width += extra_width;
          button->rect.clickable.height += extra_height;
        }

      x = rect.x - layout->button_border.left * scale;
    }

  /* save right edge of titlebar for later use */
  title_right_edge = x - layout->metacity.title_border.right * scale;

  /* Now x changes to be position from the left and we go through
   * the left-side buttons
   */
  x = layout->metacity.left_titlebar_edge * scale + borders.invisible.left;

  for (i = 0; i < button_layout->n_left_buttons; i++)
    {
      MetaButton *button;
      GdkRectangle rect;

      button = &button_layout->left_buttons[i];

      if (button->visible == FALSE)
        continue;

      rect.x = x + layout->button_border.left * scale;;
      rect.y = button_y;
      rect.width = button_width;
      rect.height = button_height;

      if (button->type == META_BUTTON_TYPE_SPACER)
        rect.width *= 0.75;

      button->rect.visible = rect;
      button->rect.clickable = rect;

      if ((flags & META_FRAME_MAXIMIZED || flags & META_FRAME_TILED_LEFT) &&
          i == 0)
        {
          gint extra_width;
          gint extra_height;

          extra_width = layout->metacity.left_titlebar_edge * scale +
                        layout->metacity.left_width * scale +
                        layout->button_border.left * scale;

          /* FIXME: */
          extra_height = 0;

          button->rect.clickable.x -= extra_width;
          button->rect.clickable.y -= extra_height;
          button->rect.clickable.width += extra_width;
          button->rect.clickable.height += extra_height;
        }

      x = rect.x + rect.width + layout->button_border.right * scale;
    }

  /* We always fill as much vertical space as possible with title rect,
   * rather than centering it like the buttons
   */
  fgeom->title_rect.x = x + layout->metacity.title_border.left * scale;
  fgeom->title_rect.y = layout->metacity.title_border.top * scale + borders.invisible.top;
  fgeom->title_rect.width = title_right_edge - fgeom->title_rect.x;
  fgeom->title_rect.height = borders.visible.top -
                             layout->metacity.title_border.top * scale -
                             layout->metacity.title_border.bottom * scale;

  /* Nuke title if it won't fit */
  if (fgeom->title_rect.width < 0 ||
      fgeom->title_rect.height < 0)
    {
      fgeom->title_rect.width = 0;
      fgeom->title_rect.height = 0;
    }

  if (flags & META_FRAME_SHADED)
    min_size_for_rounding = 0;
  else
    min_size_for_rounding = 5 * scale;

  fgeom->top_left_corner_rounded_radius = 0;
  fgeom->top_right_corner_rounded_radius = 0;
  fgeom->bottom_left_corner_rounded_radius = 0;
  fgeom->bottom_right_corner_rounded_radius = 0;

  if (borders.visible.top + borders.visible.left >= min_size_for_rounding)
    fgeom->top_left_corner_rounded_radius = layout->top_left_corner_rounded_radius * scale;
  if (borders.visible.top + borders.visible.right >= min_size_for_rounding)
    fgeom->top_right_corner_rounded_radius = layout->top_right_corner_rounded_radius * scale;

  if (borders.visible.bottom + borders.visible.left >= min_size_for_rounding)
    fgeom->bottom_left_corner_rounded_radius = layout->bottom_left_corner_rounded_radius * scale;
  if (borders.visible.bottom + borders.visible.right >= min_size_for_rounding)
    fgeom->bottom_right_corner_rounded_radius = layout->bottom_right_corner_rounded_radius * scale;
}

static void
clip_to_rounded_corners (cairo_t                 *cr,
                         MetaRectangleDouble      rect,
                         const MetaFrameGeometry *fgeom,
                         gint                     scale)
{
  gdouble x;
  gdouble y;
  gdouble width;
  gdouble height;
  gint radius;

  x = rect.x;
  y = rect.y;
  width = rect.width;
  height = rect.height;

  cairo_new_path (cr);

  if (fgeom->top_left_corner_rounded_radius != 0)
    {
      radius = fgeom->top_left_corner_rounded_radius / scale;
      radius += sqrt(fgeom->top_left_corner_rounded_radius / scale);

      cairo_line_to (cr, x, y + radius);
      cairo_arc (cr, x + radius, y + radius, radius,
                 180.0 * G_PI / 180.0, 270.0 * G_PI / 180.0);
    }
  else
    cairo_line_to (cr, x, y);

  if (fgeom->top_right_corner_rounded_radius != 0)
    {
      radius = fgeom->top_right_corner_rounded_radius / scale;
      radius += sqrt(fgeom->top_right_corner_rounded_radius / scale);

      cairo_line_to (cr, x + width - radius, y);
      cairo_arc (cr, x + width - radius, y + radius, radius,
                 -90.0 * G_PI / 180.0, 0.0 * G_PI / 180.0);
    }
  else
    cairo_line_to (cr, x + width, y);

  if (fgeom->bottom_right_corner_rounded_radius != 0)
    {
      radius = fgeom->bottom_right_corner_rounded_radius / scale;
      radius += sqrt(fgeom->bottom_right_corner_rounded_radius / scale);

      cairo_line_to (cr, x + width, y + height - radius);
      cairo_arc (cr, x + width - radius, y + height - radius, radius,
                 0.0 * G_PI / 180.0, 90.0 * G_PI / 180.0);
    }
  else
    cairo_line_to (cr, x + width, y + height);

  if (fgeom->bottom_left_corner_rounded_radius != 0)
    {
      radius = fgeom->bottom_left_corner_rounded_radius / scale;
      radius += sqrt(fgeom->bottom_left_corner_rounded_radius / scale);

      cairo_line_to (cr, x + radius, y + height);
      cairo_arc (cr, x + radius, y + height - radius, radius,
                 90.0 * G_PI / 180.0, 180.0 * G_PI / 180.0);
    }
  else
    cairo_line_to (cr, x, y + height);

  cairo_close_path (cr);
  cairo_clip (cr);
}

static MetaButtonFunction
get_button_function (MetaButtonType type,
                     gboolean       background,
                     gint           button,
                     gint           n_buttons,
                     gint           side)
{
  if (background)
    {
      if (side == 0) /* left */
        {
          if (n_buttons == 1)
            {
              return META_BUTTON_FUNCTION_LEFT_SINGLE_BACKGROUND;
            }
          else if (n_buttons == 2)
            {
              if (button == 0)
                return META_BUTTON_FUNCTION_LEFT_LEFT_BACKGROUND;
              else
                return META_BUTTON_FUNCTION_LEFT_RIGHT_BACKGROUND;
            }
          else if (n_buttons > 2)
            {
              if (button == 0)
                return META_BUTTON_FUNCTION_LEFT_LEFT_BACKGROUND;
              else if (button == n_buttons - 1)
                return META_BUTTON_FUNCTION_LEFT_RIGHT_BACKGROUND;
              else
                return META_BUTTON_FUNCTION_LEFT_MIDDLE_BACKGROUND;
            }
        }
      else if (side == 1) /* right */
        {
          if (n_buttons == 1)
            {
              return META_BUTTON_FUNCTION_RIGHT_SINGLE_BACKGROUND;
            }
          else if (n_buttons == 2)
            {
              if (button == 0)
                return META_BUTTON_FUNCTION_RIGHT_LEFT_BACKGROUND;
              else
                return META_BUTTON_FUNCTION_RIGHT_RIGHT_BACKGROUND;
            }
          else if (n_buttons > 2)
            {
              if (button == 0)
                return META_BUTTON_FUNCTION_RIGHT_LEFT_BACKGROUND;
              else if (button == n_buttons - 1)
                return META_BUTTON_FUNCTION_RIGHT_RIGHT_BACKGROUND;
              else
                return META_BUTTON_FUNCTION_RIGHT_MIDDLE_BACKGROUND;
            }
        }
      else
        {
          g_assert_not_reached ();
        }
    }
  else
    {
      switch (type)
        {
          case META_BUTTON_TYPE_MENU:
            return META_BUTTON_FUNCTION_MENU;

          case META_BUTTON_TYPE_MINIMIZE:
            return META_BUTTON_FUNCTION_MINIMIZE;

          case META_BUTTON_TYPE_MAXIMIZE:
            return META_BUTTON_FUNCTION_MAXIMIZE;

          case META_BUTTON_TYPE_CLOSE:
            return META_BUTTON_FUNCTION_CLOSE;

          case META_BUTTON_TYPE_SPACER:
          case META_BUTTON_TYPE_LAST:
          default:
            break;
        }
    }

  return META_BUTTON_FUNCTION_LAST;
}

static void
meta_theme_metacity_draw_frame (MetaThemeImpl           *impl,
                                MetaFrameStyle          *style,
                                MetaStyleInfo           *style_info,
                                cairo_t                 *cr,
                                const MetaFrameGeometry *fgeom,
                                PangoLayout             *title_layout,
                                MetaFrameFlags           flags,
                                const MetaButtonLayout  *button_layout,
                                GdkPixbuf               *mini_icon,
                                GdkPixbuf               *icon)
{
  gdouble scale;
  gint i;
  MetaRectangleDouble visible_rect;
  MetaRectangleDouble titlebar_rect;
  MetaRectangleDouble left_titlebar_edge;
  MetaRectangleDouble right_titlebar_edge;
  MetaRectangleDouble bottom_titlebar_edge;
  MetaRectangleDouble top_titlebar_edge;
  MetaRectangleDouble left_edge;
  MetaRectangleDouble right_edge;
  MetaRectangleDouble bottom_edge;
  PangoRectangle extents;
  MetaDrawInfo draw_info;
  const MetaFrameBorders *borders;
  GtkStyleContext *context;

  /* We opt out of GTK+ HiDPI handling, so we have to do the scaling
   * ourselves; the nitty-gritty is a bit confusing, so here is an overview:
   *  - the values in MetaFrameLayout are always as they appear in the theme,
   *    i.e. unscaled
   *  - calculated values (borders, MetaFrameGeometry) include the scale - as
   *    the geometry is comprised of scaled decorations and the client size
   *    which we must not scale, we don't have another option
   *  - for drawing, we scale the canvas to have GTK+ render elements (borders,
   *    radii, ...) at the correct scale - as a result, we have to "unscale"
   *    the geometry again to not apply the scaling twice
   */
  scale = meta_theme_impl_get_scale (impl);
  cairo_scale (cr, scale, scale);

  borders = &fgeom->borders;

  visible_rect.x = borders->invisible.left / scale;
  visible_rect.y = borders->invisible.top / scale;
  visible_rect.width = (fgeom->width - borders->invisible.left - borders->invisible.right) / scale;
  visible_rect.height = (fgeom->height - borders->invisible.top - borders->invisible.bottom) / scale;

  titlebar_rect.x = visible_rect.x;
  titlebar_rect.y = visible_rect.y;
  titlebar_rect.width = visible_rect.width;
  titlebar_rect.height = borders->visible.top / scale;

  left_titlebar_edge.x = titlebar_rect.x;
  left_titlebar_edge.y = titlebar_rect.y + style->layout->metacity.title_border.top;
  left_titlebar_edge.width = style->layout->metacity.title_border.left;
  left_titlebar_edge.height = titlebar_rect.height - style->layout->metacity.title_border.top -
                              style->layout->metacity.title_border.bottom;

  right_titlebar_edge.y = left_titlebar_edge.y;
  right_titlebar_edge.height = left_titlebar_edge.height;
  right_titlebar_edge.width = style->layout->metacity.title_border.right;
  right_titlebar_edge.x = titlebar_rect.x + titlebar_rect.width - right_titlebar_edge.width;

  top_titlebar_edge.x = titlebar_rect.x;
  top_titlebar_edge.y = titlebar_rect.y;
  top_titlebar_edge.width = titlebar_rect.width;
  top_titlebar_edge.height = style->layout->metacity.title_border.top;

  bottom_titlebar_edge.x = titlebar_rect.x;
  bottom_titlebar_edge.width = titlebar_rect.width;
  bottom_titlebar_edge.height = style->layout->metacity.title_border.bottom;
  bottom_titlebar_edge.y = titlebar_rect.y + titlebar_rect.height - bottom_titlebar_edge.height;

  left_edge.x = visible_rect.x;
  left_edge.y = visible_rect.y + borders->visible.top / scale;
  left_edge.width = borders->visible.left / scale;
  left_edge.height = visible_rect.height - borders->visible.top / scale - borders->visible.bottom / scale;

  right_edge.x = visible_rect.x + visible_rect.width - borders->visible.right / scale;
  right_edge.y = visible_rect.y + borders->visible.top / scale;
  right_edge.width = borders->visible.right / scale;
  right_edge.height = visible_rect.height - borders->visible.top / scale - borders->visible.bottom / scale;

  bottom_edge.x = visible_rect.x;
  bottom_edge.y = visible_rect.y + visible_rect.height - borders->visible.bottom / scale;
  bottom_edge.width = visible_rect.width;
  bottom_edge.height = borders->visible.bottom / scale;

  if (title_layout)
    pango_layout_get_pixel_extents (title_layout,
                                    NULL, &extents);

  draw_info.scale = scale;

  draw_info.mini_icon = mini_icon;
  draw_info.icon = icon;
  draw_info.title_layout = title_layout;
  draw_info.title_layout_width = title_layout ? extents.width : 0;
  draw_info.title_layout_height = title_layout ? extents.height : 0;

  draw_info.left_width = borders->visible.left / scale;
  draw_info.right_width = borders->visible.right / scale;
  draw_info.top_height = borders->visible.top / scale;
  draw_info.bottom_height = borders->visible.bottom / scale;

  draw_info.width = fgeom->width / scale;
  draw_info.height = fgeom->height / scale;

  context = meta_style_info_get_style (style_info, META_STYLE_ELEMENT_WINDOW);

  /* The enum is in the order the pieces should be rendered. */
  i = 0;
  while (i < META_FRAME_PIECE_LAST)
    {
      MetaRectangleDouble rect;

      switch ((MetaFramePiece) i)
        {
        case META_FRAME_PIECE_ENTIRE_BACKGROUND:
          rect = visible_rect;
          break;

        case META_FRAME_PIECE_TITLEBAR:
          rect = titlebar_rect;
          break;

        case META_FRAME_PIECE_LEFT_TITLEBAR_EDGE:
          rect = left_titlebar_edge;
          break;

        case META_FRAME_PIECE_RIGHT_TITLEBAR_EDGE:
          rect = right_titlebar_edge;
          break;

        case META_FRAME_PIECE_TOP_TITLEBAR_EDGE:
          rect = top_titlebar_edge;
          break;

        case META_FRAME_PIECE_BOTTOM_TITLEBAR_EDGE:
          rect = bottom_titlebar_edge;
          break;

        case META_FRAME_PIECE_TITLEBAR_MIDDLE:
          rect.x = left_titlebar_edge.x + left_titlebar_edge.width;
          rect.y = top_titlebar_edge.y + top_titlebar_edge.height;
          rect.width = titlebar_rect.width - left_titlebar_edge.width - right_titlebar_edge.width;
          rect.height = titlebar_rect.height - top_titlebar_edge.height - bottom_titlebar_edge.height;
          break;

        case META_FRAME_PIECE_TITLE:
          rect.x = fgeom->title_rect.x / scale;
          rect.y = fgeom->title_rect.y / scale;
          rect.width = fgeom->title_rect.width / scale;
          rect.height = fgeom->title_rect.height / scale;
          break;

        case META_FRAME_PIECE_LEFT_EDGE:
          rect = left_edge;
          break;

        case META_FRAME_PIECE_RIGHT_EDGE:
          rect = right_edge;
          break;

        case META_FRAME_PIECE_BOTTOM_EDGE:
          rect = bottom_edge;
          break;

        case META_FRAME_PIECE_OVERLAY:
          rect = visible_rect;
          break;

        case META_FRAME_PIECE_LAST:
        default:
          g_assert_not_reached ();
          break;
        }

      cairo_save (cr);

      if (i == META_FRAME_PIECE_ENTIRE_BACKGROUND)
        {
          clip_to_rounded_corners (cr, rect, fgeom, scale);

          if (style->window_background_color != NULL)
            {
              GdkRGBA color;

              meta_color_spec_render (style->window_background_color, context, &color);

              if (meta_theme_impl_get_composited (impl))
                color.alpha = style->window_background_alpha / 255.0;

              gdk_cairo_set_source_rgba (cr, &color);
              cairo_paint (cr);
            }
        }
      else
        {
          cairo_rectangle (cr, rect.x, rect.y, rect.width, rect.height);
          cairo_clip (cr);
        }

      if (gdk_cairo_get_clip_rectangle (cr, NULL))
        {
          MetaDrawOpList *op_list;
          MetaFrameStyle *parent;

          parent = style;
          op_list = NULL;
          while (parent && op_list == NULL)
            {
              op_list = parent->pieces[i];
              parent = parent->parent;
            }

          if (op_list)
            {
              meta_draw_op_list_draw_with_style (op_list, context, cr,
                                                 &draw_info, rect);
            }
        }

      cairo_restore (cr);

      /* Draw buttons just before overlay */
      if ((i + 1) == META_FRAME_PIECE_OVERLAY)
        {
          gint side;

          for (side = 0; side < 2; side++)
            {
              MetaButton *buttons;
              gint n_buttons;
              gint j;

              if (side == 0)
                {
                  buttons = button_layout->left_buttons;
                  n_buttons = button_layout->n_left_buttons;
                }
              else if (side == 1)
                {
                  buttons = button_layout->right_buttons;
                  n_buttons = button_layout->n_right_buttons;
                }
              else
                {
                  g_assert_not_reached ();
                }

              for (j = 0; j < n_buttons; j++)
                {
                  MetaButton *button;
                  gint op;

                  button = &buttons[j];

                  rect.x = button->rect.visible.x / scale;
                  rect.y = button->rect.visible.y / scale;
                  rect.width = button->rect.visible.width / scale;
                  rect.height = button->rect.visible.height / scale;

                  if (!button->visible ||
                      button->type == META_BUTTON_TYPE_SPACER ||
                      rect.width <= 0 || rect.height <= 0)
                    {
                      continue;
                    }

                  for (op = 0; op < 2; op++)
                    {
                      MetaButtonFunction function;
                      MetaDrawOpList *op_list;

                      function = get_button_function (button->type, op == 0,
                                                      j, n_buttons, op);

                      op_list = meta_frame_style_get_button (style, function,
                                                             button->state);

                      if (op_list)
                        {
                          cairo_save (cr);

                          cairo_rectangle (cr, rect.x, rect.y,
                                           rect.width, rect.height);
                          cairo_clip (cr);

                          if (gdk_cairo_get_clip_rectangle (cr, NULL))
                            {
                              meta_draw_op_list_draw_with_style (op_list,
                                                                 context, cr,
                                                                 &draw_info,
                                                                 rect);
                            }

                          cairo_restore (cr);
                        }
                    }
                }
            }
        }

      ++i;
    }
}

static void
meta_theme_metacity_class_init (MetaThemeMetacityClass *metacity_class)
{
  GObjectClass *object_class;
  MetaThemeImplClass *impl_class;

  object_class = G_OBJECT_CLASS (metacity_class);
  impl_class = META_THEME_IMPL_CLASS (metacity_class);

  object_class->dispose = meta_theme_metacity_dispose;
  object_class->finalize = meta_theme_metacity_finalize;

  impl_class->load = meta_theme_metacity_load;
  impl_class->get_frame_style = meta_theme_metacity_get_frame_style;
  impl_class->get_frame_borders = meta_theme_metacity_get_frame_borders;
  impl_class->calc_geometry = meta_theme_metacity_calc_geometry;
  impl_class->draw_frame = meta_theme_metacity_draw_frame;
}

static void
meta_theme_metacity_init (MetaThemeMetacity *metacity)
{
  metacity->draw_op_lists = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                                   (GDestroyNotify) meta_draw_op_list_unref);

  metacity->frame_layouts = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                                   (GDestroyNotify) meta_frame_layout_unref);

  metacity->styles = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                            (GDestroyNotify) meta_frame_style_unref);

  metacity->style_sets = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                                (GDestroyNotify) meta_frame_style_set_unref);

  metacity->images = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                            (GDestroyNotify) g_object_unref);
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

MetaDrawOpList *
meta_theme_metacity_lookup_draw_op_list (MetaThemeMetacity *metacity,
                                         const gchar       *name)
{
  return g_hash_table_lookup (metacity->draw_op_lists, name);
}

MetaFrameLayout *
meta_theme_metacity_lookup_layout (MetaThemeMetacity *metacity,
                                   const gchar       *name)
{
  return g_hash_table_lookup (metacity->frame_layouts, name);
}

MetaFrameStyle *
meta_theme_metacity_lookup_style (MetaThemeMetacity *metacity,
                                  const gchar       *name)
{
  return g_hash_table_lookup (metacity->styles, name);
}

MetaFrameStyleSet *
meta_theme_metacity_lookup_style_set (MetaThemeMetacity *metacity,
                                      const gchar       *name)
{
  return g_hash_table_lookup (metacity->style_sets, name);
}

/**
 * Returns the earliest version of the theme format which required support
 * for a particular button.  (For example, "shade" first appeared in v2, and
 * "close" in v1.)
 *
 * \param type  the button type
 * \return  the number of the theme format
 */
guint
meta_theme_metacity_earliest_version_with_button (MetaButtonFunction function)
{
  switch (function)
    {
      case META_BUTTON_FUNCTION_CLOSE:
      case META_BUTTON_FUNCTION_MAXIMIZE:
      case META_BUTTON_FUNCTION_MINIMIZE:
      case META_BUTTON_FUNCTION_MENU:
      case META_BUTTON_FUNCTION_LEFT_LEFT_BACKGROUND:
      case META_BUTTON_FUNCTION_LEFT_MIDDLE_BACKGROUND:
      case META_BUTTON_FUNCTION_LEFT_RIGHT_BACKGROUND:
      case META_BUTTON_FUNCTION_RIGHT_LEFT_BACKGROUND:
      case META_BUTTON_FUNCTION_RIGHT_MIDDLE_BACKGROUND:
      case META_BUTTON_FUNCTION_RIGHT_RIGHT_BACKGROUND:
        return 1000;

      case META_BUTTON_FUNCTION_SHADE:
      case META_BUTTON_FUNCTION_ABOVE:
      case META_BUTTON_FUNCTION_STICK:
      case META_BUTTON_FUNCTION_UNSHADE:
      case META_BUTTON_FUNCTION_UNABOVE:
      case META_BUTTON_FUNCTION_UNSTICK:
        return 2000;

      case META_BUTTON_FUNCTION_LEFT_SINGLE_BACKGROUND:
      case META_BUTTON_FUNCTION_RIGHT_SINGLE_BACKGROUND:
        return 3003;

      case META_BUTTON_FUNCTION_LAST:
      default:
        g_warning ("Unknown button %d", (gint) function);
        break;
    }

  return 1000;
}
