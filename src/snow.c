#include <X11/Xlib.h>
#include <glib.h>
#include <X11/extensions/Xfixes.h>
#include "config.h"
#include "snow.h"
#include "screen.h"
#include <math.h>

typedef struct Flake Flake;

#define MAX_RADIUS 6.2
#define MIN_RADIUS 4.5
#define MIN_ALPHA  20.0
#define MAX_ALPHA  70.0

struct Flake
{
    World *	world;
    double	x;
    double	y;
    int		alpha;
    int		radius;
    double	y_speed;
    double	increment;
    double	angle;
    XRenderColor color;
};

struct World
{
    Display *dpy;
    MetaScreen *screen;
    GList *flakes;
    gdouble time;
    gint xmouse;
};

static void
flake_renew (Flake *flake, gboolean first)
{
    flake->x = g_random_double () * flake->world->screen->width;

    if (first)
	flake->y = g_random_double () * flake->world->screen->height;
    else
	flake->y = 0.0;
    
    flake->y_speed = (g_random_double () * 1.8 + 0.2) * flake->world->screen->height;
    flake->alpha = g_random_double_range (MIN_ALPHA, MAX_ALPHA);
    flake->radius = MIN_RADIUS + ((flake->alpha - MIN_ALPHA) / (MAX_ALPHA - MIN_ALPHA)) * (MAX_RADIUS - MIN_RADIUS);
    flake->increment = -0.025 + g_random_double() * 0.05;
    flake->angle = 0.0;

#define BORING

#define GRAYNESS 0xffff
    
#ifdef BORING
    flake->color.red = (flake->alpha / 100.0) * GRAYNESS;
    flake->color.green = (flake->alpha / 100.0) * GRAYNESS;
    flake->color.blue = (flake->alpha / 100.0) * GRAYNESS;
    flake->color.alpha = (flake->alpha / 100.0) * 0xffff;
#else
    flake->color.red = (flake->alpha / 100.0) * g_random_int_range (2 << 14, GRAYNESS);
    flake->color.green = (flake->alpha / 100.0) * 0xEE00;
    flake->color.blue = (flake->alpha / 100.0) * GRAYNESS;
    flake->color.alpha = (flake->alpha / 100.0) * 0xFFFF;
#endif
}

static Flake *
flake_new (World *world,
	   gdouble time,
	   int radius)
{
    Flake *flake = g_new (Flake, 1);
    
    flake->world = world;

    flake_renew (flake, TRUE);
    
    return flake;
}

static void
flake_move (Flake *flake,
	    gdouble delta)
{
    gboolean mouse_enabled = TRUE;
    
    flake->angle += delta * flake->increment;

    if (mouse_enabled)
    {
	flake->x += (flake->world->screen->width / 2 - flake->world->xmouse) / 100;
	flake->y += delta * 10 * flake->y_speed;

	if ((flake->y > flake->world->screen->height || flake->y < 0))
	{
	    flake_renew (flake, FALSE);
	}

	while (flake->x < 0)
	    flake->x += flake->world->screen->width;
    }
    else
    {
	flake->x += flake->radius * 25000 * delta * sin (flake->angle);
	flake->y += delta * flake->y_speed;

	if ((flake->x > flake->world->screen->width || flake->x < 0) ||
	    (flake->y > flake->world->screen->height || flake->y < 0))
	{
	    flake_renew (flake, FALSE);
	}
    }
}

static void
flake_get_position (Flake *flake,
		    int *x,
		    int *y)
{
    if (x)
    {
	*x = flake->x;
	*x = *x % flake->world->screen->width;
    }

    if (y)
    {
	*y = flake->y;
	*y = *y % flake->world->screen->height;
    }
}

static void
flake_get_rectangle (Flake *flake, double time, XRectangle *rect)
{
    int x, y;

    flake_get_position (flake, &x, &y);
    
    rect->x = x - flake->radius;
    rect->y = y - flake->radius;
    rect->width = 2 * flake->radius;
    rect->height = 2 * flake->radius;
}

static void
flake_destroy (Flake *flake)
{
    g_free (flake);
}

static void
flake_invalidate (Flake *flake,
		  gdouble time,
		  XserverRegion region)
{ 
    XRectangle rect;
    XserverRegion flake_region;

    g_return_if_fail (region != None);

    flake_get_rectangle (flake, time, &rect);
    
    flake_region = XFixesCreateRegion (flake->world->dpy, &rect, 1);

    XFixesUnionRegion (flake->world->dpy, region, region, flake_region);

    XFixesDestroyRegion (flake->world->dpy, flake_region);
}

void
XRenderCompositeTrapezoids (Display		*dpy,
			    int			op,
			    Picture		src,
			    Picture		dst,
			    _Xconst XRenderPictFormat	*maskFormat,
			    int			xSrc,
			    int			ySrc,
			    _Xconst XTrapezoid	*traps,
			    int			ntrap);

static double
integral (int r, int x)
{
    g_return_val_if_fail (x <= r, -1.0);
    
    if (x == r)
	return 0.25 * M_PI * r * r;
    
    return 0.5 * (x * sqrt (r * r - x * x) + r * r * atan ( x / sqrt (r * r - x * x)));
}

static void
fill_circle (Display *dpy, Picture destination, int x, int y, int radius, XRenderColor *color)
{
    int i;
    for (i = 0; i < radius; ++i)
    {
	XRenderColor antialias;
	
	double value = integral (radius, i + 1) - integral (radius, i);
	int intpart = value;
	double fract = value - intpart;

	static int j;

#if 0
	if (j++ % 5000 == 0)
	    g_print ("%d (r: %d, i: %d)\n", intpart, radius, i);
#endif
	
	XRenderFillRectangle (dpy, PictOpOver, destination, color,
			      i + x, y - intpart,
			      1, 2 * intpart);
	XRenderFillRectangle (dpy, PictOpOver, destination, color,
			      x - i - 1, y - intpart,
			      1, 2 * intpart);

	antialias = *color;
	antialias.red *= fract;
	antialias.green *= fract;
	antialias.blue *= fract;
	antialias.alpha *= fract;
	
	XRenderFillRectangle (dpy, PictOpOver, destination, &antialias,
			      i + x, y - intpart - 1,
			      1, 1);
	XRenderFillRectangle (dpy, PictOpOver, destination, &antialias,
			      i + x, y + intpart,
			      1, 1);
	XRenderFillRectangle (dpy, PictOpOver, destination, &antialias,
			      x - i - 1, y - intpart - 1,
			      1, 1);
	XRenderFillRectangle (dpy, PictOpOver, destination, &antialias,
			      x - i - 1, y + intpart,
			      1, 1);
    }
}

static void
flake_paint (Flake *flake, Picture destination)
{
    int x, y;

    flake_get_position (flake, &x, &y);

    double radius = flake->radius;

#if 0
    g_print (" %d %d %d %d \n", color.red, color.green, color.blue, color.alpha);
#endif

    fill_circle (flake->world->dpy, destination, x, y, radius, &flake->color);
    
#if 0
    XRenderFillRectangle (flake->world->dpy, PictOpOver, destination, &flake->color,
			  x - radius, y - radius,
			  2 * radius, 2 * radius);
#endif
    
    radius = 0.63 * radius;

    fill_circle (flake->world->dpy, destination, x, y, radius, &flake->color);
    
#if 0
    XRenderFillRectangle (flake->world->dpy, PictOpOver, destination, &flake->color,
			  x - radius, y - radius,
			  2 * radius, 2 * radius);
#endif
}

/*
 * World
 */

World *
world_new (Display *dpy,
	   MetaScreen *screen)
{
#define N_FLAKES (screen->width / 20)
#if 0
#define N_FLAKES 10
#endif
    int i;
    World *world = g_new (World, 1);

    world->dpy = dpy;
    world->flakes = NULL;
    world->screen = screen;

    for (i = 0; i < N_FLAKES; ++i)
    {
	Flake *flake = flake_new (world, 0.0, 2);

	world->flakes = g_list_prepend (world->flakes, flake);
    }
    
    return world;
}

void
world_set_time (World *world, double time)
{
    GList *list;
    gdouble delta = time - world->time;
    world->time = time;
    Window dummy;
    int dummyint;

    XQueryPointer (world->dpy,
		   world->screen->xroot,
		   &dummy,	/* root_return */
		   &dummy,	/* child return */
		   &world->xmouse,	/* root x return */
		   &dummyint,		/* root y return */
		   &dummyint,		/* win x return */
		   &dummyint,		/* win y return */
		   &dummyint		/* mask return */);
    
    for (list = world->flakes; list; list = list->next)
    {
	Flake *flake = list->data;

	flake_move (flake, delta);
    }
}

#if 0
void
world_start (World *world, gdouble time)
{
    g_timeout_add (25, update_world, world);
}
#endif

MetaScreen *
world_get_screen (World *world)
{
    return world->screen;
}

XserverRegion 
world_invalidate (World *world)
{
    GList *list;

    XserverRegion region;

    region = XFixesCreateRegion (world->dpy, NULL, 0);

    for (list = world->flakes; list != NULL; list = list->next)
    {
	Flake *flake = list->data;
	
	flake_invalidate (flake, world->time, region);
    }

    return region;
}

void
world_paint (World *world, Picture destination)
{
    GList *list;

    list = world->flakes;
    while (list)
    {
	GList *next = list->next;
	Flake *flake = list->data;

	flake_paint (flake, destination);

	list = next;
    }
}
