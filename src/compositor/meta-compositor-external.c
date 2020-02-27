/*
 * Copyright (C) 2020 Alberts Muktupāvels
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
#include "meta-compositor-external.h"

#include <X11/extensions/Xfixes.h>

#include "display-private.h"

struct _MetaCompositorExternal
{
  MetaCompositor parent;

  Atom           cm_atom;
};

G_DEFINE_TYPE (MetaCompositorExternal, meta_compositor_external, META_TYPE_COMPOSITOR)

static gboolean
meta_compositor_external_manage (MetaCompositor  *compositor,
                                 GError         **error)
{
  MetaCompositorExternal *self;
  MetaDisplay *display;
  Display *xdisplay;
  gchar *atom_name;
  gboolean composited;

  self = META_COMPOSITOR_EXTERNAL (compositor);

  display = meta_compositor_get_display (compositor);
  xdisplay = meta_display_get_xdisplay (display);

  if (!display->have_xfixes)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Missing xfixes extension");

      return FALSE;
    }

  atom_name = g_strdup_printf ("_NET_WM_CM_S%d", DefaultScreen (xdisplay));
  self->cm_atom = XInternAtom (xdisplay, atom_name, FALSE);
  g_free (atom_name);

  XFixesSelectSelectionInput (xdisplay,
                              DefaultRootWindow (xdisplay),
                              self->cm_atom,
                              XFixesSetSelectionOwnerNotifyMask);

  composited = XGetSelectionOwner (xdisplay, self->cm_atom) != None;
  meta_compositor_set_composited (compositor, composited);

  return TRUE;
}

static MetaSurface *
meta_compositor_external_add_window (MetaCompositor *compositor,
                                     MetaWindow     *window)
{
  return NULL;
}

static void
meta_compositor_external_process_event (MetaCompositor *compositor,
                                        XEvent         *event,
                                        MetaWindow     *window)
{
  MetaCompositorExternal *self;
  MetaDisplay *display;
  XFixesSelectionNotifyEvent *xfixes_event;

  self = META_COMPOSITOR_EXTERNAL (compositor);
  display = meta_compositor_get_display (compositor);

  if (event->type != display->xfixes_event_base + XFixesSelectionNotify)
    return;

  xfixes_event = (XFixesSelectionNotifyEvent *) event;
  if (xfixes_event->selection != self->cm_atom)
    return;

  meta_compositor_set_composited (compositor, xfixes_event->owner != None);
}

static void
meta_compositor_external_sync_screen_size (MetaCompositor *compositor)
{
}

static void
meta_compositor_external_redraw (MetaCompositor *compositor,
                                 XserverRegion   all_damage)
{
}

static void
meta_compositor_external_class_init (MetaCompositorExternalClass *self_class)
{
  MetaCompositorClass *compositor_class;

  compositor_class = META_COMPOSITOR_CLASS (self_class);

  compositor_class->manage = meta_compositor_external_manage;
  compositor_class->add_window = meta_compositor_external_add_window;
  compositor_class->process_event = meta_compositor_external_process_event;
  compositor_class->sync_screen_size = meta_compositor_external_sync_screen_size;
  compositor_class->redraw = meta_compositor_external_redraw;
}

static void
meta_compositor_external_init (MetaCompositorExternal *self)
{
}

MetaCompositor *
meta_compositor_external_new (MetaDisplay  *display,
                              GError      **error)
{
  return g_initable_new (META_TYPE_COMPOSITOR_EXTERNAL, NULL, error,
                         "display", display,
                         NULL);
}
