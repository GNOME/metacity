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

#ifndef META_DRAW_SPEC_PRIVATE_H
#define META_DRAW_SPEC_PRIVATE_H

#include <gdk/gdk.h>

#include "meta-rectangle-private.h"

G_BEGIN_DECLS

typedef struct _MetaDrawSpec MetaDrawSpec;
typedef struct _MetaThemeMetacity MetaThemeMetacity;
typedef struct _MetaPositionExprEnv MetaPositionExprEnv;

struct _MetaPositionExprEnv
{
  MetaRectangleDouble rect;

  /* size of an object being drawn, if it has a natural size */
  gdouble             object_width;
  gdouble             object_height;

  /* global object sizes, always available */
  gdouble             left_width;
  gdouble             right_width;
  gdouble             top_height;
  gdouble             bottom_height;
  gdouble             title_width;
  gdouble             title_height;
  gdouble             frame_x_center;
  gdouble             frame_y_center;
  gdouble             mini_icon_width;
  gdouble             mini_icon_height;
  gdouble             icon_width;
  gdouble             icon_height;

  gint                scale;
};

G_GNUC_INTERNAL
MetaDrawSpec *meta_draw_spec_new              (MetaThemeMetacity          *metacity,
                                               const char                 *expr,
                                               GError                    **error);

G_GNUC_INTERNAL
void          meta_draw_spec_free             (MetaDrawSpec               *spec);

G_GNUC_INTERNAL
gdouble       meta_draw_spec_parse_x_position (MetaDrawSpec               *spec,
                                               const MetaPositionExprEnv  *env);

G_GNUC_INTERNAL
gdouble       meta_draw_spec_parse_y_position (MetaDrawSpec               *spec,
                                               const MetaPositionExprEnv  *env);

G_GNUC_INTERNAL
gdouble       meta_draw_spec_parse_size       (MetaDrawSpec               *spec,
                                               const MetaPositionExprEnv  *env);

G_END_DECLS

#endif
