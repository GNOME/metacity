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
#include <libmetacity/meta-theme.h>

#include "theme-viewer-window.h"

#define PADDING 60
#define MINI_ICON_SIZE 16
#define ICON_SIZE 96

struct _ThemeViewerWindow
{
  GtkWindow         parent;

  GtkWidget        *header_bar;
  GtkWidget        *type_combo_box;
  GtkWidget        *theme_combo_box;
  GtkWidget        *reload_button;

  GtkWidget        *sidebar;

  GtkWidget        *choose_theme;
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

  MetaFrameType     frame_type;
  MetaFrameFlags    frame_flags;

  PangoLayout      *title_layout;
  gint              title_height;

  MetaFrameBorders  borders;

  MetaButtonLayout  button_layout;
  MetaButtonState   button_states[META_BUTTON_TYPE_LAST];

  gboolean          button_pressed;

  GdkPixbuf        *mini_icon;
  GdkPixbuf        *icon;
};

G_DEFINE_TYPE (ThemeViewerWindow, theme_viewer_window, GTK_TYPE_WINDOW)

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
get_icon (gint size)
{
  GtkIconTheme *theme;
  const gchar *icon;

  theme = gtk_icon_theme_get_default ();

  if (gtk_icon_theme_has_icon (theme, "start-here-symbolic"))
    icon = "start-here-symbolic";
  else
    icon = "image-missing";

  return gtk_icon_theme_load_icon (theme, icon, size, 0, NULL);;
}

static gboolean
point_in_rect (gint         x,
               gint         y,
               GdkRectangle rect)
{
  if (x >= rect.x && x < (rect.x + rect.width) &&
      y >= rect.y && y < (rect.y + rect.height))
    return TRUE;

  return FALSE;
}

static void
get_client_width_and_height (GtkWidget         *widget,
                             ThemeViewerWindow *window,
                             gint              *width,
                             gint              *height)
{
  *width = gtk_widget_get_allocated_width (widget) - PADDING * 2;
  *height = gtk_widget_get_allocated_height (widget) - PADDING * 2;

  *width -= window->borders.total.left + window->borders.total.right;
  *height -= window->borders.total.top + window->borders.total.bottom;
}

static void
update_button_state (GtkWidget         *widget,
                     GdkDevice         *device,
                     ThemeViewerWindow *window)
{
  gint x;
  gint y;
  gint width;
  gint height;
  MetaFrameGeometry fgeom;
  MetaButtonType type;
  guint i;

  gdk_window_get_device_position (gtk_widget_get_window (widget),
                                  device, &x, &y, NULL);

  get_client_width_and_height (widget, window, &width, &height);

  meta_theme_calc_geometry (window->theme, window->theme_variant,
                            window->frame_type, window->title_height,
                            window->frame_flags, width, height,
                            &window->button_layout, &fgeom);

  x -= PADDING;
  y -= PADDING;

  if (point_in_rect (x, y, fgeom.menu_rect.clickable))
    type = META_BUTTON_TYPE_MENU;

  if (point_in_rect (x, y, fgeom.appmenu_rect.clickable))
    type = META_BUTTON_TYPE_APPMENU;

  if (point_in_rect (x, y, fgeom.min_rect.clickable))
    type = META_BUTTON_TYPE_MINIMIZE;

  if (point_in_rect (x, y, fgeom.max_rect.clickable))
    type = META_BUTTON_TYPE_MAXIMIZE;

  if (point_in_rect (x, y, fgeom.close_rect.clickable))
    type = META_BUTTON_TYPE_CLOSE;

  if (point_in_rect (x, y, fgeom.shade_rect.clickable))
    type = META_BUTTON_TYPE_SHADE;

  if (point_in_rect (x, y, fgeom.unshade_rect.clickable))
    type = META_BUTTON_TYPE_UNSHADE;

  if (point_in_rect (x, y, fgeom.above_rect.clickable))
    type = META_BUTTON_TYPE_ABOVE;

  if (point_in_rect (x, y, fgeom.unabove_rect.clickable))
    type = META_BUTTON_TYPE_UNABOVE;

  if (point_in_rect (x, y, fgeom.stick_rect.clickable))
    type = META_BUTTON_TYPE_STICK;

  if (point_in_rect (x, y, fgeom.unstick_rect.clickable))
    type = META_BUTTON_TYPE_UNSTICK;

  for (i = 0; i < META_BUTTON_TYPE_LAST; i++)
    {
      if (i == type)
        {
          if (window->button_pressed)
            window->button_states[i] = META_BUTTON_STATE_PRESSED;
          else
            window->button_states[i] = META_BUTTON_STATE_PRELIGHT;
        }
      else
        window->button_states[i] = META_BUTTON_STATE_NORMAL;
    }

  gtk_widget_queue_draw (window->theme_box);
}

static void
update_button_layout (ThemeViewerWindow *window)
{
  const gchar *text;
  gint i;

  text = gtk_entry_get_text (GTK_ENTRY (window->button_layout_entry));
  window->button_layout = meta_button_layout_new (text, FALSE);

  for (i = 0; i < META_BUTTON_TYPE_LAST; i++)
    window->button_states[i] = META_BUTTON_STATE_NORMAL;
}

static void
update_title_layout (ThemeViewerWindow *window)
{
  GtkWidget *widget;
  PangoLayout *layout;
  PangoFontDescription *font_desc;
  MetaFrameType type;
  MetaFrameFlags flags;
  MetaFrameStyle *style;
  PangoContext *context;
  gint height;

  widget = GTK_WIDGET (window);

  layout = gtk_widget_create_pango_layout (widget, "Metacity Theme Viewer");
  font_desc = meta_theme_create_font_desc (window->theme, window->theme_variant);

  type = window->frame_type;
  flags = window->frame_flags;

  style = meta_theme_get_frame_style (window->theme, type, flags);
  meta_frame_style_apply_scale (style, font_desc);

  context = gtk_widget_get_pango_context (widget);
  height = meta_pango_font_desc_get_text_height (font_desc, context);

  pango_layout_set_auto_dir (layout, FALSE);
  pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);
  pango_layout_set_font_description (layout, font_desc);
  pango_layout_set_single_paragraph_mode (layout, TRUE);

  if (window->title_layout)
    g_object_unref (window->title_layout);

  window->title_layout = layout;
  window->title_height = height;

  pango_font_description_free (font_desc);
}

static void
update_frame_borders (ThemeViewerWindow *window)
{
  meta_theme_get_frame_borders (window->theme, window->theme_variant,
                                window->frame_type, window->title_height,
                                window->frame_flags, &window->borders);
}

static void
update_frame_flags (ThemeViewerWindow *window)
{
  MetaFrameFlags flags;
  GtkToggleButton *button;

  flags = META_FRAME_ALLOWS_DELETE | META_FRAME_ALLOWS_MENU |
          META_FRAME_ALLOWS_APPMENU | META_FRAME_ALLOWS_MINIMIZE |
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

  update_title_layout (window);
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
  gtk_widget_hide (window->theme_box);

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
  g_clear_object (&window->title_layout);
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
  GSList *theme;

  theme_combo_box_text = GTK_COMBO_BOX_TEXT (window->theme_combo_box);

  gtk_combo_box_text_remove_all (theme_combo_box_text);
  gtk_widget_set_sensitive (window->reload_button, FALSE);

  clear_theme (window);

  type = gtk_combo_box_get_active (combo_box);
  themes = NULL;

  themes_dir = g_build_filename (DATADIR, "themes", NULL);
  get_valid_themes (window, themes_dir, type, &themes);
  g_free (themes_dir);

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

  theme = gtk_combo_box_get_active_id (combo_box);

  gtk_widget_set_sensitive (window->reload_button, FALSE);

  if (theme == NULL)
    {
      clear_theme (window);

      return;
    }

  type = gtk_combo_box_get_active (GTK_COMBO_BOX (window->type_combo_box));

  window->theme = meta_theme_new (type);

  meta_theme_load (window->theme, theme, NULL);
  meta_theme_set_composited (window->theme, window->composited);

  update_frame_flags (window);
  update_button_layout (window);

  gtk_widget_hide (window->choose_theme);
  gtk_widget_show (window->theme_box);

  gtk_widget_set_sensitive (window->sidebar, TRUE);

  if (type == META_THEME_TYPE_METACITY)
     gtk_widget_set_sensitive (window->reload_button, TRUE);
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

  update_frame_borders (window);

  gtk_widget_queue_draw (window->theme_box);
}

static gboolean
theme_box_draw_cb (GtkWidget         *widget,
                   cairo_t           *cr,
                   ThemeViewerWindow *window)
{
  gint client_width;
  gint client_height;

  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
  cairo_paint (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  theme_box_draw_grid (widget, cr);

  get_client_width_and_height (widget, window, &client_width, &client_height);

  if (!window->mini_icon)
    window->mini_icon = get_icon (MINI_ICON_SIZE);

  if (!window->mini_icon)
    window->icon = get_icon (ICON_SIZE);

  cairo_translate (cr, PADDING, PADDING);
  meta_theme_draw_frame (window->theme, window->theme_variant, cr,
                         window->frame_type, window->frame_flags,
                         client_width, client_height,
                         window->title_layout, window->title_height,
                         &window->button_layout, window->button_states,
                         window->mini_icon, window->icon);

  return TRUE;
}

static gboolean
theme_box_button_press_event_cb (GtkWidget         *widget,
                                 GdkEventButton    *event,
                                 ThemeViewerWindow *window)
{
  window->button_pressed = TRUE;

  update_button_state (widget, event->device, window);

  return TRUE;
}

static gboolean
theme_box_button_release_event_cb (GtkWidget         *widget,
                                   GdkEventButton    *event,
                                   ThemeViewerWindow *window)
{
  window->button_pressed = FALSE;

  update_button_state (widget, event->device, window);

  return TRUE;
}

static gboolean
theme_box_motion_notify_event_cb (GtkWidget         *widget,
                                  GdkEventMotion    *event,
                                  ThemeViewerWindow *window)
{
  update_button_state (widget, event->device, window);

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

  gtk_widget_queue_draw (window->theme_box);
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

  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, header_bar);

  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, type_combo_box);
  gtk_widget_class_bind_template_callback (widget_class, type_combo_box_changed_cb);

  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, theme_combo_box);
  gtk_widget_class_bind_template_callback (widget_class, theme_combo_box_changed_cb);

  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, reload_button);
  gtk_widget_class_bind_template_callback (widget_class, reload_button_clicked_cb);

  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, sidebar);

  gtk_widget_class_bind_template_child (widget_class, ThemeViewerWindow, choose_theme);

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
}

static void
theme_viewer_window_init (ThemeViewerWindow *window)
{
  window->composited = TRUE;

  gtk_widget_init_template (GTK_WIDGET (window));
  gtk_window_set_titlebar (GTK_WINDOW (window), window->header_bar);

  gtk_widget_add_events (window->theme_box, GDK_POINTER_MOTION_MASK);

  type_combo_box_changed_cb (GTK_COMBO_BOX (window->type_combo_box), window);
}

GtkWidget *
theme_viewer_window_new (void)
{
  return g_object_new (THEME_VIEWER_TYPE_WINDOW,
                       "title", _("Metacity Theme Viewer"),
                       NULL);
}