/*
 * Copyright (C) 2017 Alberts Muktupāvels
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

#ifndef META_COMPOSITOR_NONE_H
#define META_COMPOSITOR_NONE_H

#include "meta-compositor-private.h"

G_BEGIN_DECLS

#define META_TYPE_COMPOSITOR_NONE meta_compositor_none_get_type ()
G_DECLARE_FINAL_TYPE (MetaCompositorNone, meta_compositor_none,
                      META, COMPOSITOR_NONE, MetaCompositor)

MetaCompositor *meta_compositor_none_new (MetaDisplay  *display,
                                          GError      **error);

G_END_DECLS

#endif
