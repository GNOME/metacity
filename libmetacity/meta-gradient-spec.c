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

#include "meta-color-spec-private.h"
#include "meta-gradient-spec-private.h"
#include "meta-theme.h"

struct _MetaGradientSpec
{
  MetaGradientType  type;
  GSList           *color_specs;
};

struct _MetaAlphaGradientSpec
{
  MetaGradientType  type;
  guchar           *alphas;
  gint              n_alphas;
};


static cairo_pattern_t *
create_cairo_pattern_from_gradient_spec (const MetaGradientSpec      *spec,
                                         const MetaAlphaGradientSpec *alpha_spec,
                                         GtkStyleContext             *context)
{
  gint n_colors;
  cairo_pattern_t *pattern;
  GSList *tmp;
  gint i;

  n_colors = g_slist_length (spec->color_specs);
  if (n_colors == 0)
    return NULL;

  if (alpha_spec != NULL && alpha_spec->n_alphas != 1)
    g_assert (n_colors == alpha_spec->n_alphas);

  if (spec->type == META_GRADIENT_HORIZONTAL)
    pattern = cairo_pattern_create_linear (0, 0, 1, 0);
  else if (spec->type == META_GRADIENT_VERTICAL)
    pattern = cairo_pattern_create_linear (0, 0, 0, 1);
  else if (spec->type == META_GRADIENT_DIAGONAL)
    pattern = cairo_pattern_create_linear (0, 0, 1, 1);
  else
    g_assert_not_reached ();

  i = 0;
  tmp = spec->color_specs;
  while (tmp != NULL)
    {
      GdkRGBA color;

      meta_color_spec_render (tmp->data, context, &color);

      if (alpha_spec != NULL)
        {
          gdouble alpha;

          if (alpha_spec->n_alphas == 1)
            alpha = alpha_spec->alphas[0] / 255.0;
          else
            alpha = alpha_spec->alphas[i] / 255.0;

          cairo_pattern_add_color_stop_rgba (pattern, i / (gfloat) (n_colors - 1),
                                             color.red, color.green, color.blue,
                                             alpha);
        }
      else
        cairo_pattern_add_color_stop_rgb (pattern, i / (gfloat) (n_colors - 1),
                                          color.red, color.green, color.blue);

      tmp = tmp->next;
      ++i;
    }

  if (cairo_pattern_status (pattern) != CAIRO_STATUS_SUCCESS)
    {
      cairo_pattern_destroy (pattern);
      return NULL;
    }

  return pattern;
}

static void
free_color_spec (gpointer spec,
                 gpointer user_data)
{
  meta_color_spec_free (spec);
}

MetaGradientSpec *
meta_gradient_spec_new (MetaGradientType type)
{
  MetaGradientSpec *spec;

  spec = g_new (MetaGradientSpec, 1);

  spec->type = type;
  spec->color_specs = NULL;

  return spec;
}

void
meta_gradient_spec_free (MetaGradientSpec *spec)
{
  g_return_if_fail (spec != NULL);

  g_slist_foreach (spec->color_specs, free_color_spec, NULL);
  g_slist_free (spec->color_specs);

  g_free (spec);
}

void
meta_gradient_spec_add_color_spec (MetaGradientSpec *spec,
                                   MetaColorSpec    *color_spec)
{
  spec->color_specs = g_slist_append (spec->color_specs, color_spec);
}

void
meta_gradient_spec_render (const MetaGradientSpec      *spec,
                           const MetaAlphaGradientSpec *alpha_spec,
                           cairo_t                     *cr,
                           GtkStyleContext             *context,
                           gdouble                      x,
                           gdouble                      y,
                           gdouble                      width,
                           gdouble                      height)
{
  cairo_pattern_t *pattern;

  pattern = create_cairo_pattern_from_gradient_spec (spec, alpha_spec, context);
  if (pattern == NULL)
    return;

  cairo_save (cr);

  cairo_rectangle (cr, x, y, width, height);

  cairo_translate (cr, x, y);
  cairo_scale (cr, width, height);

  cairo_set_source (cr, pattern);
  cairo_fill (cr);
  cairo_pattern_destroy (pattern);

  cairo_restore (cr);
}

gboolean
meta_gradient_spec_validate (MetaGradientSpec  *spec,
                             GError           **error)
{
  g_return_val_if_fail (spec != NULL, FALSE);

  if (g_slist_length (spec->color_specs) < 2)
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("Gradients should have at least two colors"));

      return FALSE;
    }

  return TRUE;
}

MetaAlphaGradientSpec *
meta_alpha_gradient_spec_new (MetaGradientType type,
                              gint             n_alphas)
{
  MetaAlphaGradientSpec *spec;

  g_return_val_if_fail (n_alphas > 0, NULL);

  spec = g_new0 (MetaAlphaGradientSpec, 1);

  spec->type = type;
  spec->alphas = g_new0 (guchar, n_alphas);
  spec->n_alphas = n_alphas;

  return spec;
}

void
meta_alpha_gradient_spec_free (MetaAlphaGradientSpec *spec)
{
  g_return_if_fail (spec != NULL);

  g_free (spec->alphas);
  g_free (spec);
}

void
meta_alpha_gradient_spec_add_alpha (MetaAlphaGradientSpec *spec,
                                    gint                   n_alpha,
                                    gdouble                alpha)
{
  spec->alphas[n_alpha] = (guchar) (alpha * 255);
}

guchar
meta_alpha_gradient_spec_get_alpha (MetaAlphaGradientSpec *spec,
                                    gint                   n_alpha)
{
  return spec->alphas[n_alpha];
}

void
meta_alpha_gradient_spec_render (MetaAlphaGradientSpec *spec,
                                 GdkRGBA                color,
                                 cairo_t               *cr,
                                 gdouble                x,
                                 gdouble                y,
                                 gdouble                width,
                                 gdouble                height)
{
  if (!spec || spec->n_alphas == 1)
    {
      if (spec)
        color.alpha = spec->alphas[0] / 255.0;

      gdk_cairo_set_source_rgba (cr, &color);
      cairo_rectangle (cr, x, y, width, height);
      cairo_fill (cr);
    }
  else
    {
      cairo_pattern_t *pattern;
      gint n_alphas;
      gint i;

      /* Hardcoded in meta-theme-metacity.c */
      g_assert (spec->type == META_GRADIENT_HORIZONTAL);

      pattern = cairo_pattern_create_linear (0, 0, 1, 0);
      n_alphas = spec->n_alphas;

      for (i = 0; i < n_alphas; i++)
        cairo_pattern_add_color_stop_rgba (pattern, i / (gfloat) (n_alphas - 1),
                                           color.red, color.green, color.blue,
                                           spec->alphas[i] / 255.0);

      if (cairo_pattern_status (pattern) != CAIRO_STATUS_SUCCESS)
        {
          cairo_pattern_destroy (pattern);
          return;
        }

      cairo_save (cr);
      cairo_rectangle (cr, x, y, width, height);

      cairo_translate (cr, x, y);
      cairo_scale (cr, width, height);

      cairo_set_source (cr, pattern);
      cairo_fill (cr);

      cairo_pattern_destroy (pattern);
      cairo_restore (cr);
    }
}

cairo_pattern_t *
meta_alpha_gradient_spec_get_mask (const MetaAlphaGradientSpec *spec)
{
  gint n_alphas;
  cairo_pattern_t *pattern;
  gint i;

  /* Hardcoded in meta-theme-metacity.c */
  g_assert (spec->type == META_GRADIENT_HORIZONTAL);

  n_alphas = spec->n_alphas;
  if (n_alphas == 0)
    return NULL;

  if (n_alphas == 1)
    return cairo_pattern_create_rgba (0, 0, 0, spec->alphas[0] / 255.0);

  pattern = cairo_pattern_create_linear (0, 0, 1, 0);

  for (i = 0; i < n_alphas; i++)
    cairo_pattern_add_color_stop_rgba (pattern, i / (gfloat) (n_alphas - 1),
                                       0, 0, 0, spec->alphas[i] / 255.0);

  if (cairo_pattern_status (pattern) != CAIRO_STATUS_SUCCESS)
    {
      cairo_pattern_destroy (pattern);
      return NULL;
    }

  return pattern;
}
