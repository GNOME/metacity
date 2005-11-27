/* Metacity compositing manager */

/* 
 * Copyright (C) 2003 Red Hat, Inc.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef META_COMPOSITOR_H
#define META_COMPOSITOR_H

#include "util.h"
#include "display.h"

#include <X11/extensions/Xfixes.h>

MetaCompositor* meta_compositor_new           (MetaDisplay       *display);
void            meta_compositor_unref         (MetaCompositor    *compositor);
void            meta_compositor_process_event (MetaCompositor    *compositor,
                                               XEvent            *xevent,
                                               MetaWindow        *window);
void            meta_compositor_add_window    (MetaCompositor    *compositor,
                                               Window             xwindow,
                                               XWindowAttributes *attrs);
void            meta_compositor_remove_window (MetaCompositor    *compositor,
                                               Window             xwindow);
void		meta_compositor_set_debug_updates (MetaCompositor *compositor,
						   gboolean	   debug_updates);

void meta_compositor_manage_screen   (MetaCompositor *compositor,
                                      MetaScreen     *screen);
void meta_compositor_unmanage_screen (MetaCompositor *compositor,
                                      MetaScreen     *screen);

void meta_compositor_stop_compositing (MetaCompositor *compositor,
				       MetaWindow     *window);
void meta_compositor_start_compositing (MetaCompositor *compositor,
					MetaWindow     *window);
MetaDisplay *meta_compositor_get_display (MetaCompositor *compositor);

void
meta_compositor_genie (MetaCompositor *compositor,
		       MetaWindow       *window);
void
meta_compositor_invalidate_region (MetaCompositor *compositor,
				   MetaScreen	*screen,
				   XserverRegion   invalid_area);

void
meta_compositor_set_translucent (MetaCompositor *compositor,
				 MetaWindow *window,
				 gboolean translucent);
gboolean
meta_compositor_repair_now (MetaCompositor *compositor);

XID
meta_compositor_get_gl_window (MetaCompositor *compositor);

#endif /* META_COMPOSITOR_H */
