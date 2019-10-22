/*
 * Copyright (C) 2019 Alberts Muktupāvels
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
#include "meta-shadow-xrender.h"

void
meta_shadow_xrender_free (MetaShadowXRender *self)
{
  if (self->black != None)
    {
      XRenderFreePicture (self->xdisplay, self->black);
      self->black = None;
    }

  if (self->shadow != None)
    {
      XRenderFreePicture (self->xdisplay, self->shadow);
      self->shadow = None;
    }

  if (self->region != None)
    {
      XFixesDestroyRegion (self->xdisplay, self->region);
      self->region = None;
    }

  g_free (self);
}

XserverRegion
meta_shadow_xrender_get_region (MetaShadowXRender *self)
{
  XserverRegion region;

  region = XFixesCreateRegion (self->xdisplay, NULL, 0);
  XFixesCopyRegion (self->xdisplay, region, self->region);

  return region;
}

void
meta_shadow_xrender_paint (MetaShadowXRender *self,
                           XserverRegion      paint_region,
                           Picture            paint_buffer,
                           int                x,
                           int                y)
{
  XserverRegion shadow_clip;

  shadow_clip = XFixesCreateRegion (self->xdisplay, NULL, 0);
  XFixesCopyRegion (self->xdisplay, shadow_clip, self->region);
  XFixesTranslateRegion (self->xdisplay, shadow_clip, x, y);

  XFixesIntersectRegion (self->xdisplay, shadow_clip, shadow_clip, paint_region);
  XFixesSetPictureClipRegion (self->xdisplay, paint_buffer, 0, 0, shadow_clip);
  XFixesDestroyRegion (self->xdisplay, shadow_clip);

  XRenderComposite (self->xdisplay, PictOpOver,
                    self->black, self->shadow, paint_buffer,
                    0, 0, 0, 0,
                    x + self->dx, y + self->dy,
                    self->width, self->height);
}
