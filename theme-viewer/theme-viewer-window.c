/*
 * Copyright (C) 2016 Alberts Muktupāvels
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

#include <glib/gi18n.h>
#include <time.h>

#include "theme-viewer-window.h"

#define BENCHMARK_ITERATIONS 100

#define PADDING 60
#define MINI_ICON_SIZE 16
#define ICON_SIZE 96

struct _ThemeViewerWindow
{
  GtkWindow         parent;

  GtkWidget        *type_combo_box;
  GtkWidget        *theme_combo_box;
  GtkWidget        *reload_button;

  GtkWidget        *sidebar;

  GtkWidget        *choose_theme;
  GtkWidget        *notebook;
  GtkWidget        *theme_box;

  GtkWidget        *has_focus;
  GtkWidget        *shaded;
  GtkWidget        *maximized;
  GtkWidget        *fullscreen;
  GtkWidget        *tiled;

  GtkWidget        *button_layout_entry;

  MetaTheme        *theme;

  const gchar      *theme_variant;
  gboolean          composited;

  GtkWidget        *scale_button;
  gint              scale;

  MetaFrameType     frame_type;
  MetaFrameFlags    frame_flags;

  MetaFrameBorders  borders;

  gboolean          button_pressed;

  GdkPixbuf        *mini_icon;
  GdkPixbuf        *icon;

  GtkWidget        *benchmark_frame;
  GtkWidget        *load_time;
  GtkWidget        *get_borders_time;
  GtkWidget        *draw_time;

  GtkWidget        *benchmark_button;
};

G_DEFINE_TYPE (ThemeViewerWindow, theme_viewer_window, GTK_TYPE_WINDOW)

static void
benchmark_load_time (ThemeViewerWindow *window,
                     MetaTheme         *theme,
                     MetaThemeType      type,
                     const gchar       *name)
{
  clock_t start;
  clock_t end;
  clock_t elapsed;
  gdouble seconds;
  const gchar *type_string;
  gchar *message;

  start = clock ();
  meta_theme_load (theme, name, NULL);
  end = clock ();

  elapsed = end - start;
  seconds = (gdouble) elapsed / CLOCKS_PER_SEC;

  type_string = type == META_THEME_TYPE_GTK ? "GTK+" : "Metacity";
  message = g_strdup_printf (_("Loaded <b>%s</b> theme <b>%s</b> in <b>%f</b> seconds."),
                             type_string, name, seconds);

  gtk_label_set_markup (GTK_LABEL (window->load_time), message);
  gtk_widget_show (window->load_time);
  g_free (message);
}

static void
benchmark_get_borders (ThemeViewerWindow *window,
                       MetaTheme         *theme,
                       MetaFrameBorders  *borders)
{
  clock_t start;
  clock_t end;
  clock_t elapsed;
  gdouble seconds;
  gchar *message;

  start = clock ();

  meta_theme_get_frame_borders (theme, window->theme_variant,
                                window->frame_type, window->frame_flags,
                                borders);

  end = clock ();

  elapsed = end - start;
  seconds = (gdouble) elapsed / CLOCKS_PER_SEC;

  message = g_strdup_printf (_("Got MetaFrameBorders in <b>%f</b> seconds (CSS loading, PangoFontDescription creation and title height calculation)."),
                             seconds);

  gtk_label_set_markup (GTK_LABEL (window->get_borders_time), message);
  gtk_widget_show (window->get_borders_time);
  g_free (message);
}

static void
benchmark_draw_time (ThemeViewerWindow *window,
                     MetaTheme         *theme,
                     MetaFrameBorders  *borders)
{
  GTimer *timer;
  clock_t start;
  clock_t end;
  clock_t elapsed;
  gdouble seconds;
  gdouble wall_seconds;
  gchar *message;

  timer = g_timer_new ();
  start = clock ();

  {
    GdkWindow *gdk_window;
    gint client_width;
    gint client_height;
    gint inc;
    gint i;

    gdk_window = gtk_widget_get_window (GTK_WIDGET (window));
    client_width = 200;
    client_height = 120;
    inc = 1000 / BENCHMARK_ITERATIONS;
    i = 0;

    while (i < BENCHMARK_ITERATIONS)
      {
        gint width;
        gint height;
        cairo_surface_t *surface;
        cairo_t *cr;

        width = client_width + borders->total.left + borders->total.right;
        height = client_height + borders->total.top + borders->total.bottom;
        surface = gdk_window_create_similar_surface (gdk_window,
                                                     CAIRO_CONTENT_COLOR,
                                                     width, height);

        cr = cairo_create (surface);

        meta_theme_draw_frame (theme, window->theme_variant, cr,
                               window->frame_type, window->frame_flags,
                               width, height, "Benchmark",
                               NULL, NULL, window->mini_icon, window->icon);

        cairo_destroy (cr);
        cairo_surface_destroy (surface);

        client_width += inc;
        client_height += inc;
        ++i;
      }
  }

  end = clock ();
  g_timer_stop (timer);

  g_object_unref (theme);

  elapsed = end - start;
  seconds = (gdouble) elapsed / CLOCKS_PER_SEC;
  wall_seconds = g_timer_elapsed (timer, NULL);

  message = g_strdup_printf (_("Drew <b>%d</b> frames in <b>%f</b> client-side seconds (<b>%f</b> milliseconds per frame) and <b>%f</b> seconds wall clock time including X server resources (<b>%f</b> milliseconds per frame)."),
                             BENCHMARK_ITERATIONS,
                             seconds, (seconds / BENCHMARK_ITERATIONS) * 1000,
                             wall_seconds, (wall_seconds / BENCHMARK_ITERATIONS) * 1000);

  g_timer_destroy (timer);

  gtk_label_set_markup (GTK_LABEL (window->draw_time), message);
  gtk_widget_show (window->draw_time);
  g_free (message);
}

static void
run_benchmark (ThemeViewerWindow *window)
{
  MetaThemeType theme_type;
  const gchar *theme_name;
  MetaTheme *theme;
  MetaFrameBorders borders;

  gtk_widget_set_sensitive (window->benchmark_button, FALSE);
  gtk_widget_show (window->benchmark_frame);

  theme_type = gtk_combo_box_get_active (GTK_COMBO_BOX (window->type_combo_box));
  theme_name = gtk_combo_box_get_active_id (GTK_COMBO_BOX (window->theme_combo_box));

  theme = meta_theme_new (theme_type);

  /* 1. benchmark load time */
  benchmark_load_time (window, theme, theme_type, theme_name);

  /* 2. benchmark get borders */
  benchmark_get_borders (window, theme, &borders);

  /* 3. benchmark draw time */
  benchmark_draw_time (window, theme, &borders);

  gtk_button_set_label (GTK_BUTTON (window->benchmark_button), _("Run again"));
  gtk_widget_set_sensitive (window->benchmark_button, TRUE);
}

static void
reset_benchmark_page (ThemeViewerWindow *window)
{
  gtk_widget_hide (window->benchmark_frame);
  gtk_button_set_label (GTK_BUTTON (window->benchmark_button), _("Run"));
}

static void
update_frame_flags_sensitivity (ThemeViewerWindow *window)
{
  gtk_widget_set_sensitive (window->has_focus, TRUE);
  gtk_widget_set_sensitive (window->shaded, TRUE);
  gtk_widget_set_sensitive (window->maximized, TRUE);
  gtk_widget_set_sensitive (window->fullscreen, TRUE);
  gtk_widget_set_sensitive (window->tiled, TRUE);

  if (window->frame_flags & META_FRAME_SHADED)
    gtk_widget_set_sensitive (window->fullscreen, FALSE);

  if (window->frame_flags & META_FRAME_MAXIMIZED)
    {
      gtk_widget_set_sensitive (window->fullscreen, FALSE);
      gtk_widget_set_sensitive (window->tiled, FALSE);
    }

  if (window->frame_flags & META_FRAME_FULLSCREEN)
    {
      gtk_widget_set_sensitive (window->shaded, FALSE);
      gtk_widget_set_sensitive (window->maximized, FALSE);
      gtk_widget_set_sensitive (window->tiled, FALSE);
    }

  if (window->frame_flags & META_FRAME_TILED_LEFT)
    {
      gtk_widget_set_sensitive (window->maximized, FALSE);
      gtk_widget_set_sensitive (window->fullscreen, FALSE);
    }
}

static GdkPixbuf *
get_icon (ThemeViewerWindow *window,
          gint               size)
{
  GtkIconTheme *theme;
  const gchar *icon;

  theme = gtk_icon_theme_get_default ();

  if (gtk_icon_theme_has_icon (theme, "start-here-symbolic"))
    icon = "start-here-symbolic";
  else
    icon = "image-missing";

  return gtk_icon_theme_load_icon_for_scale (theme, icon, size,
                                             window->scale,
                                             0, NULL);
}

static void
get_client_width_and_height (GtkWidget         *widget,
                             ThemeViewerWindow *window,
                             gint               widget_scale,
                             gint              *width,
                             gint              *height)
{
  *width = gtk_widget_get_allocated_width (widget) - PADDING * 2;
  *height = gtk_widget_get_allocated_height (widget) - PADDING * 2;

  *width /= 1.0 / widget_scale;
  *height /= 1.0 / widget_scale;

  *width -= window->borders.total.left + window->borders.total.right;
  *height -= window->borders.total.top + window->borders.total.bottom;
}

static MetaButtonState
update_button_state (MetaButtonType type,
                     GdkRectangle   rect,
                     gpointer       user_data)
{
  ThemeViewerWindow *window;
  MetaButtonState state;
  GdkDisplay *display;
  GdkSeat *seat;
  GdkDevice *device;
  gint x;
  gint y;
  gint widget_scale;

  window = THEME_VIEWER_WINDOW (user_data);
  state = META_BUTTON_STATE_NORMAL;

  display = gdk_display_get_default ();
  seat = gdk_display_get_default_seat (display);
  device = gdk_seat_get_pointer (seat);

  gdk_window_get_device_position (gtk_widget_get_window (window->theme_box),
                                  device, &x, &y, NULL);

  x -= PADDING;
  y -= PADDING;

  widget_scale = gtk_widget_get_scale_factor (GTK_WIDGET (window));
  x /= 1.0 / widget_scale;
  y /= 1.0 / widget_scale;

  if (x >= rect.x && x < (rect.x + rect.width) &&
      y >= rect.y && y < (rect.y + rect.height))
    {
      if (window->button_pressed)
        state = META_BUTTON_STATE_PRESSED;
      else
        state = META_BUTTON_STATE_PRELIGHT;
    }

  return state;
}

static void
update_button_layout (ThemeViewerWindow *window)
{
  const gchar *text;

  if (!window->theme)
    return;

  text = gtk_entry_get_text (GTK_ENTRY (window->button_layout_entry));

  meta_theme_set_button_layout (window->theme, text, FALSE);
}

static void
update_frame_borders (ThemeViewerWindow *window)
{
  meta_theme_get_frame_borders (window->theme, window->theme_variant,
                                window->frame_type, window->frame_flags,
                                &window->borders);
}

static void
update_frame_flags (ThemeViewerWindow *window)
{
  MetaFrameFlags flags;
  GtkToggleButton *button;

  flags = META_FRAME_ALLOWS_DELETE | META_FRAME_ALLOWS_MENU |
          META_FRAME_ALLOWS_MINIMIZE |
          META_FRAME_ALLOWS_MAXIMIZE | META_FRAME_ALLOWS_VERTICAL_RESIZE |
          META_FRAME_ALLOWS_HORIZONTAL_RESIZE | META_FRAME_ALLOWS_SHADE |
          META_FRAME_ALLOWS_MOVE;

  button = GTK_TOGGLE_BUTTON (window->has_focus);
  if (gtk_toggle_button_get_active (button))
    flags |= META_FRAME_HAS_FOCUS;

  button = GTK_TOGGLE_BUTTON (window->shaded);
  if (gtk_toggle_button_get_active (button))
    flags |= META_FRAME_SHADED;

  button = GTK_TOGGLE_BUTTON (window->maximized);
  if (gtk_toggle_button_get_active (button))
    flags |= META_FRAME_MAXIMIZED;

  button = GTK_TOGGLE_BUTTON (window->fullscreen);
  if (gtk_toggle_button_get_active (button))
    flags |= META_FRAME_FULLSCREEN;

  button = GTK_TOGGLE_BUTTON (window->tiled);
  if (gtk_toggle_button_get_active (button))
    flags |= META_FRAME_TILED_LEFT;

  window->frame_flags = flags;

  update_frame_borders (window);
  update_frame_flags_sensitivity (window);
}

static void
theme_box_draw_grid (GtkWidget *widget,
                     cairo_t   *cr)
{
  PangoContext *context;
  PangoLayout *layout;
  gint width;
  gint height;
  GdkRectangle clip;
  gint x;
  gint y;

  context = gtk_widget_get_pango_context (widget);
  layout = pango_layout_new (context);

  pango_layout_set_text (layout, "X", 1);

  pango_layout_get_pixel_size (layout, &width, &height);
  g_object_unref (layout);

  height /= 2;

  cairo_save (cr);

  cairo_set_line_width (cr, 1.0);
  cairo_set_source_rgba (cr, 0.8, 0.8, 0.8, 0.2);

  gdk_cairo_get_clip_rectangle (cr, &clip);

  for (x = 0; x <= clip.x + clip.width; x += width)
    {
      cairo_move_to (cr, x + .5, clip.y - .5);
      cairo_line_to (cr, x + .5, clip.y + clip.height - .5);
    }

  for (y = 0; y <= clip.y + clip.height; y += height)
    {
      cairo_move_to (cr, clip.x + .5, y - .5);
      cairo_line_to (cr, clip.x + clip.width + .5, y - .5);
    }

  cairo_stroke (cr);

  cairo_restore (cr);
}

static void
clear_theme (ThemeViewerWindow *window)
{
  gtk_widget_show (window->choose_theme);
  gtk_widget_hide (window->notebook);

  gtk_widget_set_sensitive (window->sidebar, FALSE);

  g_clear_object (&window->theme);
}

static gboolean
is_valid_theme (const gchar   *themes_dir,
                const gchar   *theme,
                MetaThemeType  type)
{
  gchar *path;
  GDir *dir;
  const gchar *name;

  path = g_build_filename (themes_dir, theme, NULL);
  dir = g_dir_open (path, 0, NULL);
  g_free (path);

  if (dir == NULL)
    return FALSE;

  while ((name = g_dir_read_name (dir)) != NULL)
    {
      gchar *filename;

      if (type == META_THEME_TYPE_METACITY &&
          g_strcmp0 (name, "metacity-1") == 0)
        {
          gint i;

          for (i = 1; i <=3; i++)
            {
              gchar *theme_format;

              theme_format = g_strdup_printf ("metacity-theme-%d.xml", i);
              filename = g_build_filename (themes_dir, theme, name,
                                           theme_format, NULL);

              if (g_file_test (filename, G_FILE_TEST_IS_REGULAR))
                {
                  g_free (filename);
                  g_free (theme_format);
                  g_dir_close (dir);

                  return TRUE;
                }

              g_free (filename);
              g_free (theme_format);
            }
        }
      else if (type == META_THEME_TYPE_GTK &&
               g_strcmp0 (name, "gtk-3.0") == 0)
        {
          filename = g_build_filename (themes_dir, theme, name,
                                       "gtk.css", NULL);

          if (g_file_test (filename, G_FILE_TEST_IS_REGULAR))
            {
              g_free (filename);
              g_dir_close (dir);

              return TRUE;
            }

          g_free (filename);
        }
    }

  g_dir_close (dir);

  return FALSE;
}

static void
get_valid_themes (ThemeViewerWindow  *window,
                  const gchar        *themes_dir,
                  MetaThemeType       type,
                  GSList            **themes)
{
  GDir *dir;
  const gchar *theme;

  dir = g_dir_open (themes_dir, 0, NULL);

  if (dir == NULL)
    return;

  while ((theme = g_dir_read_name (dir)) != NULL)
    {
      if (!is_valid_theme (themes_dir, theme, type))
        continue;

      if (g_slist_find_custom (*themes, theme, (GCompareFunc) g_strcmp0))
        continue;

      *themes = g_slist_prepend (*themes, g_strdup (theme));
    }

  g_dir_close (dir);
}

static void
theme_viewer_window_dispose (GObject *object)
{
  ThemeViewerWindow *window;

  window = THEME_VIEWER_WINDOW (object);

  g_clear_object (&window->theme);
  g_clear_object (&window->mini_icon);
  g_clear_object (&window->icon);

  G_OBJECT_CLASS (theme_viewer_window_parent_class)->dispose (object);
}

static void
type_combo_box_changed_cb (GtkComboBox       *combo_box,
                           ThemeViewerWindow *window)
{
  GtkComboBoxText *theme_combo_box_text;
  MetaThemeType type;
  GSList *themes;
  gchar *themes_dir;
  const gchar *const *xdg_data_dirs;
  gint i;
  GSList *theme;

  theme_combo_box_text = GTK_COMBO_BOX_TEXT (window->theme_combo_box);

  gtk_combo_box_text_remove_all (theme_combo_box_text);
  gtk_widget_set_sensitive (window->reload_button, FALSE);

  clear_theme (window);
  reset_benchmark_page (window);

  type = gtk_combo_box_get_active (combo_box);
  themes = NULL;

  themes_dir = g_build_filename (DATADIR, "themes", NULL);
  get_valid_themes (window, themes_dir, type, &themes);
  g_free (themes_dir);

  xdg_data_dirs = g_get_system_data_dirs ();
  for (i = 0; xdg_data_dirs[i] != NULL; i++)
    {
      themes_dir = g_build_filename (xdg_data_dirs[i], "themes", NULL);
      get_valid_themes (window, themes_dir, type, &themes);
      g_free (themes_dir);
    }

  themes_dir = g_build_filename (g_get_user_data_dir (), "themes", NULL);
  get_valid_themes (window, themes_dir, type, &themes);
  g_free (themes_dir);

  themes_dir = g_build_filename (g_get_home_dir (), ".themes", NULL);
  get_valid_themes (window, themes_dir, type, &themes);
  g_free (themes_dir);

  gtk_combo_box_text_insert (theme_combo_box_text, 0, NULL, _("Choose Theme"));
  gtk_combo_box_set_active (GTK_COMBO_BOX (window->theme_combo_box), 0);

  themes = g_slist_sort (themes, (GCompareFunc) g_strcmp0);
  for (theme = themes; theme != NULL; theme = g_slist_next (theme))
    {
      const gchar *name;

      name = (const gchar *) theme->data;

      gtk_combo_box_text_append (theme_combo_box_text, name, name);
    }

  g_slist_free_full (themes, g_free);
}

static void
theme_combo_box_changed_cb (GtkComboBox       *combo_box,
                            ThemeViewerWindow *window)
{
  const gchar *theme;
  MetaThemeType type;
  gboolean sensitive;

  theme = gtk_combo_box_get_active_id (combo_box);

  gtk_widget_set_sensitive (window->reload_button, FALSE);

  if (theme == NULL)
    {
      clear_theme (window);

      return;
    }

  reset_benchmark_page (window);

  type = gtk_combo_box_get_active (GTK_COMBO_BOX (window->type_combo_box));

  window->theme = meta_theme_new (type);

  meta_theme_load (window->theme, theme, NULL);
  meta_theme_set_composited (window->theme, window->composited);
  meta_theme_set_scale (window->theme, window->scale);

  update_frame_flags (window);
  update_button_layout (window);

  gtk_widget_hide (window->choose_theme);
  gtk_widget_show (window->notebook);

  sensitive = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook)) == 0;

  gtk_widget_set_sensitive (window->sidebar, TRUE);
  gtk_widget_set_sensitive (window->reload_button, sensitive);

  gtk_widget_queue_draw (window->theme_box);
}

static void
reload_button_clicked_cb (GtkButton         *button,
                          ThemeViewerWindow *window)
{
  GtkComboBox *combo_box;
  const gchar *theme;

  combo_box = GTK_COMBO_BOX (window->theme_combo_box);
  theme = gtk_combo_box_get_active_id (combo_box);

  meta_theme_load (window->theme, theme, NULL);
  meta_theme_invalidate (window->theme);

  update_frame_borders (window);

  gtk_widget_queue_draw (window->theme_box);
}

static gboolean
theme_box_draw_cb (GtkWidget         *widget,
                   cairo_t           *cr,
                   ThemeViewerWindow *window)
{
  gint widget_scale;
  gint client_width;
  gint client_height;

  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
  cairo_paint (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  theme_box_draw_grid (widget, cr);

  widget_scale = gtk_widget_get_scale_factor (widget);
  get_client_width_and_height (widget, window, widget_scale,
                               &client_width, &client_height);

  if (!window->mini_icon)
    window->mini_icon = get_icon (window, MINI_ICON_SIZE);

  if (!window->icon)
    window->icon = get_icon (window, ICON_SIZE);

  cairo_translate (cr, PADDING, PADDING);

  cairo_save (cr);
  cairo_scale (cr, 1.0 / widget_scale, 1.0 / widget_scale);

  meta_theme_draw_frame (window->theme, window->theme_variant, cr,
                         window->frame_type, window->frame_flags,
                         client_width, client_height, "Metacity Theme Viewer",
                         update_button_state, window,
                         window->mini_icon, window->icon);

  cairo_restore (cr);

  return TRUE;
}

static gboolean
theme_box_button_press_event_cb (GtkWidget         *widget,
                                 GdkEventButton    *event,
                                 ThemeViewerWindow *window)
{
  window->button_pressed = TRUE;

  gtk_widget_queue_draw (window->theme_box);

  return TRUE;
}

static gboolean
theme_box_button_release_event_cb (GtkWidget         *widget,
                                   GdkEventButton    *event,
                                   ThemeViewerWindow *window)
{
  window->button_pressed = FALSE;

  gtk_widget_queue_draw (window->theme_box);

  return TRUE;
}

static gboolean
theme_box_motion_notify_event_cb (GtkWidget         *widget,
                                  GdkEventMotion    *event,
                                  ThemeViewerWindow *window)
{
  gtk_widget_queue_draw (window->theme_box);

  return TRUE;
}

static void
flags_toggled_cb (GtkToggleButton   *togglebutton,
                  ThemeViewerWindow *window)
{
  update_frame_flags (window);

  gtk_widget_queue_draw (window->theme_box);
}

static void
button_layout_entry_changed_cb (GtkEditable       *editable,
                                ThemeViewerWindow *window)
{
  update_button_layout (window);

  gtk_widget_queue_draw (window->theme_box);
}

static void
dark_theme_state_set_cb (GtkSwitch         *widget,
                         gboolean           state,
                         ThemeViewerWindow *window)
{
  gboolean active;

  active = gtk_switch_get_active (GTK_SWITCH (widget));
  window->theme_variant = active ? "dark" : NULL;

  update_frame_borders (window);

  gtk_widget_queue_draw (window->theme_box);
}

static void
frame_type_combo_box_changed_cb (GtkComboBox       *combo_box,
                                 ThemeViewerWindow *window)
{
  window->frame_type = gtk_combo_box_get_active (combo_box);

  update_frame_flags (window);

  gtk_widget_queue_draw (window->theme_box);
}

static void
composited_state_set_cb (GtkSwitch         *widget,
                         gboolean           state,
                         ThemeViewerWindow *window)
{
  gboolean active;

  active = gtk_switch_get_active (GTK_SWITCH (widget));
  window->composited = active;

  if (!window->theme)
    return;

  meta_theme_set_composited (window->theme, active);

  update_frame_borders (window);

  gtk_widget_queue_draw (window->theme_box);
}

static void
scale_changed_cb (GtkSpinButton     *spin_button,
                  ThemeViewerWindow *window)
{
  gint scale;

  scale = (gint) gtk_spin_button_get_value (spin_button);

  if (window->scale == scale)
    return;

  window->scale = scale;

  meta_theme_set_scale (window->theme, scale);

  g_clear_object (&window->mini_icon);
  g_clear_object (&window->icon);

  update_frame_borders (window);

  gtk_widget_queue_draw (window->theme_box);
}

static void
notebook_switch_page_cb (GtkNotebook       *notebook,
                         GtkWidget         *page,
                         guint              page_num,
                         ThemeViewerWindow *window)
{
  gboolean sensitive;

  sensitive = page_num == 0 && window->theme != NULL;

  gtk_widget_set_sensitive (window->reload_button, sensitive);
}

static void
benchmark_button_clicked_cb (GtkButton         *button,
                             ThemeViewerWindow *window)
{
  run_benchmark (window);
}

static void
theme_viewer_window_class_init (ThemeViewerWindowClass *window_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  const gchar *resource;

  object_class = G_OBJECT_CLASS (window_class);
  widget_class = GTK_WIDGET_CLASS (window_class);

  object_class->dispose = theme_viewer_window_dispose;

  resource = "/org/gnome/metacity/ui/theme-viewer-window.ui";
  gtk_widget_class_set_template_from_resource (widget_class, resource);

  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, type_combo_box);
  gtk_widget_class_bind_template_callback (widget_class, type_combo_box_changed_cb);

  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, theme_combo_box);
  gtk_widget_class_bind_template_callback (widget_class, theme_combo_box_changed_cb);

  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, reload_button);
  gtk_widget_class_bind_template_callback (widget_class, reload_button_clicked_cb);

  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, sidebar);

  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, choose_theme);
  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, notebook);

  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, theme_box);
  gtk_widget_class_bind_template_callback (widget_class, theme_box_draw_cb);
  gtk_widget_class_bind_template_callback (widget_class, theme_box_button_press_event_cb);
  gtk_widget_class_bind_template_callback (widget_class, theme_box_button_release_event_cb);
  gtk_widget_class_bind_template_callback (widget_class, theme_box_motion_notify_event_cb);

  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, has_focus);
  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, shaded);
  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, maximized);
  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, fullscreen);
  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, tiled);
  gtk_widget_class_bind_template_callback (widget_class, flags_toggled_cb);

  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, button_layout_entry);
  gtk_widget_class_bind_template_callback (widget_class, button_layout_entry_changed_cb);

  gtk_widget_class_bind_template_callback (widget_class, dark_theme_state_set_cb);
  gtk_widget_class_bind_template_callback (widget_class, frame_type_combo_box_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, composited_state_set_cb);

  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, scale_button);
  gtk_widget_class_bind_template_callback (widget_class, scale_changed_cb);

  gtk_widget_class_bind_template_callback (widget_class, notebook_switch_page_cb);

  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, benchmark_frame);
  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, load_time);
  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, get_borders_time);
  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, draw_time);

  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, benchmark_button);
  gtk_widget_class_bind_template_callback (widget_class, benchmark_button_clicked_cb);
}

static void
theme_viewer_window_init (ThemeViewerWindow *window)
{
  window->composited = TRUE;
  window->scale = gtk_widget_get_scale_factor (GTK_WIDGET (window));

  gtk_widget_init_template (GTK_WIDGET (window));

  gtk_widget_add_events (window->theme_box, GDK_POINTER_MOTION_MASK);

  gtk_spin_button_set_value (GTK_SPIN_BUTTON (window->scale_button), window->scale);

  type_combo_box_changed_cb (GTK_COMBO_BOX (window->type_combo_box), window);

  gtk_label_set_xalign (GTK_LABEL (window->load_time), 0.0);
  gtk_label_set_xalign (GTK_LABEL (window->get_borders_time), 0.0);
  gtk_label_set_xalign (GTK_LABEL (window->draw_time), 0.0);
}

GtkWidget *
theme_viewer_window_new (void)
{
  return g_object_new (THEME_VIEWER_TYPE_WINDOW,
                       "title", _("Metacity Theme Viewer"),
                       NULL);
}

void
theme_viewer_window_set_theme_type (ThemeViewerWindow *window,
                                    MetaThemeType      theme_type)
{
  gtk_combo_box_set_active (GTK_COMBO_BOX (window->type_combo_box), theme_type);
}

void
theme_viewer_window_set_theme_name (ThemeViewerWindow *window,
                                    const gchar       *theme_name)
{
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (window->theme_combo_box),
                               theme_name);
}
