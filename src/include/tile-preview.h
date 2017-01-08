/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Meta tile preview */

/*
 * Copyright (C) 2010 Florian Müllner
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */
#ifndef META_TILE_PREVIEW_H
#define META_TILE_PREVIEW_H

#include "boxes.h"

typedef struct _MetaTilePreview MetaTilePreview;

MetaTilePreview   *meta_tile_preview_new    (void);
void               meta_tile_preview_free   (MetaTilePreview   *preview);
void               meta_tile_preview_show   (MetaTilePreview   *preview,
                                             MetaRectangle     *rect);
void               meta_tile_preview_hide   (MetaTilePreview   *preview);

#endif /* META_TILE_PREVIEW_H */
