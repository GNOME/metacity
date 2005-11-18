#include <sys/types.h>
#include <sys/shm.h>
#include <X11/Xlib.h>
#include "cwindow.h"
#include "errors.h"
#include "compositor.h"
#include "matrix.h"
#include <X11/extensions/Xcomposite.h>
#include <math.h>
#include <string.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/XShm.h>
#include "lmctexture.h"
#include "window.h"


typedef struct Geometry Geometry;
typedef struct FreezeInfo FreezeInfo;

struct Geometry
{
    int x;
    int y;
    int width;
    int height;
    int border_width;
};

struct FreezeInfo
{
    Geometry	geometry;
    Pixmap	pixmap;
};

/* Unlike MetaWindow, there's one of these for _all_ toplevel windows,
 * override redirect or not. We also track unmapped windows as
 * otherwise on window map we'd have to determine where the
 * newly-mapped window was in the stack. A MetaCompositorWindow may
 * correspond to a metacity window frame rather than an application
 * window.
 */
struct CWindow
{
    Window xwindow;

#ifdef HAVE_COMPOSITE_EXTENSIONS
    MetaCompositor *compositor;
    
    Geometry geometry;
    
    Damage          damage;
    XserverRegion   last_painted_extents;
    
    XserverRegion   border_size;
    
    FreezeInfo *freeze_info;
    
    unsigned int managed : 1;
    unsigned int damaged : 1;
    unsigned int viewable : 1;
    unsigned int input_only : 1;
    unsigned int translucent : 1;
    
    unsigned int screen_index : 8;
    
    Visual *visual;
    
    Distortion *distortions;
    int n_distortions;
    
    int depth;
    XImage *image;
    XShmSegmentInfo shm_info;
    Pixmap shm_pixmap;
    GC shm_gc;
    LmcBits *bits;
    XserverRegion parts_region;
    LmcTexture *texture;
    
    long damage_serial;
#endif  
};

static Display *cwindow_get_xdisplay (CWindow *cwindow);

#ifdef HAVE_COMPOSITE_EXTENSIONS
void
cwindow_free (CWindow *cwindow)
{
    g_assert (cwindow->damage != None);
    
    meta_error_trap_push (meta_compositor_get_display (cwindow->compositor));
    /* This seems to cause an error if the window
     * is destroyed?
     */
    g_print ("destroying damage for %lx\n", cwindow->xwindow);
    XDamageDestroy (cwindow_get_xdisplay (cwindow), cwindow->damage);
    
    /* Free our window pixmap name */
    if (cwindow->freeze_info)
	cwindow_thaw (cwindow);
    
    meta_error_trap_pop (meta_compositor_get_display (cwindow->compositor),
			 FALSE);
    
    g_free (cwindow);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
XserverRegion
cwindow_extents (CWindow *cwindow)
{
    Geometry *geometry;
    XRectangle r;
    
    if (cwindow->freeze_info)
	geometry = &cwindow->freeze_info->geometry;
    else
	geometry = &cwindow->geometry;
    
    r.x = geometry->x;
    r.y = geometry->y;
    r.width = geometry->width;
    r.height = geometry->height;
    
    return XFixesCreateRegion (cwindow_get_xdisplay (cwindow), &r, 1);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

static gboolean
cwindow_has_alpha (CWindow *cwindow)
{
    return TRUE;
}

XserverRegion
cwindow_get_opaque_region (CWindow *cwindow)
{
    return XFixesCreateRegion (cwindow_get_xdisplay (cwindow), NULL, 0);
}

static void
cwindow_queue_paint (CWindow *cwindow)
{
    XserverRegion region;
    MetaScreen *screen;
    
    region = cwindow_extents (cwindow);
    screen = cwindow_get_screen (cwindow);
    
    meta_compositor_invalidate_region (cwindow->compositor,
				       screen,
				       region);
    
    XFixesDestroyRegion (cwindow_get_xdisplay (cwindow), region);
}

void
cwindow_set_translucent (CWindow *cwindow, gboolean translucent)
{
#if 0
    cwindow->translucent = !!translucent;
#endif
    
    cwindow_queue_paint (cwindow);
}

Drawable
cwindow_get_drawable (CWindow *cwindow)
{
    if (cwindow->freeze_info)
    {
	return cwindow->freeze_info->pixmap;
    }
    else
    {
	return cwindow->xwindow;
    }
}

void
cwindow_destroy_last_painted_extents (CWindow *cwindow)
{
    if (cwindow->last_painted_extents)
    {
	XFixesDestroyRegion (cwindow_get_xdisplay (cwindow), cwindow->last_painted_extents);
	cwindow->last_painted_extents = None;
    }
}

void
cwindow_set_last_painted_extents (CWindow *cwindow, XserverRegion extents)
{
    cwindow_destroy_last_painted_extents (cwindow);
    cwindow->last_painted_extents = extents;
}

MetaScreen*
cwindow_get_screen (CWindow *cwindow)
{
    MetaScreen *screen;
    GSList *tmp;
    
    screen = NULL;
    tmp = meta_compositor_get_display (cwindow->compositor)->screens;
    while (tmp != NULL)
    {
	MetaScreen *s = tmp->data;
	
	if (s->number == cwindow->screen_index)
        {
	    screen = s;
	    break;
        }
	
	tmp = tmp->next;
    }
    g_assert (screen != NULL);
    
    return screen;
}

/* From luminocity */
static void
destroy_bits (gpointer  data)
{
    XShmSegmentInfo *shm_info = data;
    
    shmdt (shm_info->shmaddr);
    
    g_free (shm_info);
}

static gboolean
create_window_image (CWindow *cwindow)
{
    Display *xdisplay = cwindow_get_xdisplay (cwindow);
    int image_size;
    XGCValues gcv;
    LmcBitsFormat format;
    Geometry *geometry = cwindow->freeze_info?
	&cwindow->freeze_info->geometry :
	&cwindow->geometry;
    
    g_return_val_if_fail (cwindow->image == NULL, False);
    
    if (cwindow->depth == 16 &&
	cwindow->visual->red_mask == 0xf800 &&
	cwindow->visual->green_mask == 0x7e0 &&
	cwindow->visual->blue_mask == 0x1f)
    {
	format = LMC_BITS_RGB_16;
    }
    else if (cwindow->depth == 24 &&
	     cwindow->visual->red_mask == 0xff0000 &&
	     cwindow->visual->green_mask == 0xff00 &&
	     cwindow->visual->blue_mask == 0xff)
    {
	format = LMC_BITS_RGB_32;
    }
    else if (cwindow->depth == 32 &&
	     cwindow->visual->red_mask == 0xff0000 &&
	     cwindow->visual->green_mask == 0xff00 &&
	     cwindow->visual->blue_mask == 0xff)
    {
	format = LMC_BITS_ARGB_32;
    }
    else
    {
	g_warning ("Unknown visual format depth=%d, r=%#lx/g=%#lx/b=%#lx",
		   cwindow->depth, cwindow->visual->red_mask, cwindow->visual->green_mask, cwindow->visual->blue_mask);
	
	return FALSE;
    }
    
    cwindow->image = XCreateImage (xdisplay,
				   cwindow->visual,
				   cwindow->depth,
				   ZPixmap,
				   0,
				   0,
				   geometry->width,
				   geometry->height,
				   32,
				   0);
    if (!cwindow->image)
	return FALSE;
    
    image_size = (cwindow->image->bytes_per_line * geometry->height);
    
    cwindow->shm_info.shmid = shmget (IPC_PRIVATE, image_size, IPC_CREAT|0600);
    if (cwindow->shm_info.shmid < 0)
    {
	XDestroyImage (cwindow->image);
	cwindow->image = NULL;
	return FALSE;
    }
    
    cwindow->shm_info.shmaddr = (char *) shmat (cwindow->shm_info.shmid, 0, 0);
    if (cwindow->shm_info.shmaddr == ((char *) -1))
    {
	XDestroyImage (cwindow->image);
	cwindow->image = NULL;
	shmctl (cwindow->shm_info.shmid, IPC_RMID, 0);
	return FALSE;
    }
    
    meta_error_trap_push_with_return (meta_compositor_get_display (cwindow->compositor));
    
    cwindow->shm_info.readOnly = False;
    XShmAttach (xdisplay, &cwindow->shm_info);
    XSync (xdisplay, False);
    
    if (meta_error_trap_pop_with_return (meta_compositor_get_display (cwindow->compositor), FALSE))
    {
	XDestroyImage (cwindow->image);
	cwindow->image = NULL;
	shmdt (&cwindow->shm_info.shmaddr);
	shmctl (cwindow->shm_info.shmid, IPC_RMID, 0);
	return FALSE;
    }
    
    /* Detach now so we clean up on abnormal exit */
    shmctl (cwindow->shm_info.shmid, IPC_RMID, 0);
    
    cwindow->image->data = cwindow->shm_info.shmaddr;
    cwindow->image->obdata = (char *) &cwindow->shm_info;
    cwindow->shm_pixmap = XShmCreatePixmap (xdisplay,
					    cwindow->xwindow,
					    cwindow->image->data,
					    &cwindow->shm_info,
					    geometry->width,
					    geometry->height,
					    cwindow->depth);
    
    gcv.graphics_exposures = False;
    gcv.subwindow_mode = IncludeInferiors;
    cwindow->shm_gc = XCreateGC (xdisplay,
				 cwindow->xwindow,
				 GCGraphicsExposures|GCSubwindowMode,
				 &gcv);

    cwindow->bits = lmc_bits_new (format,
				  geometry->width, geometry->height,
				  cwindow->image->data,
				  cwindow->image->bytes_per_line,
				  destroy_bits,
				  g_memdup (&cwindow->shm_info, sizeof (XShmSegmentInfo)));
    
    return TRUE;
}

static void
initialize_damage (CWindow *cwindow)
{
    create_window_image (cwindow);
    
    cwindow->damage_serial = 0;
    
    cwindow_queue_paint (cwindow);
}

CWindow *
cwindow_new (MetaCompositor *compositor,
	     Window xwindow,
	     XWindowAttributes *attrs)
{
    CWindow *cwindow;
    Damage damage;
    gboolean is_gl_window;
    
    /* Create Damage object to monitor window damage */
    meta_error_trap_push (meta_compositor_get_display (compositor));
    g_print ("creating damage for %lx\n", xwindow);

    is_gl_window = (xwindow == meta_compositor_get_gl_window (compositor));
    
    if (!is_gl_window)
    {
	damage = XDamageCreate (meta_compositor_get_display (compositor)->xdisplay,
				xwindow, XDamageReportNonEmpty);
    }
    else
    {
	damage = None;
    }
    
    meta_error_trap_pop (meta_compositor_get_display (compositor), FALSE);
    
    if (damage == None && !is_gl_window)
	return NULL;

    cwindow = g_new0 (CWindow, 1);
    
    cwindow->compositor = compositor;
    cwindow->xwindow = xwindow;
    cwindow->screen_index = XScreenNumberOfScreen (attrs->screen);
    cwindow->damage = damage;
    cwindow->depth = attrs->depth;

    if (is_gl_window)
    {
	return cwindow;
    }
    
    cwindow->freeze_info = NULL;
    
    cwindow->geometry.x = attrs->x;
    cwindow->geometry.y = attrs->y;
    cwindow->geometry.width = attrs->width;
    cwindow->geometry.height = attrs->height;
    cwindow->geometry.border_width = attrs->border_width;
    
    if (attrs->class == InputOnly)
	cwindow->input_only = TRUE;
    else
	cwindow->input_only = FALSE;
    
    cwindow->visual = attrs->visual;
    
    /* viewable == mapped for the root window, since root can't be unmapped */
    cwindow->viewable = (attrs->map_state == IsViewable);
    
    cwindow->parts_region = XFixesCreateRegion (cwindow_get_xdisplay (cwindow), 0, 0);
    
    if (!cwindow->input_only)
	initialize_damage (cwindow);
    
    return cwindow;
}

XID *
cwindow_get_xid_address (CWindow *cwindow)
{
    return &cwindow->xwindow;
}

static Display *
cwindow_get_xdisplay (CWindow *cwindow)
{
    return meta_compositor_get_display (cwindow->compositor)->xdisplay;
}


Window
cwindow_get_xwindow (CWindow *cwindow)
{
    return cwindow->xwindow;
}

gboolean
cwindow_get_viewable (CWindow *cwindow)
{
    return cwindow->viewable;
}

gboolean
cwindow_get_input_only (CWindow *cwindow)
{
    return cwindow->input_only;
}

Visual *
cwindow_get_visual (CWindow *cwindow)
{
    return cwindow->visual;
}

XserverRegion
cwindow_get_last_painted_extents (CWindow *cwindow)
{
    return cwindow->last_painted_extents;
}


int
cwindow_get_x (CWindow *cwindow)
{
    return cwindow->geometry.x;
}

int
cwindow_get_y (CWindow *cwindow)
{
    return cwindow->geometry.y;
}

int
cwindow_get_width (CWindow *cwindow)
{
    return cwindow->geometry.width;
}

int
cwindow_get_height (CWindow *cwindow)
{
    return cwindow->geometry.height;
}

int
cwindow_get_border_width (CWindow *cwindow)
{
    return cwindow->geometry.border_width;
}

Damage
cwindow_get_damage (CWindow *cwindow)
{
    return cwindow->damage;
}

MetaCompositor *
cwindow_get_compositor (CWindow *cwindow)
{
    return cwindow->compositor;
}

void
cwindow_set_x (CWindow *cwindow, int x)
{
    cwindow->geometry.x = x;
}

void
cwindow_set_y (CWindow *cwindow, int y)
{
    cwindow->geometry.y = y;
}

void
cwindow_set_width (CWindow *cwindow, int width)
{
    cwindow->geometry.width = width;
}

void
cwindow_set_height (CWindow *cwindow, int height)
{
    cwindow->geometry.height = height;
}


void
cwindow_set_viewable (CWindow *cwindow, gboolean viewable)
{
    viewable = !!viewable;
    if (cwindow->viewable != viewable)
    {
	cwindow_queue_paint (cwindow);
	cwindow->viewable = viewable;
    }
} 

void
cwindow_set_border_width (CWindow *cwindow, int border_width)
{
    cwindow->geometry.border_width = border_width;
}

static XFixed
double_to_fixed (gdouble d)
{
    return XDoubleToFixed (d);
}

static void
convert_matrix (Matrix3 *matrix, XTransform *trans)
{
#if 0
    matrix3_transpose (matrix);
#endif
    trans->matrix[0][0] = double_to_fixed (matrix->coeff[0][0]);
    trans->matrix[1][0] = double_to_fixed (matrix->coeff[1][0]);
    trans->matrix[2][0] = double_to_fixed (matrix->coeff[2][0]);
    trans->matrix[0][1] = double_to_fixed (matrix->coeff[0][1]);
    trans->matrix[1][1] = double_to_fixed (matrix->coeff[1][1]);
    trans->matrix[2][1] = double_to_fixed (matrix->coeff[2][1]);
    trans->matrix[0][2] = double_to_fixed (matrix->coeff[0][2]);
    trans->matrix[1][2] = double_to_fixed (matrix->coeff[1][2]);
    trans->matrix[2][2] = double_to_fixed (matrix->coeff[2][2]);
}

gboolean
cwindow_is_translucent (CWindow *cwindow)
{
    return FALSE;
    MetaCompositor *compositor = cwindow_get_compositor (cwindow);
    MetaWindow *window = meta_display_lookup_x_window (meta_compositor_get_display (compositor), cwindow_get_xwindow (cwindow));
    return (window != NULL &&
	    window == meta_compositor_get_display (compositor)->grab_window &&
	    (meta_grab_op_is_resizing (meta_compositor_get_display (compositor)->grab_op) ||
	     meta_grab_op_is_moving (meta_compositor_get_display (compositor)->grab_op)));
}


static XRectangle
bbox (Quad *q)
{
    int x1, x2, y1, y2;
    XRectangle result;
    int i;
    
    x2 = x1 = q->points[0].x;
    y2 = y1 = q->points[0].y;
    
    for (i = 0; i < 4; ++i)
	x1 = MIN (x1, q->points[i].x);
    
    for (i = 0; i < 4; ++i)
	y1 = MIN (y1, q->points[i].y);
    
    for (i = 0; i < 4; ++i)
	x2 = MAX (x2, q->points[i].x);
    
    for (i = 0; i < 4; ++i)
	y2 = MAX (y2, q->points[i].y);
    
    result.x = x1;
    result.y = y1;
    result.width = x2 - x1 + 1;
    result.height = y2 - y1 + 1;
    
#if 0
    g_print ("bbox: %d %d %d %d\n", result.x, result.y, result.width, result.height);
#endif
    
    return result;
}

static void
compute_transform (int x, int y,
		   int width, int height,
		   Quad *destination,
		   XTransform *transform)
{
    int tx, ty;
    int i;
    Quad tmp = *destination;
    Matrix3 matrix;
    
    /* Translate destination so it starts in (x, y);
     * 
     * We will position it correctly with the composite request
     * Coordinates are source coordinates
     *
     * I believe this is a hackaround a bug in render transformation (basically
     * it translates source coordinates, not destination as it's supposed to).
     */
    tx = bbox (&tmp).x - x;
    ty = bbox (&tmp).y - y;
    
    for (i = 0; i < 4; ++i)
    {
	tmp.points[i].x -= tx;
	tmp.points[i].y -= ty;
    }
    
    /* Compute the matrix */
    matrix3_identity (&matrix);
    
#if 0
    matrix3_translate (&matrix, (gdouble)-x, (gdouble)-y);
#endif
    
#if 0
    g_print ("mapping from %d %d %d %d to (%d %d) (%d %d) (%d %d) (%d %d)\n", x, y, width, height,
	     tmp.points[0].x, tmp.points[0].y,
	     tmp.points[1].x, tmp.points[1].y,
	     tmp.points[2].x, tmp.points[2].y,
	     tmp.points[3].x, tmp.points[3].y);
#endif
    
    transform_matrix_perspective (x, y,
				  x + width - 1,
				  y + height - 1,
				  
				  tmp.points[0].x, tmp.points[0].y,
				  tmp.points[1].x, tmp.points[1].y,
				  tmp.points[2].x, tmp.points[2].y,
				  tmp.points[3].x, tmp.points[3].y,
				  
				  &matrix);
    
    matrix3_invert (&matrix);
    
    /* convert to XTransform */
    convert_matrix (&matrix, transform);
}

static void
print_region (Display *dpy, const char *name, XserverRegion region)
{
    XRectangle *rects;
    int i, n_rects;
    
    rects = XFixesFetchRegion (dpy, region, &n_rects);
    
    g_print ("region \"%s\":\n", name);
    for (i = 0; i < n_rects; ++i)
	g_print ("  %d %d %d %d\n", rects[i].x, rects[i].y, rects[i].width, rects[i].height);
    XFree (rects);
}

void
cwindow_process_damage_notify (CWindow *cwindow, XDamageNotifyEvent *event)
{
    MetaScreen *screen;
    MetaWindow *window;
    
    window = meta_display_lookup_x_window (meta_compositor_get_display (cwindow->compositor),
					   cwindow_get_xwindow (cwindow));
#if 0
    if (window)
	g_print ("damage on %lx (%s)\n", cwindow->xwindow, window->title);
    else
	g_print ("damage on unknown window\n");
#endif
    
    screen = cwindow_get_screen (cwindow);

    if (cwindow->xwindow == screen->xroot)
	g_print ("huh????\n");
    
    {
	XserverRegion region;
	
	region = XFixesCreateRegion (cwindow_get_xdisplay (cwindow), NULL, 0);

	meta_error_trap_push (meta_compositor_get_display (cwindow->compositor));
#if 0
	g_print ("cleaning damage for %lx\n", cwindow->xwindow);
#endif
	XDamageSubtract (cwindow_get_xdisplay (cwindow),
			 cwindow_get_damage (cwindow),
			 None,
			 region);
	meta_error_trap_pop (meta_compositor_get_display (cwindow->compositor), FALSE);
	
	XFixesUnionRegion (cwindow_get_xdisplay (cwindow),
			   cwindow->parts_region, cwindow->parts_region, region);

#if 0
	print_region (cwindow_get_xdisplay (cwindow), "parts region is now", cwindow->parts_region);
#endif

	XFixesTranslateRegion (cwindow_get_xdisplay (cwindow),
			       region,
			       cwindow_get_x (cwindow),
			       cwindow_get_y (cwindow));
	
	meta_compositor_invalidate_region (cwindow->compositor, screen, region);

	XFixesDestroyRegion (cwindow_get_xdisplay (cwindow), region);
	
#if 0
	XFixesDestroyRegion (cwindow_get_xdisplay (cwindow), region);

	rect.width = screen->width;
	rect.height = screen->height;
	rect.x = 0;
	rect.y = 0;

	region = XFixesCreateRegion (cwindow_get_xdisplay (cwindow), &rect, 1);
	
#endif
    }
}

static void
destroy_window_image (CWindow *cwindow);

void
cwindow_process_configure_notify (CWindow *cwindow, XConfigureEvent *event)
{
    MetaScreen *screen;
    int old_width, old_height;
    
    screen = cwindow_get_screen (cwindow);

    if (cwindow->xwindow == meta_compositor_get_gl_window (cwindow->compositor))
	return;

    if (cwindow_get_last_painted_extents (cwindow) && !cwindow->freeze_info)
    {
	meta_compositor_invalidate_region (cwindow->compositor, screen, cwindow_get_last_painted_extents (cwindow));
	cwindow_set_last_painted_extents (cwindow, None);
    }
    
    cwindow_set_x (cwindow, event->x);
    cwindow_set_y (cwindow, event->y);
    old_width = cwindow_get_width (cwindow);
    cwindow_set_width (cwindow, event->width);
    old_height = cwindow_get_height (cwindow);
    cwindow_set_height (cwindow, event->height);
    cwindow_set_border_width (cwindow, event->border_width);
    
    if (cwindow->freeze_info)
    {
	return;
    }
    else
    {
	if (old_width != cwindow_get_width (cwindow) || 
	    old_height != cwindow_get_height (cwindow))
	{
	    if (cwindow->texture)
	    {
		lmc_texture_unref (cwindow->texture);
		cwindow->texture = NULL;
	    }
	    
	    destroy_window_image (cwindow);
	    initialize_damage (cwindow);
	}
	
	cwindow_queue_paint (cwindow);
    }
}

void
cwindow_set_transformation (CWindow *cwindow,
			    const Distortion *distortions,
			    int n_distortions)
{
    if (cwindow->distortions)
    {
	g_free (cwindow->distortions);
	cwindow->distortions = NULL;
	cwindow->n_distortions = 0;
    }
    
    if (n_distortions)
    {
	cwindow->distortions = g_memdup (distortions, n_distortions * sizeof (Distortion));
	cwindow->n_distortions = n_distortions;
    }
}

void
cwindow_freeze (CWindow *cwindow)
{
    if (!cwindow)
	return;
    
    if (cwindow->freeze_info)
    {
	meta_print_backtrace();
	return;
    }
    
    meta_error_trap_push (meta_compositor_get_display (cwindow->compositor));
    
    cwindow->freeze_info = g_new (FreezeInfo, 1);
    cwindow->freeze_info->geometry = cwindow->geometry;
    
    cwindow->freeze_info->pixmap =
	XCompositeNameWindowPixmap (cwindow_get_xdisplay (cwindow),
				    cwindow->xwindow);
    
    meta_error_trap_pop (meta_compositor_get_display (cwindow->compositor), FALSE);
}

void
cwindow_thaw (CWindow *cwindow)
{
    if (!cwindow->freeze_info)
	return;
    
    if (cwindow_get_last_painted_extents (cwindow))
    {
	meta_compositor_invalidate_region (cwindow->compositor,
					   cwindow_get_screen (cwindow),
					   cwindow_get_last_painted_extents (cwindow));
	cwindow_set_last_painted_extents (cwindow, None);
    }
    
    if (cwindow->texture)
    {
	lmc_texture_unref (cwindow->texture);
	cwindow->texture = NULL;
    }
    
    destroy_window_image (cwindow);
    initialize_damage (cwindow);
    
    g_free (cwindow->freeze_info);
    cwindow->freeze_info = NULL;
    
    cwindow_queue_paint (cwindow);
}

static void
cwindow_undamage (CWindow *cwindow);


static GdkRectangle *
server_region_to_gdk_rects (Display *dpy, XserverRegion region, int *n_rects)
{
    int dummy, i;
    XRectangle *xrects;
    GdkRectangle *gdk_rects;

    if (!n_rects)
	n_rects = &dummy;

    if (!region)
    {
	*n_rects = 0;
	return NULL;
    }
    
    xrects = XFixesFetchRegion (dpy, region, n_rects);

    gdk_rects = g_new (GdkRectangle, *n_rects);
    
    for (i = 0; i < *n_rects; ++i)
    {
	gdk_rects[i].x = xrects[i].x;
	gdk_rects[i].y = xrects[i].y;
	gdk_rects[i].width = xrects[i].width;
	gdk_rects[i].height = xrects[i].height;
    }

    return gdk_rects;
}

static void
destroy_window_image (CWindow *cwindow)
{
    Display *xdisplay = cwindow_get_xdisplay (cwindow);
    
    if (cwindow->image)
    {
	XShmDetach (xdisplay, &cwindow->shm_info);
	XSync (xdisplay, False);
	
	cwindow->image->data = NULL;
	cwindow->image->obdata = NULL;
	XDestroyImage (cwindow->image);
	cwindow->image = NULL;
	
	XFreePixmap (xdisplay, cwindow->shm_pixmap);
	cwindow->shm_pixmap = None;
	
	XFreeGC (xdisplay, cwindow->shm_gc);
	cwindow->shm_gc = None;
	
	lmc_bits_unref (cwindow->bits);
	cwindow->bits = NULL;
    }
}

void
cwindow_draw (CWindow *cwindow,
	      Picture destination,
	      XserverRegion clip_region)
{
    XRenderPictFormat *format;
    int i;
    Picture picture;
    XRenderPictureAttributes pa;

    if (cwindow_get_input_only (cwindow))
	return;
    
    if (!cwindow_get_viewable (cwindow))
	return;

    {
	GdkRectangle *rects;
	int n_rects;
	GdkRegion *gdk_clip_region;
	
	Geometry *geometry = cwindow->freeze_info?
	    &cwindow->freeze_info->geometry :
	    &cwindow->geometry;

#if 0
	g_print ("drawing: %lx\n", cwindow->xwindow);
#endif
    
	cwindow_undamage (cwindow);
	
	if (!cwindow->texture)
	{
	    cwindow->texture = lmc_texture_new (cwindow->bits);
	}

	g_assert (cwindow->texture);

	rects = server_region_to_gdk_rects (cwindow_get_xdisplay (cwindow),
					    clip_region, &n_rects);

	gdk_clip_region = gdk_region_new ();

	for (i = 0; i < n_rects; ++i)
	    gdk_region_union_with_rect (gdk_clip_region, &rects[i]);

	if (rects)
	{
	    gdk_region_offset (gdk_clip_region, -geometry->x, -geometry->y);
	    lmc_texture_draw (cwindow_get_screen (cwindow), cwindow->texture, 1.0, geometry->x, geometry->y, gdk_clip_region);
	    gdk_region_offset (gdk_clip_region, geometry->x, geometry->y);
	}
	else
	    lmc_texture_draw (cwindow_get_screen (cwindow),
			      cwindow->texture, 1.0, geometry->x, geometry->y, NULL);

	gdk_region_destroy (gdk_clip_region);
    }
    
    if (cwindow_get_last_painted_extents (cwindow))
	cwindow_destroy_last_painted_extents (cwindow);
    
    cwindow_set_last_painted_extents (cwindow, cwindow_extents (cwindow));
}


static void
cwindow_undamage (CWindow *cwindow)
{
    Display *xdisplay = cwindow_get_xdisplay (cwindow);
    Window xwindow = cwindow->xwindow;
    gboolean get_all;
    Geometry *geometry = cwindow->freeze_info?
	&cwindow->freeze_info->geometry :
	&cwindow->geometry;
    
    if (!cwindow->image)
	return;
    
    if (cwindow->input_only)
	return;
    
    if (!cwindow->viewable)
	return;
    
    /* If we've already undamaged at least once at this size, just get the
     * part that changed. Otherwise, we get everything.
     */
    get_all = cwindow->damage_serial == 0;
    cwindow->damage_serial = NextRequest (xdisplay);

    if (get_all)
    {
	if (cwindow->texture)
	{
	    lmc_texture_unref (cwindow->texture);
	    cwindow->texture = NULL;
	}
	
	destroy_window_image (cwindow);
	create_window_image (cwindow);
    }

    
    if (!get_all)
	XFixesSetGCClipRegion (xdisplay, cwindow->shm_gc,
			       0, 0, cwindow->parts_region);
    else
	XFixesSetGCClipRegion (xdisplay, cwindow->shm_gc,
			       0, 0, None);

#if 0
    g_print ("copying: %lx\n", cwindow->xwindow);
#endif
    XCopyArea (xdisplay,
	       cwindow_get_drawable (cwindow),
	       cwindow->shm_pixmap,
	       cwindow->shm_gc,
	       0, 0,
	       geometry->width, geometry->height,
	       0, 0);

    XSync (xdisplay, False);

    if (!get_all && cwindow->texture)
    {
	GdkRectangle *rects;
	int n_rects, i;

	rects = server_region_to_gdk_rects (cwindow_get_xdisplay (cwindow),
					    cwindow->parts_region, &n_rects);

	for (i = 0; i < n_rects; ++i)
	{
	    lmc_texture_update_rect (cwindow->texture, &(rects[i]));
	    
	}
    }

    XFixesDestroyRegion (cwindow_get_xdisplay (cwindow), cwindow->parts_region);
    cwindow->parts_region = XFixesCreateRegion (cwindow_get_xdisplay (cwindow), 0, 0);
}
