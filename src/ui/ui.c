/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity interface for talking to GTK+ UI module */

/*
 * Copyright (C) 2002 Havoc Pennington
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

#include "config.h"

#include "prefs.h"
#include "ui.h"
#include "ui-private.h"
#include "frames.h"
#include "util.h"
#include "menu.h"
#include "core.h"

#include <string.h>
#include <stdlib.h>
#include <cairo-xlib.h>
#include <libmetacity/meta-theme.h>

#define META_DEFAULT_ICON_NAME "window"

struct _MetaUI
{
  Display *xdisplay;

  gboolean composited;
  gint scale;
  gdouble dpi;

  MetaTheme *theme;
  MetaFrames *frames;

  /* For double-click tracking */
  guint button_click_number;
  Window button_click_window;
  int button_click_x;
  int button_click_y;
  guint32 button_click_time;
};

static gboolean
get_int_setting (const gchar *name,
                 gint        *value)
{
  GValue gvalue = G_VALUE_INIT;

  g_value_init (&gvalue, G_TYPE_INT);

  if (gdk_screen_get_setting (gdk_screen_get_default (), name, &gvalue))
    {
      *value = g_value_get_int (&gvalue);
      return TRUE;
    }

  return FALSE;
}

static gint
get_window_scaling_factor (MetaUI *ui)
{
  gint scale;

  if (get_int_setting ("gdk-window-scaling-factor", &scale))
    return scale;

  return 1;
}

static gdouble
get_xft_dpi (MetaUI *ui)
{
  const gchar *dpi_scale_env;
  gint xft_dpi;
  gdouble dpi;

  dpi = 96.0;
  if (get_int_setting ("gtk-xft-dpi", &xft_dpi) && xft_dpi > 0)
    dpi = xft_dpi / 1024.0 / ui->scale;

  dpi_scale_env = g_getenv ("GDK_DPI_SCALE");
  if (dpi_scale_env != NULL)
    {
      gdouble dpi_scale;

      dpi_scale = g_ascii_strtod (dpi_scale_env, NULL);
      if (dpi_scale != 0)
        dpi *= dpi_scale;
    }

  return dpi;
}

static void
notify_gtk_xft_dpi_cb (GtkSettings *settings,
                       GParamSpec  *pspec,
                       MetaUI      *ui)
{
  ui->scale = get_window_scaling_factor (ui);
  ui->dpi = get_xft_dpi (ui);

  meta_theme_set_scale (ui->theme, ui->scale);
  meta_theme_set_dpi (ui->theme, ui->dpi);
}

void
meta_ui_init (int *argc, char ***argv)
{
  /* As of 2.91.7, Gdk uses XI2 by default, which conflicts with the
   * direct X calls we use - in particular, events caused by calls to
   * XGrabPointer/XGrabKeyboard are no longer understood by GDK, while
   * GDK will no longer generate the core XEvents we process.
   * So at least for now, enforce the previous behavior.
   */
#if GTK_CHECK_VERSION(2, 91, 7)
  gdk_disable_multidevice ();
#endif

  gdk_set_allowed_backends ("x11");

  if (!gtk_init_check (argc, argv))
    {
      g_critical ("Unable to open X display %s", XDisplayName (NULL));
      exit (EXIT_FAILURE);
    }

  /* We need to be able to fully trust that the window and monitor sizes
   * that GDK reports corresponds to the X ones, so we disable the automatic
   * scale handling */
  gdk_x11_display_set_window_scale (gdk_display_get_default (), 1);
}

Display*
meta_ui_get_display (void)
{
  return GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
}

/* We do some of our event handling in frames.c, which expects
 * GDK events delivered by GTK+.  However, since the transition to
 * client side windows, we can't let GDK see button events, since the
 * client-side tracking of implicit and explicit grabs it does will
 * get confused by our direct use of X grabs in the core code.
 *
 * So we do a very minimal GDK => GTK event conversion here and send on the
 * events we care about, and then filter them out so they don't go
 * through the normal GDK event handling.
 *
 * To reduce the amount of code, the only events fields filled out
 * below are the ones that frames.c uses. If frames.c is modified to
 * use more fields, more fields need to be filled out below.
 */

static gboolean
maybe_redirect_mouse_event (XEvent *xevent)
{
  GdkDisplay *gdisplay;
  GdkSeat *seat;
  GdkDevice *gdevice;
  MetaUI *ui;
  GdkEvent *gevent;
  GdkWindow *gdk_window;
  Window window;

  switch (xevent->type)
    {
    case ButtonPress:
    case ButtonRelease:
      window = xevent->xbutton.window;
      break;
    case MotionNotify:
      window = xevent->xmotion.window;
      break;
    case EnterNotify:
    case LeaveNotify:
      window = xevent->xcrossing.window;
      break;
    default:
      return FALSE;
    }

  gdisplay = gdk_x11_lookup_xdisplay (xevent->xany.display);
  ui = g_object_get_data (G_OBJECT (gdisplay), "meta-ui");
  if (!ui)
    return FALSE;

  gdk_window = gdk_x11_window_lookup_for_display (gdisplay, window);
  if (gdk_window == NULL)
    return FALSE;

  seat = gdk_display_get_default_seat (gdisplay);
  gdevice = gdk_seat_get_pointer (seat);

  /* If GDK already thinks it has a grab, we better let it see events; this
   * is the menu-navigation case and events need to get sent to the appropriate
   * (client-side) subwindow for individual menu items.
   */
  if (gdk_display_device_is_grabbed (gdisplay, gdevice))
    return FALSE;

  switch (xevent->type)
    {
    case ButtonPress:
    case ButtonRelease:
      if (xevent->type == ButtonPress)
        {
          GtkSettings *settings = gtk_settings_get_default ();
          int double_click_time;
          int double_click_distance;

          g_object_get (settings,
                        "gtk-double-click-time", &double_click_time,
                        "gtk-double-click-distance", &double_click_distance,
                        NULL);

          if (xevent->xbutton.button == ui->button_click_number &&
              xevent->xbutton.window == ui->button_click_window &&
              xevent->xbutton.time < ui->button_click_time + double_click_time &&
              ABS (xevent->xbutton.x - ui->button_click_x) <= double_click_distance &&
              ABS (xevent->xbutton.y - ui->button_click_y) <= double_click_distance)
            {
              gevent = gdk_event_new (GDK_2BUTTON_PRESS);

              ui->button_click_number = 0;
            }
          else
            {
              gevent = gdk_event_new (GDK_BUTTON_PRESS);
              ui->button_click_number = xevent->xbutton.button;
              ui->button_click_window = xevent->xbutton.window;
              ui->button_click_time = xevent->xbutton.time;
              ui->button_click_x = xevent->xbutton.x;
              ui->button_click_y = xevent->xbutton.y;
            }
        }
      else
        {
          gevent = gdk_event_new (GDK_BUTTON_RELEASE);
        }

      gevent->button.window = g_object_ref (gdk_window);
      gevent->button.button = xevent->xbutton.button;
      gevent->button.time = xevent->xbutton.time;
      gevent->button.x = xevent->xbutton.x;
      gevent->button.y = xevent->xbutton.y;
      gevent->button.x_root = xevent->xbutton.x_root;
      gevent->button.y_root = xevent->xbutton.y_root;

      break;
    case MotionNotify:
      gevent = gdk_event_new (GDK_MOTION_NOTIFY);
      gevent->motion.type = GDK_MOTION_NOTIFY;
      gevent->motion.window = g_object_ref (gdk_window);
      break;
    case EnterNotify:
    case LeaveNotify:
      gevent = gdk_event_new (xevent->type == EnterNotify ? GDK_ENTER_NOTIFY : GDK_LEAVE_NOTIFY);
      gevent->crossing.window = g_object_ref (gdk_window);
      gevent->crossing.x = xevent->xcrossing.x;
      gevent->crossing.y = xevent->xcrossing.y;
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  /* If we've gotten here, we've filled in the gdk_event and should send it on */
  gdk_event_set_device (gevent, gdevice);
  gtk_main_do_event (gevent);
  gdk_event_free (gevent);

  return TRUE;
}

typedef struct _EventFunc EventFunc;

struct _EventFunc
{
  MetaEventFunc func;
  gpointer data;
};

static EventFunc *ef = NULL;

static GdkFilterReturn
filter_func (GdkXEvent *xevent,
             GdkEvent *event,
             gpointer data)
{
  g_return_val_if_fail (ef != NULL, GDK_FILTER_CONTINUE);

  if ((* ef->func) (xevent, ef->data) ||
      maybe_redirect_mouse_event (xevent))
    return GDK_FILTER_REMOVE;
  else
    return GDK_FILTER_CONTINUE;
}

void
meta_ui_add_event_func (Display       *xdisplay,
                        MetaEventFunc  func,
                        gpointer       data)
{
  g_return_if_fail (ef == NULL);

  ef = g_new (EventFunc, 1);
  ef->func = func;
  ef->data = data;

  gdk_window_add_filter (NULL, filter_func, ef);
}

/* removal is by data due to proxy function */
void
meta_ui_remove_event_func (Display       *xdisplay,
                           MetaEventFunc  func,
                           gpointer       data)
{
  g_return_if_fail (ef != NULL);

  gdk_window_remove_filter (NULL, filter_func, ef);

  g_free (ef);
  ef = NULL;
}

MetaUI*
meta_ui_new (Display  *xdisplay,
             gboolean  composited)
{
  GdkDisplay *gdisplay;
  MetaUI *ui;

  ui = g_new0 (MetaUI, 1);
  ui->xdisplay = xdisplay;

  ui->composited = composited;
  ui->scale = get_window_scaling_factor (ui);
  ui->dpi = get_xft_dpi (ui);

  g_signal_connect (gtk_settings_get_default (), "notify::gtk-xft-dpi",
                    G_CALLBACK (notify_gtk_xft_dpi_cb), ui);

  gdisplay = gdk_x11_lookup_xdisplay (xdisplay);
  g_assert (gdisplay == gdk_display_get_default ());
  g_assert (xdisplay == GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));

  meta_ui_reload_theme (ui);

  ui->frames = meta_frames_new (ui);

  /* GTK+ needs the frame-sync protocol to work in order to properly
   * handle style changes. This means that the dummy widget we create
   * to get the style for title bars actually needs to be mapped
   * and fully tracked as a MetaWindow. Horrible, but mostly harmless -
   * the window is a 1x1 overide redirect window positioned offscreen.
   */
  gtk_widget_show (GTK_WIDGET (ui->frames));

  g_object_set_data (G_OBJECT (gdisplay), "meta-ui", ui);

  return ui;
}

void
meta_ui_free (MetaUI *ui)
{
  GdkDisplay *gdisplay;

  gtk_widget_destroy (GTK_WIDGET (ui->frames));

  gdisplay = gdk_x11_lookup_xdisplay (ui->xdisplay);
  g_object_set_data (G_OBJECT (gdisplay), "meta-ui", NULL);

  g_free (ui);
}

void
meta_ui_set_composited (MetaUI   *ui,
                        gboolean  composited)
{
  if (ui->composited == composited)
    return;

  ui->composited = composited;

  meta_theme_set_composited (ui->theme, composited);
  meta_frames_composited_changed (ui->frames);
}

gint
meta_ui_get_scale (MetaUI *ui)
{
  return ui->scale;
}

void
meta_ui_get_frame_borders (MetaUI *ui,
                           Window frame_xwindow,
                           MetaFrameBorders *borders)
{
  meta_frames_get_borders (ui->frames, frame_xwindow, borders);
}

static void
set_background_none (Display *xdisplay,
                     Window   xwindow)
{
  XSetWindowAttributes attrs;

  attrs.background_pixmap = None;
  XChangeWindowAttributes (xdisplay, xwindow,
                           CWBackPixmap, &attrs);
}

Window
meta_ui_create_frame_window (MetaUI  *ui,
                             Display *xdisplay,
                             Visual  *xvisual,
                             gint     x,
                             gint     y,
                             gint     width,
                             gint     height,
                             gulong  *create_serial)
{
  GdkScreen *screen = gdk_screen_get_default ();
  GdkWindowAttr attrs;
  gint attributes_mask;
  GdkWindow *window;
  GdkVisual *visual;

  /* Default depth/visual handles clients with weird visuals; they can
   * always be children of the root depth/visual obviously, but
   * e.g. DRI games can't be children of a parent that has the same
   * visual as the client.
   */
  if (!xvisual)
    visual = gdk_screen_get_system_visual (screen);
  else
    {
      visual = gdk_x11_screen_lookup_visual (screen,
                                             XVisualIDFromVisual (xvisual));
    }

  attrs.title = NULL;

  /* frame.c is going to replace the event mask immediately, but
   * we still have to set it here to let GDK know what it is.
   */
  attrs.event_mask =
    GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
    GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK |
    GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_FOCUS_CHANGE_MASK;
  attrs.x = x;
  attrs.y = y;
  attrs.wclass = GDK_INPUT_OUTPUT;
  attrs.visual = visual;
  attrs.window_type = GDK_WINDOW_CHILD;
  attrs.cursor = NULL;
  attrs.wmclass_name = NULL;
  attrs.wmclass_class = NULL;
  attrs.override_redirect = FALSE;

  attrs.width  = width;
  attrs.height = height;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  /* We make an assumption that gdk_window_new() is going to call
   * XCreateWindow as it's first operation; this seems to be true currently
   * as long as you pass in a colormap.
   */
  if (create_serial)
    *create_serial = XNextRequest (xdisplay);
  window =
    gdk_window_new (gdk_screen_get_root_window(screen),
		    &attrs, attributes_mask);

  gdk_window_resize (window, width, height);
  set_background_none (xdisplay, GDK_WINDOW_XID (window));

  meta_frames_manage_window (ui->frames, GDK_WINDOW_XID (window), window);

  return GDK_WINDOW_XID (window);
}

void
meta_ui_destroy_frame_window (MetaUI *ui,
			      Window  xwindow)
{
  meta_frames_unmanage_window (ui->frames, xwindow);
}

void
meta_ui_move_resize_frame (MetaUI *ui,
			   Window frame,
			   int x,
			   int y,
			   int width,
			   int height)
{
  meta_frames_move_resize_frame (ui->frames, frame, x, y, width, height);
}

void
meta_ui_map_frame   (MetaUI *ui,
                     Window  xwindow)
{
  GdkWindow *window;
  GdkDisplay *display;

  display = gdk_x11_lookup_xdisplay (ui->xdisplay);
  window = gdk_x11_window_lookup_for_display (display, xwindow);

  if (window)
    gdk_window_show_unraised (window);
}

void
meta_ui_unmap_frame (MetaUI *ui,
                     Window  xwindow)
{
  GdkWindow *window;
  GdkDisplay *display;

  display = gdk_x11_lookup_xdisplay (ui->xdisplay);
  window = gdk_x11_window_lookup_for_display (display, xwindow);

  if (window)
    gdk_window_hide (window);
}

void
meta_ui_update_frame_style (MetaUI *ui,
                            Window  xwindow)
{
  meta_frames_update_frame_style (ui->frames, xwindow);
}

void
meta_ui_repaint_frame (MetaUI *ui,
                       Window xwindow)
{
  meta_frames_repaint_frame (ui->frames, xwindow);
}

void
meta_ui_apply_frame_shape  (MetaUI  *ui,
                            Window   xwindow,
                            int      new_window_width,
                            int      new_window_height,
                            gboolean window_has_shape)
{
  meta_frames_apply_shapes (ui->frames, xwindow,
                            new_window_width, new_window_height,
                            window_has_shape);
}

cairo_region_t *
meta_ui_get_frame_bounds (MetaUI *ui,
                          Window  xwindow,
                          int     window_width,
                          int     window_height)
{
  return meta_frames_get_frame_bounds (ui->frames,
                                       xwindow,
                                       window_width,
                                       window_height);
}

void
meta_ui_queue_frame_draw (MetaUI *ui,
                          Window xwindow)
{
  meta_frames_queue_draw (ui->frames, xwindow);
}


void
meta_ui_set_frame_title (MetaUI     *ui,
                         Window      xwindow,
                         const char *title)
{
  meta_frames_set_title (ui->frames, xwindow, title);
}

MetaWindowMenu*
meta_ui_window_menu_new  (MetaUI             *ui,
                          Window              client_xwindow,
                          MetaMenuOp          ops,
                          MetaMenuOp          insensitive,
                          unsigned long       active_workspace,
                          int                 n_workspaces,
                          MetaWindowMenuFunc  func,
                          gpointer            data)
{
  return meta_window_menu_new (ui->frames,
                               ops, insensitive,
                               client_xwindow,
                               active_workspace,
                               n_workspaces,
                               func, data);
}

void
meta_ui_window_menu_popup (MetaWindowMenu     *menu,
                           const GdkRectangle *rect,
                           guint32             timestamp)
{
  meta_window_menu_popup (menu, rect, timestamp);
}

void
meta_ui_window_menu_free (MetaWindowMenu *menu)
{
  meta_window_menu_free (menu);
}

GdkPixbuf*
meta_gdk_pixbuf_get_from_pixmap (Pixmap       xpixmap,
                                 int          src_x,
                                 int          src_y,
                                 int          width,
                                 int          height)
{
  cairo_surface_t *surface;
  Display *display;
  Window root_return;
  int x_ret, y_ret;
  unsigned int w_ret, h_ret, bw_ret, depth_ret;
  XWindowAttributes attrs;
  GdkPixbuf *retval;

  display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

  if (!XGetGeometry (display, xpixmap, &root_return,
                     &x_ret, &y_ret, &w_ret, &h_ret, &bw_ret, &depth_ret))
    return NULL;

  if (depth_ret == 1)
    {
      surface = cairo_xlib_surface_create_for_bitmap (display,
                                                      xpixmap,
                                                      GDK_SCREEN_XSCREEN (gdk_screen_get_default ()),
                                                      w_ret,
                                                      h_ret);
    }
  else
    {
      if (!XGetWindowAttributes (display, root_return, &attrs))
        return NULL;

      surface = cairo_xlib_surface_create (display,
                                           xpixmap,
                                           attrs.visual,
                                           w_ret, h_ret);
    }

  retval = gdk_pixbuf_get_from_surface (surface,
                                        src_x,
                                        src_y,
                                        width,
                                        height);
  cairo_surface_destroy (surface);

  return retval;
}

static GdkPixbuf *
load_default_window_icon (int size)
{
  GtkIconTheme *theme = gtk_icon_theme_get_default ();
  const char *icon_name;

  if (gtk_icon_theme_has_icon (theme, META_DEFAULT_ICON_NAME))
    icon_name = META_DEFAULT_ICON_NAME;
  else
    icon_name = "image-missing";

  return gtk_icon_theme_load_icon (theme, icon_name, size, 0, NULL);
}

GdkPixbuf*
meta_ui_get_default_window_icon (MetaUI *ui,
                                 int     ideal_size)
{
  static GdkPixbuf *default_icon = NULL;

  if (default_icon == NULL)
    {
      default_icon = load_default_window_icon (ideal_size);
      g_assert (default_icon);
    }

  g_object_ref (G_OBJECT (default_icon));

  return default_icon;
}

GdkPixbuf*
meta_ui_get_default_mini_icon (MetaUI *ui,
                               int     ideal_size)
{
  static GdkPixbuf *default_icon = NULL;

  if (default_icon == NULL)
    {
      default_icon = load_default_window_icon (ideal_size);
      g_assert (default_icon);
    }

  g_object_ref (G_OBJECT (default_icon));

  return default_icon;
}

gboolean
meta_ui_window_should_not_cause_focus (Display *xdisplay,
                                       Window   xwindow)
{
  GdkWindow *window;
  GdkDisplay *display;

  display = gdk_x11_lookup_xdisplay (xdisplay);
  window = gdk_x11_window_lookup_for_display (display, xwindow);

  /* we shouldn't cause focus if we're an override redirect
   * toplevel which is not foreign
   */
  if (window && gdk_window_get_window_type (window) == GDK_WINDOW_TEMP)
    return TRUE;
  else
    return FALSE;
}

void
meta_ui_theme_get_frame_borders (MetaUI           *ui,
                                 MetaFrameType     type,
                                 MetaFrameFlags    flags,
                                 MetaFrameBorders *borders)
{
  MetaTheme *theme;
  const gchar *theme_variant;

  theme = meta_ui_get_theme (ui);
  theme_variant = NULL;

  meta_theme_get_frame_borders (theme, theme_variant, type, flags, borders);
}

MetaTheme *
meta_ui_get_theme (MetaUI *ui)
{
  return ui->theme;
}

gboolean
meta_ui_is_composited (MetaUI *ui)
{
  return ui->composited;
}

static gchar *
get_theme_name (MetaThemeType theme_type)
{
  gchar *theme_name;

  if (theme_type == META_THEME_TYPE_METACITY)
    {
      theme_name = g_strdup (meta_prefs_get_theme_name ());
    }
  else
    {
      GtkSettings *settings;

      settings = gtk_settings_get_default ();

      g_object_get (settings, "gtk-theme-name", &theme_name, NULL);
    }

  return theme_name;
}

static MetaTheme *
load_theme (MetaUI        *ui,
            MetaThemeType  theme_type,
            const gchar   *theme_name)
{
  MetaTheme *theme;
  const PangoFontDescription *titlebar_font;
  GError *error;
  const gchar *button_layout;
  gboolean invert;

  theme = meta_theme_new (theme_type);

  meta_theme_set_composited (theme, ui->composited);
  meta_theme_set_scale (theme, ui->scale);
  meta_theme_set_dpi (theme, ui->dpi);

  titlebar_font = meta_prefs_get_titlebar_font ();
  meta_theme_set_titlebar_font (theme, titlebar_font);

  error = NULL;
  if (!meta_theme_load (theme, theme_name, &error))
    {
      g_warning ("%s", error->message);
      g_error_free (error);

      g_object_unref (theme);
      return NULL;
    }

  button_layout = meta_prefs_get_button_layout ();
  invert = meta_ui_get_direction() == META_UI_DIRECTION_RTL;

  meta_theme_set_button_layout (theme, button_layout, invert);

  return theme;
}

void
meta_ui_reload_theme (MetaUI *ui)
{
  MetaThemeType theme_type;
  gchar *theme_name;
  MetaTheme *theme;

  theme_type = meta_prefs_get_theme_type ();
  theme_name = get_theme_name (theme_type);

  theme = load_theme (ui, theme_type, theme_name);
  g_free (theme_name);

  if (theme == NULL)
    {
      g_warning (_("Falling back to default GTK+ theme - Adwaita"));
      theme = load_theme (ui, META_THEME_TYPE_GTK, "Adwaita");
    }

  g_assert (theme);
  g_clear_object (&ui->theme);
  ui->theme = theme;

  meta_invalidate_default_icons ();
}

void
meta_ui_update_button_layout (MetaUI *ui)
{
  const gchar *button_layout;
  gboolean invert;

  button_layout = meta_prefs_get_button_layout ();
  invert = meta_ui_get_direction() == META_UI_DIRECTION_RTL;

  meta_theme_set_button_layout (ui->theme, button_layout, invert);
}

static void
meta_ui_accelerator_parse (const char      *accel,
                           guint           *keysym,
                           guint           *keycode,
                           GdkModifierType *keymask)
{
  const char *above_tab;

  if (accel[0] == '0' && accel[1] == 'x')
    {
      *keysym = 0;
      *keycode = (guint) strtoul (accel, NULL, 16);
      *keymask = 0;

      return;
    }

  /* The key name 'Above_Tab' is special - it's not an actual keysym name,
   * but rather refers to the key above the tab key. In order to use
   * the GDK parsing for modifiers in combination with it, we substitute
   * it with 'Tab' temporarily before calling gtk_accelerator_parse().
   */
#define is_word_character(c) (g_ascii_isalnum(c) || ((c) == '_'))
#define ABOVE_TAB "Above_Tab"
#define ABOVE_TAB_LEN 9

  above_tab = strstr (accel, ABOVE_TAB);
  if (above_tab &&
      (above_tab == accel || !is_word_character (above_tab[-1])) &&
      !is_word_character (above_tab[ABOVE_TAB_LEN]))
    {
      char *before = g_strndup (accel, above_tab - accel);
      char *after = g_strdup (above_tab + ABOVE_TAB_LEN);
      char *replaced = g_strconcat (before, "Tab", after, NULL);

      gtk_accelerator_parse (replaced, NULL, keymask);

      g_free (before);
      g_free (after);
      g_free (replaced);

      *keysym = META_KEY_ABOVE_TAB;
      return;
    }

#undef is_word_character
#undef ABOVE_TAB
#undef ABOVE_TAB_LEN

  gtk_accelerator_parse (accel, keysym, keymask);
}

gboolean
meta_ui_parse_accelerator (const char          *accel,
                           unsigned int        *keysym,
                           unsigned int        *keycode,
                           MetaVirtualModifier *mask)
{
  GdkModifierType gdk_mask = 0;
  guint gdk_sym = 0;
  guint gdk_code = 0;

  *keysym = 0;
  *keycode = 0;
  *mask = 0;

  if (!accel[0] || strcmp (accel, "disabled") == 0)
    return TRUE;

  meta_ui_accelerator_parse (accel, &gdk_sym, &gdk_code, &gdk_mask);
  if (gdk_mask == 0 && gdk_sym == 0 && gdk_code == 0)
    return FALSE;

  if (gdk_sym == None && gdk_code == 0)
    return FALSE;

  if (gdk_mask & GDK_RELEASE_MASK) /* we don't allow this */
    return FALSE;

  *keysym = gdk_sym;
  *keycode = gdk_code;

  if (gdk_mask & GDK_SHIFT_MASK)
    *mask |= META_VIRTUAL_SHIFT_MASK;
  if (gdk_mask & GDK_CONTROL_MASK)
    *mask |= META_VIRTUAL_CONTROL_MASK;
  if (gdk_mask & GDK_MOD1_MASK)
    *mask |= META_VIRTUAL_ALT_MASK;
  if (gdk_mask & GDK_MOD2_MASK)
    *mask |= META_VIRTUAL_MOD2_MASK;
  if (gdk_mask & GDK_MOD3_MASK)
    *mask |= META_VIRTUAL_MOD3_MASK;
  if (gdk_mask & GDK_MOD4_MASK)
    *mask |= META_VIRTUAL_MOD4_MASK;
  if (gdk_mask & GDK_MOD5_MASK)
    *mask |= META_VIRTUAL_MOD5_MASK;
  if (gdk_mask & GDK_SUPER_MASK)
    *mask |= META_VIRTUAL_SUPER_MASK;
  if (gdk_mask & GDK_HYPER_MASK)
    *mask |= META_VIRTUAL_HYPER_MASK;
  if (gdk_mask & GDK_META_MASK)
    *mask |= META_VIRTUAL_META_MASK;

  return TRUE;
}

gboolean
meta_ui_parse_modifier (const char          *accel,
                        MetaVirtualModifier *mask)
{
  GdkModifierType gdk_mask = 0;
  guint gdk_sym = 0;
  guint gdk_code = 0;

  *mask = 0;

  if (accel == NULL || !accel[0] || strcmp (accel, "disabled") == 0)
    return TRUE;

  meta_ui_accelerator_parse (accel, &gdk_sym, &gdk_code, &gdk_mask);
  if (gdk_mask == 0 && gdk_sym == 0 && gdk_code == 0)
    return FALSE;

  if (gdk_sym != None || gdk_code != 0)
    return FALSE;

  if (gdk_mask & GDK_RELEASE_MASK) /* we don't allow this */
    return FALSE;

  if (gdk_mask & GDK_SHIFT_MASK)
    *mask |= META_VIRTUAL_SHIFT_MASK;
  if (gdk_mask & GDK_CONTROL_MASK)
    *mask |= META_VIRTUAL_CONTROL_MASK;
  if (gdk_mask & GDK_MOD1_MASK)
    *mask |= META_VIRTUAL_ALT_MASK;
  if (gdk_mask & GDK_MOD2_MASK)
    *mask |= META_VIRTUAL_MOD2_MASK;
  if (gdk_mask & GDK_MOD3_MASK)
    *mask |= META_VIRTUAL_MOD3_MASK;
  if (gdk_mask & GDK_MOD4_MASK)
    *mask |= META_VIRTUAL_MOD4_MASK;
  if (gdk_mask & GDK_MOD5_MASK)
    *mask |= META_VIRTUAL_MOD5_MASK;
  if (gdk_mask & GDK_SUPER_MASK)
    *mask |= META_VIRTUAL_SUPER_MASK;
  if (gdk_mask & GDK_HYPER_MASK)
    *mask |= META_VIRTUAL_HYPER_MASK;
  if (gdk_mask & GDK_META_MASK)
    *mask |= META_VIRTUAL_META_MASK;

  return TRUE;
}

gboolean
meta_ui_window_is_widget (MetaUI *ui,
                          Window  xwindow)
{
  GdkDisplay *display;
  GdkWindow *window;

  display = gdk_x11_lookup_xdisplay (ui->xdisplay);
  window = gdk_x11_window_lookup_for_display (display, xwindow);

  if (window)
    {
      void *user_data = NULL;
      gdk_window_get_user_data (window, &user_data);
      return user_data != NULL && user_data != ui->frames;
    }
  else
    return FALSE;
}

int
meta_ui_get_drag_threshold (MetaUI *ui)
{
  GtkSettings *settings;
  int threshold;

  settings = gtk_widget_get_settings (GTK_WIDGET (ui->frames));

  threshold = 8;
  g_object_get (G_OBJECT (settings), "gtk-dnd-drag-threshold", &threshold, NULL);

  return threshold;
}

MetaUIDirection
meta_ui_get_direction (void)
{
  if (gtk_widget_get_default_direction() == GTK_TEXT_DIR_RTL)
    return META_UI_DIRECTION_RTL;

  return META_UI_DIRECTION_LTR;
}

GdkPixbuf *
meta_ui_get_pixbuf_from_surface (cairo_surface_t *surface)
{
  gint width;
  gint height;

  width = cairo_xlib_surface_get_width (surface);
  height = cairo_xlib_surface_get_height (surface);

  return gdk_pixbuf_get_from_surface (surface, 0, 0, width, height);
}
