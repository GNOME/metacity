#ifndef __LMC_TEXTURE_H__
#define __LMC_TEXTURE_H__

#include "lmcbits.h"
#include "lmctypes.h"
#include <gdk/gdk.h>

#include <X11/Xutil.h>		/* For Region */
#include <GL/gl.h>		/* For GLuint */

#include "config.h"
#include "screen.h"

G_BEGIN_DECLS

typedef struct _LmcTexture LmcTexture;

void lmc_texture_set_deformation (LmcTexture         *texture,
				  LmcDeformationFunc  func,
				  void               *data);

LmcTexture* lmc_texture_new         (LmcBits    *bits);
LmcTexture* lmc_texture_ref         (LmcTexture *texture);
void        lmc_texture_unref       (LmcTexture *texture);
void        lmc_texture_update_rect (LmcTexture *texture,
				     GdkRectangle *rect);
void        lmc_texture_draw        (MetaScreen  *screen,
				     LmcTexture *texture,
				     double      alpha,
				     int         x,
				     int         y,
				     GdkRegion  *clip);

G_END_DECLS

#endif /* __LMC_TEXTURE_H__ */
