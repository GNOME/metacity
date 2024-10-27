/*
 * Copyright (C) 2021 Alberts Muktupāvels
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

#define _GNU_SOURCE

#include "config.h"
#include "meta-request-csd.h"

#include <dlfcn.h>

typedef GType (* RegisterStaticSimple)   (GType              parent_type,
                                          const gchar       *type_name,
                                          guint              class_size,
                                          GClassInitFunc     class_init,
                                          guint              instance_size,
                                          GInstanceInitFunc  instance_init,
                                          GTypeFlags         flags);

typedef gint  (* AddInstancePrivateFunc) (GType              class_type,
                                          gsize              private_size);

typedef struct _GtkMnemnonicHash GtkMnemonicHash;
typedef struct _GtkCssNode GtkCssNode;

struct _GtkWindowPrivate
{
  GtkMnemonicHash       *mnemonic_hash;

  GtkWidget             *attach_widget;
  GtkWidget             *default_widget;
  GtkWidget             *initial_focus;
  GtkWidget             *focus_widget;
  GtkWindow             *transient_parent;
  GtkWindowGeometryInfo *geometry_info;
  GtkWindowGroup        *group;
  GdkScreen             *screen;
  GdkDisplay            *display;
  GtkApplication        *application;

  GList                 *popovers;

  GdkModifierType        mnemonic_modifier;

  gchar                 *startup_id;
  gchar                 *title;
  gchar                 *wmclass_class;
  gchar                 *wmclass_name;
  gchar                 *wm_role;

  guint                  keys_changed_handler;
  guint                  delete_event_handler;

  guint32                initial_timestamp;

  guint16                configure_request_count;

  guint                  mnemonics_display_timeout_id;

  gint                   scale;

  gint                   title_height;
  GtkWidget             *title_box;
  GtkWidget             *titlebar;
  GtkWidget             *popup_menu;

  GdkWindow             *border_window[8];
  gint                   initial_fullscreen_monitor;
  guint                  edge_constraints;

  guint                  need_default_position        : 1;
  guint                  need_default_size            : 1;

  guint                  above_initially              : 1;
  guint                  accept_focus                 : 1;
  guint                  below_initially              : 1;
  guint                  builder_visible              : 1;
  guint                  configure_notify_received    : 1;
  guint                  decorated                    : 1;
  guint                  deletable                    : 1;
  guint                  destroy_with_parent          : 1;
  guint                  focus_on_map                 : 1;
  guint                  fullscreen_initially         : 1;
  guint                  has_focus                    : 1;
  guint                  has_user_ref_count           : 1;
  guint                  has_toplevel_focus           : 1;
  guint                  hide_titlebar_when_maximized : 1;
  guint                  iconify_initially            : 1;
  guint                  is_active                    : 1;
  guint                  maximize_initially           : 1;
  guint                  mnemonics_visible            : 1;
  guint                  mnemonics_visible_set        : 1;
  guint                  focus_visible                : 1;
  guint                  modal                        : 1;
  guint                  position                     : 3;
  guint                  resizable                    : 1;
  guint                  skips_pager                  : 1;
  guint                  skips_taskbar                : 1;
  guint                  stick_initially              : 1;
  guint                  transient_parent_group       : 1;
  guint                  type                         : 4;
  guint                  urgent                       : 1;
  guint                  gravity                      : 5;
  guint                  csd_requested                : 1;
  guint                  client_decorated             : 1;
  guint                  use_client_shadow            : 1;
  guint                  maximized                    : 1;
  guint                  fullscreen                   : 1;
  guint                  tiled                        : 1;
  guint                  unlimited_guessed_size_x     : 1;
  guint                  unlimited_guessed_size_y     : 1;
  guint                  force_resize                 : 1;
  guint                  fixate_size                  : 1;

  guint                  use_subsurface               : 1;

  guint                  in_present                   : 1;

  GdkWindowTypeHint      type_hint;

  GtkGesture            *multipress_gesture;
  GtkGesture            *drag_gesture;

  GdkWindow             *hardcoded_window;

  GtkCssNode            *decoration_node;
};

static RegisterStaticSimple register_static_simple_orig_func = NULL;
static RegisterStaticSimple register_static_simple_func = NULL;
static GType gtk_window_type = 0;

static AddInstancePrivateFunc add_instance_private_orig_func = NULL;
static AddInstancePrivateFunc add_instance_private_func = NULL;
static gsize gtk_window_private_size = 0;

static GType
find_gtk_window_type (GType              parent_type,
                      const gchar       *type_name,
                      guint              class_size,
                      GClassInitFunc     class_init,
                      guint              instance_size,
                      GInstanceInitFunc  instance_init,
                      GTypeFlags         flags)
{
  GType type_id;

  type_id = register_static_simple_orig_func (parent_type,
                                              type_name,
                                              class_size,
                                              class_init,
                                              instance_size,
                                              instance_init,
                                              flags);

  if (g_strcmp0 (type_name, "GtkWindow") == 0)
    {
      register_static_simple_func = register_static_simple_orig_func;
      gtk_window_type = type_id;
    }

  return type_id;
}

static gint
find_gtk_window_private_size (GType class_type,
                              gsize private_size)
{
  if (class_type == gtk_window_type)
    {
      add_instance_private_func = add_instance_private_orig_func;
      gtk_window_private_size = private_size;
    }

  return add_instance_private_orig_func (class_type, private_size);
}

__attribute__((constructor))
static void
add_instance_private_init (void)
{
  void *func;

  func = dlsym (RTLD_NEXT, "g_type_register_static_simple");
  register_static_simple_orig_func = func;
  register_static_simple_func = find_gtk_window_type;

  func = dlsym (RTLD_NEXT, "g_type_add_instance_private");
  add_instance_private_orig_func = func;
  add_instance_private_func = find_gtk_window_private_size;
}

GType
g_type_register_static_simple (GType              parent_type,
                               const gchar       *type_name,
                               guint              class_size,
                               GClassInitFunc     class_init,
                               guint              instance_size,
                               GInstanceInitFunc  instance_init,
                               GTypeFlags         flags)
{
  return register_static_simple_func (parent_type,
                                      type_name,
                                      class_size,
                                      class_init,
                                      instance_size,
                                      instance_init,
                                      flags);
}

gint
g_type_add_instance_private (GType class_type,
                             gsize private_size)
{
  return add_instance_private_func (class_type, private_size);
}

static gboolean
check_gtk_window_private (void)
{
  static gboolean ret = FALSE;
  static gboolean checked = FALSE;

  if (!checked)
    {
      GtkWindow *window;

      if (gtk_window_private_size < sizeof (GtkWindowPrivate))
        {
          checked = TRUE;
          return FALSE;
        }

      window = g_object_new (GTK_TYPE_WINDOW,
                             "type", GTK_WINDOW_POPUP,
                             "type-hint", GDK_WINDOW_TYPE_HINT_TOOLTIP,
                             NULL);

      while (TRUE)
        {
          GtkWindowPrivate *priv;
          GtkWidget *titlebar;

          priv = window->priv;

          if (priv->type != GTK_WINDOW_POPUP ||
              priv->type_hint != GDK_WINDOW_TYPE_HINT_TOOLTIP)
            break;

          gtk_window_set_gravity (window, GDK_GRAVITY_STATIC);

          if (priv->gravity != GDK_GRAVITY_STATIC)
            break;

          titlebar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
          gtk_window_set_titlebar (window, titlebar);

          if (priv->title_box != titlebar ||
              !priv->client_decorated)
            break;

          if (priv->csd_requested)
            break;

          ret = TRUE;
          break;
        }

      gtk_widget_destroy (GTK_WIDGET (window));
      checked = TRUE;
    }

  return ret;
}

void
meta_request_csd (GtkWindow *window)
{
  if (!check_gtk_window_private ())
    return;

  window->priv->csd_requested = TRUE;
}
