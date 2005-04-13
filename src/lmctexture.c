#include "lmctexture.h"
#include <string.h>
#include <GL/glu.h>

typedef struct Tile Tile;

struct Tile
{
    GdkRectangle geometry;
    GLuint	 texture;
};

struct _LmcTexture
{
    /*< private >*/
    int ref_count;
    
    LmcBits *bits;
    
    GList *tiles;
};

static int
get_max_texture_width (void)
{
    return 512;
    return GL_MAX_TEXTURE_SIZE;
}

static int
get_max_texture_height (void)
{
    return 512;
    return GL_MAX_TEXTURE_SIZE;
}

static void
dump_error (const char *context)
{
  /* glGetError() is a server roundtrip */
  GLenum err;

  err = glGetError ();
  if (err != GL_NO_ERROR)
  {
      const GLubyte *message = gluErrorString (err);
      g_warning ("GL Error: %s [at %s]\n", message, context);
  }
}

static int
power_of_two_less_than_or_equal (int v)
{
    int t = 1;

#if 0
    g_print ("input: %d\n", v);
#endif
    
    g_return_val_if_fail (v >= 1, -1);

    while (t <= v)
	t *= 2;

    t /= 2;

#if 0
    g_print ("output: %d\n", t);
#endif
    
    return t;
}

static GList *
get_tile_sizes (int width, int height)
{
    GList *l1, *l2;
    GList *vertical, *horizontal, *tile_sizes;
    int x, y;
    int max_texture_width;
    int max_texture_height;

    g_return_val_if_fail (width > 0, NULL);
    g_return_val_if_fail (height > 0, NULL);
    
    max_texture_width = get_max_texture_width ();
    max_texture_height = get_max_texture_height ();

#if 0
    g_print ("maxw, maxh: %d %d\n", max_texture_width,
	     max_texture_height);
#endif
    
    horizontal = NULL;
    while (width)
    {
	int t = power_of_two_less_than_or_equal (MIN (width, max_texture_width));
	
	horizontal = g_list_prepend (horizontal, GINT_TO_POINTER (t));
	
	width -= t;
    }

    vertical = NULL;
    while (height)
    {
	int t = power_of_two_less_than_or_equal (MIN (height, max_texture_height));
	
	vertical = g_list_prepend (vertical, GINT_TO_POINTER (t));
	
	height -= t;
    }
    
    tile_sizes = NULL;
    x = 0;
    for (l1 = horizontal; l1 != NULL; l1 = l1->next)
    {
	y = 0;
	for (l2 = vertical; l2 != NULL; l2 = l2->next)
	{
	    GdkRectangle *rect = g_new0 (GdkRectangle, 1);
	    
	    rect->width = GPOINTER_TO_INT (l1->data);
	    rect->height = GPOINTER_TO_INT (l2->data);
	    rect->x = x;
	    rect->y = y;
	    
	    tile_sizes = g_list_prepend (tile_sizes, rect);
	    
	    y += GPOINTER_TO_INT (l2->data);
	}
	
	x += GPOINTER_TO_INT (l1->data);
    }
    
    g_list_free (horizontal);
    g_list_free (vertical);
    
    return tile_sizes;
}

static guchar *
create_buffer (LmcBits      *bits,
	       GdkRectangle *rect)
{
    guchar *buffer = g_malloc (4 * rect->width * rect->height);
    int i, j;
    int bpp;
    
    switch (bits->format)
    {
    case LMC_BITS_RGB_16:
	bpp = 2;
	break;
    case LMC_BITS_RGB_24:
	bpp = 3;
	break;
    case LMC_BITS_RGB_32:
    case LMC_BITS_RGBA_MSB_32:
    case LMC_BITS_ARGB_32:
	bpp = 4;
	break;
    default:
	g_assert_not_reached ();
	bpp = 4;
	break;
    }
    
    for (j = 0; j < rect->height; j++)
    {
	guchar *src;
	guint32 *dest;
	gint src_max;
	
	dest = (guint32 *)buffer + rect->width * j;
	
	if (j + rect->y >= bits->height)
	{
	    memset (dest, 0,  rect->width * 4);
	    continue;
	}
	
	src = bits->data_ + bits->rowstride * (j + rect->y) + bpp * rect->x;
	src_max = MIN (rect->width, bits->width - rect->x);
	
	for (i = 0; i < src_max; i++)
	{
	    guchar r,g,b,a;
	    
	    switch (bits->format)
	    {
	    case LMC_BITS_RGB_16:
	    {
		guint16 t = *(guint16 *)src;
		guint tr, tg, tb;
		
		tr = t & 0xf800;
		r = (tr >> 8) + (tr >> 13);
		
		tg = t & 0x07e0;
		g = (tg >> 3) + (tg >> 9);
		
		tb = t & 0x001f;
		b = (tb << 3) + (tb >> 2);
		
		a = 0xff;
	    }
	    break;
	    case LMC_BITS_RGB_24:
		r = src[0];
		g = src[1];
		b = src[2];
		a = 0xff;
		break;
	    case LMC_BITS_RGB_32:
	    {
		guint32 t = *(guint32 *)src;
		r = (t >> 16) & 0xff;
		g = (t >> 8) & 0xff;
		b = t & 0xff;
	        a = 0xff;
	    }
	    break;
	    case LMC_BITS_RGBA_MSB_32:
	    {
		guint tr, tg, tb;
		
		a = src[3];
		tr = src[0] * a + 0x80;
		r = (tr + (tr >> 8)) >> 8;
		tg = src[1] * a + 0x80;
		g = (tg + (tg >> 8)) >> 8;
		tb = src[2] * a + 0x80;
		b = (tb + (tb >> 8)) >> 8;
	    }
	    break;
	    case LMC_BITS_ARGB_32:
	    {
		guint32 t = *(guint32 *)src;
		r = (t >> 16) & 0xff;
		g = (t >> 8) & 0xff;
		b = t & 0xff;
		a = t >> 24;
	    }
	    break;
	    default:
		g_assert_not_reached();
		r = g = b = a = 0; /* Quiet GCC */
		break;
	    }
	    
	    *dest = (a << 24) | (r << 16) | (g << 8) | b;
	    
	    src += bpp;
	    dest++;
	}
	
	for (; i < rect->width; i++)
	{
	    *dest = 0;
	    dest++;
	}
    }
    
    return buffer;
}

static GLuint
allocate_texture_name (void)
{
    GLuint name;
    glGenTextures (1, &name);
    return name;
}

static Tile *
tile_new (LmcBits *bits,
	  GdkRectangle *tile_geometry)
{
    Tile *tile = g_new (Tile, 1);
    guchar *buffer;

    tile->geometry = *tile_geometry;
    tile->texture = allocate_texture_name ();

#if 0
    g_print ("allocated %p (%d)\n", tile, tile->texture);
#endif
    
    buffer = create_buffer (bits, tile_geometry);
    
#if 0
    g_print ("%x\n", *(int *)buffer);
#endif
    
    glBindTexture (GL_TEXTURE_2D, tile->texture);

    dump_error ("TexImage2D");
    
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    dump_error ("TexImage2D");
    
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    dump_error ("TexImage2D");
    
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    dump_error ("TexImage2D");
    
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    dump_error ("TexImage2D");

    glPixelStorei (GL_UNPACK_ROW_LENGTH, tile->geometry.width);
    dump_error ("TexImage2D");
    
    glPixelStorei (GL_UNPACK_ALIGNMENT, 4);

    dump_error ("TexImage2D");
    
    glTexImage2D (GL_TEXTURE_2D, 0 /* level */, GL_RGBA,
		  tile->geometry.width, tile->geometry.height, 0 /* border */,
		  GL_BGRA, GL_UNSIGNED_BYTE, buffer);

    dump_error ("TexImage2D");
    
    g_free (buffer);
    
    return tile;
}

static void
tile_free (Tile *tile)
{
#if 0
    g_print ("deleting %p, %d\n", tile, tile->texture);
#endif
    glDeleteTextures (1, &(tile->texture));

    g_free (tile);
}

static GList *
load_bits (LmcBits *bits)
{
    GList *tile_sizes = get_tile_sizes (bits->width, bits->height);
    GList *list, *tiles = NULL;
    
    for (list = tile_sizes; list != NULL; list = list->next)
    {
	GdkRectangle *tile_size = list->data;
	
	Tile *tile = tile_new (bits, tile_size);
	
	tiles = g_list_prepend (tiles, tile);
    }

    for (list = tile_sizes; list != NULL; list = list->next)
	g_free (list->data);
    g_list_free (tile_sizes);
    
    return tiles;
}

LmcTexture*
lmc_texture_new         (LmcBits    *bits)
{
    LmcTexture *texture;
    
    g_return_val_if_fail (bits != NULL, NULL);
    
    texture = g_new0 (LmcTexture, 1);
    
    texture->bits = lmc_bits_ref (bits);
    
    texture->tiles = load_bits (bits);

    texture->ref_count = 1;
    
    return texture;
}

LmcTexture*
lmc_texture_ref         (LmcTexture *texture)
{
    texture->ref_count++;
    return texture;
}

void
lmc_texture_unref       (LmcTexture *texture)
{
    if (--texture->ref_count == 0)
    {
	GList *list;
	
	lmc_bits_unref (texture->bits);

	for (list = texture->tiles; list != NULL; list = list->next)
	    tile_free (list->data);

	g_list_free (texture->tiles);
	
	g_free (texture);
    }
    else
	g_print ("new value: %d\n", texture->ref_count);
}

#if 0
static gboolean
xrect_intersect (const XRectangle *src1, const XRectangle *src2, XRectangle *dest)
{
  gint dest_x, dest_y;
  gint dest_w, dest_h;
  gint return_val;

  g_return_val_if_fail (src1 != NULL, FALSE);
  g_return_val_if_fail (src2 != NULL, FALSE);
  g_return_val_if_fail (dest != NULL, FALSE);

  return_val = FALSE;

  dest_x = MAX (src1->x, src2->x);
  dest_y = MAX (src1->y, src2->y);
  dest_w = MIN (src1->x + src1->width, src2->x + src2->width) - dest_x;
  dest_h = MIN (src1->y + src1->height, src2->y + src2->height) - dest_y;

  if (dest_w > 0 && dest_h > 0)
    {
      dest->x = dest_x;
      dest->y = dest_y;
      dest->width = dest_w;
      dest->height = dest_h;
      return_val = TRUE;
    }
  else
    {
      dest->width = 0;
      dest->height = 0;
    }

  return return_val;
}
#endif

void
lmc_texture_update_rect (LmcTexture *texture,
			 GdkRectangle *rect)
{
    GList *list;

    for (list = texture->tiles; list != NULL; list = list->next)
    {
	Tile *tile = list->data;
	GdkRectangle intersection;

	if (gdk_rectangle_intersect (&(tile->geometry), rect, &intersection))
	{
	    guchar *buffer;
#if 0
	    int i;
#endif
	    
	    glBindTexture (GL_TEXTURE_2D, tile->texture);

	    buffer = create_buffer (texture->bits, &intersection);

#if 0
	    for (i = 0; i < 4 * intersection.width * intersection.height; ++i)
		buffer[i] = g_random_int ();
#endif
	    
#if 0
	    g_print ("updating %d %d %d %d\n",
		     intersection.x, intersection.y, intersection.width, intersection.height);
#endif

	    intersection.x -= tile->geometry.x;
	    intersection.y -= tile->geometry.y;

#if 0
	    g_print ("tile     %d %d %d %d\n",
		     tile->geometry.x, tile->geometry.y, tile->geometry.width, tile->geometry.height);
#endif
	    
	    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	    dump_error ("TexImage2D");
	    
	    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	    dump_error ("TexImage2D");
	    
	    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	    dump_error ("TexImage2D");
	    
	    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	    dump_error ("TexImage2D");

	    glPixelStorei (GL_UNPACK_ROW_LENGTH, intersection.width);
	    dump_error ("TexImage2D");
    
	    glPixelStorei (GL_UNPACK_ALIGNMENT, 4);

	    glTexSubImage2D (GL_TEXTURE_2D, 0,
			     intersection.x,
			     intersection.y,
			     intersection.width,
			     intersection.height,
			     GL_BGRA, GL_UNSIGNED_BYTE,
			     buffer);

	    dump_error ("glTexSubImage2D");
	    
	    g_free (buffer);
	}
    }
}

static void
set_clip_region (GdkRegion *region, int x, int y)
{

    GdkRectangle *rects;
    int n_rects, i;

    /* clear stencil buffer */
    glClearStencil (0);
    glClear (GL_STENCIL_BUFFER_BIT);
    glStencilFunc (GL_NEVER, 1, 1);

    glEnable (GL_STENCIL_TEST);

    glStencilOp (GL_REPLACE, GL_REPLACE, GL_REPLACE);
    
    /* draw region to stencil buffer */

    gdk_region_offset (region, x, y);

    gdk_region_get_rectangles (region, &rects, &n_rects);

    glDisable (GL_TEXTURE_2D);
    for (i = 0; i < n_rects; ++i)
    {
	glBegin(GL_QUADS);

	glVertex3i (rects[i].x, rects[i].y, 0);
	glVertex3i (rects[i].x + rects[i].width, rects[i].y, 0);
	glVertex3i (rects[i].x + rects[i].width, rects[i].y + rects[i].height, 0);
	glVertex3i (rects[i].x, rects[i].y + rects[i].height, 0);

	glEnd ();
    }

    g_free (rects);

    gdk_region_offset (region, -x, -y);

    glStencilFunc (GL_EQUAL, 0x1, 0x1);
    glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);
    
    glEnable (GL_TEXTURE_2D);
}

static void
unset_clip_region ()
{
    glDisable (GL_STENCIL_TEST);
}

static gboolean
region_intersects_rect (GdkRegion *region, GdkRectangle *rect)
{
    return (gdk_region_rect_in (region, rect) != GDK_OVERLAP_RECTANGLE_OUT);

}

static void
print_gdk_region (const char *name, GdkRegion *region)
{
    GdkRectangle *rects;
  int i, n_rects;
  
  gdk_region_get_rectangles (region, &rects, &n_rects);
  
  g_print ("region \"%s\":\n", name);
  for (i = 0; i < n_rects; ++i)
    g_print ("  %d %d %d %d\n", rects[i].x, rects[i].y, rects[i].width, rects[i].height);
  g_free (rects);
}

/**
 * lmc_texture_draw:
 * @texture: 
 * @alpha: 
 * @x: 
 * @y: 
 * @clip: 
 * 
 * Draw the texture, the clip is in window coordinates.
 **/
void
lmc_texture_draw        (LmcTexture   *texture,
			 double        alpha,
			 int           x,
			 int           y,
			 GdkRegion    *clip)
{
    GList *list;

    glPushAttrib (GL_TEXTURE_BIT);
    glEnable (GL_TEXTURE_2D);
    glDisable (GL_LIGHTING);
    
    dump_error ("gldisable light");

    /* Setup alhpa */

    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable (GL_BLEND);

#if 0
    glShadeModel(GL_SMOOTH);
#endif
    dump_error ("glenable");

    g_assert (glIsEnabled (GL_TEXTURE_2D));

#if 0
    g_print ("alpha %f\n", alpha);
#endif

    set_clip_region (clip, x, y);

#if 0
    glColor4f (g_random_double(),
	       g_random_double(),
	       g_random_double(), 0.3);
    
    glBegin (GL_QUADS);

    glVertex3i (0, 0, 0);
    glVertex3i (1600, 0, 0);
    glVertex3i (1600, 1200, 0);
    glVertex3i (0, 1200, 0);
    
    glEnd ();

    goto out;
#endif
    
    glColor4f (1.0, 1.0, 1.0, alpha);

    /* Emit quads */
    for (list = texture->tiles; list != NULL; list = list->next)
    {
	Tile *tile = list->data;

	if ((!clip || region_intersects_rect (clip, &tile->geometry)))
	{
	    int translated_x = tile->geometry.x + x;
	    int translated_y = tile->geometry.y + y;
	    
	    glBindTexture (GL_TEXTURE_2D, tile->texture);
#if 0
	    g_print ("bound %p, %d\n", tile, tile->texture);
#endif
	    
	    dump_error ("glbindtexture");
	    
	    glBegin (GL_QUADS);
	    
	    /* corner 1 */
#if 0
	    glColor4f (1.0, 0.2, 0.2, alpha);
#endif
	    glTexCoord2f (0.0, 0.0);
	    glVertex3i (translated_x, translated_y, 0);
	    
	    /* corner 2 */
#if 0
	    glColor4f (0.0, 0.4, 0.2, alpha);
#endif
	    glTexCoord2f (1.0, 0.0);
	    glVertex3i (translated_x + tile->geometry.width, translated_y, 0);
	    
	    /* corner 3 */
#if 0
	    glColor4f (0.0, 0.2, 0.8, alpha);
#endif
	    glTexCoord2f (1.0, 1.0);
	    glVertex3i (translated_x + tile->geometry.width, translated_y + tile->geometry.height, 0);
	    
	    /* corner 4 */
#if 0
	    glColor4f (0.8, 0.8, 0.0, alpha);
#endif
	    glTexCoord2f (0.0, 1.0);
	    glVertex3i (translated_x, translated_y + tile->geometry.height, 0);
	    
	    glEnd ();
	    dump_error ("glEnd");

	    unset_clip_region ();
	    
	    glColor4f (0.8, 0.0, 0.8, 0.2);
	    
	    glDisable (GL_TEXTURE_2D);
	    glBegin (GL_QUADS);
	    glVertex3i (translated_x, translated_y, 0);
	    glVertex3i (translated_x + tile->geometry.width, translated_y, 0);
	    glVertex3i (translated_x + tile->geometry.width,
			translated_y + tile->geometry.height, 0);
	    glVertex3i (translated_x, translated_y + tile->geometry.height, 0);
	    glEnd ();
	    glEnable (GL_TEXTURE_2D);

	    set_clip_region (clip, x, y);
	    glColor4f (1.0, 1.0, 1.0, alpha);
	}

	else
	{
#if 0
	    int translated_x = tile->geometry.x + x;
	    int translated_y = tile->geometry.y + y;

	    print_gdk_region ("clip", clip);
	    
	    glColor4f (g_random_double(),
		       g_random_double(),
		       g_random_double(), 0.2);
	    
	    glDisable (GL_TEXTURE_2D);
	    glBegin (GL_QUADS);
	    glVertex3i (translated_x, translated_y, 0);
	    glVertex3i (translated_x + tile->geometry.width, translated_y, 0);
	    glVertex3i (translated_x + tile->geometry.width,
			translated_y + tile->geometry.height, 0);
	    glVertex3i (translated_x, translated_y + tile->geometry.height, 0);
	    glEnd ();
	    glEnable (GL_TEXTURE_2D);
	    glColor4f (1.0, 1.0, 1.0, alpha);
#endif
	}
    }

    glDisable (GL_TEXTURE_2D);

#if 0
    glColor4f (0.0, 0.0, 0.0, 0.1);
    for (list = texture->tiles; list != NULL; list = list->next)
    {
	Tile *tile = list->data;
	int translated_x = tile->geometry.x + x;
	int translated_y = tile->geometry.y + y;

	glBegin (GL_QUADS);
#if 0
	glColor4f (g_random_double(),
		   g_random_double(),
		   g_random_double(), 0.3);
#endif
	glVertex3i (translated_x, translated_y, 0);
	glVertex3i (translated_x + tile->geometry.width, translated_y, 0);
	glVertex3i (translated_x + tile->geometry.width,
		    translated_y + tile->geometry.height, 0);
	glVertex3i (translated_x, translated_y + tile->geometry.height, 0);

	glEnd ();
    }
#endif

    unset_clip_region ();

 out:
    
    glPopAttrib ();
}
