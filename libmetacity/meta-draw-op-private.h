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

#ifndef META_DRAW_OP_PRIVATE_H
#define META_DRAW_OP_PRIVATE_H

#include <gtk/gtk.h>

#include "meta-color-spec-private.h"
#include "meta-draw-spec-private.h"
#include "meta-gradient-spec-private.h"

G_BEGIN_DECLS

typedef struct _MetaDrawInfo MetaDrawInfo;
typedef struct _MetaDrawOp MetaDrawOp;
typedef struct _MetaDrawOpList MetaDrawOpList;

/**
 * A drawing operation in our simple vector drawing language.
 */
typedef enum
{
  /** Basic drawing-- line */
  META_DRAW_LINE,
  /** Basic drawing-- rectangle */
  META_DRAW_RECTANGLE,
  /** Basic drawing-- arc */
  META_DRAW_ARC,

  /** Clip to a rectangle */
  META_DRAW_CLIP,

  /* Texture thingies */

  /** Just a filled rectangle with alpha */
  META_DRAW_TINT,
  META_DRAW_GRADIENT,
  META_DRAW_IMAGE,

  /** GTK theme engine stuff */
  META_DRAW_GTK_ARROW,
  META_DRAW_GTK_BOX,
  META_DRAW_GTK_VLINE,

  /** App's window icon */
  META_DRAW_ICON,
  /** App's window title */
  META_DRAW_TITLE,
  /** a draw op list */
  META_DRAW_OP_LIST,
  /** tiled draw op list */
  META_DRAW_TILE
} MetaDrawType;

typedef enum
{
  META_IMAGE_FILL_SCALE, /* default, needs to be all-bits-zero for g_new0 */
  META_IMAGE_FILL_TILE
} MetaImageFillType;

struct _MetaDrawInfo
{
  gint         scale;

  GdkPixbuf   *mini_icon;
  GdkPixbuf   *icon;
  PangoLayout *title_layout;
  gint         title_layout_width;
  gint         title_layout_height;

  gint         left_width;
  gint         right_width;
  gint         top_height;
  gint         bottom_height;

  gdouble      width;
  gdouble      height;
};

/**
 * A single drawing operation in our simple vector drawing language.
 */
struct _MetaDrawOp
{
  MetaDrawType type;

  /* Positions are strings because they can be expressions */
  union
  {
    struct {
      MetaColorSpec *color_spec;
      int dash_on_length;
      int dash_off_length;
      int width;
      MetaDrawSpec *x1;
      MetaDrawSpec *y1;
      MetaDrawSpec *x2;
      MetaDrawSpec *y2;
    } line;

    struct {
      MetaColorSpec *color_spec;
      gboolean filled;
      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *width;
      MetaDrawSpec *height;
    } rectangle;

    struct {
      MetaColorSpec *color_spec;
      gboolean filled;
      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *width;
      MetaDrawSpec *height;
      double start_angle;
      double extent_angle;
    } arc;

    struct {
      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *width;
      MetaDrawSpec *height;
    } clip;

    struct {
      MetaColorSpec *color_spec;
      MetaAlphaGradientSpec *alpha_spec;
      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *width;
      MetaDrawSpec *height;
    } tint;

    struct {
      MetaGradientSpec *gradient_spec;
      MetaAlphaGradientSpec *alpha_spec;
      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *width;
      MetaDrawSpec *height;
    } gradient;

    struct {
      MetaColorSpec *colorize_spec;
      MetaAlphaGradientSpec *alpha_spec;
      GdkPixbuf *pixbuf;
      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *width;
      MetaDrawSpec *height;

      guint32 colorize_cache_pixel;
      GdkPixbuf *colorize_cache_pixbuf;
      MetaImageFillType fill_type;
      unsigned int vertical_stripes : 1;
      unsigned int horizontal_stripes : 1;
    } image;

    struct {
      GtkStateFlags state;
      GtkShadowType shadow;
      GtkArrowType arrow;
      gboolean filled;

      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *width;
      MetaDrawSpec *height;
    } gtk_arrow;

    struct {
      GtkStateFlags state;
      GtkShadowType shadow;
      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *width;
      MetaDrawSpec *height;
    } gtk_box;

    struct {
      GtkStateFlags state;
      MetaDrawSpec *x;
      MetaDrawSpec *y1;
      MetaDrawSpec *y2;
    } gtk_vline;

    struct {
      MetaAlphaGradientSpec *alpha_spec;
      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *width;
      MetaDrawSpec *height;
      MetaImageFillType fill_type;
    } icon;

    struct {
      MetaColorSpec *color_spec;
      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *ellipsize_width;
    } title;

    struct {
      MetaDrawOpList *op_list;
      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *width;
      MetaDrawSpec *height;
    } op_list;

    struct {
      MetaDrawOpList *op_list;
      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *width;
      MetaDrawSpec *height;
      MetaDrawSpec *tile_xoffset;
      MetaDrawSpec *tile_yoffset;
      MetaDrawSpec *tile_width;
      MetaDrawSpec *tile_height;
    } tile;

  } data;
};

G_GNUC_INTERNAL
MetaDrawOp     *meta_draw_op_new                  (MetaDrawType           type);

G_GNUC_INTERNAL
void            meta_draw_op_free                 (MetaDrawOp            *op);

G_GNUC_INTERNAL
MetaDrawOpList *meta_draw_op_list_new             (int                    n_preallocs);

G_GNUC_INTERNAL
void            meta_draw_op_list_ref             (MetaDrawOpList        *op_list);

G_GNUC_INTERNAL
void            meta_draw_op_list_unref           (MetaDrawOpList        *op_list);

G_GNUC_INTERNAL
void            meta_draw_op_list_draw_with_style (const MetaDrawOpList  *op_list,
                                                   GtkStyleContext       *context,
                                                   cairo_t               *cr,
                                                   const MetaDrawInfo    *info,
                                                   MetaRectangleDouble    rect);

G_GNUC_INTERNAL
void            meta_draw_op_list_append          (MetaDrawOpList        *op_list,
                                                   MetaDrawOp            *op);

G_GNUC_INTERNAL
gboolean        meta_draw_op_list_validate        (MetaDrawOpList        *op_list,
                                                   GError               **error);

G_GNUC_INTERNAL
gboolean        meta_draw_op_list_contains        (MetaDrawOpList        *op_list,
                                                   MetaDrawOpList        *child);

G_END_DECLS

#endif
