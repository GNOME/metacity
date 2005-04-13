#ifndef __LMC_TEXTURE_H__
#define __LMC_TEXTURE_H__

#include "lmcbits.h"
#include "lmctypes.h"
#include <gdk/gdk.h>

#include <X11/Xutil.h>		/* For Region */
#include <GL/gl.h>		/* For GLuint */

G_BEGIN_DECLS

typedef struct _LmcTexture LmcTexture;

LmcTexture* lmc_texture_new         (LmcBits    *bits);
LmcTexture* lmc_texture_ref         (LmcTexture *texture);
void        lmc_texture_unref       (LmcTexture *texture);
void        lmc_texture_update_rect (LmcTexture *texture,
				     GdkRectangle *rect);
void        lmc_texture_draw        (LmcTexture *texture,
				     double      alpha,
				     int         x,
				     int         y,
				     GdkRegion  *clip);

G_END_DECLS

#endif /* __LMC_TEXTURE_H__ */
