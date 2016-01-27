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

#include "meta-hsla-private.h"
#include "meta-color-private.h"
#include "meta-color.h"

#define LIGHTNESS_MULT 1.3
#define DARKNESS_MULT 0.7

/**
 * meta_color_shade:
 * @source: #GdkRGBA color
 * @factor: amount to scale saturation and lightness
 * @destination: (out): location to store #GdkRGBA color
 *
 * Takes a @source color, scales saturation and lightness by @factor and
 * sets @destionation to the resulting color.
 */
void
meta_color_shade (const GdkRGBA *source,
                  const gdouble  factor,
                  GdkRGBA       *destination)
{
  MetaHSLA hsla;

  meta_hsla_from_rgba (&hsla, source);
  meta_hsla_shade (&hsla, factor, &hsla);
  meta_hsla_to_rgba (&hsla, destination);
}

/**
 * meta_color_get_background_color:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the color for
 * @color: (out): location to store the background color
 *
 * Gets the background color for a given state.
 */
void
meta_color_get_background_color (GtkStyleContext *context,
                                 GtkStateFlags    state,
                                 GdkRGBA         *color)
{
  GdkRGBA *tmp;

  g_return_if_fail (color != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_style_context_get (context, state, "background-color", &tmp, NULL);

  *color = *tmp;

  gdk_rgba_free (tmp);
}

/**
 * meta_color_get_light_color:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the color for
 * @color: (out): location to store the light color
 *
 * Gets the light color of background color for a given state.
 */
void
meta_color_get_light_color (GtkStyleContext *context,
                            GtkStateFlags    state,
                            GdkRGBA         *color)
{
  meta_color_get_background_color (context, state, color);
  meta_color_shade (color, LIGHTNESS_MULT, color);
}

/**
 * meta_color_get_dark_color:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the color for
 * @color: (out): location to store the dark color
 *
 * Gets the dark color of background color for a given state.
 */
void
meta_color_get_dark_color (GtkStyleContext *context,
                           GtkStateFlags    state,
                           GdkRGBA         *color)
{
  meta_color_get_background_color (context, state, color);
  meta_color_shade (color, DARKNESS_MULT, color);
}
