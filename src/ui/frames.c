/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity window frame manager widget */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Red Hat, Inc.
 * Copyright (C) 2005, 2006 Elijah Newren
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <math.h>
#include <string.h>
#include "boxes.h"
#include "frames.h"
#include "util.h"
#include "core.h"
#include "menu.h"
#include "fixedtip.h"
#include "prefs.h"
#include "ui.h"
#include "ui-private.h"

#include <cairo-xlib.h>

#include <X11/extensions/shape.h>

#define DEFAULT_INNER_BUTTON_BORDER 3

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327
#endif

static void meta_frames_destroy       (GtkWidget *widget);
static void meta_frames_finalize      (GObject   *object);
static void meta_frames_style_updated (GtkWidget *widget);

static gboolean meta_frames_button_press_event    (GtkWidget           *widget,
                                                   GdkEventButton      *event);
static gboolean meta_frames_button_release_event  (GtkWidget           *widget,
                                                   GdkEventButton      *event);
static gboolean meta_frames_motion_notify_event   (GtkWidget           *widget,
                                                   GdkEventMotion      *event);
static gboolean meta_frames_draw                  (GtkWidget           *widget,
                                                   cairo_t             *cr);
static gboolean meta_frames_enter_notify_event    (GtkWidget           *widget,
                                                   GdkEventCrossing    *event);
static gboolean meta_frames_leave_notify_event    (GtkWidget           *widget,
                                                   GdkEventCrossing    *event);

static void meta_frames_attach_style (MetaFrames  *frames,
                                      MetaUIFrame *frame);

static void meta_frames_paint (MetaFrames  *frames,
                               MetaUIFrame *frame,
                               cairo_t     *cr);

static void meta_frames_calc_geometry (MetaFrames        *frames,
                                       MetaUIFrame         *frame,
                                       MetaFrameGeometry *fgeom);

static MetaUIFrame* meta_frames_lookup_window (MetaFrames *frames,
                                               Window      xwindow);

static void meta_frames_font_changed          (MetaFrames *frames);
static void meta_frames_button_layout_changed (MetaFrames *frames);
static void meta_frames_reattach_all_styles   (MetaFrames *frames);

static void clear_tip (MetaFrames *frames);
static void invalidate_all_caches (MetaFrames *frames);
static void invalidate_whole_window (MetaFrames *frames,
                                     MetaUIFrame *frame);

struct _MetaFrames
{
  GtkWindow    parent;

  MetaUI      *ui;

  Display     *xdisplay;

  GHashTable  *frames;

  GSettings   *interface_settings;

  guint        tooltip_timeout;
  MetaUIFrame *last_motion_frame;

  gint         invalidate_cache_timeout_id;
  GList       *invalidate_frames;
  GHashTable  *cache;
};

G_DEFINE_TYPE (MetaFrames, meta_frames, GTK_TYPE_WINDOW)

static void
get_client_rect (MetaFrameGeometry *fgeom,
                 GdkRectangle      *rect)
{
  rect->x = fgeom->borders.total.left;
  rect->y = fgeom->borders.total.top;
  rect->width = fgeom->width - fgeom->borders.total.right - rect->x;
  rect->height = fgeom->height - fgeom->borders.total.bottom - rect->y;
}

#define RESIZE_EXTENDS 15
#define TOP_RESIZE_HEIGHT 4
static MetaFrameControl
get_control (MetaFrames  *frames,
             MetaUIFrame *frame,
             gint         x,
             gint         y)
{
  MetaFrameGeometry fgeom;
  MetaFrameFlags flags;
  MetaFrameType type;
  gboolean has_vert, has_horiz;
  gboolean has_north_resize;
  GdkRectangle client;
  MetaFrameBorders borders;
  MetaTheme *theme;
  MetaButton *button;

  meta_frames_calc_geometry (frames, frame, &fgeom);
  get_client_rect (&fgeom, &client);

  borders = fgeom.borders;

  if (x < borders.invisible.left - borders.resize.left ||
      y < borders.invisible.top - borders.resize.top ||
      x > fgeom.width - borders.invisible.right + borders.resize.right ||
      y > fgeom.height - borders.invisible.bottom + borders.resize.bottom)
    return META_FRAME_CONTROL_NONE;

  if (POINT_IN_RECT (x, y, client))
    return META_FRAME_CONTROL_CLIENT_AREA;

  meta_core_get (frames->xdisplay, frame->xwindow,
                 META_CORE_GET_FRAME_FLAGS, &flags,
                 META_CORE_GET_FRAME_TYPE, &type,
                 META_CORE_GET_END);

  theme = meta_ui_get_theme (frames->ui);
  button = meta_theme_get_button (theme, x, y);

  if (button != NULL)
    {
      switch (meta_button_get_type (button))
        {
          case META_BUTTON_TYPE_CLOSE:
            return META_FRAME_CONTROL_DELETE;

          case META_BUTTON_TYPE_MINIMIZE:
            return META_FRAME_CONTROL_MINIMIZE;

          case META_BUTTON_TYPE_MENU:
            return META_FRAME_CONTROL_MENU;

          case META_BUTTON_TYPE_MAXIMIZE:
            if (flags & META_FRAME_MAXIMIZED)
              return META_FRAME_CONTROL_UNMAXIMIZE;
            else
              return META_FRAME_CONTROL_MAXIMIZE;

          case META_BUTTON_TYPE_SPACER:
          case META_BUTTON_TYPE_LAST:
          default:
            break;
        }
    }

  has_north_resize = (type != META_FRAME_TYPE_ATTACHED);
  has_vert = (flags & META_FRAME_ALLOWS_VERTICAL_RESIZE) != 0;
  has_horiz = (flags & META_FRAME_ALLOWS_HORIZONTAL_RESIZE) != 0;

  if (POINT_IN_RECT (x, y, fgeom.title_rect))
    {
      if (has_vert && y <= (fgeom.borders.invisible.top + TOP_RESIZE_HEIGHT) && has_north_resize)
        return META_FRAME_CONTROL_RESIZE_N;
      else
        return META_FRAME_CONTROL_TITLE;
    }

  /* South resize always has priority over north resize,
   * in case of overlap.
   */

  if (y >= (fgeom.height - fgeom.borders.total.bottom - RESIZE_EXTENDS) &&
      x >= (fgeom.width - fgeom.borders.total.right - RESIZE_EXTENDS))
    {
      if (has_vert && has_horiz)
        return META_FRAME_CONTROL_RESIZE_SE;
      else if (has_vert)
        return META_FRAME_CONTROL_RESIZE_S;
      else if (has_horiz)
        return META_FRAME_CONTROL_RESIZE_E;
    }
  else if (y >= (fgeom.height - fgeom.borders.total.bottom - RESIZE_EXTENDS) &&
           x <= (fgeom.borders.total.left + RESIZE_EXTENDS))
    {
      if (has_vert && has_horiz)
        return META_FRAME_CONTROL_RESIZE_SW;
      else if (has_vert)
        return META_FRAME_CONTROL_RESIZE_S;
      else if (has_horiz)
        return META_FRAME_CONTROL_RESIZE_W;
    }
  else if (y < (fgeom.borders.invisible.top + RESIZE_EXTENDS) &&
           x <= (fgeom.borders.total.left + RESIZE_EXTENDS) && has_north_resize)
    {
      if (has_vert && has_horiz)
        return META_FRAME_CONTROL_RESIZE_NW;
      else if (has_vert)
        return META_FRAME_CONTROL_RESIZE_N;
      else if (has_horiz)
        return META_FRAME_CONTROL_RESIZE_W;
    }
  else if (y < (fgeom.borders.invisible.top + RESIZE_EXTENDS) &&
           x >= (fgeom.width - fgeom.borders.total.right - RESIZE_EXTENDS) && has_north_resize)
    {
      if (has_vert && has_horiz)
        return META_FRAME_CONTROL_RESIZE_NE;
      else if (has_vert)
        return META_FRAME_CONTROL_RESIZE_N;
      else if (has_horiz)
        return META_FRAME_CONTROL_RESIZE_E;
    }
  else if (y < (fgeom.borders.invisible.top + TOP_RESIZE_HEIGHT))
    {
      if (has_vert && has_north_resize)
        return META_FRAME_CONTROL_RESIZE_N;
    }
  else if (y >= (fgeom.height - fgeom.borders.total.bottom - RESIZE_EXTENDS))
    {
      if (has_vert)
        return META_FRAME_CONTROL_RESIZE_S;
    }
  else if (x <= fgeom.borders.total.left + RESIZE_EXTENDS)
    {
      if (has_horiz)
        return META_FRAME_CONTROL_RESIZE_W;
    }
  else if (x >= (fgeom.width - fgeom.borders.total.right - RESIZE_EXTENDS))
    {
      if (has_horiz)
        return META_FRAME_CONTROL_RESIZE_E;
    }

  if (y >= fgeom.borders.total.top)
    return META_FRAME_CONTROL_NONE;
  else
    return META_FRAME_CONTROL_TITLE;
}

static gboolean
get_control_rect (MetaFrames        *frames,
                  MetaFrameControl   control,
                  MetaFrameGeometry *fgeom,
                  gint               x,
                  gint               y,
                  GdkRectangle      *rect)
{
  MetaButtonType type;
  MetaTheme *theme;
  MetaButton *button;

  switch (control)
    {
      case META_FRAME_CONTROL_TITLE:
        *rect = fgeom->title_rect;
        return TRUE;

      case META_FRAME_CONTROL_DELETE:
        type = META_BUTTON_TYPE_CLOSE;
        break;

      case META_FRAME_CONTROL_MENU:
        type = META_BUTTON_TYPE_MENU;
        break;

      case META_FRAME_CONTROL_MINIMIZE:
        type = META_BUTTON_TYPE_MINIMIZE;
        break;

      case META_FRAME_CONTROL_MAXIMIZE:
      case META_FRAME_CONTROL_UNMAXIMIZE:
        type = META_BUTTON_TYPE_MAXIMIZE;
        break;

      case META_FRAME_CONTROL_CLIENT_AREA:
      case META_FRAME_CONTROL_RESIZE_SE:
      case META_FRAME_CONTROL_RESIZE_S:
      case META_FRAME_CONTROL_RESIZE_SW:
      case META_FRAME_CONTROL_RESIZE_N:
      case META_FRAME_CONTROL_RESIZE_NE:
      case META_FRAME_CONTROL_RESIZE_NW:
      case META_FRAME_CONTROL_RESIZE_W:
      case META_FRAME_CONTROL_RESIZE_E:
      case META_FRAME_CONTROL_NONE:
      default:
        type = META_BUTTON_TYPE_LAST;
        break;
    }

  if (type == META_BUTTON_TYPE_LAST)
    return FALSE;

  theme = meta_ui_get_theme (frames->ui);
  button = meta_theme_get_button (theme, x, y);

  if (button == NULL || meta_button_get_type (button) != type)
    return FALSE;

  meta_button_get_event_rect (button, rect);

  return TRUE;
}

static void
meta_frames_class_init (MetaFramesClass *frames_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (frames_class);
  widget_class = GTK_WIDGET_CLASS (frames_class);

  object_class->finalize = meta_frames_finalize;

  widget_class->destroy = meta_frames_destroy;
  widget_class->style_updated = meta_frames_style_updated;

  widget_class->draw = meta_frames_draw;
  widget_class->button_press_event = meta_frames_button_press_event;
  widget_class->button_release_event = meta_frames_button_release_event;
  widget_class->motion_notify_event = meta_frames_motion_notify_event;
  widget_class->enter_notify_event = meta_frames_enter_notify_event;
  widget_class->leave_notify_event = meta_frames_leave_notify_event;
}

static gint
unsigned_long_equal (gconstpointer v1,
                     gconstpointer v2)
{
  return *((const gulong*) v1) == *((const gulong*) v2);
}

static guint
unsigned_long_hash (gconstpointer v)
{
  gulong val = * (const gulong *) v;

  /* I'm not sure this works so well. */
#if GLIB_SIZEOF_LONG > 4
  return (guint) (val ^ (val >> 32));
#else
  return val;
#endif
}

static void
prefs_changed_callback (MetaPreference pref,
                        void          *data)
{
  if (pref == META_PREF_TITLEBAR_FONT)
    meta_frames_font_changed (META_FRAMES (data));
  else if (pref == META_PREF_BUTTON_LAYOUT)
    meta_frames_button_layout_changed (META_FRAMES (data));
}

static void
meta_frames_init (MetaFrames *frames)
{
  GdkDisplay *display;

  display = gdk_display_get_default ();

  frames->interface_settings = g_settings_new ("org.gnome.desktop.interface");
  g_signal_connect_swapped (frames->interface_settings,
                            "changed::color-scheme",
                            G_CALLBACK (meta_frames_reattach_all_styles),
                            frames);

  frames->xdisplay = gdk_x11_display_get_xdisplay (display);

  frames->frames = g_hash_table_new (unsigned_long_hash, unsigned_long_equal);

  frames->tooltip_timeout = 0;

  frames->invalidate_cache_timeout_id = 0;
  frames->invalidate_frames = NULL;
  frames->cache = g_hash_table_new (g_direct_hash, g_direct_equal);

  meta_prefs_add_listener (prefs_changed_callback, frames);
}

static void
listify_func (gpointer key, gpointer value, gpointer data)
{
  GSList **listp;

  listp = data;
  *listp = g_slist_prepend (*listp, value);
}

static void
meta_frames_destroy (GtkWidget *widget)
{
  GSList *winlist;
  GSList *tmp;
  MetaFrames *frames;

  frames = META_FRAMES (widget);

  clear_tip (frames);

  winlist = NULL;
  g_hash_table_foreach (frames->frames, listify_func, &winlist);

  /* Unmanage all frames */
  for (tmp = winlist; tmp != NULL; tmp = tmp->next)
    {
      MetaUIFrame *frame;

      frame = tmp->data;

      meta_frames_unmanage_window (frames, frame->xwindow);
    }
  g_slist_free (winlist);

  g_clear_object (&frames->interface_settings);

  GTK_WIDGET_CLASS (meta_frames_parent_class)->destroy (widget);
}

static void
meta_frames_finalize (GObject *object)
{
  MetaFrames *frames;

  frames = META_FRAMES (object);

  meta_prefs_remove_listener (prefs_changed_callback, frames);

  invalidate_all_caches (frames);
  if (frames->invalidate_cache_timeout_id)
    g_source_remove (frames->invalidate_cache_timeout_id);

  g_assert (g_hash_table_size (frames->frames) == 0);
  g_hash_table_destroy (frames->frames);
  g_hash_table_destroy (frames->cache);

  G_OBJECT_CLASS (meta_frames_parent_class)->finalize (object);
}

typedef struct
{
  cairo_rectangle_int_t rect;
  cairo_surface_t *pixmap;
} CachedFramePiece;

typedef struct
{
  /* Caches of the four rendered sides in a MetaFrame.
   * Order: top (titlebar), left, right, bottom.
   */
  CachedFramePiece piece[4];
} CachedPixels;

static CachedPixels *
get_cache (MetaFrames *frames,
           MetaUIFrame *frame)
{
  CachedPixels *pixels;

  pixels = g_hash_table_lookup (frames->cache, frame);

  if (!pixels)
    {
      pixels = g_new0 (CachedPixels, 1);
      g_hash_table_insert (frames->cache, frame, pixels);
    }

  return pixels;
}

static void
invalidate_cache (MetaFrames *frames,
                  MetaUIFrame *frame)
{
  CachedPixels *pixels = get_cache (frames, frame);
  int i;

  for (i = 0; i < 4; i++)
    if (pixels->piece[i].pixmap)
      cairo_surface_destroy (pixels->piece[i].pixmap);

  g_free (pixels);
  g_hash_table_remove (frames->cache, frame);
}

static void
invalidate_all_caches (MetaFrames *frames)
{
  GList *l;

  for (l = frames->invalidate_frames; l; l = l->next)
    {
      MetaUIFrame *frame = l->data;

      invalidate_cache (frames, frame);
    }

  g_list_free (frames->invalidate_frames);
  frames->invalidate_frames = NULL;
}

static gboolean
invalidate_cache_timeout (gpointer data)
{
  MetaFrames *frames = data;

  invalidate_all_caches (frames);
  frames->invalidate_cache_timeout_id = 0;
  return FALSE;
}

static void
queue_recalc_func (gpointer key, gpointer value, gpointer data)
{
  MetaUIFrame *frame;
  MetaFrames *frames;

  frames = META_FRAMES (data);
  frame = value;

  invalidate_whole_window (frames, frame);
  meta_core_queue_frame_resize (frames->xdisplay, frame->xwindow);
}

static void
meta_frames_font_changed (MetaFrames *frames)
{
  MetaTheme *theme;
  const PangoFontDescription *titlebar_font;

  theme = meta_ui_get_theme (frames->ui);
  titlebar_font = meta_prefs_get_titlebar_font ();

  meta_theme_set_titlebar_font (theme, titlebar_font);

  /* Queue a draw/resize on all frames */
  g_hash_table_foreach (frames->frames,
                        queue_recalc_func, frames);

}

static void
queue_draw_func (gpointer key, gpointer value, gpointer data)
{
  MetaUIFrame *frame;
  MetaFrames *frames;

  frames = META_FRAMES (data);
  frame = value;

  invalidate_whole_window (frames, frame);
}

static void
meta_frames_button_layout_changed (MetaFrames *frames)
{
  g_hash_table_foreach (frames->frames,
                        queue_draw_func, frames);
}

static void
reattach_style_func (gpointer key, gpointer value, gpointer data)
{
  MetaUIFrame *frame;
  MetaFrames *frames;

  frames = META_FRAMES (data);
  frame = value;

  meta_frames_attach_style (frames, frame);
}

static void
meta_frames_reattach_all_styles (MetaFrames *frames)
{
  g_hash_table_foreach (frames->frames, reattach_style_func, frames);
  meta_retheme_all ();
}

static void
meta_frames_style_updated (GtkWidget *widget)
{
  MetaFrames *frames;
  MetaTheme *theme;

  frames = META_FRAMES (widget);
  theme = meta_ui_get_theme (frames->ui);

  meta_theme_invalidate (theme);

  meta_frames_font_changed (frames);

  meta_frames_reattach_all_styles (frames);

  GTK_WIDGET_CLASS (meta_frames_parent_class)->style_updated (widget);
}

static void
meta_frames_calc_geometry (MetaFrames        *frames,
                           MetaUIFrame       *frame,
                           MetaFrameGeometry *fgeom)
{
  int width, height;
  MetaFrameFlags flags;
  MetaFrameType type;

  meta_core_get (frames->xdisplay, frame->xwindow,
                 META_CORE_GET_CLIENT_WIDTH, &width,
                 META_CORE_GET_CLIENT_HEIGHT, &height,
                 META_CORE_GET_FRAME_FLAGS, &flags,
                 META_CORE_GET_FRAME_TYPE, &type,
                 META_CORE_GET_END);

  meta_theme_calc_geometry (meta_ui_get_theme (frames->ui),
                            frame->theme_variant, type, flags,
                            width, height, fgeom);
}

MetaFrames*
meta_frames_new (MetaUI *ui)
{
  MetaFrames *frames;

  frames = g_object_new (META_TYPE_FRAMES, "type", GTK_WINDOW_POPUP, NULL);
  frames->ui = ui;

  /* Put the window at an arbitrary offscreen location; the one place
   * it can't be is at -100x-100, since the meta_window_new() will
   * mistake it for a window created via meta_create_offscreen_window()
   * and ignore it, and we need this window to get frame-synchronization
   * messages so that GTK+'s style change handling works.
   */
  gtk_window_move (GTK_WINDOW (frames), -200, -200);
  gtk_window_resize (GTK_WINDOW (frames), 1, 1);

  return frames;
}

static const gchar *
get_global_theme_variant (MetaFrames *frames)
{
  GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (frames));
  GtkSettings *settings = gtk_settings_get_for_screen (screen);
  gboolean dark_theme_requested;

  g_object_get (settings,
                "gtk-application-prefer-dark-theme", &dark_theme_requested,
                NULL);

  if (dark_theme_requested)
    return "dark";

  return NULL;
}

static const char *
get_color_scheme_variant (MetaFrames *frames)
{
  GDesktopColorScheme color_scheme;

  color_scheme = g_settings_get_enum (frames->interface_settings, "color-scheme");

  if (color_scheme == G_DESKTOP_COLOR_SCHEME_PREFER_DARK)
    return "dark";

  return NULL;
}

/* In order to use a style with a window it has to be attached to that
 * window. Actually, the colormaps just have to match, but since GTK+
 * already takes care of making sure that its cheap to attach a style
 * to multiple windows with the same colormap, we can just go ahead
 * and attach separately for each window.
 */
static void
meta_frames_attach_style (MetaFrames  *frames,
                          MetaUIFrame *frame)
{
  GdkDisplay *display;
  const char *variant;

  display = gdk_display_get_default ();
  variant = NULL;

  meta_core_get (GDK_DISPLAY_XDISPLAY (display), frame->xwindow,
                 META_CORE_GET_THEME_VARIANT, &variant,
                 META_CORE_GET_END);

  if (variant == NULL)
    variant = get_global_theme_variant (frames);

  if (variant == NULL)
    variant = get_color_scheme_variant (frames);

  if (variant != NULL && *variant == '\0')
    variant = NULL;

  g_free (frame->theme_variant);
  frame->theme_variant = g_strdup (variant);
}

void
meta_frames_manage_window (MetaFrames *frames,
                           Window      xwindow,
                           GdkWindow  *window)
{
  MetaUIFrame *frame;

  g_assert (window);

  frame = g_new (MetaUIFrame, 1);

  frame->window = window;

  gdk_window_set_user_data (frame->window, frames);

  frame->theme_variant = NULL;

  /* Don't set event mask here, it's in frame.c */

  frame->xwindow = xwindow;
  frame->title = NULL;
  frame->shape_applied = FALSE;
  frame->ignore_leave_notify = FALSE;
  frame->prelit_control = META_FRAME_CONTROL_NONE;
  frame->prelit_x = 0;
  frame->prelit_y = 0;

  meta_core_grab_buttons (frames->xdisplay, frame->xwindow);

  g_hash_table_replace (frames->frames, &frame->xwindow, frame);
}

void
meta_frames_unmanage_window (MetaFrames *frames,
                             Window      xwindow)
{
  MetaUIFrame *frame;

  clear_tip (frames);

  frame = g_hash_table_lookup (frames->frames, &xwindow);

  if (frame)
    {
      /* invalidating all caches ensures the frame
       * is not actually referenced anymore
       */
      invalidate_all_caches (frames);

      /* restore the cursor */
      meta_core_set_screen_cursor (frames->xdisplay, frame->xwindow,
                                   META_CURSOR_DEFAULT);

      gdk_window_set_user_data (frame->window, NULL);

      if (frames->last_motion_frame == frame)
        frames->last_motion_frame = NULL;

      g_hash_table_remove (frames->frames, &frame->xwindow);

      g_free (frame->theme_variant);

      gdk_window_destroy (frame->window);

      if (frame->title)
        g_free (frame->title);

      g_free (frame);
    }
  else
    {
      g_warning ("Frame 0x%lx not managed, can't unmanage", xwindow);
    }
}

static MetaUIFrame*
meta_frames_lookup_window (MetaFrames *frames,
                           Window      xwindow)
{
  MetaUIFrame *frame;

  frame = g_hash_table_lookup (frames->frames, &xwindow);

  return frame;
}

static void
meta_ui_frame_get_borders (MetaFrames       *frames,
                           MetaUIFrame      *frame,
                           MetaFrameBorders *borders)
{
  MetaFrameFlags flags;
  MetaFrameType type;

  meta_core_get (frames->xdisplay, frame->xwindow,
                 META_CORE_GET_FRAME_FLAGS, &flags,
                 META_CORE_GET_FRAME_TYPE, &type,
                 META_CORE_GET_END);

  g_return_if_fail (type < META_FRAME_TYPE_LAST);

  /* We can't get the full geometry, because that depends on
   * the client window size and probably we're being called
   * by the core move/resize code to decide on the client
   * window size
   */
  meta_theme_get_frame_borders (meta_ui_get_theme (frames->ui),
                                frame->theme_variant, type,
                                flags, borders);
}

void
meta_frames_get_borders (MetaFrames       *frames,
                         Window            xwindow,
                         MetaFrameBorders *borders)
{
  MetaUIFrame *frame;

  frame = meta_frames_lookup_window (frames, xwindow);

  if (frame == NULL)
    g_error ("No such frame 0x%lx", xwindow);

  meta_ui_frame_get_borders (frames, frame, borders);
}

static void
apply_cairo_region_to_window (Display        *display,
                              Window          xwindow,
                              cairo_region_t *region,
                              int             op)
{
  int n_rects, i;
  XRectangle *rects;

  n_rects = cairo_region_num_rectangles (region);
  rects = g_new (XRectangle, n_rects);

  for (i = 0; i < n_rects; i++)
    {
      cairo_rectangle_int_t rect;

      cairo_region_get_rectangle (region, i, &rect);

      rects[i].x = rect.x;
      rects[i].y = rect.y;
      rects[i].width = rect.width;
      rects[i].height = rect.height;
    }

  XShapeCombineRectangles (display, xwindow,
                           ShapeBounding, 0, 0, rects, n_rects,
                           op, YXBanded);

  g_free (rects);
}

/* The visible frame rectangle surrounds the visible portion of the
 * frame window; it subtracts only the invisible borders from the frame
 * window's size.
 */
static void
get_visible_frame_rect (MetaFrameGeometry     *fgeom,
                        int                    window_width,
                        int                    window_height,
                        cairo_rectangle_int_t *rect)
{
  rect->x = fgeom->borders.invisible.left;
  rect->y = fgeom->borders.invisible.top;
  rect->width = window_width - fgeom->borders.invisible.right - rect->x;
  rect->height = window_height - fgeom->borders.invisible.bottom - rect->y;
}

static cairo_region_t *
get_visible_region (MetaFrames        *frames,
                    MetaUIFrame       *frame,
                    MetaFrameGeometry *fgeom,
                    int                window_width,
                    int                window_height)
{
  cairo_region_t *corners_region;
  cairo_region_t *visible_region;
  cairo_rectangle_int_t rect;
  cairo_rectangle_int_t frame_rect;

  corners_region = cairo_region_create ();
  get_visible_frame_rect (fgeom, window_width, window_height, &frame_rect);

  if (fgeom->top_left_corner_rounded_radius != 0)
    {
      const int corner = fgeom->top_left_corner_rounded_radius;
      const gdouble radius = sqrt (corner) + corner;
      int i;

      for (i=0; i<corner; i++)
        {
          const int width = floor(0.5 + radius - sqrt(radius*radius - (radius-(i+0.5))*(radius-(i+0.5))));
          rect.x = frame_rect.x;
          rect.y = frame_rect.y + i;
          rect.width = width;
          rect.height = 1;

          cairo_region_union_rectangle (corners_region, &rect);
        }
    }

  if (fgeom->top_right_corner_rounded_radius != 0)
    {
      const int corner = fgeom->top_right_corner_rounded_radius;
      const gdouble radius = sqrt (corner) + corner;
      int i;

      for (i=0; i<corner; i++)
        {
          const int width = floor(0.5 + radius - sqrt(radius*radius - (radius-(i+0.5))*(radius-(i+0.5))));
          rect.x = frame_rect.x + frame_rect.width - width;
          rect.y = frame_rect.y + i;
          rect.width = width;
          rect.height = 1;

          cairo_region_union_rectangle (corners_region, &rect);
        }
    }

  if (fgeom->bottom_left_corner_rounded_radius != 0)
    {
      const int corner = fgeom->bottom_left_corner_rounded_radius;
      const gdouble radius = sqrt (corner) + corner;
      int i;

      for (i=0; i<corner; i++)
        {
          const int width = floor(0.5 + radius - sqrt(radius*radius - (radius-(i+0.5))*(radius-(i+0.5))));
          rect.x = frame_rect.x;
          rect.y = frame_rect.y + frame_rect.height - i - 1;
          rect.width = width;
          rect.height = 1;

          cairo_region_union_rectangle (corners_region, &rect);
        }
    }

  if (fgeom->bottom_right_corner_rounded_radius != 0)
    {
      const int corner = fgeom->bottom_right_corner_rounded_radius;
      const gdouble radius = sqrt (corner) + corner;
      int i;

      for (i=0; i<corner; i++)
        {
          const int width = floor(0.5 + radius - sqrt(radius*radius - (radius-(i+0.5))*(radius-(i+0.5))));
          rect.x = frame_rect.x + frame_rect.width - width;
          rect.y = frame_rect.y + frame_rect.height - i - 1;
          rect.width = width;
          rect.height = 1;

          cairo_region_union_rectangle (corners_region, &rect);
        }
    }

  visible_region = cairo_region_create_rectangle (&frame_rect);
  cairo_region_subtract (visible_region, corners_region);
  cairo_region_destroy (corners_region);

  return visible_region;
}

static cairo_region_t *
get_client_region (MetaFrameGeometry *fgeom,
                   int                window_width,
                   int                window_height)
{
  cairo_rectangle_int_t rect;

  rect.x = fgeom->borders.total.left;
  rect.y = fgeom->borders.total.top;
  rect.width = window_width - fgeom->borders.total.right - rect.x;
  rect.height = window_height - fgeom->borders.total.bottom - rect.y;

  return cairo_region_create_rectangle (&rect);
}

static cairo_region_t *
get_frame_region (int window_width,
                  int window_height)
{
  cairo_rectangle_int_t rect;

  rect.x = 0;
  rect.y = 0;
  rect.width = window_width;
  rect.height = window_height;

  return cairo_region_create_rectangle (&rect);
}

void
meta_frames_apply_shapes (MetaFrames *frames,
                          Window      xwindow,
                          int         new_window_width,
                          int         new_window_height,
                          gboolean    window_has_shape)
{
  /* Apply shapes as if window had new_window_width, new_window_height */
  MetaUIFrame *frame;
  MetaFrameGeometry fgeom;
  cairo_region_t *window_region;

  frame = meta_frames_lookup_window (frames, xwindow);
  g_return_if_fail (frame != NULL);

  if (frame->shape_applied)
    {
      meta_topic (META_DEBUG_SHAPES,
                  "Unsetting shape mask on frame 0x%lx\n",
                  frame->xwindow);

      XShapeCombineMask (frames->xdisplay, frame->xwindow,
                         ShapeBounding, 0, 0, None, ShapeSet);
      frame->shape_applied = FALSE;
    }

  meta_frames_calc_geometry (frames, frame, &fgeom);

  if (!window_has_shape && meta_ui_is_composited (frames->ui))
    return;

  window_region = get_visible_region (frames,
                                      frame,
                                      &fgeom,
                                      new_window_width,
                                      new_window_height);

  if (window_has_shape)
    {
      /* The client window is oclock or something and has a shape
       * mask. To avoid a round trip to get its shape region, we
       * create a fake window that's never mapped, build up our shape
       * on that, then combine. Wasting the window is assumed cheaper
       * than a round trip, but who really knows for sure.
       */
      XSetWindowAttributes attrs;
      Window shape_window;
      Window client_window;
      cairo_region_t *frame_region;
      cairo_region_t *client_region;
      cairo_region_t *tmp_region;
      GdkScreen *screen;
      int screen_number;

      meta_topic (META_DEBUG_SHAPES,
                  "Frame 0x%lx needs to incorporate client shape\n",
                  frame->xwindow);

      screen = gtk_widget_get_screen (GTK_WIDGET (frames));
      screen_number = gdk_x11_screen_get_screen_number (screen);

      attrs.override_redirect = True;

      shape_window = XCreateWindow (frames->xdisplay,
                                    RootWindow (frames->xdisplay, screen_number),
                                    -5000, -5000,
                                    new_window_width,
                                    new_window_height,
                                    0,
                                    CopyFromParent,
                                    CopyFromParent,
                                    (Visual *)CopyFromParent,
                                    CWOverrideRedirect,
                                    &attrs);

      /* Copy the client's shape to the temporary shape_window */
      meta_core_get (frames->xdisplay, frame->xwindow,
                     META_CORE_GET_CLIENT_XWINDOW, &client_window,
                     META_CORE_GET_END);

      XShapeCombineShape (frames->xdisplay, shape_window, ShapeBounding,
                          fgeom.borders.total.left,
                          fgeom.borders.total.top,
                          client_window,
                          ShapeBounding,
                          ShapeSet);

      /* Punch the client area out of the normal frame shape,
       * then union it with the shape_window's existing shape
       */
      frame_region = get_frame_region (new_window_width,
                                       new_window_height);
      client_region = get_client_region (&fgeom,
                                         new_window_width,
                                         new_window_height);

      tmp_region = meta_ui_is_composited (frames->ui) ? frame_region : window_region;

      cairo_region_subtract (tmp_region, client_region);

      cairo_region_destroy (client_region);

      apply_cairo_region_to_window (frames->xdisplay, shape_window,
                                    tmp_region, ShapeUnion);

      cairo_region_destroy (frame_region);

      /* Now copy shape_window shape to the real frame */
      XShapeCombineShape (frames->xdisplay, frame->xwindow, ShapeBounding,
                          0, 0,
                          shape_window,
                          ShapeBounding,
                          ShapeSet);

      XDestroyWindow (frames->xdisplay, shape_window);
    }
  else
    {
      /* No shape on the client, so just do simple stuff */

      meta_topic (META_DEBUG_SHAPES,
                  "Frame 0x%lx has shaped corners\n",
                  frame->xwindow);

      if (!meta_ui_is_composited (frames->ui))
        apply_cairo_region_to_window (frames->xdisplay,
                                      frame->xwindow, window_region,
                                      ShapeSet);
    }

  frame->shape_applied = TRUE;

  cairo_region_destroy (window_region);
}

cairo_region_t *
meta_frames_get_frame_bounds (MetaFrames *frames,
                              Window      xwindow,
                              int         window_width,
                              int         window_height)
{
  MetaUIFrame *frame;
  MetaFrameGeometry fgeom;

  frame = meta_frames_lookup_window (frames, xwindow);
  g_return_val_if_fail (frame != NULL, NULL);

  meta_frames_calc_geometry (frames, frame, &fgeom);

  return get_visible_region (frames,
                             frame,
                             &fgeom,
                             window_width,
                             window_height);
}

void
meta_frames_move_resize_frame (MetaFrames *frames,
                               Window      xwindow,
                               int         x,
                               int         y,
                               int         width,
                               int         height)
{
  MetaUIFrame *frame = meta_frames_lookup_window (frames, xwindow);
  int old_width, old_height;

  old_width = gdk_window_get_width (frame->window);
  old_height = gdk_window_get_height (frame->window);

  gdk_window_move_resize (frame->window, x, y, width, height);

  if (old_width != width || old_height != height)
    invalidate_whole_window (frames, frame);
}

void
meta_frames_queue_draw (MetaFrames *frames,
                        Window      xwindow)
{
  MetaUIFrame *frame;

  frame = meta_frames_lookup_window (frames, xwindow);

  invalidate_whole_window (frames, frame);
}

void
meta_frames_set_title (MetaFrames *frames,
                       Window      xwindow,
                       const char *title)
{
  MetaUIFrame *frame;

  frame = meta_frames_lookup_window (frames, xwindow);

  g_assert (frame);

  g_free (frame->title);
  frame->title = g_strdup (title);

  invalidate_whole_window (frames, frame);
}

void
meta_frames_update_frame_style (MetaFrames *frames,
                                Window      xwindow)
{
  MetaUIFrame *frame;

  frame = meta_frames_lookup_window (frames, xwindow);

  g_assert (frame);

  meta_frames_attach_style (frames, frame);
  invalidate_whole_window (frames, frame);
}

void
meta_frames_repaint_frame (MetaFrames *frames,
                           Window      xwindow)
{
  MetaUIFrame *frame;

  frame = meta_frames_lookup_window (frames, xwindow);

  g_assert (frame);

  /* repaint everything, so the other frame don't
   * lag behind if they are exposed
   */
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gdk_window_process_all_updates ();
  G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
show_tip_now (MetaFrames *frames)
{
  const char *tiptext;
  MetaUIFrame *frame;
  int x, y, root_x, root_y;
  Window root, child;
  guint mask;
  MetaFrameControl control;

  frame = frames->last_motion_frame;
  if (frame == NULL)
    return;

  XQueryPointer (frames->xdisplay, frame->xwindow, &root, &child,
                 &root_x, &root_y, &x, &y, &mask);

  control = get_control (frames, frame, x, y);

  tiptext = NULL;
  switch (control)
    {
    case META_FRAME_CONTROL_TITLE:
      break;
    case META_FRAME_CONTROL_DELETE:
      tiptext = _("Close Window");
      break;
    case META_FRAME_CONTROL_MENU:
      tiptext = _("Window Menu");
      break;
    case META_FRAME_CONTROL_MINIMIZE:
      tiptext = _("Minimize Window");
      break;
    case META_FRAME_CONTROL_MAXIMIZE:
      tiptext = _("Maximize Window");
      break;
    case META_FRAME_CONTROL_UNMAXIMIZE:
      tiptext = _("Restore Window");
      break;
    case META_FRAME_CONTROL_RESIZE_SE:
      break;
    case META_FRAME_CONTROL_RESIZE_S:
      break;
    case META_FRAME_CONTROL_RESIZE_SW:
      break;
    case META_FRAME_CONTROL_RESIZE_N:
      break;
    case META_FRAME_CONTROL_RESIZE_NE:
      break;
    case META_FRAME_CONTROL_RESIZE_NW:
      break;
    case META_FRAME_CONTROL_RESIZE_W:
      break;
    case META_FRAME_CONTROL_RESIZE_E:
      break;
    case META_FRAME_CONTROL_NONE:
      break;
    case META_FRAME_CONTROL_CLIENT_AREA:
      break;
    default:
      break;
    }

  if (tiptext)
    {
      MetaFrameGeometry fgeom;
      GdkRectangle rect;
      int dx, dy;

      meta_frames_calc_geometry (frames, frame, &fgeom);

      if (!get_control_rect (frames, control, &fgeom, x, y, &rect))
        return;

      /* get conversion delta for root-to-frame coords */
      dx = root_x - x;
      dy = root_y - y;

      /* Align the tooltip to the button right end if RTL */
      if (meta_ui_get_direction() == META_UI_DIRECTION_RTL)
        dx += rect.width;

      meta_fixed_tip_show (rect.x + dx,
                           rect.y + rect.height + 2 + dy,
                           tiptext);
    }
}

static gboolean
tip_timeout_func (gpointer data)
{
  MetaFrames *frames;

  frames = data;

  show_tip_now (frames);

  frames->tooltip_timeout = 0;

  return FALSE;
}

#define TIP_DELAY 450
static void
queue_tip (MetaFrames *frames)
{
  clear_tip (frames);

  frames->tooltip_timeout = g_timeout_add (TIP_DELAY,
                                           tip_timeout_func,
                                           frames);
}

static void
clear_tip (MetaFrames *frames)
{
  if (frames->tooltip_timeout)
    {
      g_source_remove (frames->tooltip_timeout);
      frames->tooltip_timeout = 0;
    }
  meta_fixed_tip_hide ();
}

static void
redraw_control (MetaFrames       *frames,
                MetaUIFrame      *frame,
                MetaFrameControl  control,
                gint              x,
                gint              y)
{
  MetaFrameGeometry fgeom;
  GdkRectangle rect;

  meta_frames_calc_geometry (frames, frame, &fgeom);

  if (!get_control_rect (frames, control, &fgeom, x, y, &rect))
    return;

  gdk_window_invalidate_rect (frame->window, &rect, FALSE);
  invalidate_cache (frames, frame);
}

static void
update_prelit_control (MetaFrames       *frames,
                       MetaUIFrame      *frame,
                       MetaFrameControl  control,
                       gint              x,
                       gint              y)
{
  MetaCursor cursor;
  MetaFrameControl old_control;
  gint old_x;
  gint old_y;

  meta_verbose ("Updating prelit control from %u to %u\n",
                frame->prelit_control, control);

  switch (control)
    {
      case META_FRAME_CONTROL_RESIZE_SE:
        cursor = META_CURSOR_SE_RESIZE;
        break;

      case META_FRAME_CONTROL_RESIZE_S:
        cursor = META_CURSOR_SOUTH_RESIZE;
        break;

      case META_FRAME_CONTROL_RESIZE_SW:
        cursor = META_CURSOR_SW_RESIZE;
        break;

      case META_FRAME_CONTROL_RESIZE_N:
        cursor = META_CURSOR_NORTH_RESIZE;
        break;

      case META_FRAME_CONTROL_RESIZE_NE:
        cursor = META_CURSOR_NE_RESIZE;
        break;

      case META_FRAME_CONTROL_RESIZE_NW:
        cursor = META_CURSOR_NW_RESIZE;
        break;

      case META_FRAME_CONTROL_RESIZE_W:
        cursor = META_CURSOR_WEST_RESIZE;
        break;

      case META_FRAME_CONTROL_RESIZE_E:
        cursor = META_CURSOR_EAST_RESIZE;
        break;

      case META_FRAME_CONTROL_CLIENT_AREA:
      case META_FRAME_CONTROL_NONE:
      case META_FRAME_CONTROL_TITLE:
      case META_FRAME_CONTROL_DELETE:
      case META_FRAME_CONTROL_MENU:
      case META_FRAME_CONTROL_MINIMIZE:
      case META_FRAME_CONTROL_MAXIMIZE:
      case META_FRAME_CONTROL_UNMAXIMIZE:
      default:
        cursor = META_CURSOR_DEFAULT;
        break;
    }

  /* set/unset the prelight cursor */
  meta_core_set_screen_cursor (frames->xdisplay, frame->xwindow, cursor);

  switch (control)
    {
      case META_FRAME_CONTROL_MENU:
      case META_FRAME_CONTROL_MINIMIZE:
      case META_FRAME_CONTROL_MAXIMIZE:
      case META_FRAME_CONTROL_DELETE:
      case META_FRAME_CONTROL_UNMAXIMIZE:
        /* leave control set */
        break;

      case META_FRAME_CONTROL_NONE:
      case META_FRAME_CONTROL_TITLE:
      case META_FRAME_CONTROL_RESIZE_SE:
      case META_FRAME_CONTROL_RESIZE_S:
      case META_FRAME_CONTROL_RESIZE_SW:
      case META_FRAME_CONTROL_RESIZE_N:
      case META_FRAME_CONTROL_RESIZE_NE:
      case META_FRAME_CONTROL_RESIZE_NW:
      case META_FRAME_CONTROL_RESIZE_W:
      case META_FRAME_CONTROL_RESIZE_E:
      case META_FRAME_CONTROL_CLIENT_AREA:
      default:
        /* Only prelight buttons */
        control = META_FRAME_CONTROL_NONE;
        break;
    }

  if (control == frame->prelit_control)
    return;

  /* Save the old control so we can unprelight it */
  old_control = frame->prelit_control;
  old_x = frame->prelit_x;
  old_y = frame->prelit_y;

  frame->prelit_control = control;
  frame->prelit_x = x;
  frame->prelit_y = y;

  redraw_control (frames, frame, old_control, old_x, old_y);
  redraw_control (frames, frame, control, x, y);
}

static gboolean
meta_frame_titlebar_event (MetaFrames     *frames,
                           MetaUIFrame    *frame,
                           GdkEventButton *event,
                           int            action)
{
  MetaFrameFlags flags;

  switch (action)
    {
    case G_DESKTOP_TITLEBAR_ACTION_TOGGLE_SHADE:
      {
        meta_core_get (frames->xdisplay, frame->xwindow,
                       META_CORE_GET_FRAME_FLAGS, &flags,
                       META_CORE_GET_END);

        if (flags & META_FRAME_ALLOWS_SHADE)
          {
            if (flags & META_FRAME_SHADED)
              meta_core_unshade (frames->xdisplay, frame->xwindow, event->time);
            else
              meta_core_shade (frames->xdisplay, frame->xwindow, event->time);
          }
      }
      break;

    case G_DESKTOP_TITLEBAR_ACTION_TOGGLE_MAXIMIZE:
      {
        meta_core_get (frames->xdisplay, frame->xwindow,
                       META_CORE_GET_FRAME_FLAGS, &flags,
                       META_CORE_GET_END);

        if (flags & META_FRAME_ALLOWS_MAXIMIZE)
          {
            meta_core_toggle_maximize (frames->xdisplay, frame->xwindow);
          }
      }
      break;

    case G_DESKTOP_TITLEBAR_ACTION_TOGGLE_MAXIMIZE_HORIZONTALLY:
      {
        meta_core_get (frames->xdisplay, frame->xwindow,
                       META_CORE_GET_FRAME_FLAGS, &flags,
                       META_CORE_GET_END);

        if (flags & META_FRAME_ALLOWS_MAXIMIZE)
          {
            meta_core_toggle_maximize_horizontally (frames->xdisplay, frame->xwindow);
          }
      }
      break;

    case G_DESKTOP_TITLEBAR_ACTION_TOGGLE_MAXIMIZE_VERTICALLY:
      {
        meta_core_get (frames->xdisplay, frame->xwindow,
                       META_CORE_GET_FRAME_FLAGS, &flags,
                       META_CORE_GET_END);

        if (flags & META_FRAME_ALLOWS_MAXIMIZE)
          {
            meta_core_toggle_maximize_vertically (frames->xdisplay, frame->xwindow);
          }
      }
      break;

    case G_DESKTOP_TITLEBAR_ACTION_MINIMIZE:
      {
        meta_core_get (frames->xdisplay, frame->xwindow,
                       META_CORE_GET_FRAME_FLAGS, &flags,
                       META_CORE_GET_END);

        if (flags & META_FRAME_ALLOWS_MINIMIZE)
          {
            meta_core_minimize (frames->xdisplay, frame->xwindow);
          }
      }
      break;

    case G_DESKTOP_TITLEBAR_ACTION_NONE:
      /* Yaay, a sane user that doesn't use that other weird crap! */
      break;

    case G_DESKTOP_TITLEBAR_ACTION_LOWER:
      meta_core_user_lower_and_unfocus (frames->xdisplay, frame->xwindow, event->time);
      break;

    case G_DESKTOP_TITLEBAR_ACTION_MENU:
      {
        GdkRectangle rect;

        rect.x = event->x_root;
        rect.y = event->y_root;
        rect.width = 0;
        rect.height = 0;

        meta_core_show_window_menu (frames->xdisplay, frame->xwindow,
                                    &rect, event->time);
      }
      break;

    default:
      break;
    }

  return TRUE;
}

static gboolean
meta_frame_double_click_event (MetaFrames     *frames,
                               MetaUIFrame    *frame,
                               GdkEventButton *event)
{
  int action = meta_prefs_get_action_double_click_titlebar ();

  return meta_frame_titlebar_event (frames, frame, event, action);
}

static gboolean
meta_frame_middle_click_event (MetaFrames     *frames,
                               MetaUIFrame    *frame,
                               GdkEventButton *event)
{
  int action = meta_prefs_get_action_middle_click_titlebar();

  return meta_frame_titlebar_event (frames, frame, event, action);
}

static gboolean
meta_frame_right_click_event (MetaFrames     *frames,
                              MetaUIFrame    *frame,
                              GdkEventButton *event)
{
  int action = meta_prefs_get_action_right_click_titlebar();

  return meta_frame_titlebar_event (frames, frame, event, action);
}

static gboolean
meta_frames_button_press_event (GtkWidget      *widget,
                                GdkEventButton *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;
  MetaFrameControl control;

  frames = META_FRAMES (widget);

  /* Remember that the display may have already done something with this event.
   * If so there's probably a GrabOp in effect.
   */

  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  clear_tip (frames);

  control = get_control (frames, frame, event->x, event->y);

  /* focus on click, even if click was on client area */
  if (event->button == 1 &&
      !(control == META_FRAME_CONTROL_MINIMIZE ||
        control == META_FRAME_CONTROL_DELETE ||
        control == META_FRAME_CONTROL_MAXIMIZE))
    {
      meta_topic (META_DEBUG_FOCUS,
                  "Focusing window with frame 0x%lx due to button 1 press\n",
                  frame->xwindow);
      meta_core_user_focus (frames->xdisplay, frame->xwindow, event->time);
    }

  /* don't do the rest of this if on client area */
  if (control == META_FRAME_CONTROL_CLIENT_AREA)
    return FALSE; /* not on the frame, just passed through from client */

  /* We want to shade even if we have a GrabOp, since we'll have a move grab
   * if we double click the titlebar.
   */
  if (control == META_FRAME_CONTROL_TITLE &&
      event->button == 1 &&
      event->type == GDK_2BUTTON_PRESS)
    {
      meta_core_end_grab_op (frames->xdisplay, event->time);
      return meta_frame_double_click_event (frames, frame, event);
    }

  if (meta_core_get_grab_op (frames->xdisplay) != META_GRAB_OP_NONE)
    return FALSE; /* already up to something */

  if (event->button == 1 &&
      (control == META_FRAME_CONTROL_MAXIMIZE ||
       control == META_FRAME_CONTROL_UNMAXIMIZE ||
       control == META_FRAME_CONTROL_MINIMIZE ||
       control == META_FRAME_CONTROL_DELETE ||
       control == META_FRAME_CONTROL_MENU))
    {
      MetaGrabOp op = META_GRAB_OP_NONE;

      if (control == META_FRAME_CONTROL_MINIMIZE)
        op = META_GRAB_OP_CLICKING_MINIMIZE;
      else if (control == META_FRAME_CONTROL_MAXIMIZE)
        op = META_GRAB_OP_CLICKING_MAXIMIZE;
      else if (control == META_FRAME_CONTROL_UNMAXIMIZE)
        op = META_GRAB_OP_CLICKING_UNMAXIMIZE;
      else if (control == META_FRAME_CONTROL_DELETE)
        op = META_GRAB_OP_CLICKING_DELETE;
      else if (control == META_FRAME_CONTROL_MENU)
        op = META_GRAB_OP_CLICKING_MENU;
      else
        g_assert_not_reached ();

      meta_core_begin_grab_op (frames->xdisplay,
                               frame->xwindow,
                               op,
                               TRUE,
                               TRUE,
                               event->button,
                               0,
                               event->time,
                               event->x_root,
                               event->y_root);

      frame->prelit_control = control;
      frame->prelit_x = event->x;
      frame->prelit_y = event->y;

      redraw_control (frames, frame, control, event->x, event->y);

      if (op == META_GRAB_OP_CLICKING_MENU)
        {
          MetaFrameGeometry fgeom;
          GdkRectangle rect;

          meta_frames_calc_geometry (frames, frame, &fgeom);

          if (!get_control_rect (frames, META_FRAME_CONTROL_MENU, &fgeom,
                                 event->x, event->y, &rect))
            {
              return FALSE;
            }

          /* convert to root coords */
          rect.x += event->x_root - event->x;
          rect.y += event->y_root - event->y;

          frame->ignore_leave_notify = TRUE;
          meta_core_show_window_menu (frames->xdisplay,
                                      frame->xwindow,
                                      &rect, event->time);
        }
    }
  else if (event->button == 1 &&
           (control == META_FRAME_CONTROL_RESIZE_SE ||
            control == META_FRAME_CONTROL_RESIZE_S ||
            control == META_FRAME_CONTROL_RESIZE_SW ||
            control == META_FRAME_CONTROL_RESIZE_NE ||
            control == META_FRAME_CONTROL_RESIZE_N ||
            control == META_FRAME_CONTROL_RESIZE_NW ||
            control == META_FRAME_CONTROL_RESIZE_E ||
            control == META_FRAME_CONTROL_RESIZE_W))
    {
      MetaGrabOp op;

      op = META_GRAB_OP_NONE;

      if (control == META_FRAME_CONTROL_RESIZE_SE)
        op = META_GRAB_OP_RESIZING_SE;
      else if (control == META_FRAME_CONTROL_RESIZE_S)
        op = META_GRAB_OP_RESIZING_S;
      else if (control == META_FRAME_CONTROL_RESIZE_SW)
        op = META_GRAB_OP_RESIZING_SW;
      else if (control == META_FRAME_CONTROL_RESIZE_NE)
        op = META_GRAB_OP_RESIZING_NE;
      else if (control == META_FRAME_CONTROL_RESIZE_N)
        op = META_GRAB_OP_RESIZING_N;
      else if (control == META_FRAME_CONTROL_RESIZE_NW)
        op = META_GRAB_OP_RESIZING_NW;
      else if (control == META_FRAME_CONTROL_RESIZE_E)
        op = META_GRAB_OP_RESIZING_E;
      else if (control == META_FRAME_CONTROL_RESIZE_W)
        op = META_GRAB_OP_RESIZING_W;
      else
        g_assert_not_reached ();

      meta_core_begin_grab_op (frames->xdisplay,
                               frame->xwindow,
                               op,
                               TRUE,
                               TRUE,
                               event->button,
                               0,
                               event->time,
                               event->x_root,
                               event->y_root);
    }
  else if (control == META_FRAME_CONTROL_TITLE &&
           event->button == 1)
    {
      MetaFrameFlags flags;

      meta_core_get (frames->xdisplay, frame->xwindow,
                     META_CORE_GET_FRAME_FLAGS, &flags,
                     META_CORE_GET_END);

      if (flags & META_FRAME_ALLOWS_MOVE)
        {
          meta_core_begin_grab_op (frames->xdisplay,
                                   frame->xwindow,
                                   META_GRAB_OP_MOVING,
                                   TRUE,
                                   TRUE,
                                   event->button,
                                   0,
                                   event->time,
                                   event->x_root,
                                   event->y_root);
        }
    }
  else if (event->button == 2)
    {
      return meta_frame_middle_click_event (frames, frame, event);
    }
  else if (event->button == 3)
    {
      return meta_frame_right_click_event (frames, frame, event);
    }

  return TRUE;
}

void
meta_frames_notify_menu_hide (MetaFrames *frames)
{
  if (meta_core_get_grab_op (frames->xdisplay) == META_GRAB_OP_CLICKING_MENU)
    {
      Window grab_frame;

      grab_frame = meta_core_get_grab_frame (frames->xdisplay);

      if (grab_frame != None)
        {
          MetaUIFrame *frame;

          frame = meta_frames_lookup_window (frames, grab_frame);

          if (frame)
            {
              redraw_control (frames, frame, META_FRAME_CONTROL_MENU,
                              frame->prelit_x, frame->prelit_y);

              meta_core_end_grab_op (frames->xdisplay, CurrentTime);
            }
        }
    }
}

static gboolean
meta_frames_button_release_event    (GtkWidget           *widget,
                                     GdkEventButton      *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;
  MetaGrabOp op;

  frames = META_FRAMES (widget);

  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  clear_tip (frames);

  op = meta_core_get_grab_op (frames->xdisplay);

  if (op == META_GRAB_OP_NONE)
    return FALSE;

  /* We only handle the releases we handled the presses for (things
   * involving frame controls). Window ops that don't require a
   * frame are handled in the Xlib part of the code, display.c/window.c
   */
  if (frame->xwindow == meta_core_get_grab_frame (frames->xdisplay) &&
      ((int) event->button) == meta_core_get_grab_button (frames->xdisplay))
    {
      MetaFrameControl control;

      control = get_control (frames, frame, event->x, event->y);

      switch (op)
        {
        case META_GRAB_OP_CLICKING_MINIMIZE:
          if (control == META_FRAME_CONTROL_MINIMIZE)
            meta_core_minimize (frames->xdisplay, frame->xwindow);

          meta_core_end_grab_op (frames->xdisplay, event->time);
          break;

        case META_GRAB_OP_CLICKING_MAXIMIZE:
          if (control == META_FRAME_CONTROL_MAXIMIZE)
          {
            /* Focus the window on the maximize */
            meta_core_user_focus (frames->xdisplay, frame->xwindow, event->time);
            meta_core_maximize (frames->xdisplay, frame->xwindow);
          }
          meta_core_end_grab_op (frames->xdisplay, event->time);
          break;

        case META_GRAB_OP_CLICKING_UNMAXIMIZE:
          if (control == META_FRAME_CONTROL_UNMAXIMIZE)
            meta_core_unmaximize (frames->xdisplay, frame->xwindow);

          meta_core_end_grab_op (frames->xdisplay, event->time);
          break;

        case META_GRAB_OP_CLICKING_DELETE:
          if (control == META_FRAME_CONTROL_DELETE)
            meta_core_delete (frames->xdisplay, frame->xwindow, event->time);

          meta_core_end_grab_op (frames->xdisplay, event->time);
          break;

        case META_GRAB_OP_CLICKING_MENU:
          meta_core_end_grab_op (frames->xdisplay, event->time);
          break;

        case META_GRAB_OP_MOVING:
        case META_GRAB_OP_RESIZING_SE:
        case META_GRAB_OP_RESIZING_S:
        case META_GRAB_OP_RESIZING_SW:
        case META_GRAB_OP_RESIZING_N:
        case META_GRAB_OP_RESIZING_NE:
        case META_GRAB_OP_RESIZING_NW:
        case META_GRAB_OP_RESIZING_W:
        case META_GRAB_OP_RESIZING_E:
        case META_GRAB_OP_KEYBOARD_MOVING:
        case META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN:
        case META_GRAB_OP_KEYBOARD_RESIZING_S:
        case META_GRAB_OP_KEYBOARD_RESIZING_N:
        case META_GRAB_OP_KEYBOARD_RESIZING_W:
        case META_GRAB_OP_KEYBOARD_RESIZING_E:
        case META_GRAB_OP_KEYBOARD_RESIZING_SE:
        case META_GRAB_OP_KEYBOARD_RESIZING_NE:
        case META_GRAB_OP_KEYBOARD_RESIZING_SW:
        case META_GRAB_OP_KEYBOARD_RESIZING_NW:
        case META_GRAB_OP_KEYBOARD_TABBING_NORMAL:
        case META_GRAB_OP_KEYBOARD_TABBING_DOCK:
        case META_GRAB_OP_KEYBOARD_ESCAPING_NORMAL:
        case META_GRAB_OP_KEYBOARD_ESCAPING_DOCK:
        case META_GRAB_OP_KEYBOARD_ESCAPING_GROUP:
        case META_GRAB_OP_KEYBOARD_TABBING_GROUP:
        case META_GRAB_OP_KEYBOARD_WORKSPACE_SWITCHING:
          break;

        case META_GRAB_OP_NONE:
          g_assert_not_reached ();
          break;

        default:
          break;
        }

      /* Update the prelit control regardless of what button the mouse
       * was released over; needed so that the new button can become
       * prelit so to let the user know that it can now be pressed.
       * :)
       */
      update_prelit_control (frames, frame, control, event->x, event->y);
    }

  return TRUE;
}

static gboolean
meta_frames_motion_notify_event     (GtkWidget           *widget,
                                     GdkEventMotion      *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;
  MetaGrabOp grab_op;

  frames = META_FRAMES (widget);

  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  clear_tip (frames);

  frames->last_motion_frame = frame;

  grab_op = meta_core_get_grab_op (frames->xdisplay);

  switch (grab_op)
    {
    case META_GRAB_OP_CLICKING_MENU:
    case META_GRAB_OP_CLICKING_DELETE:
    case META_GRAB_OP_CLICKING_MINIMIZE:
    case META_GRAB_OP_CLICKING_MAXIMIZE:
    case META_GRAB_OP_CLICKING_UNMAXIMIZE:
      {
        MetaFrameControl control;
        int x, y;

        gdk_window_get_device_position (frame->window, event->device,
                                        &x, &y, NULL);

        /* Control is set to none unless it matches
         * the current grab
         */
        control = get_control (frames, frame, x, y);
        if (! ((control == META_FRAME_CONTROL_MENU &&
                grab_op == META_GRAB_OP_CLICKING_MENU) ||
               (control == META_FRAME_CONTROL_DELETE &&
                grab_op == META_GRAB_OP_CLICKING_DELETE) ||
               (control == META_FRAME_CONTROL_MINIMIZE &&
                grab_op == META_GRAB_OP_CLICKING_MINIMIZE) ||
               ((control == META_FRAME_CONTROL_MAXIMIZE ||
                 control == META_FRAME_CONTROL_UNMAXIMIZE) &&
                (grab_op == META_GRAB_OP_CLICKING_MAXIMIZE ||
                 grab_op == META_GRAB_OP_CLICKING_UNMAXIMIZE))))
           control = META_FRAME_CONTROL_NONE;

        /* Update prelit control and cursor */
        update_prelit_control (frames, frame, control, x, y);

        /* No tooltip while in the process of clicking */
      }
      break;
    case META_GRAB_OP_NONE:
      {
        MetaFrameControl control;
        int x, y;

        gdk_window_get_device_position (frame->window, event->device,
                                        &x, &y, NULL);

        control = get_control (frames, frame, x, y);

        /* Update prelit control and cursor */
        update_prelit_control (frames, frame, control, x, y);

        queue_tip (frames);
      }
      break;

    case META_GRAB_OP_MOVING:
    case META_GRAB_OP_RESIZING_SE:
    case META_GRAB_OP_RESIZING_S:
    case META_GRAB_OP_RESIZING_SW:
    case META_GRAB_OP_RESIZING_N:
    case META_GRAB_OP_RESIZING_NE:
    case META_GRAB_OP_RESIZING_NW:
    case META_GRAB_OP_RESIZING_W:
    case META_GRAB_OP_RESIZING_E:
    case META_GRAB_OP_KEYBOARD_MOVING:
    case META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN:
    case META_GRAB_OP_KEYBOARD_RESIZING_S:
    case META_GRAB_OP_KEYBOARD_RESIZING_N:
    case META_GRAB_OP_KEYBOARD_RESIZING_W:
    case META_GRAB_OP_KEYBOARD_RESIZING_E:
    case META_GRAB_OP_KEYBOARD_RESIZING_SE:
    case META_GRAB_OP_KEYBOARD_RESIZING_NE:
    case META_GRAB_OP_KEYBOARD_RESIZING_SW:
    case META_GRAB_OP_KEYBOARD_RESIZING_NW:
    case META_GRAB_OP_KEYBOARD_TABBING_NORMAL:
    case META_GRAB_OP_KEYBOARD_TABBING_DOCK:
    case META_GRAB_OP_KEYBOARD_ESCAPING_NORMAL:
    case META_GRAB_OP_KEYBOARD_ESCAPING_DOCK:
    case META_GRAB_OP_KEYBOARD_ESCAPING_GROUP:
    case META_GRAB_OP_KEYBOARD_TABBING_GROUP:
    case META_GRAB_OP_KEYBOARD_WORKSPACE_SWITCHING:
      break;

    default:
      break;
    }

  return TRUE;
}

/* Returns a pixmap with a piece of the windows frame painted on it.
*/

static cairo_surface_t *
generate_pixmap (MetaFrames            *frames,
                 MetaUIFrame           *frame,
                 cairo_rectangle_int_t *rect)
{
  cairo_surface_t *result;
  cairo_t *cr;

  /* do not create a pixmap for nonexisting areas */
  if (rect->width <= 0 || rect->height <= 0)
    return NULL;

  result = gdk_window_create_similar_surface (frame->window,
                                              CAIRO_CONTENT_COLOR_ALPHA,
                                              rect->width, rect->height);

  cr = cairo_create (result);
  cairo_translate (cr, -rect->x, -rect->y);

  meta_frames_paint (frames, frame, cr);

  cairo_destroy (cr);

  return result;
}

static void
populate_cache (MetaFrames  *frames,
                MetaUIFrame *frame)
{
  MetaFrameBorders borders;
  int width, height;
  int frame_width, frame_height, screen_width, screen_height;
  CachedPixels *pixels;
  MetaFrameType frame_type;
  MetaFrameFlags frame_flags;
  int i;

  meta_core_get (frames->xdisplay, frame->xwindow,
                 META_CORE_GET_FRAME_WIDTH, &frame_width,
                 META_CORE_GET_FRAME_HEIGHT, &frame_height,
                 META_CORE_GET_SCREEN_WIDTH, &screen_width,
                 META_CORE_GET_SCREEN_HEIGHT, &screen_height,
                 META_CORE_GET_CLIENT_WIDTH, &width,
                 META_CORE_GET_CLIENT_HEIGHT, &height,
                 META_CORE_GET_FRAME_TYPE, &frame_type,
                 META_CORE_GET_FRAME_FLAGS, &frame_flags,
                 META_CORE_GET_END);

  /* don't cache extremely large windows */
  if (frame_width > 2 * screen_width ||
      frame_height > 2 * screen_height)
    {
      return;
    }

  meta_theme_get_frame_borders (meta_ui_get_theme (frames->ui),
                                frame->theme_variant, frame_type,
                                frame_flags, &borders);

  pixels = get_cache (frames, frame);

  /* Setup the rectangles for the four visible frame borders. First top, then
   * left, right and bottom. Top and bottom extend to the invisible borders
   * while left and right snugly fit in between:
   * -----
   * | |
   * -----
   */

  /* width and height refer to the client window's
   * size without any border added. */

  /* top */
  pixels->piece[0].rect.x = borders.invisible.left - borders.shadow.left;
  pixels->piece[0].rect.y = borders.invisible.top - borders.shadow.top;
  pixels->piece[0].rect.width = width + borders.visible.left + borders.shadow.left +
                                borders.visible.right + borders.shadow.right;
  pixels->piece[0].rect.height = borders.visible.top + borders.shadow.top;

  /* left */
  pixels->piece[1].rect.x = borders.invisible.left - borders.shadow.left;
  pixels->piece[1].rect.y = borders.total.top;
  pixels->piece[1].rect.height = height;
  pixels->piece[1].rect.width = borders.visible.left + borders.shadow.left;

  /* right */
  pixels->piece[2].rect.x = borders.total.left + width;
  pixels->piece[2].rect.y = borders.total.top;
  pixels->piece[2].rect.width = borders.visible.right  + borders.shadow.right;
  pixels->piece[2].rect.height = height;

  /* bottom */
  pixels->piece[3].rect.x = borders.invisible.left - borders.shadow.left;
  pixels->piece[3].rect.y = borders.total.top + height;
  pixels->piece[3].rect.width = width + borders.visible.left + borders.shadow.left +
                                borders.visible.right + borders.shadow.right;
  pixels->piece[3].rect.height = borders.visible.bottom + borders.shadow.bottom;

  for (i = 0; i < 4; i++)
    {
      CachedFramePiece *piece = &pixels->piece[i];
      if (!piece->pixmap)
        piece->pixmap = generate_pixmap (frames, frame, &piece->rect);
    }

  if (frames->invalidate_cache_timeout_id)
    g_source_remove (frames->invalidate_cache_timeout_id);

  frames->invalidate_cache_timeout_id = g_timeout_add (1000, invalidate_cache_timeout, frames);

  if (!g_list_find (frames->invalidate_frames, frame))
    frames->invalidate_frames =
      g_list_prepend (frames->invalidate_frames, frame);
}

static void
subtract_client_area (MetaFrames     *frames,
                      cairo_region_t *region,
                      MetaUIFrame    *frame)
{
  cairo_rectangle_int_t area;
  MetaFrameFlags flags;
  MetaFrameType type;
  MetaFrameBorders borders;
  cairo_region_t *tmp_region;

  meta_core_get (frames->xdisplay, frame->xwindow,
                 META_CORE_GET_FRAME_FLAGS, &flags,
                 META_CORE_GET_FRAME_TYPE, &type,
                 META_CORE_GET_CLIENT_WIDTH, &area.width,
                 META_CORE_GET_CLIENT_HEIGHT, &area.height,
                 META_CORE_GET_END);

  meta_theme_get_frame_borders (meta_ui_get_theme (frames->ui),
                                frame->theme_variant, type,
                                flags, &borders);

  area.x = borders.total.left;
  area.y = borders.total.top;

  tmp_region = cairo_region_create_rectangle (&area);
  cairo_region_subtract (region, tmp_region);
  cairo_region_destroy (tmp_region);
}

static void
cached_pixels_draw (CachedPixels   *pixels,
                    cairo_t        *cr,
                    cairo_region_t *region)
{
  cairo_region_t *region_piece;
  int i;

  for (i = 0; i < 4; i++)
    {
      CachedFramePiece *piece;
      piece = &pixels->piece[i];

      if (piece->pixmap)
        {
          cairo_set_source_surface (cr, piece->pixmap,
                                    piece->rect.x, piece->rect.y);
          cairo_paint (cr);

          region_piece = cairo_region_create_rectangle (&piece->rect);
          cairo_region_subtract (region, region_piece);
          cairo_region_destroy (region_piece);
        }
    }
}

/* XXX -- this is disgusting. Find a better approach here.
 * Use multiple widgets? */
static MetaUIFrame *
find_frame_to_draw (MetaFrames *frames,
                    cairo_t    *cr)
{
  GHashTableIter iter;
  MetaUIFrame *frame;

  g_hash_table_iter_init (&iter, frames->frames);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &frame))
    if (gtk_cairo_should_draw_window (cr, frame->window))
      return frame;

  return NULL;
}

static gboolean
meta_frames_draw (GtkWidget *widget,
                  cairo_t   *cr)
{
  MetaUIFrame *frame;
  MetaFrames *frames;
  CachedPixels *pixels;
  cairo_region_t *region;
  cairo_rectangle_int_t clip;
  int i, n_areas;

  frames = META_FRAMES (widget);
  gdk_cairo_get_clip_rectangle (cr, &clip);

  frame = find_frame_to_draw (frames, cr);
  if (frame == NULL)
    return FALSE;

  populate_cache (frames, frame);

  pixels = get_cache (frames, frame);
  region = cairo_region_create_rectangle (&clip);

  cached_pixels_draw (pixels, cr, region);

  subtract_client_area (frames, region, frame);

  n_areas = cairo_region_num_rectangles (region);

  for (i = 0; i < n_areas; i++)
    {
      cairo_rectangle_int_t area;

      cairo_region_get_rectangle (region, i, &area);

      cairo_save (cr);

      cairo_rectangle (cr, area.x, area.y, area.width, area.height);
      cairo_clip (cr);

      cairo_push_group (cr);

      meta_frames_paint (frames, frame, cr);

      cairo_pop_group_to_source (cr);
      cairo_paint (cr);

      cairo_restore (cr);
    }

  cairo_region_destroy (region);

  return TRUE;
}

typedef struct
{
  MetaFrames  *frames;
  MetaUIFrame *frame;
} ButtonStateData;

static MetaButtonState
update_button_state (MetaButtonType type,
                     GdkRectangle   rect,
                     gpointer       user_data)
{
  ButtonStateData *data;
  MetaButtonState state;
  Window grab_frame;
  MetaGrabOp grab_op;
  MetaFrameControl control;
  GdkDisplay *display;
  GdkSeat *seat;
  GdkDevice *device;
  gint x;
  gint y;

  data = (ButtonStateData *) user_data;

  state = META_BUTTON_STATE_NORMAL;

  grab_frame = meta_core_get_grab_frame (data->frames->xdisplay);
  grab_op = meta_core_get_grab_op (data->frames->xdisplay);
  if (grab_frame != data->frame->xwindow)
    grab_op = META_GRAB_OP_NONE;

  control = data->frame->prelit_control;

  display = gdk_display_get_default ();
  seat = gdk_display_get_default_seat (display);
  device = gdk_seat_get_pointer (seat);

  gdk_window_get_device_position (data->frame->window, device, &x, &y, NULL);

  if (!POINT_IN_RECT (x, y, rect))
    return state;

  /* Set prelight state */
  if (control == META_FRAME_CONTROL_MENU &&
      type == META_BUTTON_TYPE_MENU)
    {
      if (grab_op == META_GRAB_OP_CLICKING_MENU)
        state = META_BUTTON_STATE_PRESSED;
      else
        state = META_BUTTON_STATE_PRELIGHT;
    }
  else if (control == META_FRAME_CONTROL_MINIMIZE &&
           type == META_BUTTON_TYPE_MINIMIZE)
    {
      if (grab_op == META_GRAB_OP_CLICKING_MINIMIZE)
        state = META_BUTTON_STATE_PRESSED;
      else
        state = META_BUTTON_STATE_PRELIGHT;
    }
  else if (control == META_FRAME_CONTROL_MAXIMIZE &&
           type == META_BUTTON_TYPE_MAXIMIZE)
    {
      if (grab_op == META_GRAB_OP_CLICKING_MAXIMIZE)
        state = META_BUTTON_STATE_PRESSED;
      else
        state = META_BUTTON_STATE_PRELIGHT;
    }
  else if (control == META_FRAME_CONTROL_UNMAXIMIZE &&
           type == META_BUTTON_TYPE_MAXIMIZE)
    {
      if (grab_op == META_GRAB_OP_CLICKING_UNMAXIMIZE)
        state = META_BUTTON_STATE_PRESSED;
      else
        state = META_BUTTON_STATE_PRELIGHT;
    }
  else if (control == META_FRAME_CONTROL_DELETE &&
           type == META_BUTTON_TYPE_CLOSE)
    {
      if (grab_op == META_GRAB_OP_CLICKING_DELETE)
        state = META_BUTTON_STATE_PRESSED;
      else
        state = META_BUTTON_STATE_PRELIGHT;
    }

  return state;
}

static void
meta_frames_paint (MetaFrames  *frames,
                   MetaUIFrame *frame,
                   cairo_t     *cr)
{
  MetaFrameFlags flags;
  MetaFrameType type;
  GdkPixbuf *mini_icon;
  GdkPixbuf *icon;
  int w, h;
  ButtonStateData data;

  meta_core_get (frames->xdisplay, frame->xwindow,
                 META_CORE_GET_FRAME_FLAGS, &flags,
                 META_CORE_GET_FRAME_TYPE, &type,
                 META_CORE_GET_MINI_ICON, &mini_icon,
                 META_CORE_GET_ICON, &icon,
                 META_CORE_GET_CLIENT_WIDTH, &w,
                 META_CORE_GET_CLIENT_HEIGHT, &h,
                 META_CORE_GET_END);

  data.frames = frames;
  data.frame = frame;

  meta_theme_draw_frame (meta_ui_get_theme (frames->ui), frame->theme_variant,
                         cr, type, flags, w, h, frame->title,
                         update_button_state, &data, mini_icon, icon);
}

static gboolean
meta_frames_enter_notify_event      (GtkWidget           *widget,
                                     GdkEventCrossing    *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;
  MetaFrameControl control;

  frames = META_FRAMES (widget);

  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  frame->ignore_leave_notify = FALSE;

  control = get_control (frames, frame, event->x, event->y);
  update_prelit_control (frames, frame, control, event->x, event->y);

  return TRUE;
}

static gboolean
meta_frames_leave_notify_event      (GtkWidget           *widget,
                                     GdkEventCrossing    *event)
{
  MetaUIFrame *frame;
  MetaFrames *frames;
  MetaGrabOp grab_op;

  frames = META_FRAMES (widget);

  frame = meta_frames_lookup_window (frames, GDK_WINDOW_XID (event->window));
  if (frame == NULL)
    return FALSE;

  grab_op = meta_core_get_grab_op (frames->xdisplay);
  frame->ignore_leave_notify = frame->ignore_leave_notify &&
                               grab_op == META_GRAB_OP_CLICKING_MENU;

  if (frame->ignore_leave_notify)
    return FALSE;

  update_prelit_control (frames, frame, META_FRAME_CONTROL_NONE,
                         event->x, event->y);

  clear_tip (frames);

  return TRUE;
}

static void
invalidate_whole_window (MetaFrames *frames,
                         MetaUIFrame *frame)
{
  gdk_window_invalidate_rect (frame->window, NULL, FALSE);
  invalidate_cache (frames, frame);
}

void
meta_frames_composited_changed (MetaFrames *frames)
{
  g_hash_table_foreach (frames->frames, queue_recalc_func, frames);
}
