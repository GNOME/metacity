/*
 * Copyright (C) 2001 Havoc Pennington, 99% copied from wrlib in
 * WindowMaker, Copyright (C) 1997-2000 Dan Pascu and Alfredo Kojima
 * Copyright (C) 2005 Elijah Newren
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION: gradient
 * @title: Gradients
 * @short_description: Metacity gradient rendering
 */

#include "config.h"

#include <string.h>

#include "meta-gradient-private.h"

static void
simple_multiply_alpha (GdkPixbuf *pixbuf,
                       guchar     alpha)
{
  guchar *pixels;
  int rowstride;
  int height;
  int row;

  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

  if (alpha == 255)
    return;

  g_assert (gdk_pixbuf_get_has_alpha (pixbuf));

  pixels = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  row = 0;
  while (row < height)
    {
      guchar *p;
      guchar *end;

      p = pixels + row * rowstride;
      end = p + rowstride;

      while (p != end)
        {
          p += 3; /* skip RGB */

          /* multiply the two alpha channels. not sure this is right.
           * but some end cases are that if the pixbuf contains 255,
           * then it should be modified to contain "alpha"; if the
           * pixbuf contains 0, it should remain 0.
           */
          /* ((*p / 255.0) * (alpha / 255.0)) * 255; */
          *p = (guchar) (((int) *p * (int) alpha) / (int) 255);

          ++p; /* skip A */
        }

      ++row;
    }
}

static void
meta_gradient_add_alpha_horizontal (GdkPixbuf           *pixbuf,
                                    const unsigned char *alphas,
                                    int                  n_alphas)
{
  int i, j;
  long a, da;
  unsigned char *p;
  unsigned char *pixels;
  int width2;
  int rowstride;
  int width, height;
  unsigned char *gradient;
  unsigned char *gradient_p;
  unsigned char *gradient_end;

  g_return_if_fail (n_alphas > 0);

  if (n_alphas == 1)
    {
      /* Optimize this */
      simple_multiply_alpha (pixbuf, alphas[0]);
      return;
    }

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  gradient = g_new (unsigned char, width);
  gradient_end = gradient + width;

  if (n_alphas > width)
    n_alphas = width;

  if (n_alphas > 1)
    width2 = width / (n_alphas - 1);
  else
    width2 = width;

  a = alphas[0] << 8;
  gradient_p = gradient;

  /* render the gradient into an array */
  for (i = 1; i < n_alphas; i++)
    {
      da = (((int)(alphas[i] - (int) alphas[i-1])) << 8) / (int) width2;

      for (j = 0; j < width2; j++)
        {
          *gradient_p++ = (a >> 8);

          a += da;
	}

      a = alphas[i] << 8;
    }

  /* get leftover pixels */
  while (gradient_p != gradient_end)
    {
      *gradient_p++ = a >> 8;
    }

  /* Now for each line of the pixbuf, fill in with the gradient */
  pixels = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);

  p = pixels;
  i = 0;
  while (i < height)
    {
      unsigned char *row_end = p + rowstride;
      gradient_p = gradient;

      p += 3;
      while (gradient_p != gradient_end)
        {
          /* multiply the two alpha channels. not sure this is right.
           * but some end cases are that if the pixbuf contains 255,
           * then it should be modified to contain "alpha"; if the
           * pixbuf contains 0, it should remain 0.
           */
          /* ((*p / 255.0) * (alpha / 255.0)) * 255; */
          *p = (guchar) (((int) *p * (int) *gradient_p) / (int) 255);

          p += 4;
          ++gradient_p;
        }

      p = row_end;
      ++i;
    }

  g_free (gradient);
}

/**
 * meta_gradient_add_alpha:
 * @pixbuf:
 * @alphas:
 * @n_alphas:
 * @type:
 *
 * Generate an alpha gradient and multiply it with the existing alpha
 * channel of the given pixbuf.
 */
void
meta_gradient_add_alpha (GdkPixbuf        *pixbuf,
                         const guchar     *alphas,
                         int               n_alphas,
                         MetaGradientType  type)
{
  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));
  g_return_if_fail (gdk_pixbuf_get_has_alpha (pixbuf));
  g_return_if_fail (n_alphas > 0);

  switch (type)
    {
      case META_GRADIENT_HORIZONTAL:
        meta_gradient_add_alpha_horizontal (pixbuf, alphas, n_alphas);
        break;

      case META_GRADIENT_VERTICAL:
        g_printerr ("metacity: vertical alpha channel gradient not implemented yet\n");
        break;

      case META_GRADIENT_DIAGONAL:
        g_printerr ("metacity: diagonal alpha channel gradient not implemented yet\n");
        break;

      case META_GRADIENT_LAST:
        g_assert_not_reached ();
        break;

      default:
        g_assert_not_reached ();
        break;
    }
}
