#include "config.h"
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrender.h>
#include <glib.h>
#include "screen.h"

typedef struct CWindow CWindow;
typedef struct Quad Quad;
typedef struct Point Point;
typedef struct Rectangle Rectangle;
typedef struct Distortion Distortion;

struct Point
{
    int x, y;
};

struct Quad
{
    Point points[4];
};

struct Rectangle
{
    int x, y;
    int width, height;
};

struct Distortion 
{
    Rectangle source;
    Quad destination;
};

void            cwindow_set_transformation           (CWindow            *window,
						      const Distortion   *distortions,
						      int                 n_distortions);
void            cwindow_free                         (CWindow            *cwindow);
XserverRegion   cwindow_extents                      (CWindow            *cwindow);
void            cwindow_get_paint_bounds             (CWindow            *cwindow,
						      int                *x,
						      int                *y,
						      int                *w,
						      int                *h);
Drawable        cwindow_get_drawable                 (CWindow            *cwindow);
Window          cwindow_get_xwindow                  (CWindow            *cwindow);
gboolean        cwindow_get_viewable                 (CWindow            *cwindow);
gboolean        cwindow_get_input_only               (CWindow            *cwindow);
Visual *        cwindow_get_visual                   (CWindow            *cwindow);
XserverRegion   cwindow_get_last_painted_extents     (CWindow            *cwindow);
void            cwindow_set_last_painted_extents     (CWindow            *cwindow,
						      XserverRegion       region);
void            cwindow_destroy_last_painted_extents (CWindow            *cwindow);
int             cwindow_get_x                        (CWindow            *cwindow);
int             cwindow_get_y                        (CWindow            *cwindow);
int             cwindow_get_width                    (CWindow            *cwindow);
int             cwindow_get_height                   (CWindow            *cwindow);
int             cwindow_get_border_width             (CWindow            *cwindow);
MetaScreen *    cwindow_get_screen                   (CWindow            *cwindow);
Damage          cwindow_get_damage                   (CWindow            *cwindow);
void            cwindow_set_pending_x                (CWindow            *cwindow,
						      int                 pending_x);
void            cwindow_set_pending_y                (CWindow            *cwindow,
						      int                 pending_y);
void            cwindow_set_pending_width            (CWindow            *cwindow,
						      int                 width);
void            cwindow_set_pending_height           (CWindow            *cwindow,
						      int                 height);
void            cwindow_set_pending_border_width     (CWindow            *cwindow,
						      int                 border_width);
void            cwindow_set_x                        (CWindow            *cwindow,
						      int                 x);
void            cwindow_set_y                        (CWindow            *cwindow,
						      int                 y);
void            cwindow_set_width                    (CWindow            *cwindow,
						      int                 width);
void            cwindow_set_height                   (CWindow            *cwindow,
						      int                 height);
Pixmap          cwindow_get_pixmap                   (CWindow            *cwindow);
void            cwindow_set_border_width             (CWindow            *cwindow,
						      int                 border_width);
void            cwindow_set_viewable                 (CWindow            *cwindow,
						      gboolean            viewable);
CWindow *       cwindow_new                          (MetaCompositor     *compositor,
						      Window              xwindow,
						      XWindowAttributes  *attrs);
XID *           cwindow_get_xid_address              (CWindow            *cwindow);
MetaCompositor *cwindow_get_compositor               (CWindow            *cwindow);
#if 0
void            cwindow_draw                         (CWindow            *cwindow,
						      Picture             picture,
						      XserverRegion       damaged_region);
#endif
gboolean        cwindow_is_translucent               (CWindow            *cwindow);
void            cwindow_draw_warped                  (CWindow            *cwindow,
						      MetaScreen         *screen,
						      Picture             picture,
						      Quad               *destination);
void            cwindow_process_damage_notify        (CWindow            *cwindow,
						      XDamageNotifyEvent *event);
void            cwindow_process_configure_notify     (CWindow            *cwindow,
						      XConfigureEvent    *event);
void            cwindow_new_draw                     (CWindow            *cwindow,
						      Picture             destination,
						      XserverRegion       damaged_region);
