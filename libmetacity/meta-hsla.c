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

/**
 * meta_hsla_from_rgba:
 * @hsla: (out): location to store #MetaHSLA color
 * @rgba: #GdkRGBA color
 *
 * Convert RGBA color to HSLA color.
 */
void
meta_hsla_from_rgba (MetaHSLA      *hsla,
                     const GdkRGBA *rgba)
{
  gdouble min;
  gdouble max;
  gdouble red;
  gdouble green;
  gdouble blue;
  gdouble delta;

  g_return_if_fail (hsla != NULL || rgba != NULL);

  red = rgba->red;
  green = rgba->green;
  blue = rgba->blue;

  if (red > green)
    {
      if (red > blue)
        max = red;
      else
        max = blue;

      if (green < blue)
        min = green;
      else
        min = blue;
    }
  else
    {
      if (green > blue)
        max = green;
      else
        max = blue;

      if (red < blue)
        min = red;
      else
        min = blue;
    }

  hsla->hue = 0;
  hsla->saturation = 0;
  hsla->lightness = (max + min) / 2;
  hsla->alpha = rgba->alpha;

  if (max != min)
    {
      if (hsla->lightness <= 0.5)
        hsla->saturation = (max - min) / (max + min);
      else
        hsla->saturation = (max - min) / (2 - max - min);

      delta = max - min;

      if (red == max)
        hsla->hue = (green - blue) / delta;
      else if (green == max)
        hsla->hue = 2 + (blue - red) / delta;
      else if (blue == max)
        hsla->hue = 4 + (red - green) / delta;

      hsla->hue *= 60;

      if (hsla->hue < 0.0)
        hsla->hue += 360;
    }
}

/**
 * meta_hsla_to_rgba:
 * @hsla: #MetaHSLA color
 * @rgba: (out): location to store #GdkRGBA color
 *
 * Convert HSLA color to RGBA color.
 */
void
meta_hsla_to_rgba (const MetaHSLA *hsla,
                   GdkRGBA        *rgba)
{
  gdouble hue;
  gdouble saturation;
  gdouble lightness;
  gdouble m1;
  gdouble m2;

  g_return_if_fail (hsla != NULL || rgba != NULL);

  saturation = hsla->saturation;
  lightness = hsla->lightness;

  if (lightness <= 0.5)
    m2 = lightness * (1 + saturation);
  else
    m2 = lightness + saturation - lightness * saturation;

  m1 = 2 * lightness - m2;

  if (saturation == 0)
    {
      rgba->red = lightness;
      rgba->green = lightness;
      rgba->blue = lightness;
    }
  else
    {
      hue = hsla->hue + 120;

      while (hue > 360)
        hue -= 360;

      while (hue < 0)
        hue += 360;

      if (hue < 60)
        rgba->red = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        rgba->red = m2;
      else if (hue < 240)
        rgba->red = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        rgba->red = m1;

      hue = hsla->hue;

      while (hue > 360)
        hue -= 360;

      while (hue < 0)
        hue += 360;

      if (hue < 60)
        rgba->green = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        rgba->green = m2;
      else if (hue < 240)
        rgba->green = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        rgba->green = m1;

      hue = hsla->hue - 120;

      while (hue > 360)
        hue -= 360;

      while (hue < 0)
        hue += 360;

      if (hue < 60)
        rgba->blue = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        rgba->blue = m2;
      else if (hue < 240)
        rgba->blue = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        rgba->blue = m1;
    }

  rgba->alpha = hsla->alpha;
}

/**
 * meta_hsla_shade:
 * @source: #MetaHSLA color
 * @factor: amount to scale saturation and lightness
 * @destination: (out): location to store #MetaHSLA color
 *
 * Takes a @source color, scales saturation and lightness by @factor and
 * sets @destionation to the resulting color.
 */
void
meta_hsla_shade (const MetaHSLA *source,
                 const gdouble   factor,
                 MetaHSLA       *destination)
{
  g_return_if_fail (source != NULL || destination != NULL);

  destination->hue = source->hue;

  destination->saturation = source->saturation * factor;
  destination->saturation = CLAMP (destination->saturation, 0.0, 1.0);

  destination->lightness = source->lightness * factor;
  destination->lightness = CLAMP (destination->lightness, 0.0, 1.0);

  destination->alpha = source->alpha;
}
