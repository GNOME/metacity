/*
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

#include "meta-frame-style.h"
#include "meta-theme-impl.h"

typedef struct
{
  MetaFrameStyleSet *style_sets_by_type[META_FRAME_TYPE_LAST];
} MetaThemeImplPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaThemeImpl, meta_theme_impl, G_TYPE_OBJECT)

static void
meta_theme_impl_dispose (GObject *object)
{
  MetaThemeImpl *impl;
  MetaThemeImplPrivate *priv;
  gint i;

  impl = META_THEME_IMPL (object);
  priv = meta_theme_impl_get_instance_private (impl);

  for (i = 0; i < META_FRAME_TYPE_LAST; i++)
    {
      if (priv->style_sets_by_type[i])
        {
          meta_frame_style_set_unref (priv->style_sets_by_type[i]);
          priv->style_sets_by_type[i] = NULL;
        }
    }

  G_OBJECT_CLASS (meta_theme_impl_parent_class)->dispose (object);
}

static void
meta_theme_impl_class_init (MetaThemeImplClass *impl_class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (impl_class);

  object_class->dispose = meta_theme_impl_dispose;
}

static void
meta_theme_impl_init (MetaThemeImpl *impl)
{
}

void
meta_theme_impl_add_style_set (MetaThemeImpl     *impl,
                               MetaFrameType      type,
                               MetaFrameStyleSet *style_set)
{
  MetaThemeImplPrivate *priv;

  priv = meta_theme_impl_get_instance_private (impl);

  priv->style_sets_by_type[type] = style_set;
}

MetaFrameStyleSet *
meta_theme_impl_get_style_set (MetaThemeImpl *impl,
                               MetaFrameType  type)
{
  MetaThemeImplPrivate *priv;

  priv = meta_theme_impl_get_instance_private (impl);

  return priv->style_sets_by_type[type];
}
