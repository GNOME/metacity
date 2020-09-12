/*
 * Copyright (C) 2017-2020 Alberts Muktupāvels
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

#ifndef META_COMPOSITOR_XRENDER_H
#define META_COMPOSITOR_XRENDER_H

#include "meta-compositor-private.h"
#include "meta-shadow-xrender.h"
#include "meta-surface-private.h"

G_BEGIN_DECLS

#define META_TYPE_COMPOSITOR_XRENDER meta_compositor_xrender_get_type ()
G_DECLARE_DERIVABLE_TYPE (MetaCompositorXRender, meta_compositor_xrender,
                          META, COMPOSITOR_XRENDER, MetaCompositor)

struct _MetaCompositorXRenderClass
{
  MetaCompositorClass parent_class;

  void (* ensure_root_buffers) (MetaCompositorXRender *self);
  void (* free_root_buffers)   (MetaCompositorXRender *self);
};

MetaCompositor    *meta_compositor_xrender_new                (MetaDisplay            *display,
                                                               GError                **error);

gboolean           meta_compositor_xrender_have_shadows       (MetaCompositorXRender  *self);

MetaShadowXRender *meta_compositor_xrender_create_shadow      (MetaCompositorXRender  *self,
                                                               MetaSurface            *surface);

void               meta_compositor_xrender_create_root_buffer (MetaCompositorXRender  *self,
                                                               Pixmap                 *pixmap,
                                                               Picture                *buffer);

void               meta_compositor_xrender_draw               (MetaCompositorXRender  *self,
                                                               Picture                 buffer,
                                                               XserverRegion           region);


G_END_DECLS

#endif
