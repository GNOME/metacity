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
#include "meta-compositor-xpresent.h"

#include <X11/extensions/Xpresent.h>

#include "display-private.h"
#include "errors.h"
#include "screen-private.h"

#define NUM_BUFFER 2

struct _MetaCompositorXPresent
{
  MetaCompositorXRender parent;

  int                   major_opcode;
  int                   event_base;
  int                   error_base;

  Picture               root_buffers[NUM_BUFFER];
  Pixmap                root_pixmaps[NUM_BUFFER];
  int                   root_current;

  gboolean              present_pending;
};

G_DEFINE_TYPE (MetaCompositorXPresent,
               meta_compositor_xpresent,
               META_TYPE_COMPOSITOR_XRENDER)

static gboolean
meta_compositor_xpresent_manage (MetaCompositor  *compositor,
                                 GError         **error)
{
  MetaCompositorClass *compositor_class;
  MetaCompositorXPresent *self;
  MetaDisplay *display;
  Display *xdisplay;

  compositor_class = META_COMPOSITOR_CLASS (meta_compositor_xpresent_parent_class);

  if (!compositor_class->manage (compositor, error))
    return FALSE;

  self = META_COMPOSITOR_XPRESENT (compositor);

  display = meta_compositor_get_display (compositor);
  xdisplay = meta_display_get_xdisplay (display);

  if (!XPresentQueryExtension (xdisplay,
                               &self->major_opcode,
                               &self->event_base,
                               &self->error_base))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Missing present extension required for compositing");

      return FALSE;
    }

  XPresentSelectInput (xdisplay,
                       meta_compositor_get_overlay_window (compositor),
                       PresentCompleteNotifyMask);

  return TRUE;
}

static void
meta_compositor_xpresent_process_event (MetaCompositor *compositor,
                                        XEvent         *event,
                                        MetaWindow     *window)
{
  MetaCompositorXPresent *self;
  MetaCompositorClass *compositor_class;

  self = META_COMPOSITOR_XPRESENT (compositor);

  if (event->type == GenericEvent)
    {
      XGenericEvent *generic_event;
      XGenericEventCookie *generic_event_cookie;
      MetaDisplay *display;
      Display *xdisplay;

      generic_event = (XGenericEvent *) event;
      generic_event_cookie = (XGenericEventCookie *) generic_event;

      display = meta_compositor_get_display (compositor);
      xdisplay = meta_display_get_xdisplay (display);

      if (generic_event_cookie->extension == self->major_opcode)
        {
          XGetEventData (xdisplay, generic_event_cookie);

          if (generic_event_cookie->evtype == PresentCompleteNotify)
            {
              meta_compositor_queue_redraw (compositor);
              self->present_pending = FALSE;
            }

          XFreeEventData (xdisplay, generic_event_cookie);
        }
    }

  compositor_class = META_COMPOSITOR_CLASS (meta_compositor_xpresent_parent_class);
  compositor_class->process_event (compositor, event, window);
}

static gboolean
meta_compositor_xpresent_ready_to_redraw (MetaCompositor *compositor)
{
  MetaCompositorXPresent *self;

  self = META_COMPOSITOR_XPRESENT (compositor);

  return !self->present_pending;
}

static void
meta_compositor_xpresent_redraw (MetaCompositor *compositor,
                                 XserverRegion   all_damage)
{
  MetaCompositorXPresent *self;
  MetaDisplay *display;
  Display *xdisplay;
  int result;

  self = META_COMPOSITOR_XPRESENT (compositor);

  display = meta_compositor_get_display (META_COMPOSITOR (self));
  xdisplay = meta_display_get_xdisplay (display);

  meta_compositor_xrender_draw (META_COMPOSITOR_XRENDER (compositor),
                                self->root_buffers[self->root_current],
                                all_damage);

  meta_error_trap_push (display);

  XPresentPixmap (xdisplay,
                  meta_compositor_get_overlay_window (compositor),
                  self->root_pixmaps[self->root_current],
                  0,
                  all_damage,
                  all_damage,
                  0,
                  0,
                  None,
                  None,
                  None,
                  PresentOptionNone,
                  0,
                  1,
                  0,
                  NULL,
                  0);

  result = meta_error_trap_pop_with_return (display);

  if (result != Success)
    {
      char error_text[64];

      XGetErrorText (xdisplay, result, error_text, 63);

      g_warning ("XPresentPixmap failed with error %i (%s)",
                 result, error_text);

      g_unsetenv ("META_COMPOSITOR");
      meta_display_update_compositor (display);

      return;
    }

  self->root_current = !self->root_current;
  self->present_pending = TRUE;
}

static void
meta_compositor_xpresent_ensure_root_buffers (MetaCompositorXRender *xrender)
{
  MetaCompositorXPresent *self;
  int i;

  self = META_COMPOSITOR_XPRESENT (xrender);

  for (i = 0; i < NUM_BUFFER; i++)
    {
      if (self->root_buffers[i] == None &&
          self->root_pixmaps[i] == None)
        {
          meta_compositor_xrender_create_root_buffer (xrender,
                                                      &self->root_pixmaps[i],
                                                      &self->root_buffers[i]);
        }
    }
}

static void
meta_compositor_xpresent_free_root_buffers (MetaCompositorXRender *xrender)
{
  MetaCompositorXPresent *self;
  MetaDisplay *display;
  Display *xdisplay;
  int i;

  self = META_COMPOSITOR_XPRESENT (xrender);

  display = meta_compositor_get_display (META_COMPOSITOR (self));
  xdisplay = meta_display_get_xdisplay (display);

  for (i = 0; i < NUM_BUFFER; i++)
    {
      if (self->root_buffers[i] != None)
        {
          XRenderFreePicture (xdisplay, self->root_buffers[i]);
          self->root_buffers[i] = None;
        }

      if (self->root_pixmaps[i] != None)
        {
          XFreePixmap (xdisplay, self->root_pixmaps[i]);
          self->root_pixmaps[i] = None;
        }
    }
}

static void
meta_compositor_xpresent_class_init (MetaCompositorXPresentClass *self_class)
{
  MetaCompositorClass *compositor_class;
  MetaCompositorXRenderClass *xrender_class;

  compositor_class = META_COMPOSITOR_CLASS (self_class);
  xrender_class = META_COMPOSITOR_XRENDER_CLASS (self_class);

  compositor_class->manage = meta_compositor_xpresent_manage;
  compositor_class->process_event = meta_compositor_xpresent_process_event;
  compositor_class->ready_to_redraw = meta_compositor_xpresent_ready_to_redraw;
  compositor_class->redraw = meta_compositor_xpresent_redraw;

  xrender_class->ensure_root_buffers = meta_compositor_xpresent_ensure_root_buffers;
  xrender_class->free_root_buffers = meta_compositor_xpresent_free_root_buffers;
}

static void
meta_compositor_xpresent_init (MetaCompositorXPresent *self)
{
  int i;

  for (i = 0; i < NUM_BUFFER; i++)
    {
      self->root_buffers[i] = None;
      self->root_pixmaps[i] = None;
    }
}

MetaCompositor *
meta_compositor_xpresent_new (MetaDisplay  *display,
                              GError      **error)
{
  return g_initable_new (META_TYPE_COMPOSITOR_XPRESENT, NULL, error,
                         "display", display,
                         NULL);
}
