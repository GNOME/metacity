#include <X11/Xlib.h>
#include "cwindow.h"
#include "errors.h"
#include "compositor.h"
#include "matrix.h"

#define SHADOW_OFFSET 10
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
  
  int x;
  int y;
  int width;
  int height;
  int border_width;
  
  int pending_x;
  int pending_y;
  int pending_width;
  int pending_height;
  int pending_border_width;
  
  Damage          damage;
  XserverRegion   last_painted_extents;
  
  XserverRegion   border_size;
  
  Pixmap          pixmap;
  
  unsigned int managed : 1;
  unsigned int damaged : 1;
  unsigned int viewable : 1;
  unsigned int input_only : 1;
  
  unsigned int screen_index : 8;
  
  Visual *visual;
  
  Distortion *distortions;
  int n_distortions;
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
  XDamageDestroy (cwindow_get_xdisplay (cwindow), cwindow->damage);
  
  /* Free our window pixmap name */
  if (cwindow->pixmap != None)
    XFreePixmap (cwindow_get_xdisplay (cwindow),
		 cwindow->pixmap);
  meta_error_trap_pop (meta_compositor_get_display (cwindow->compositor), FALSE);
  
  g_free (cwindow);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

#ifdef HAVE_COMPOSITE_EXTENSIONS
XserverRegion
cwindow_extents (CWindow *cwindow)
{
  XRectangle r;
  
  r.x = cwindow->x;
  r.y = cwindow->y;
  r.width = cwindow->width;
  r.height = cwindow->height;
  
  r.width += SHADOW_OFFSET;
  r.height += SHADOW_OFFSET;
  
  return XFixesCreateRegion (cwindow_get_xdisplay (cwindow), &r, 1);
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

void
cwindow_get_paint_bounds (CWindow *cwindow,
			  int *x,
			  int *y,
			  int *w,
			  int *h)
{
  if (cwindow->pixmap != None)
    {
      *x = cwindow->x;
      *y = cwindow->y;
      *w = cwindow->width + cwindow->border_width * 2;
      *h = cwindow->height + cwindow->border_width * 2;
    }
  else
    {
      *x = cwindow->x + cwindow->border_width;
      *y = cwindow->y + cwindow->border_width;
      *w = cwindow->width;
      *h = cwindow->height;
    }
}

Drawable
cwindow_get_drawable (CWindow *cwindow)
{
  if (cwindow->pixmap)
    return cwindow->pixmap;
  else
    return cwindow->xwindow;
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

CWindow *
cwindow_new (MetaCompositor *compositor, Window xwindow, XWindowAttributes *attrs)
{
  CWindow *cwindow;
  Damage damage;
  
  /* Create Damage object to monitor window damage */
  meta_error_trap_push (meta_compositor_get_display (compositor));
  damage = XDamageCreate (meta_compositor_get_display (compositor)->xdisplay,
			  xwindow, XDamageReportNonEmpty);
  meta_error_trap_pop (meta_compositor_get_display (compositor), FALSE);
  
  if (damage == None)
    return NULL;
  
  cwindow = g_new0 (CWindow, 1);
  
  cwindow->compositor = compositor;
  cwindow->xwindow = xwindow;
  cwindow->screen_index = XScreenNumberOfScreen (attrs->screen);
  cwindow->damage = damage;
  cwindow->x = attrs->x;
  cwindow->y = attrs->y;
  cwindow->width = attrs->width;
  cwindow->height = attrs->height;
  cwindow->border_width = attrs->border_width;
  
  if (attrs->class == InputOnly)
    cwindow->input_only = TRUE;
  else
    cwindow->input_only = FALSE;
  
  cwindow->visual = attrs->visual;
  
#if 0
  if (compositor->have_name_window_pixmap)
    {
      meta_error_trap_push (meta_compositor_get_display (compositor));
      cwindow->pixmap = XCompositeNameWindowPixmap (meta_compositor_get_display (compositor)->xdisplay,
						    cwindow->xwindow);
      meta_error_trap_pop (meta_compositor_get_display (compositor), FALSE);
    }
#endif
  
  /* viewable == mapped for the root window, since root can't be unmapped */
  cwindow->viewable = (attrs->map_state == IsViewable);
  
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
  return cwindow->x;
}

int
cwindow_get_y (CWindow *cwindow)
{
  return cwindow->y;
}

int
cwindow_get_width (CWindow *cwindow)
{
  return cwindow->width;
}

int
cwindow_get_height (CWindow *cwindow)
{
  return cwindow->height;
}

int
cwindow_get_border_width (CWindow *cwindow)
{
  return cwindow->border_width;
}

Damage
cwindow_get_damage (CWindow *cwindow)
{
  return cwindow->damage;
}

Pixmap
cwindow_get_pixmap (CWindow *cwindow)
{
  return cwindow->pixmap;
}

MetaCompositor *
cwindow_get_compositor (CWindow *cwindow)
{
  return cwindow->compositor;
}

void
cwindow_set_pending_x (CWindow *cwindow, int pending_x)
{
  cwindow->pending_x = pending_x;
}

void
cwindow_set_pending_y (CWindow *cwindow, int pending_y)
{
  cwindow->pending_y = pending_y;
}

void
cwindow_set_pending_width (CWindow *cwindow, int pending_width)
{
  cwindow->pending_width = pending_width;
}

void
cwindow_set_pending_height (CWindow *cwindow, int pending_height)
{
  cwindow->pending_height = pending_height;
}

void
cwindow_set_pending_border_width (CWindow *cwindow, int pending_border_width)
{
  cwindow->pending_border_width = pending_border_width;
}

void
cwindow_set_x (CWindow *cwindow, int x)
{
  cwindow->x = x;
}

void
cwindow_set_y (CWindow *cwindow, int y)
{
  cwindow->y = y;
}

void
cwindow_set_width (CWindow *cwindow, int width)
{
  cwindow->width = width;
}

void
cwindow_set_height (CWindow *cwindow, int height)
{
  cwindow->height = height;
}


void
cwindow_set_viewable (CWindow *cwindow, gboolean viewable)
{
  cwindow->viewable = viewable;
}


void
cwindow_set_border_width (CWindow *cwindow, int border_width)
{
  cwindow->border_width = border_width;
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

static void
get_transform (XTransform *trans, int x, int y, int w, int h)
{
  Matrix3 tmp;
  
  matrix3_identity (&tmp);
  
  transform_matrix_perspective (0, 0, w, h,
				0, 0,				  w - 1 - 0.1 * w, 0,
				0, 0 + h - 1 - 0.1 * h,	  0 + w - 1, 0 + h - 1,
				
				&tmp);
  
#if 0
  matrix3_translate (&tmp, 50, 50);
#endif
  
  matrix3_invert (&tmp);
  
  convert_matrix (&tmp, trans);
}

void
cwindow_draw (CWindow *cwindow, Picture picture, XserverRegion damaged_region)
{
  /* Actually draw the window */
  XRenderPictFormat *format;
  Picture wpicture;
  XRenderPictureAttributes pa;
  XTransform trans;
  int x, y, w, h;
  
  if (cwindow_get_input_only (cwindow))
    return;
  
  if (!cwindow_get_viewable (cwindow))
    return;
  
  cwindow_get_paint_bounds (cwindow, &x, &y, &w, &h);
  
  format = XRenderFindVisualFormat (cwindow_get_xdisplay (cwindow),
				    cwindow_get_visual (cwindow));
  pa.subwindow_mode = IncludeInferiors;
  wpicture = XRenderCreatePicture (cwindow_get_xdisplay (cwindow),
				   cwindow_get_drawable (cwindow),
				   format,
				   CPSubwindowMode,
				   &pa);
  
  get_transform (&trans, x, y, w, h);
  
  if (cwindow_get_last_painted_extents (cwindow))
    cwindow_destroy_last_painted_extents (cwindow);
  
  cwindow_set_last_painted_extents (cwindow, cwindow_extents (cwindow));
  
#if 0
  meta_topic (META_DEBUG_COMPOSITOR, "  Compositing window 0x%lx %d,%d %dx%d\n",
	      cwindow_get_xwindow (cwindow),
	      cwindow->x, cwindow->y,
	      cwindow->width, cwindow->height);
#endif
  
#if 0
  {
    XGCValues value;
    GC gc;
    
    value.function = GXcopy;
    value.subwindow_mode = IncludeInferiors;
    
    gc = XCreateGC (dpy, screen->xroot, GCFunction | GCSubwindowMode, &value);
    XSetForeground (dpy, gc, rand());
    XFixesSetGCClipRegion (dpy, gc, 0, 0, damaged_region);
    XFillRectangle (dpy, screen->xroot, gc, 0, 0,
		    screen->width, screen->height);
    XFreeGC (dpy, gc);
    XSync (dpy, False);
    g_usleep (70000);
  }
#endif
  
  if (cwindow_is_translucent (cwindow))
    {
      XRenderColor shadow_color = { 0x0000, 0, 0x0000, 0x70c0 };
      XFixesSetPictureClipRegion (cwindow_get_xdisplay (cwindow),
				  picture, 0, 0,
				  damaged_region);
      
      
      
      XRenderFillRectangle (cwindow_get_xdisplay (cwindow), PictOpOver,
			    picture,
			    &shadow_color,
			    cwindow_get_x (cwindow) + SHADOW_OFFSET,
			    cwindow_get_y (cwindow) + SHADOW_OFFSET,
			    cwindow_get_width (cwindow),
			    cwindow_get_height (cwindow));
      /* Draw window transparent while resizing */
      XRenderComposite (cwindow_get_xdisplay (cwindow), PictOpOver, /* PictOpOver for alpha, PictOpSrc without */
			wpicture,
			cwindow_get_screen (cwindow)->trans_picture,
			picture,
			0, 0, 0, 0,
			x, y, w, h);
    }
  else
    {
#if 0
      XRenderSetPictureTransform (cwindow_get_xdisplay (cwindow),
				  wpicture,
				  &trans);
      XRenderSetPictureFilter (cwindow_get_xdisplay (cwindow), wpicture, "bilinear", 0, 0);
#endif
      
      /* Draw window normally */
      XRenderColor shadow_color = { 0x0000, 0, 0x0000, 0x70c0 };
      
#if 0
      XFixesSetPictureClipRegion (dpy,
				  picture, 0, 0,
				  region_below);
#endif
      
      XFixesSetPictureClipRegion (cwindow_get_xdisplay (cwindow),
				  picture, 0, 0,
				  damaged_region);
      
      /* superlame drop shadow */
      XRenderFillRectangle (cwindow_get_xdisplay (cwindow), PictOpOver,
			    picture,
			    &shadow_color,
			    cwindow_get_x (cwindow) + SHADOW_OFFSET,
			    cwindow_get_y (cwindow) + SHADOW_OFFSET,
			    cwindow_get_width (cwindow),
			    cwindow_get_height (cwindow));
      
      XRenderComposite (cwindow_get_xdisplay (cwindow),
			PictOpOver, /* PictOpOver for alpha, PictOpSrc without */
			wpicture,
			None,
			picture,
			0, 0, 0, 0,
			x, y, w, h);
      
    }
  XRenderFreePicture (cwindow_get_xdisplay (cwindow), wpicture);
}

gboolean
cwindow_is_translucent (CWindow *cwindow)
{
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
  
  g_print ("mapping from %d %d %d %d to (%d %d) (%d %d) (%d %d) (%d %d)\n", x, y, width, height,
	   tmp.points[0].x, tmp.points[0].y,
	   tmp.points[1].x, tmp.points[1].y,
	   tmp.points[2].x, tmp.points[2].y,
	   tmp.points[3].x, tmp.points[3].y);
  
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

void
cwindow_draw_warped (CWindow *cwindow,
		     MetaScreen		 *screen,
		     Picture		  picture,
		     Quad		 *destination)
{
  MetaCompositor *compositor;
  Display *display;
  
  Picture wpicture;
  XRenderPictureAttributes pa;
  XRenderPictFormat *format;
  
  XTransform transform;
  
  if (!cwindow)
    return;
  
  if (cwindow_get_input_only (cwindow))
    return;
  
  if (!cwindow_get_viewable (cwindow))
    return;
  
  compositor = cwindow_get_compositor (cwindow);
  display = meta_compositor_get_display (compositor)->xdisplay;
  
  
  format = XRenderFindVisualFormat (meta_compositor_get_display (compositor)->xdisplay,
				    cwindow_get_visual (cwindow));
  pa.subwindow_mode = IncludeInferiors;
  g_assert (meta_compositor_get_display (compositor));
  
  wpicture = XRenderCreatePicture (display, cwindow_get_drawable (cwindow), format, CPSubwindowMode, &pa);
  
  g_assert (wpicture);
  
  compute_transform (0, 0, cwindow_get_width (cwindow), cwindow_get_height (cwindow),
		     destination, &transform);
  
  XRenderSetPictureTransform (display, wpicture, &transform);
  XRenderSetPictureFilter (meta_compositor_get_display (compositor)->xdisplay, wpicture, "bilinear", 0, 0);
  
  XRenderComposite (meta_compositor_get_display (compositor)->xdisplay,
		    PictOpOver, /* PictOpOver for alpha, PictOpSrc without */
		    wpicture,
		    screen->trans_picture,
		    picture, 
		    0, 0,
		    0, 0,
		    bbox (destination).x, bbox (destination).y,
		    bbox (destination).width, bbox (destination).height + 100);
  
  XRenderFreePicture (display, wpicture);
}

#if 0
static void
compute_transformation (int x, int y, int w, int h, Quad *dest, XTransform *trans)
{
  Matrix3 tmp;
  
  matrix3_identity (&tmp);
  
  transform_matrix_perspective (x, y, w, h,
				
				dest->points[0].x, dest->points[0].y,
				dest->points[1].x, dest->points[1].y,
				dest->points[2].x, dest->points[2].y,
				dest->points[3].x, dest->points[3].y,
				
				&tmp);
  
  matrix3_invert (&tmp);
  
  convert_matrix (&tmp, trans);
}
#endif

void
cwindow_process_damage_notify (CWindow *cwindow, XDamageNotifyEvent *event)
{
  XserverRegion region;
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
  
  region = XFixesCreateRegion (cwindow_get_xdisplay (cwindow), NULL, 0);
  
  /* translate region to screen; can error if window of damage is
   * destroyed
   */
  meta_error_trap_push (meta_compositor_get_display (cwindow->compositor));
  XDamageSubtract (cwindow_get_xdisplay (cwindow),
		   cwindow_get_damage (cwindow), None, region);
  meta_error_trap_pop (meta_compositor_get_display (cwindow->compositor), FALSE);
  
  XFixesTranslateRegion (cwindow_get_xdisplay (cwindow),
			 region,
			 cwindow_get_x (cwindow),
			 cwindow_get_y (cwindow));
  
  screen = cwindow_get_screen (cwindow);
  
  meta_compositor_invalidate_region (cwindow->compositor, screen, region);
  
  XFixesDestroyRegion (cwindow_get_xdisplay (cwindow), region);
}

void
cwindow_process_configure_notify (CWindow *cwindow, XConfigureEvent *event)
{
  XserverRegion region;
  MetaScreen *screen;
  
  screen = cwindow_get_screen (cwindow);
  
  if (cwindow_get_last_painted_extents (cwindow))
    {
      meta_compositor_invalidate_region (cwindow->compositor, screen, cwindow_get_last_painted_extents (cwindow));
      cwindow_set_last_painted_extents (cwindow, None);
    }
  
  if (cwindow_get_pixmap (cwindow))
    {
      cwindow_set_pending_x (cwindow, event->x);
      cwindow_set_pending_y (cwindow, event->y);
      cwindow_set_pending_width (cwindow, event->width);
      cwindow_set_pending_height (cwindow, event->height);
      cwindow_set_pending_border_width (cwindow, event->border_width);
    }
  else
    {
      cwindow_set_x (cwindow, event->x);
      cwindow_set_y (cwindow, event->y);
      cwindow_set_width (cwindow, event->width);
      cwindow_set_height (cwindow, event->height);
      cwindow_set_border_width (cwindow, event->border_width);
    }
  
  region = cwindow_extents (cwindow);
  
  meta_compositor_invalidate_region (cwindow->compositor,
				     screen,
				     region);
  
  XFixesDestroyRegion (cwindow_get_xdisplay (cwindow), region);
}

void
cwindow_set_transformation (CWindow *cwindow,
			    const Distortion *distortions,
			    int n_distortions)
{
  if (cwindow->distortions)
    g_free (cwindow->distortions);
  
  cwindow->distortions = g_memdup (distortions, n_distortions * sizeof (Distortion));
  cwindow->n_distortions = n_distortions;
}

void
cwindow_new_draw (CWindow *cwindow, Picture destination, XserverRegion damaged_region)
{
  XRenderPictFormat *format;
  int i;
  
  if (cwindow_get_input_only (cwindow))
    return;
  
  if (!cwindow_get_viewable (cwindow))
    return;
  
  format = XRenderFindVisualFormat (cwindow_get_xdisplay (cwindow),
				    cwindow_get_visual (cwindow));
  
  for (i = 0; i < cwindow->n_distortions; ++i)
    {
      XTransform transform;
      Picture picture;
      XRenderPictureAttributes pa;
      
      Distortion *dist = &cwindow->distortions[i];
      compute_transform (dist->source.x,
			 dist->source.y,
			 dist->source.width, dist->source.height,
			 &dist->destination, &transform);
      
      /* Draw window */
      pa.subwindow_mode = IncludeInferiors;
      picture = XRenderCreatePicture (cwindow_get_xdisplay (cwindow),
				      cwindow_get_drawable (cwindow),
				      format,
				      CPSubwindowMode,
				      &pa);
      
      XRenderSetPictureTransform (cwindow_get_xdisplay (cwindow), picture, &transform);
      XRenderSetPictureFilter (cwindow_get_xdisplay (cwindow), picture, "bilinear", 0, 0);
      
      XRenderComposite (cwindow_get_xdisplay (cwindow),
			PictOpOver, /* PictOpOver for alpha, PictOpSrc without */
			picture,
			cwindow_get_screen (cwindow)->trans_picture,
			destination,
			dist->source.x, dist->source.y,
			0, 0,
			bbox (&dist->destination).x, bbox (&dist->destination).y,
			bbox (&dist->destination).width, bbox (&dist->destination).height);
      
      {
	int j = i + 2;
	XRenderColor hilit_color = { (j / 10.0) * 0x0000, 0, (j / 10.0) * 0x9999, (j / 10.0) * 0xFFFF } ;
	
#if 0
	XRenderFillRectangle (cwindow_get_xdisplay (cwindow), PictOpOver,
			      destination,
			      &hilit_color,
			      bbox (&dist->destination).x, bbox (&dist->destination).y,
			      bbox (&dist->destination).width, bbox (&dist->destination).height);
#endif
	
#if 0
	g_print ("destination (%d %d) (%d %d) (%d %d) (%d %d)\n",
		 dist->destination.points[0].x, dist->destination.points[0].y,
		 dist->destination.points[1].x, dist->destination.points[1].y,
		 dist->destination.points[2].x, dist->destination.points[2].y,
		 dist->destination.points[3].x, dist->destination.points[3].y);
	
	g_print ("filling: %d %d %d %d\n",
		 bbox (&dist->destination).x, bbox (&dist->destination).y,
		 bbox (&dist->destination).width, bbox (&dist->destination).height);
#endif
	
      }
      
      XRenderFreePicture (cwindow_get_xdisplay (cwindow), picture);
    }
}
