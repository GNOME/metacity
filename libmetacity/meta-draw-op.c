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

#include "meta-draw-op-private.h"
#include "meta-theme-impl-private.h"

#define GDK_COLOR_RGBA(color)                            \
        ((guint32) (0xff                               | \
                    ((int)((color).red * 255) << 24)   | \
                    ((int)((color).green * 255) << 16) | \
                    ((int)((color).blue * 255) << 8)))

#define GDK_COLOR_RGB(color)                             \
        ((guint32) (((int)((color).red * 255) << 16)   | \
                    ((int)((color).green * 255) << 8)  | \
                    ((int)((color).blue * 255))))

#define CLAMP_UCHAR(v) ((guchar) (CLAMP (((int)v), (int)0, (int)255)))
#define INTENSITY(r, g, b) ((r) * 0.30 + (g) * 0.59 + (b) * 0.11)

/**
 * A list of MetaDrawOp objects. Maintains a reference count.
 * Grows as necessary and allows the allocation of unused spaces
 * to keep reallocations to a minimum.
 *
 * \bug Do we really win anything from not using the equivalent
 * GLib structures?
 */
struct _MetaDrawOpList
{
  int refcount;
  MetaDrawOp **ops;
  int n_ops;
  int n_allocated;
};

static void
fill_env (MetaPositionExprEnv *env,
          const MetaDrawInfo  *info,
          GdkRectangle         logical_region)
{
  /* FIXME this stuff could be raised into draw_op_list_draw() probably
   */
  env->rect = logical_region;
  env->object_width = -1;
  env->object_height = -1;

  env->left_width = info->left_width;
  env->right_width = info->right_width;
  env->top_height = info->top_height;
  env->bottom_height = info->bottom_height;
  env->frame_x_center = info->width / 2 - logical_region.x;
  env->frame_y_center = info->height / 2 - logical_region.y;

  env->mini_icon_width = info->mini_icon ? gdk_pixbuf_get_width (info->mini_icon) : 0;
  env->mini_icon_height = info->mini_icon ? gdk_pixbuf_get_height (info->mini_icon) : 0;
  env->icon_width = info->icon ? gdk_pixbuf_get_width (info->icon) : 0;
  env->icon_height = info->icon ? gdk_pixbuf_get_height (info->icon) : 0;

  env->title_width = info->title_layout_width;
  env->title_height = info->title_layout_height;
}

static GdkPixbuf*
pixbuf_tile (GdkPixbuf *tile,
             int        width,
             int        height)
{
  GdkPixbuf *pixbuf;
  int tile_width;
  int tile_height;
  int i, j;

  tile_width = gdk_pixbuf_get_width (tile);
  tile_height = gdk_pixbuf_get_height (tile);

  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                           gdk_pixbuf_get_has_alpha (tile),
                           8, width, height);

  i = 0;
  while (i < width)
    {
      j = 0;
      while (j < height)
        {
          int w, h;

          w = MIN (tile_width, width - i);
          h = MIN (tile_height, height - j);

          gdk_pixbuf_copy_area (tile,
                                0, 0,
                                w, h,
                                pixbuf,
                                i, j);

          j += tile_height;
        }

      i += tile_width;
    }

  return pixbuf;
}

static GdkPixbuf *
replicate_rows (GdkPixbuf  *src,
                int         src_x,
                int         src_y,
                int         width,
                int         height)
{
  unsigned int n_channels = gdk_pixbuf_get_n_channels (src);
  unsigned int src_rowstride = gdk_pixbuf_get_rowstride (src);
  unsigned char *pixels = (gdk_pixbuf_get_pixels (src) + src_y * src_rowstride + src_x
                           * n_channels);
  unsigned char *dest_pixels;
  GdkPixbuf *result;
  unsigned int dest_rowstride;
  int i;

  result = gdk_pixbuf_new (GDK_COLORSPACE_RGB, n_channels == 4, 8,
                           width, height);
  dest_rowstride = gdk_pixbuf_get_rowstride (result);
  dest_pixels = gdk_pixbuf_get_pixels (result);

  for (i = 0; i < height; i++)
    memcpy (dest_pixels + dest_rowstride * i, pixels, n_channels * width);

  return result;
}

static GdkPixbuf *
replicate_cols (GdkPixbuf  *src,
                int         src_x,
                int         src_y,
                int         width,
                int         height)
{
  unsigned int n_channels = gdk_pixbuf_get_n_channels (src);
  unsigned int src_rowstride = gdk_pixbuf_get_rowstride (src);
  unsigned char *pixels = (gdk_pixbuf_get_pixels (src) + src_y * src_rowstride + src_x
                           * n_channels);
  unsigned char *dest_pixels;
  GdkPixbuf *result;
  unsigned int dest_rowstride;
  int i, j;

  result = gdk_pixbuf_new (GDK_COLORSPACE_RGB, n_channels == 4, 8,
                           width, height);
  dest_rowstride = gdk_pixbuf_get_rowstride (result);
  dest_pixels = gdk_pixbuf_get_pixels (result);

  for (i = 0; i < height; i++)
    {
      unsigned char *p = dest_pixels + dest_rowstride * i;
      unsigned char *q = pixels + src_rowstride * i;

      unsigned char r = *(q++);
      unsigned char g = *(q++);
      unsigned char b = *(q++);

      if (n_channels == 4)
        {
          unsigned char a;

          a = *(q++);

          for (j = 0; j < width; j++)
            {
              *(p++) = r;
              *(p++) = g;
              *(p++) = b;
              *(p++) = a;
            }
        }
      else
        {
          for (j = 0; j < width; j++)
            {
              *(p++) = r;
              *(p++) = g;
              *(p++) = b;
            }
        }
    }

  return result;
}

static GdkPixbuf*
scale_and_alpha_pixbuf (GdkPixbuf             *src,
                        MetaAlphaGradientSpec *alpha_spec,
                        MetaImageFillType      fill_type,
                        int                    width,
                        int                    height,
                        gboolean               vertical_stripes,
                        gboolean               horizontal_stripes)
{
  GdkPixbuf *pixbuf;
  GdkPixbuf *temp_pixbuf;

  pixbuf = NULL;

  pixbuf = src;

  if (gdk_pixbuf_get_width (pixbuf) == width &&
      gdk_pixbuf_get_height (pixbuf) == height)
    {
      g_object_ref (G_OBJECT (pixbuf));
    }
  else
    {
      if (fill_type == META_IMAGE_FILL_TILE)
        {
          pixbuf = pixbuf_tile (pixbuf, width, height);
        }
      else
        {
          int src_h, src_w, dest_h, dest_w;
          src_h = gdk_pixbuf_get_height (src);
          src_w = gdk_pixbuf_get_width (src);

          /* prefer to replicate_cols if possible, as that
           * is faster (no memory reads)
           */
          if (horizontal_stripes)
            {
              dest_w = gdk_pixbuf_get_width (src);
              dest_h = height;
            }
          else if (vertical_stripes)
            {
              dest_w = width;
              dest_h = gdk_pixbuf_get_height (src);
            }

          else
            {
              dest_w = width;
              dest_h = height;
            }

          if (dest_w == src_w && dest_h == src_h)
            {
              temp_pixbuf = src;
              g_object_ref (G_OBJECT (temp_pixbuf));
            }
          else
            {
              temp_pixbuf = gdk_pixbuf_scale_simple (src,
                                                     dest_w, dest_h,
                                                     GDK_INTERP_BILINEAR);
            }

          /* prefer to replicate_cols if possible, as that
           * is faster (no memory reads)
           */
          if (horizontal_stripes)
            {
              pixbuf = replicate_cols (temp_pixbuf, 0, 0, width, height);
              g_object_unref (G_OBJECT (temp_pixbuf));
            }
          else if (vertical_stripes)
            {
              pixbuf = replicate_rows (temp_pixbuf, 0, 0, width, height);
              g_object_unref (G_OBJECT (temp_pixbuf));
            }
          else
            {
              pixbuf = temp_pixbuf;
            }
        }
    }

  if (pixbuf)
    pixbuf = meta_alpha_gradient_spec_apply_alpha (alpha_spec, pixbuf, pixbuf == src);

  return pixbuf;
}

static GdkPixbuf *
colorize_pixbuf (GdkPixbuf *orig,
                 GdkRGBA   *new_color)
{
  GdkPixbuf *pixbuf;
  double intensity;
  int x, y;
  const guchar *src;
  guchar *dest;
  int orig_rowstride;
  int dest_rowstride;
  int width, height;
  gboolean has_alpha;
  const guchar *src_pixels;
  guchar *dest_pixels;

  pixbuf = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (orig), gdk_pixbuf_get_has_alpha (orig),
			   gdk_pixbuf_get_bits_per_sample (orig),
			   gdk_pixbuf_get_width (orig), gdk_pixbuf_get_height (orig));

  if (pixbuf == NULL)
    return NULL;

  orig_rowstride = gdk_pixbuf_get_rowstride (orig);
  dest_rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);
  has_alpha = gdk_pixbuf_get_has_alpha (orig);
  src_pixels = gdk_pixbuf_get_pixels (orig);
  dest_pixels = gdk_pixbuf_get_pixels (pixbuf);

  for (y = 0; y < height; y++)
    {
      src = src_pixels + y * orig_rowstride;
      dest = dest_pixels + y * dest_rowstride;

      for (x = 0; x < width; x++)
        {
          double dr, dg, db;

          intensity = INTENSITY (src[0], src[1], src[2]) / 255.0;

          if (intensity <= 0.5)
            {
              /* Go from black at intensity = 0.0 to new_color at intensity = 0.5 */
              dr = new_color->red * intensity * 2.0;
              dg = new_color->green * intensity * 2.0;
              db = new_color->blue * intensity * 2.0;
            }
          else
            {
              /* Go from new_color at intensity = 0.5 to white at intensity = 1.0 */
              dr = new_color->red + (1.0 - new_color->red) * (intensity - 0.5) * 2.0;
              dg = new_color->green + (1.0 - new_color->green) * (intensity - 0.5) * 2.0;
              db = new_color->blue + (1.0 - new_color->blue) * (intensity - 0.5) * 2.0;
            }

          dest[0] = CLAMP_UCHAR (255 * dr);
          dest[1] = CLAMP_UCHAR (255 * dg);
          dest[2] = CLAMP_UCHAR (255 * db);

          if (has_alpha)
            {
              dest[3] = src[3];
              src += 4;
              dest += 4;
            }
          else
            {
              src += 3;
              dest += 3;
            }
        }
    }

  return pixbuf;
}

static GdkPixbuf *
draw_op_as_pixbuf (const MetaDrawOp   *op,
                   GtkStyleContext    *context,
                   const MetaDrawInfo *info,
                   int                 width,
                   int                 height)
{
  /* Try to get the op as a pixbuf, assuming w/h in the op
   * matches the width/height passed in. return NULL
   * if the op can't be converted to an equivalent pixbuf.
   */
  GdkPixbuf *pixbuf;

  pixbuf = NULL;

  switch (op->type)
    {
    case META_DRAW_LINE:
      break;

    case META_DRAW_RECTANGLE:
      if (op->data.rectangle.filled)
        {
          GdkRGBA color;

          meta_color_spec_render (op->data.rectangle.color_spec,
                                  context,
                                  &color);

          pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                                   FALSE,
                                   8, width, height);

          gdk_pixbuf_fill (pixbuf, GDK_COLOR_RGBA (color));
        }
      break;

    case META_DRAW_ARC:
      break;

    case META_DRAW_CLIP:
      break;

    case META_DRAW_TINT:
      break;

    case META_DRAW_GRADIENT:
      break;

    case META_DRAW_IMAGE:
      {
	if (op->data.image.colorize_spec)
	  {
	    GdkRGBA color;

            meta_color_spec_render (op->data.image.colorize_spec,
                                    context, &color);

            if (op->data.image.colorize_cache_pixbuf == NULL ||
                op->data.image.colorize_cache_pixel != GDK_COLOR_RGB (color))
              {
                if (op->data.image.colorize_cache_pixbuf)
                  g_object_unref (G_OBJECT (op->data.image.colorize_cache_pixbuf));

                /* const cast here */
                ((MetaDrawOp*)op)->data.image.colorize_cache_pixbuf =
                  colorize_pixbuf (op->data.image.pixbuf,
                                   &color);
                ((MetaDrawOp*)op)->data.image.colorize_cache_pixel =
                  GDK_COLOR_RGB (color);
              }

            if (op->data.image.colorize_cache_pixbuf)
              {
                pixbuf = scale_and_alpha_pixbuf (op->data.image.colorize_cache_pixbuf,
                                                 op->data.image.alpha_spec,
                                                 op->data.image.fill_type,
                                                 width, height,
                                                 op->data.image.vertical_stripes,
                                                 op->data.image.horizontal_stripes);
              }
	  }
	else
	  {
	    pixbuf = scale_and_alpha_pixbuf (op->data.image.pixbuf,
                                             op->data.image.alpha_spec,
                                             op->data.image.fill_type,
                                             width, height,
                                             op->data.image.vertical_stripes,
                                             op->data.image.horizontal_stripes);
	  }
        break;
      }

    case META_DRAW_GTK_ARROW:
    case META_DRAW_GTK_BOX:
    case META_DRAW_GTK_VLINE:
      break;

    case META_DRAW_ICON:
      if (info->mini_icon &&
          width <= gdk_pixbuf_get_width (info->mini_icon) &&
          height <= gdk_pixbuf_get_height (info->mini_icon))
        pixbuf = scale_and_alpha_pixbuf (info->mini_icon,
                                         op->data.icon.alpha_spec,
                                         op->data.icon.fill_type,
                                         width, height,
                                         FALSE, FALSE);
      else if (info->icon)
        pixbuf = scale_and_alpha_pixbuf (info->icon,
                                         op->data.icon.alpha_spec,
                                         op->data.icon.fill_type,
                                         width, height,
                                         FALSE, FALSE);
      break;

    case META_DRAW_TITLE:
      break;

    case META_DRAW_OP_LIST:
      break;

    case META_DRAW_TILE:
      break;

    default:
      break;
    }

  return pixbuf;
}

/* This code was originally rendering anti-aliased using X primitives, and
 * now has been switched to draw anti-aliased using cairo. In general, the
 * closest correspondence between X rendering and cairo rendering is given
 * by offsetting the geometry by 0.5 pixels in both directions before rendering
 * with cairo. This is because X samples at the upper left corner of the
 * pixel while cairo averages over the entire pixel. However, in the cases
 * where the X rendering was an exact rectangle with no "jaggies"
 * we need to be a bit careful about applying the offset. We want to produce
 * the exact same pixel-aligned rectangle, rather than a rectangle with
 * fuzz around the edges.
 */
static void
draw_op_draw_with_env (const MetaDrawOp    *op,
                       GtkStyleContext     *context,
                       cairo_t             *cr,
                       const MetaDrawInfo  *info,
                       GdkRectangle         rect,
                       MetaPositionExprEnv *env)
{
  GdkRGBA color;

  cairo_save (cr);

  cairo_set_line_width (cr, 1.0);

  switch (op->type)
    {
    case META_DRAW_LINE:
      {
        int x1, x2, y1, y2;

        meta_color_spec_render (op->data.line.color_spec, context, &color);
        gdk_cairo_set_source_rgba (cr, &color);

        if (op->data.line.width > 0)
          cairo_set_line_width (cr, op->data.line.width);

        if (op->data.line.dash_on_length > 0 &&
            op->data.line.dash_off_length > 0)
          {
            double dash_list[2];
            dash_list[0] = op->data.line.dash_on_length;
            dash_list[1] = op->data.line.dash_off_length;
            cairo_set_dash (cr, dash_list, 2, 0);
          }

        x1 = meta_draw_spec_parse_x_position (op->data.line.x1, env);
        y1 = meta_draw_spec_parse_y_position (op->data.line.y1, env);

        if (!op->data.line.x2 &&
            !op->data.line.y2 &&
            op->data.line.width==0)
          {
            cairo_rectangle (cr, x1, y1, 1, 1);
            cairo_fill (cr);
          }
        else
          {
            if (op->data.line.x2)
              x2 = meta_draw_spec_parse_x_position (op->data.line.x2, env);
            else
              x2 = x1;

            if (op->data.line.y2)
              y2 = meta_draw_spec_parse_y_position (op->data.line.y2, env);
            else
              y2 = y1;

            /* This is one of the cases where we are matching the exact
             * pixel aligned rectangle produced by X; for zero-width lines
             * the generic algorithm produces the right result so we don't
             * need to handle them here.
             */
            if ((y1 == y2 || x1 == x2) && op->data.line.width != 0)
              {
                double offset = op->data.line.width % 2 ? .5 : 0;

                if (y1 == y2)
                  {
                    cairo_move_to (cr, x1, y1 + offset);
                    cairo_line_to (cr, x2, y2 + offset);
                  }
                else
                  {
                    cairo_move_to (cr, x1 + offset, y1);
                    cairo_line_to (cr, x2 + offset, y2);
                  }
              }
            else
              {
                /* zero-width lines include both end-points in X, unlike wide lines */
                if (op->data.line.width == 0)
                  cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);

                cairo_move_to (cr, x1 + .5, y1 + .5);
                cairo_line_to (cr, x2 + .5, y2 + .5);
              }
            cairo_stroke (cr);
          }
      }
      break;

    case META_DRAW_RECTANGLE:
      {
        int rx, ry, rwidth, rheight;

        meta_color_spec_render (op->data.rectangle.color_spec, context, &color);
        gdk_cairo_set_source_rgba (cr, &color);

        rx = meta_draw_spec_parse_x_position (op->data.rectangle.x, env);
        ry = meta_draw_spec_parse_y_position (op->data.rectangle.y, env);
        rwidth = meta_draw_spec_parse_size (op->data.rectangle.width, env);
        rheight = meta_draw_spec_parse_size (op->data.rectangle.height, env);

        /* Filled and stroked rectangles are the other cases
         * we pixel-align to X rasterization
         */
        if (op->data.rectangle.filled)
          {
            cairo_rectangle (cr, rx, ry, rwidth, rheight);
            cairo_fill (cr);
          }
        else
          {
            cairo_rectangle (cr, rx + .5, ry + .5, rwidth, rheight);
            cairo_stroke (cr);
          }
      }
      break;

    case META_DRAW_ARC:
      {
        int rx, ry, rwidth, rheight;
        double start_angle, end_angle;
        double center_x, center_y;

        meta_color_spec_render (op->data.arc.color_spec, context, &color);
        gdk_cairo_set_source_rgba (cr, &color);

        rx = meta_draw_spec_parse_x_position (op->data.arc.x, env);
        ry = meta_draw_spec_parse_y_position (op->data.arc.y, env);
        rwidth = meta_draw_spec_parse_size (op->data.arc.width, env);
        rheight = meta_draw_spec_parse_size (op->data.arc.height, env);

        start_angle = op->data.arc.start_angle * (M_PI / 180.)
                      - (.5 * M_PI); /* start at 12 instead of 3 oclock */
        end_angle = start_angle + op->data.arc.extent_angle * (M_PI / 180.);
        center_x = rx + (double)rwidth / 2. + .5;
        center_y = ry + (double)rheight / 2. + .5;

        cairo_save (cr);

        cairo_translate (cr, center_x, center_y);
        cairo_scale (cr, (double)rwidth / 2., (double)rheight / 2.);

        if (op->data.arc.extent_angle >= 0)
          cairo_arc (cr, 0, 0, 1, start_angle, end_angle);
        else
          cairo_arc_negative (cr, 0, 0, 1, start_angle, end_angle);

        cairo_restore (cr);

        if (op->data.arc.filled)
          {
            cairo_line_to (cr, center_x, center_y);
            cairo_fill (cr);
          }
        else
          cairo_stroke (cr);
      }
      break;

    case META_DRAW_CLIP:
      break;

    case META_DRAW_TINT:
      {
        int rx, ry, rwidth, rheight;

        rx = meta_draw_spec_parse_x_position (op->data.tint.x, env);
        ry = meta_draw_spec_parse_y_position (op->data.tint.y, env);
        rwidth = meta_draw_spec_parse_size (op->data.tint.width, env);
        rheight = meta_draw_spec_parse_size (op->data.tint.height, env);

        meta_color_spec_render (op->data.tint.color_spec, context, &color);
        meta_alpha_gradient_spec_render (op->data.tint.alpha_spec, color, cr,
                                         rx, ry, rwidth, rheight);
      }
      break;

    case META_DRAW_GRADIENT:
      {
        int rx, ry, rwidth, rheight;

        rx = meta_draw_spec_parse_x_position (op->data.gradient.x, env);
        ry = meta_draw_spec_parse_y_position (op->data.gradient.y, env);
        rwidth = meta_draw_spec_parse_size (op->data.gradient.width, env);
        rheight = meta_draw_spec_parse_size (op->data.gradient.height, env);

        meta_gradient_spec_render (op->data.gradient.gradient_spec,
                                   op->data.gradient.alpha_spec,
                                   cr, context, rx, ry, rwidth, rheight);
      }
      break;

    case META_DRAW_IMAGE:
      {
        int rx, ry, rwidth, rheight;
        GdkPixbuf *pixbuf;

        if (op->data.image.pixbuf)
          {
            env->object_width = gdk_pixbuf_get_width (op->data.image.pixbuf);
            env->object_height = gdk_pixbuf_get_height (op->data.image.pixbuf);
          }

        rwidth = meta_draw_spec_parse_size (op->data.image.width, env);
        rheight = meta_draw_spec_parse_size (op->data.image.height, env);

        pixbuf = draw_op_as_pixbuf (op, context, info, rwidth, rheight);

        if (pixbuf)
          {
            rx = meta_draw_spec_parse_x_position (op->data.image.x, env);
            ry = meta_draw_spec_parse_y_position (op->data.image.y, env);

            gdk_cairo_set_source_pixbuf (cr, pixbuf, rx, ry);
            cairo_paint (cr);

            g_object_unref (G_OBJECT (pixbuf));
          }
      }
      break;

    case META_DRAW_GTK_ARROW:
      {
        int rx, ry, rwidth, rheight;
        double angle = 0, size;

        rx = meta_draw_spec_parse_x_position (op->data.gtk_arrow.x, env);
        ry = meta_draw_spec_parse_y_position (op->data.gtk_arrow.y, env);
        rwidth = meta_draw_spec_parse_size (op->data.gtk_arrow.width, env);
        rheight = meta_draw_spec_parse_size (op->data.gtk_arrow.height, env);

        size = MAX(rwidth, rheight);

        switch (op->data.gtk_arrow.arrow)
          {
          case GTK_ARROW_UP:
            angle = 0;
            break;
          case GTK_ARROW_RIGHT:
            angle = M_PI / 2;
            break;
          case GTK_ARROW_DOWN:
            angle = M_PI;
            break;
          case GTK_ARROW_LEFT:
            angle = 3 * M_PI / 2;
            break;
          case GTK_ARROW_NONE:
            return;
          default:
            break;
          }

        gtk_style_context_set_state (context, op->data.gtk_arrow.state);
        gtk_render_arrow (context, cr, angle, rx, ry, size);
      }
      break;

    case META_DRAW_GTK_BOX:
      {
        int rx, ry, rwidth, rheight;

        rx = meta_draw_spec_parse_x_position (op->data.gtk_box.x, env);
        ry = meta_draw_spec_parse_y_position (op->data.gtk_box.y, env);
        rwidth = meta_draw_spec_parse_size (op->data.gtk_box.width, env);
        rheight = meta_draw_spec_parse_size (op->data.gtk_box.height, env);

        gtk_style_context_set_state (context, op->data.gtk_box.state);
        gtk_render_background (context, cr, rx, ry, rwidth, rheight);
        gtk_render_frame (context, cr, rx, ry, rwidth, rheight);
      }
      break;

    case META_DRAW_GTK_VLINE:
      {
        int rx, ry1, ry2;

        rx = meta_draw_spec_parse_x_position (op->data.gtk_vline.x, env);
        ry1 = meta_draw_spec_parse_y_position (op->data.gtk_vline.y1, env);
        ry2 = meta_draw_spec_parse_y_position (op->data.gtk_vline.y2, env);

        gtk_style_context_set_state (context, op->data.gtk_vline.state);
        gtk_render_line (context, cr, rx, ry1, rx, ry2);
      }
      break;

    case META_DRAW_ICON:
      {
        int rx, ry, rwidth, rheight;
        GdkPixbuf *pixbuf;

        rwidth = meta_draw_spec_parse_size (op->data.icon.width, env);
        rheight = meta_draw_spec_parse_size (op->data.icon.height, env);

        pixbuf = draw_op_as_pixbuf (op, context, info, rwidth, rheight);

        if (pixbuf)
          {
            rx = meta_draw_spec_parse_x_position (op->data.icon.x, env);
            ry = meta_draw_spec_parse_y_position (op->data.icon.y, env);

            gdk_cairo_set_source_pixbuf (cr, pixbuf, rx, ry);
            cairo_paint (cr);

            g_object_unref (G_OBJECT (pixbuf));
          }
      }
      break;

    case META_DRAW_TITLE:
      if (info->title_layout)
        {
          int rx, ry;
          PangoRectangle ink_rect, logical_rect;

          meta_color_spec_render (op->data.title.color_spec, context, &color);
          gdk_cairo_set_source_rgba (cr, &color);

          rx = meta_draw_spec_parse_x_position (op->data.title.x, env);
          ry = meta_draw_spec_parse_y_position (op->data.title.y, env);

          if (op->data.title.ellipsize_width)
            {
              int ellipsize_width;
              int right_bearing;

              ellipsize_width = meta_draw_spec_parse_x_position (op->data.title.ellipsize_width, env);
              /* HACK: meta_draw_spec_parse_x_position adds in env->rect.x, subtract out again */
              ellipsize_width -= env->rect.x;

              pango_layout_set_width (info->title_layout, -1);
              pango_layout_get_pixel_extents (info->title_layout,
                                              &ink_rect, &logical_rect);

              /* Pango's idea of ellipsization is with respect to the logical rect.
               * correct for this, by reducing the ellipsization width by the overflow
               * of the un-ellipsized text on the right... it's always the visual
               * right we want regardless of bidi, since since the X we pass in to
               * cairo_move_to() is always the left edge of the line.
               */
              right_bearing = (ink_rect.x + ink_rect.width) - (logical_rect.x + logical_rect.width);
              right_bearing = MAX (right_bearing, 0);

              ellipsize_width -= right_bearing;
              ellipsize_width = MAX (ellipsize_width, 0);

              /* Only ellipsizing when necessary is a performance optimization -
               * pango_layout_set_width() will force a relayout if it isn't the
               * same as the current width of -1.
               */
              if (ellipsize_width < logical_rect.width)
                pango_layout_set_width (info->title_layout, PANGO_SCALE * ellipsize_width);
            }
          else if (rx - env->rect.x + env->title_width >= env->rect.width)
          {
            const double alpha_margin = 30.0;
            int text_space = env->rect.x + env->rect.width -
                             (rx - env->rect.x) - env->right_width;

            double startalpha = 1.0 - (alpha_margin/((double)text_space));

            cairo_pattern_t *linpat;
            linpat = cairo_pattern_create_linear (rx, ry, text_space,
                                                  env->title_height);
            cairo_pattern_add_color_stop_rgba (linpat, 0, color.red,
                                                          color.green,
                                                          color.blue,
                                                          color.alpha);
            cairo_pattern_add_color_stop_rgba (linpat, startalpha,
                                                       color.red,
                                                       color.green,
                                                       color.blue,
                                                       color.alpha);
            cairo_pattern_add_color_stop_rgba (linpat, 1, color.red,
                                                          color.green,
                                                          color.blue, 0);
            cairo_set_source(cr, linpat);
            cairo_pattern_destroy(linpat);
          }

          cairo_move_to (cr, rx, ry);
          pango_cairo_show_layout (cr, info->title_layout);

          /* Remove any ellipsization we might have set; will short-circuit
           * if the width is already -1 */
          pango_layout_set_width (info->title_layout, -1);
        }
      break;

    case META_DRAW_OP_LIST:
      {
        GdkRectangle d_rect;

        d_rect.x = meta_draw_spec_parse_x_position (op->data.op_list.x, env);
        d_rect.y = meta_draw_spec_parse_y_position (op->data.op_list.y, env);
        d_rect.width = meta_draw_spec_parse_size (op->data.op_list.width, env);
        d_rect.height = meta_draw_spec_parse_size (op->data.op_list.height, env);

        meta_draw_op_list_draw_with_style (op->data.op_list.op_list,
                                           context, cr, info, d_rect);
      }
      break;

    case META_DRAW_TILE:
      {
        int rx, ry, rwidth, rheight;
        int tile_xoffset, tile_yoffset;
        GdkRectangle tile;

        rx = meta_draw_spec_parse_x_position (op->data.tile.x, env);
        ry = meta_draw_spec_parse_y_position (op->data.tile.y, env);
        rwidth = meta_draw_spec_parse_size (op->data.tile.width, env);
        rheight = meta_draw_spec_parse_size (op->data.tile.height, env);

        cairo_save (cr);

        cairo_rectangle (cr, rx, ry, rwidth, rheight);
        cairo_clip (cr);

        tile_xoffset = meta_draw_spec_parse_x_position (op->data.tile.tile_xoffset, env);
        tile_yoffset = meta_draw_spec_parse_y_position (op->data.tile.tile_yoffset, env);
        /* tile offset should not include x/y */
        tile_xoffset -= rect.x;
        tile_yoffset -= rect.y;

        tile.width = meta_draw_spec_parse_size (op->data.tile.tile_width, env);
        tile.height = meta_draw_spec_parse_size (op->data.tile.tile_height, env);

        tile.x = rx - tile_xoffset;

        while (tile.x < (rx + rwidth))
          {
            tile.y = ry - tile_yoffset;
            while (tile.y < (ry + rheight))
              {
                meta_draw_op_list_draw_with_style (op->data.tile.op_list,
                                                   context, cr, info, tile);

                tile.y += tile.height;
              }

            tile.x += tile.width;
          }
        cairo_restore (cr);
      }
      break;

    default:
      break;
    }

   cairo_restore (cr);
}

MetaDrawOp *
meta_draw_op_new (MetaDrawType type)
{
  MetaDrawOp *op;
  MetaDrawOp dummy;
  int size;

  size = G_STRUCT_OFFSET (MetaDrawOp, data);

  switch (type)
    {
      case META_DRAW_LINE:
        size += sizeof (dummy.data.line);
        break;

      case META_DRAW_RECTANGLE:
        size += sizeof (dummy.data.rectangle);
        break;

      case META_DRAW_ARC:
        size += sizeof (dummy.data.arc);
        break;

      case META_DRAW_CLIP:
        size += sizeof (dummy.data.clip);
        break;

      case META_DRAW_TINT:
        size += sizeof (dummy.data.tint);
        break;

      case META_DRAW_GRADIENT:
        size += sizeof (dummy.data.gradient);
        break;

      case META_DRAW_IMAGE:
        size += sizeof (dummy.data.image);
        break;

      case META_DRAW_GTK_ARROW:
        size += sizeof (dummy.data.gtk_arrow);
        break;

      case META_DRAW_GTK_BOX:
        size += sizeof (dummy.data.gtk_box);
        break;

      case META_DRAW_GTK_VLINE:
        size += sizeof (dummy.data.gtk_vline);
        break;

      case META_DRAW_ICON:
        size += sizeof (dummy.data.icon);
        break;

      case META_DRAW_TITLE:
        size += sizeof (dummy.data.title);
        break;
      case META_DRAW_OP_LIST:
        size += sizeof (dummy.data.op_list);
        break;
      case META_DRAW_TILE:
        size += sizeof (dummy.data.tile);
        break;

      default:
        break;
    }

  op = g_malloc0 (size);

  op->type = type;

  return op;
}

void
meta_draw_op_free (MetaDrawOp *op)
{
  g_return_if_fail (op != NULL);

  switch (op->type)
    {
      case META_DRAW_LINE:
        if (op->data.line.color_spec)
          meta_color_spec_free (op->data.line.color_spec);

        meta_draw_spec_free (op->data.line.x1);
        meta_draw_spec_free (op->data.line.y1);
        meta_draw_spec_free (op->data.line.x2);
        meta_draw_spec_free (op->data.line.y2);
        break;

      case META_DRAW_RECTANGLE:
        if (op->data.rectangle.color_spec)
          g_free (op->data.rectangle.color_spec);

        meta_draw_spec_free (op->data.rectangle.x);
        meta_draw_spec_free (op->data.rectangle.y);
        meta_draw_spec_free (op->data.rectangle.width);
        meta_draw_spec_free (op->data.rectangle.height);
        break;

      case META_DRAW_ARC:
        if (op->data.arc.color_spec)
          g_free (op->data.arc.color_spec);

        meta_draw_spec_free (op->data.arc.x);
        meta_draw_spec_free (op->data.arc.y);
        meta_draw_spec_free (op->data.arc.width);
        meta_draw_spec_free (op->data.arc.height);
        break;

      case META_DRAW_CLIP:
        meta_draw_spec_free (op->data.clip.x);
        meta_draw_spec_free (op->data.clip.y);
        meta_draw_spec_free (op->data.clip.width);
        meta_draw_spec_free (op->data.clip.height);
        break;

      case META_DRAW_TINT:
        if (op->data.tint.color_spec)
          meta_color_spec_free (op->data.tint.color_spec);

        if (op->data.tint.alpha_spec)
          meta_alpha_gradient_spec_free (op->data.tint.alpha_spec);

        meta_draw_spec_free (op->data.tint.x);
        meta_draw_spec_free (op->data.tint.y);
        meta_draw_spec_free (op->data.tint.width);
        meta_draw_spec_free (op->data.tint.height);
        break;

      case META_DRAW_GRADIENT:
        if (op->data.gradient.gradient_spec)
          meta_gradient_spec_free (op->data.gradient.gradient_spec);

        if (op->data.gradient.alpha_spec)
          meta_alpha_gradient_spec_free (op->data.gradient.alpha_spec);

        meta_draw_spec_free (op->data.gradient.x);
        meta_draw_spec_free (op->data.gradient.y);
        meta_draw_spec_free (op->data.gradient.width);
        meta_draw_spec_free (op->data.gradient.height);
        break;

      case META_DRAW_IMAGE:
        if (op->data.image.alpha_spec)
          meta_alpha_gradient_spec_free (op->data.image.alpha_spec);

        if (op->data.image.pixbuf)
          g_object_unref (G_OBJECT (op->data.image.pixbuf));

        if (op->data.image.colorize_spec)
          meta_color_spec_free (op->data.image.colorize_spec);

        if (op->data.image.colorize_cache_pixbuf)
          g_object_unref (G_OBJECT (op->data.image.colorize_cache_pixbuf));

        meta_draw_spec_free (op->data.image.x);
        meta_draw_spec_free (op->data.image.y);
        meta_draw_spec_free (op->data.image.width);
        meta_draw_spec_free (op->data.image.height);
        break;

      case META_DRAW_GTK_ARROW:
        meta_draw_spec_free (op->data.gtk_arrow.x);
        meta_draw_spec_free (op->data.gtk_arrow.y);
        meta_draw_spec_free (op->data.gtk_arrow.width);
        meta_draw_spec_free (op->data.gtk_arrow.height);
        break;

      case META_DRAW_GTK_BOX:
        meta_draw_spec_free (op->data.gtk_box.x);
        meta_draw_spec_free (op->data.gtk_box.y);
        meta_draw_spec_free (op->data.gtk_box.width);
        meta_draw_spec_free (op->data.gtk_box.height);
        break;

      case META_DRAW_GTK_VLINE:
        meta_draw_spec_free (op->data.gtk_vline.x);
        meta_draw_spec_free (op->data.gtk_vline.y1);
        meta_draw_spec_free (op->data.gtk_vline.y2);
        break;

      case META_DRAW_ICON:
        if (op->data.icon.alpha_spec)
          meta_alpha_gradient_spec_free (op->data.icon.alpha_spec);

        meta_draw_spec_free (op->data.icon.x);
        meta_draw_spec_free (op->data.icon.y);
        meta_draw_spec_free (op->data.icon.width);
        meta_draw_spec_free (op->data.icon.height);
        break;

      case META_DRAW_TITLE:
        if (op->data.title.color_spec)
          meta_color_spec_free (op->data.title.color_spec);

        meta_draw_spec_free (op->data.title.x);
        meta_draw_spec_free (op->data.title.y);
        if (op->data.title.ellipsize_width)
          meta_draw_spec_free (op->data.title.ellipsize_width);
        break;

      case META_DRAW_OP_LIST:
        if (op->data.op_list.op_list)
          meta_draw_op_list_unref (op->data.op_list.op_list);

        meta_draw_spec_free (op->data.op_list.x);
        meta_draw_spec_free (op->data.op_list.y);
        meta_draw_spec_free (op->data.op_list.width);
        meta_draw_spec_free (op->data.op_list.height);
        break;

      case META_DRAW_TILE:
        if (op->data.tile.op_list)
          meta_draw_op_list_unref (op->data.tile.op_list);

        meta_draw_spec_free (op->data.tile.x);
        meta_draw_spec_free (op->data.tile.y);
        meta_draw_spec_free (op->data.tile.width);
        meta_draw_spec_free (op->data.tile.height);
        meta_draw_spec_free (op->data.tile.tile_xoffset);
        meta_draw_spec_free (op->data.tile.tile_yoffset);
        meta_draw_spec_free (op->data.tile.tile_width);
        meta_draw_spec_free (op->data.tile.tile_height);
        break;

      default:
        break;
    }

  g_free (op);
}

MetaDrawOpList *
meta_draw_op_list_new (int n_preallocs)
{
  MetaDrawOpList *op_list;

  g_return_val_if_fail (n_preallocs >= 0, NULL);

  op_list = g_new (MetaDrawOpList, 1);

  op_list->refcount = 1;
  op_list->n_allocated = n_preallocs;
  op_list->ops = g_new (MetaDrawOp*, op_list->n_allocated);
  op_list->n_ops = 0;

  return op_list;
}

void
meta_draw_op_list_ref (MetaDrawOpList *op_list)
{
  g_return_if_fail (op_list != NULL);

  op_list->refcount += 1;
}

void
meta_draw_op_list_unref (MetaDrawOpList *op_list)
{
  g_return_if_fail (op_list != NULL);
  g_return_if_fail (op_list->refcount > 0);

  op_list->refcount -= 1;

  if (op_list->refcount == 0)
    {
      int i;

      for (i = 0; i < op_list->n_ops; i++)
        meta_draw_op_free (op_list->ops[i]);

      g_free (op_list->ops);

      g_free (op_list);
    }
}

void
meta_draw_op_list_draw_with_style (const MetaDrawOpList *op_list,
                                   GtkStyleContext      *context,
                                   cairo_t              *cr,
                                   const MetaDrawInfo   *info,
                                   GdkRectangle          rect)
{
  int i;
  MetaPositionExprEnv env;

  fill_env (&env, info, rect);

  /* FIXME this can be optimized, potentially a lot, by
   * compressing multiple ops when possible. For example,
   * anything convertible to a pixbuf can be composited
   * client-side, and putting a color tint over a pixbuf
   * can be done without creating the solid-color pixbuf.
   *
   * To implement this my plan is to have the idea of a
   * compiled draw op (with the string expressions already
   * evaluated), we make an array of those, and then fold
   * adjacent items when possible.
   */
  cairo_save (cr);

  for (i = 0; i < op_list->n_ops; i++)
    {
      MetaDrawOp *op = op_list->ops[i];

      if (op->type == META_DRAW_CLIP)
        {
          cairo_restore (cr);

          cairo_rectangle (cr,
                           meta_draw_spec_parse_x_position (op->data.clip.x, &env),
                           meta_draw_spec_parse_y_position (op->data.clip.y, &env),
                           meta_draw_spec_parse_size (op->data.clip.width, &env),
                           meta_draw_spec_parse_size (op->data.clip.height, &env));
          cairo_clip (cr);

          cairo_save (cr);
        }
      else if (gdk_cairo_get_clip_rectangle (cr, NULL))
        {
          draw_op_draw_with_env (op, context, cr, info, rect, &env);
        }
    }

  cairo_restore (cr);
}

void
meta_draw_op_list_append (MetaDrawOpList *op_list,
                          MetaDrawOp     *op)
{
  if (op_list->n_ops == op_list->n_allocated)
    {
      op_list->n_allocated *= 2;
      op_list->ops = g_renew (MetaDrawOp*, op_list->ops, op_list->n_allocated);
    }

  op_list->ops[op_list->n_ops] = op;
  op_list->n_ops += 1;
}

gboolean
meta_draw_op_list_validate (MetaDrawOpList  *op_list,
                            GError         **error)
{
  g_return_val_if_fail (op_list != NULL, FALSE);

  /* empty lists are OK, nothing else to check really */

  return TRUE;
}

/* This is not done in validate, since we wouldn't know the name
 * of the list to report the error. It might be nice to
 * store names inside the list sometime.
 */
gboolean
meta_draw_op_list_contains (MetaDrawOpList    *op_list,
                            MetaDrawOpList    *child)
{
  int i;

  /* mmm, huge tree recursion */

  for (i = 0; i < op_list->n_ops; i++)
    {
      if (op_list->ops[i]->type == META_DRAW_OP_LIST)
        {
          if (op_list->ops[i]->data.op_list.op_list == child)
            return TRUE;

          if (meta_draw_op_list_contains (op_list->ops[i]->data.op_list.op_list,
                                          child))
            return TRUE;
        }
      else if (op_list->ops[i]->type == META_DRAW_TILE)
        {
          if (op_list->ops[i]->data.tile.op_list == child)
            return TRUE;

          if (meta_draw_op_list_contains (op_list->ops[i]->data.tile.op_list,
                                          child))
            return TRUE;
        }
    }

  return FALSE;
}
