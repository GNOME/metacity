#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrender.h>
#include "display.h"

typedef struct World World;

World *
world_new (Display *display, MetaScreen *screen);
XserverRegion 
world_invalidate (World *world);
void
world_set_time (World *world, gdouble time);
void
world_paint (World *world, Picture destination);
MetaScreen *
world_get_screen (World *world);
