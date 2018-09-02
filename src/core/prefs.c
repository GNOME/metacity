/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity preferences */

/*
 * Copyright (C) 2001 Havoc Pennington, Copyright (C) 2002 Red Hat Inc.
 * Copyright (C) 2006 Elijah Newren
 * Copyright (C) 2008 Thomas Thurman
 * Copyright (C) 2010 Milan Bouchet-Valat, Copyright (C) 2011 Red Hat Inc.
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
#include "prefs.h"
#include "ui.h"
#include "util.h"
#include <glib.h>
#include <gio/gio.h>
#include <string.h>
#include <stdlib.h>
#include "keybindings.h"

/* If you add a key, it needs updating in init() and in the gsettings
 * notify listener and of course in the .schemas file.
 *
 * Keys which are handled by one of the unified handlers below are
 * not given a name here, because the purpose of the unified handlers
 * is that keys should be referred to exactly once.
 */
#define KEY_TITLEBAR_FONT "titlebar-font"
#define KEY_NUM_WORKSPACES "num-workspaces"
#define KEY_WORKSPACE_NAMES "workspace-names"
#define KEY_COMPOSITOR "compositing-manager"
#define KEY_PLACEMENT_MODE "placement-mode"

/* Keys from "foreign" schemas */
#define KEY_GNOME_ACCESSIBILITY "toolkit-accessibility"
#define KEY_GNOME_ANIMATIONS "enable-animations"
#define KEY_GNOME_CURSOR_THEME "cursor-theme"

/* These are the different schemas we are keeping
 * a GSettings instance for */
#define SCHEMA_GENERAL         "org.gnome.desktop.wm.preferences"
#define SCHEMA_METACITY        "org.gnome.metacity"
#define SCHEMA_METACITY_THEME  "org.gnome.metacity.theme"
#define SCHEMA_INTERFACE       "org.gnome.desktop.interface"

#define SETTINGS(s) g_hash_table_lookup (settings_schemas, (s))

static GList *changes = NULL;
static guint changed_idle;
static GList *listeners = NULL;
static GHashTable *settings_schemas;

static gboolean use_system_font = FALSE;
static PangoFontDescription *titlebar_font = NULL;
static MetaVirtualModifier mouse_button_mods = Mod1Mask;
static GDesktopFocusMode focus_mode = G_DESKTOP_FOCUS_MODE_CLICK;
static GDesktopFocusNewWindows focus_new_windows = G_DESKTOP_FOCUS_NEW_WINDOWS_SMART;
static gboolean raise_on_click = TRUE;
static gboolean attach_modal_dialogs = FALSE;
static gchar *current_theme_name = NULL;
static MetaThemeType current_theme_type = META_THEME_TYPE_GTK;
static int num_workspaces = 4;
static GDesktopTitlebarAction action_double_click_titlebar = G_DESKTOP_TITLEBAR_ACTION_TOGGLE_MAXIMIZE;
static GDesktopTitlebarAction action_middle_click_titlebar = G_DESKTOP_TITLEBAR_ACTION_LOWER;
static GDesktopTitlebarAction action_right_click_titlebar = G_DESKTOP_TITLEBAR_ACTION_MENU;
static gboolean disable_workarounds = FALSE;
static gboolean auto_raise = FALSE;
static gboolean auto_raise_delay = 500;
static gboolean bell_is_visible = FALSE;
static gboolean bell_is_audible = TRUE;
static gboolean reduced_resources = FALSE;
static gboolean gnome_accessibility = FALSE;
static gboolean gnome_animations = TRUE;
static char *cursor_theme = NULL;
static int   cursor_size = 24;
static gboolean compositing_manager = TRUE;
static gboolean resize_with_right_button = FALSE;
static gboolean edge_tiling = FALSE;
static gboolean force_fullscreen = TRUE;
static gboolean alt_tab_thumbnails = FALSE;

static GDesktopVisualBellType visual_bell_type = G_DESKTOP_VISUAL_BELL_FULLSCREEN_FLASH;
static gchar *button_layout;

static MetaPlacementMode placement_mode = META_PLACEMENT_MODE_SMART;

/* NULL-terminated array */
static char **workspace_names = NULL;

static void handle_preference_update_enum (GSettings *settings,
                                           gchar     *key);

static gboolean update_binding         (MetaKeyPref *binding,
                                        gchar      **strokes);
static gboolean update_key_binding     (const char  *key,
                                        gchar      **strokes);
static gboolean update_workspace_names (void);

static void settings_changed (GSettings      *settings,
                              gchar          *key,
                              gpointer        data);
static void bindings_changed (GSettings      *settings,
                              gchar          *key,
                              gpointer        data);


static void queue_changed (MetaPreference  pref);

static void maybe_give_disable_workarounds_warning (void);

static gboolean titlebar_handler (GVariant*, gpointer*, gpointer);
static gboolean theme_name_handler (GVariant*, gpointer*, gpointer);
static gboolean mouse_button_mods_handler (GVariant*, gpointer*, gpointer);
static gboolean button_layout_handler (GVariant*, gpointer*, gpointer);

static void     init_bindings             (void);
static void     init_workspace_names      (void);

static void update_button_layout (const gchar *string_value);

typedef struct
{
  MetaPrefsChangedFunc func;
  gpointer data;
} MetaPrefsListener;

typedef struct
{
  const gchar *key;
  const gchar *schema;
  MetaPreference pref;
} MetaBasePreference;

typedef struct
{
  MetaBasePreference base;
  gpointer target;
} MetaEnumPreference;

typedef struct
{
  MetaBasePreference base;
  gboolean *target;
  gboolean becomes_true_on_destruction;
} MetaBoolPreference;

typedef struct
{
  MetaBasePreference base;

  /**
   * A handler.  Many of the string preferences aren't stored as
   * strings and need parsing; others of them have default values
   * which can't be solved in the general case.  If you include a
   * function pointer here, it will be called instead of writing
   * the string value out to the target variable.
   *
   * The function will be passed to g_settings_get_mapped() and should
   * return %TRUE if the mapping was successful and %FALSE otherwise.
   * In the former case the function is expected to handle the result
   * of the conversion itself and call queue_changed() appropriately;
   * in particular the @result (out) parameter as returned by
   * g_settings_get_mapped() will be ignored in all cases.
   *
   * This may be NULL.  If it is, see "target", below.
   */
  GSettingsGetMapping handler;

  /**
   * Where to write the incoming string.
   *
   * This must be NULL if the handler is non-NULL.
   * If the incoming string is NULL, no change will be made.
   */
  gchar **target;

} MetaStringPreference;

typedef struct
{
  MetaBasePreference base;
  gint *target;
} MetaIntPreference;


/* All preferences that are not keybindings must be listed here,
 * plus in the GSettings schemas and the MetaPreference enum.
 */

/* FIXMEs: */
/* @@@ Don't use NULL lines at the end; glib can tell you how big it is */
static MetaEnumPreference preferences_enum[] =
  {
    {
      { "focus-new-windows",
        SCHEMA_GENERAL,
        META_PREF_FOCUS_NEW_WINDOWS,
      },
      &focus_new_windows,
    },
    {
      { "focus-mode",
        SCHEMA_GENERAL,
        META_PREF_FOCUS_MODE,
      },
      &focus_mode,
    },
    {
      { "visual-bell-type",
        SCHEMA_GENERAL,
        META_PREF_VISUAL_BELL_TYPE,
      },
      &visual_bell_type,
    },
    {
      { "action-double-click-titlebar",
        SCHEMA_GENERAL,
        META_PREF_ACTION_DOUBLE_CLICK_TITLEBAR,
      },
      &action_double_click_titlebar,
    },
    {
      { "action-middle-click-titlebar",
        SCHEMA_GENERAL,
        META_PREF_ACTION_MIDDLE_CLICK_TITLEBAR,
      },
      &action_middle_click_titlebar,
    },
    {
      { "action-right-click-titlebar",
        SCHEMA_GENERAL,
        META_PREF_ACTION_RIGHT_CLICK_TITLEBAR,
      },
      &action_right_click_titlebar,
    },
    {
      { "placement-mode",
        SCHEMA_METACITY,
        META_PREF_PLACEMENT_MODE,
      },
      &placement_mode,
    },
    {
      { "type",
        SCHEMA_METACITY_THEME,
        META_PREF_THEME_TYPE,
      },
      &current_theme_type,
    },
    { { NULL, 0, 0 }, NULL },
  };

static MetaBoolPreference preferences_bool[] =
  {
    {
      { "raise-on-click",
        SCHEMA_GENERAL,
        META_PREF_RAISE_ON_CLICK,
      },
      &raise_on_click,
      TRUE,
    },
    {
      { "titlebar-uses-system-font",
        SCHEMA_GENERAL,
        META_PREF_TITLEBAR_FONT, /* note! shares a pref */
      },
      &use_system_font,
      TRUE,
    },
    {
      { "disable-workarounds",
        SCHEMA_GENERAL,
        META_PREF_DISABLE_WORKAROUNDS,
      },
      &disable_workarounds,
      FALSE,
    },
    {
      { "auto-raise",
        SCHEMA_GENERAL,
        META_PREF_AUTO_RAISE,
      },
      &auto_raise,
      FALSE,
    },
    {
      { "visual-bell",
        SCHEMA_GENERAL,
        META_PREF_VISUAL_BELL,
      },
      &bell_is_visible, /* FIXME: change the name: it's confusing */
      FALSE,
    },
    {
      { "audible-bell",
        SCHEMA_GENERAL,
        META_PREF_AUDIBLE_BELL,
      },
      &bell_is_audible, /* FIXME: change the name: it's confusing */
      FALSE,
    },
    {
      { "reduced-resources",
        SCHEMA_METACITY,
        META_PREF_REDUCED_RESOURCES,
      },
      &reduced_resources,
      FALSE,
    },
    {
      { KEY_GNOME_ACCESSIBILITY,
        SCHEMA_INTERFACE,
        META_PREF_GNOME_ACCESSIBILITY,
      },
      &gnome_accessibility,
      FALSE,
    },
    {
      { KEY_GNOME_ANIMATIONS,
        SCHEMA_INTERFACE,
        META_PREF_GNOME_ANIMATIONS,
      },
      &gnome_animations,
      TRUE,
    },
    {
      { "compositing-manager",
        SCHEMA_METACITY,
        META_PREF_COMPOSITING_MANAGER,
      },
      &compositing_manager,
      FALSE,
    },
    {
      { "resize-with-right-button",
        SCHEMA_GENERAL,
        META_PREF_RESIZE_WITH_RIGHT_BUTTON,
      },
      &resize_with_right_button,
      FALSE,
    },
    {
      { "edge-tiling",
        SCHEMA_METACITY,
        META_PREF_EDGE_TILING,
      },
      &edge_tiling,
      FALSE,
    },
    {
      { "alt-tab-thumbnails",
        SCHEMA_METACITY,
        META_PREF_ALT_TAB_THUMBNAILS,
      },
      &alt_tab_thumbnails,
      FALSE,
    },
    { { NULL, 0, 0 }, NULL, FALSE },
  };

static MetaStringPreference preferences_string[] =
  {
    {
      { "mouse-button-modifier",
        SCHEMA_GENERAL,
        META_PREF_MOUSE_BUTTON_MODS,
      },
      mouse_button_mods_handler,
      NULL,
    },
    {
      { KEY_TITLEBAR_FONT,
        SCHEMA_GENERAL,
        META_PREF_TITLEBAR_FONT,
      },
      titlebar_handler,
      NULL,
    },
    {
      { "button-layout",
        SCHEMA_GENERAL,
        META_PREF_BUTTON_LAYOUT,
      },
      button_layout_handler,
      NULL,
    },
    {
      { KEY_GNOME_CURSOR_THEME,
        SCHEMA_INTERFACE,
        META_PREF_CURSOR_THEME,
      },
      NULL,
      &cursor_theme,
    },
    {
      { "name",
        SCHEMA_METACITY_THEME,
        META_PREF_THEME_NAME,
      },
      theme_name_handler,
      NULL,
    },
    { { NULL, 0, 0 }, NULL },
  };

static MetaIntPreference preferences_int[] =
  {
    {
      { "num-workspaces",
        SCHEMA_GENERAL,
        META_PREF_NUM_WORKSPACES,
      },
      &num_workspaces
    },
    {
      { "auto-raise-delay",
        SCHEMA_GENERAL,
        META_PREF_AUTO_RAISE_DELAY,
      },
      &auto_raise_delay
    },
    { { NULL, 0, 0 }, NULL },
  };

static void
handle_preference_init_enum (void)
{
  MetaEnumPreference *cursor = preferences_enum;

  while (cursor->base.key != NULL)
    {
      if (cursor->target==NULL)
          continue;

      *((gint *) cursor->target) =
        g_settings_get_enum (SETTINGS (cursor->base.schema), cursor->base.key);

      ++cursor;
    }
}

static void
handle_preference_init_bool (void)
{
  MetaBoolPreference *cursor = preferences_bool;

  while (cursor->base.key != NULL)
    {
      if (cursor->target!=NULL)
        *cursor->target =
          g_settings_get_boolean (SETTINGS (cursor->base.schema),
                                  cursor->base.key);

      ++cursor;
    }

  maybe_give_disable_workarounds_warning ();
}

static void
handle_preference_init_string (void)
{
  MetaStringPreference *cursor = preferences_string;

  while (cursor->base.key != NULL)
    {
      char *value;

      /* Complex keys have a mapping function to check validity */
      if (cursor->handler)
        {
          if (cursor->target)
            g_error ("%s has both a target and a handler", cursor->base.key);

          g_settings_get_mapped (SETTINGS (cursor->base.schema),
                                 cursor->base.key, cursor->handler, NULL);
        }
      else
        {
          if (!cursor->target)
            g_error ("%s must have handler or target", cursor->base.key);

          if (*(cursor->target))
            g_free (*(cursor->target));

          value = g_settings_get_string (SETTINGS (cursor->base.schema),
                                         cursor->base.key);

          *(cursor->target) = value;
        }

      ++cursor;
    }
}

static void
handle_preference_init_int (void)
{
  MetaIntPreference *cursor = preferences_int;


  while (cursor->base.key != NULL)
    {
      if (cursor->target)
        *cursor->target = g_settings_get_int (SETTINGS (cursor->base.schema),
                                              cursor->base.key);

      ++cursor;
    }
}

static void
handle_preference_update_enum (GSettings *settings,
                               gchar *key)
{
  MetaEnumPreference *cursor = preferences_enum;
  gint old_value;

  while (cursor->base.key != NULL && strcmp (key, cursor->base.key) != 0)
    ++cursor;

  if (cursor->base.key==NULL)
    /* Didn't recognise that key. */
    return;

  /* We need to know whether the value changes, so
   * store the current value away. */
  old_value = * ((gint *) cursor->target);

  *((gint *) cursor->target) =
    g_settings_get_enum (SETTINGS (cursor->base.schema), key);

  /* Did it change?  If so, tell the listeners about it. */
  if (old_value != *((gint *) cursor->target))
    queue_changed (cursor->base.pref);
}

static void
handle_preference_update_bool (GSettings *settings,
                               gchar *key)
{
  MetaBoolPreference *cursor = preferences_bool;
  gboolean old_value;

  while (cursor->base.key != NULL && strcmp (key, cursor->base.key) != 0)
    ++cursor;

  if (cursor->base.key==NULL || cursor->target==NULL)
    /* Unknown key or no work for us to do. */
    return;

  /* We need to know whether the value changes, so
   * store the current value away. */
  old_value = *((gboolean *) cursor->target);

  *((gboolean *) cursor->target) =
    g_settings_get_boolean (SETTINGS (cursor->base.schema), key);

  /* Did it change?  If so, tell the listeners about it. */
  if (old_value != *((gboolean *) cursor->target))
    queue_changed (cursor->base.pref);

  if (cursor->base.pref==META_PREF_DISABLE_WORKAROUNDS)
    maybe_give_disable_workarounds_warning ();
}

static void
handle_preference_update_string (GSettings *settings,
                                 gchar *key)
{
  MetaStringPreference *cursor = preferences_string;
  char *value;
  gboolean inform_listeners = FALSE;

  while (cursor->base.key != NULL && strcmp (key, cursor->base.key) != 0)
    ++cursor;

  if (cursor->base.key==NULL)
    /* Didn't recognise that key. */
    return;

  /* Complex keys have a mapping function to check validity */
  if (cursor->handler)
    {
      if (cursor->target)
        g_error ("%s has both a target and a handler", cursor->base.key);

      g_settings_get_mapped (SETTINGS (cursor->base.schema),
                             cursor->base.key, cursor->handler, NULL);
    }
  else
    {
      if (!cursor->target)
        g_error ("%s must have handler or target", cursor->base.key);

      value = g_settings_get_string (SETTINGS (cursor->base.schema),
                                     cursor->base.key);
      inform_listeners = (g_strcmp0 (value, *(cursor->target)) != 0);

      if (*(cursor->target))
        g_free (*(cursor->target));

      *(cursor->target) = value;
    }

  if (inform_listeners)
    queue_changed (cursor->base.pref);
}

static void
handle_preference_update_int (GSettings *settings,
                              gchar *key)
{
  MetaIntPreference *cursor = preferences_int;
  gint new_value;

  while (cursor->base.key != NULL && strcmp (key, cursor->base.key) != 0)
    ++cursor;

  if (cursor->base.key==NULL || cursor->target==NULL)
    /* Unknown key or no work for us to do. */
    return;

  new_value = g_settings_get_int (SETTINGS (cursor->base.schema), key);

  /* Did it change?  If so, tell the listeners about it. */
  if (*cursor->target != new_value)
    {
      *cursor->target = new_value;
      queue_changed (cursor->base.pref);
    }
}

/****************************************************************************/
/* Listeners.                                                               */
/****************************************************************************/

void
meta_prefs_add_listener (MetaPrefsChangedFunc func,
                         gpointer             data)
{
  MetaPrefsListener *l;

  l = g_new (MetaPrefsListener, 1);
  l->func = func;
  l->data = data;

  listeners = g_list_prepend (listeners, l);
}

void
meta_prefs_remove_listener (MetaPrefsChangedFunc func,
                            gpointer             data)
{
  GList *tmp;

  tmp = listeners;
  while (tmp != NULL)
    {
      MetaPrefsListener *l = tmp->data;

      if (l->func == func &&
          l->data == data)
        {
          g_free (l);
          listeners = g_list_delete_link (listeners, tmp);

          return;
        }

      tmp = tmp->next;
    }

  g_error ("Did not find listener to remove");
}

static void
emit_changed (MetaPreference pref)
{
  GList *tmp;
  GList *copy;

  meta_topic (META_DEBUG_PREFS, "Notifying listeners that pref %s changed\n",
              meta_preference_to_string (pref));

  copy = g_list_copy (listeners);

  tmp = copy;

  while (tmp != NULL)
    {
      MetaPrefsListener *l = tmp->data;

      (* l->func) (pref, l->data);

      tmp = tmp->next;
    }

  g_list_free (copy);
}

static gboolean
changed_idle_handler (gpointer data)
{
  GList *tmp;
  GList *copy;

  changed_idle = 0;

  copy = g_list_copy (changes); /* reentrancy paranoia */

  g_list_free (changes);
  changes = NULL;

  tmp = copy;
  while (tmp != NULL)
    {
      MetaPreference pref = GPOINTER_TO_INT (tmp->data);

      emit_changed (pref);

      tmp = tmp->next;
    }

  g_list_free (copy);

  return FALSE;
}

static void
queue_changed (MetaPreference pref)
{
  meta_topic (META_DEBUG_PREFS, "Queueing change of pref %s\n",
              meta_preference_to_string (pref));

  if (g_list_find (changes, GINT_TO_POINTER (pref)) == NULL)
    changes = g_list_prepend (changes, GINT_TO_POINTER (pref));
  else
    meta_topic (META_DEBUG_PREFS, "Change of pref %s was already pending\n",
                meta_preference_to_string (pref));

  if (changed_idle == 0)
    changed_idle = g_idle_add_full (META_PRIORITY_PREFS_NOTIFY,
                                    changed_idle_handler, NULL, NULL);
}

static void
gtk_cursor_theme_size_changed (GtkSettings *settings,
                               GParamSpec  *pspec,
                               gpointer     user_data)
{
  gint size;

  g_object_get (settings, "gtk-cursor-theme-size", &size, NULL);

  if (size == 0)
    size = 24;

  if (size != cursor_size)
    {
      cursor_size = size;
      queue_changed (META_PREF_CURSOR_SIZE);
    }
}

static void
init_gtk_cursor_theme_size (void)
{
  GtkSettings *settings;

  settings = gtk_settings_get_default ();

  g_signal_connect (settings, "notify::gtk-cursor-theme-size",
                    G_CALLBACK (gtk_cursor_theme_size_changed), NULL);

  gtk_cursor_theme_size_changed (settings, NULL, NULL);
}

static void
gtk_theme_name_changed (GtkSettings *settings,
                        GParamSpec  *pspec,
                        gpointer     user_data)
{
  if (current_theme_type == META_THEME_TYPE_GTK)
    queue_changed (META_PREF_THEME_NAME);
}

static void
init_gtk_theme_name (void)
{
  GtkSettings *settings;

  settings = gtk_settings_get_default ();

  g_signal_connect (settings, "notify::gtk-theme-name",
                    G_CALLBACK (gtk_theme_name_changed), NULL);
}

/****************************************************************************/
/* Initialisation.                                                          */
/****************************************************************************/

void
meta_prefs_init (void)
{
  GSettings *settings;

  settings_schemas = g_hash_table_new_full (g_str_hash, g_str_equal,
                                            g_free, g_object_unref);

  settings = g_settings_new (SCHEMA_GENERAL);
  g_signal_connect (settings, "changed", G_CALLBACK (settings_changed), NULL);
  g_hash_table_insert (settings_schemas, g_strdup (SCHEMA_GENERAL), settings);

  settings = g_settings_new (SCHEMA_METACITY);
  g_signal_connect (settings, "changed", G_CALLBACK (settings_changed), NULL);
  g_hash_table_insert (settings_schemas, g_strdup (SCHEMA_METACITY), settings);

  settings = g_settings_new (SCHEMA_METACITY_THEME);
  g_signal_connect (settings, "changed", G_CALLBACK (settings_changed), NULL);
  g_hash_table_insert (settings_schemas, g_strdup (SCHEMA_METACITY_THEME), settings);

  /* Individual keys we watch outside of our schemas */
  settings = g_settings_new (SCHEMA_INTERFACE);
  g_signal_connect (settings, "changed::" KEY_GNOME_ACCESSIBILITY,
                    G_CALLBACK (settings_changed), NULL);
  g_signal_connect (settings, "changed::" KEY_GNOME_ANIMATIONS,
                    G_CALLBACK (settings_changed), NULL);
  g_signal_connect (settings, "changed::" KEY_GNOME_CURSOR_THEME,
                    G_CALLBACK (settings_changed), NULL);
  g_hash_table_insert (settings_schemas, g_strdup (SCHEMA_INTERFACE), settings);

  /* Pick up initial values. */
  handle_preference_init_enum ();
  handle_preference_init_bool ();
  handle_preference_init_string ();
  handle_preference_init_int ();

  init_bindings ();
  init_workspace_names ();

  init_gtk_cursor_theme_size ();
  init_gtk_theme_name ();
}

/****************************************************************************/
/* Updates.                                                                 */
/****************************************************************************/

static void
settings_changed (GSettings *settings,
                  gchar *key,
                  gpointer data)
{
  GVariant *value;
  const GVariantType *type;
  MetaEnumPreference *cursor;
  gboolean found_enum;

  /* String array, handled separately */
  if (strcmp (key, KEY_WORKSPACE_NAMES) == 0)
    {
      if (update_workspace_names ())
        queue_changed (META_PREF_WORKSPACE_NAMES);

      return;
    }

  value = g_settings_get_value (settings, key);
  type = g_variant_get_type (value);

  if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
    handle_preference_update_bool (settings, key);
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT32))
    handle_preference_update_int (settings, key);
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING))
    {
      cursor = preferences_enum;
      found_enum = FALSE;

      while (cursor->base.key != NULL)
        {
          if (strcmp (key, cursor->base.key) == 0)
            found_enum = TRUE;

          cursor++;
        }

      if (found_enum)
        handle_preference_update_enum (settings, key);
      else
        handle_preference_update_string (settings, key);
    }
  else
    /* Someone added a preference of an unhandled type */
    g_assert_not_reached ();

  g_variant_unref (value);
}

static void
bindings_changed (GSettings *settings,
                  gchar *key,
                  gpointer data)
{
  gchar **strokes;

  strokes = g_settings_get_strv (settings, key);

   if (update_key_binding (key, strokes))
     queue_changed (META_PREF_KEYBINDINGS);

  g_strfreev (strokes);
}

/**
 * Special case: give a warning the first time disable_workarounds
 * is turned on.
 */
static void
maybe_give_disable_workarounds_warning (void)
{
  static gboolean first_disable = TRUE;

  if (first_disable && disable_workarounds)
    {
      first_disable = FALSE;

      g_warning ("Workarounds for broken applications disabled. "
                 "Some applications may not behave properly.");
    }
}

MetaVirtualModifier
meta_prefs_get_mouse_button_mods  (void)
{
  return mouse_button_mods;
}

GDesktopFocusMode
meta_prefs_get_focus_mode (void)
{
  return focus_mode;
}

GDesktopFocusNewWindows
meta_prefs_get_focus_new_windows (void)
{
  return focus_new_windows;
}

gboolean
meta_prefs_get_attach_modal_dialogs (void)
{
  return attach_modal_dialogs;
}

gboolean
meta_prefs_get_raise_on_click (void)
{
  return raise_on_click;
}

const gchar *
meta_prefs_get_theme_name (void)
{
  return current_theme_name;
}

MetaThemeType
meta_prefs_get_theme_type (void)
{
  return current_theme_type;
}

const char*
meta_prefs_get_cursor_theme (void)
{
  return cursor_theme;
}

int
meta_prefs_get_cursor_size (void)
{
  return cursor_size;
}

/****************************************************************************/
/* Handlers for string preferences.                                         */
/****************************************************************************/

static gboolean
titlebar_handler (GVariant *value,
                  gpointer *result,
                  gpointer  data)
{
  PangoFontDescription *desc;
  const gchar *string_value;

  *result = NULL; /* ignored */
  string_value = g_variant_get_string (value, NULL);
  desc = pango_font_description_from_string (string_value);

  if (desc == NULL)
    {
      g_warning ("Could not parse font description \"%s\" from GSettings key %s",
                 string_value ? string_value : "(null)", KEY_TITLEBAR_FONT);

      return FALSE;
    }

  /* Is the new description the same as the old? */
  if (titlebar_font &&
      pango_font_description_equal (desc, titlebar_font))
    {
      pango_font_description_free (desc);
    }
  else
    {
      if (titlebar_font)
        pango_font_description_free (titlebar_font);

      titlebar_font = desc;
      queue_changed (META_PREF_TITLEBAR_FONT);
    }

  return TRUE;
}

static gboolean
theme_name_handler (GVariant *value,
                    gpointer *result,
                    gpointer  data)
{
  const gchar *string_value;

  *result = NULL; /* ignored */
  string_value = g_variant_get_string (value, NULL);

  if (g_strcmp0 (current_theme_name, string_value) != 0)
    {
      g_free (current_theme_name);
      current_theme_name = g_strdup (string_value);

      queue_changed (META_PREF_THEME_NAME);
    }

  return TRUE;
}

static gboolean
mouse_button_mods_handler (GVariant *value,
                           gpointer *result,
                           gpointer  data)
{
  MetaVirtualModifier mods;
  const gchar *string_value;

  string_value = g_variant_get_string (value, NULL);

  if (!string_value || !meta_ui_parse_modifier (string_value, &mods))
    {
      g_warning ("\"%s\" found in configuration database is not a valid value "
                 "for mouse button modifier", string_value);

      return FALSE;
    }

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Mouse button modifier has new GSettings value \"%s\"\n",
              string_value);

  if (mods != mouse_button_mods)
    {
      mouse_button_mods = mods;
      queue_changed (META_PREF_MOUSE_BUTTON_MODS);
    }

  return TRUE;
}

static void
update_button_layout (const gchar *string_value)
{
  if (g_strcmp0 (button_layout, string_value) == 0)
    return;

  g_free (button_layout);
  button_layout = g_strdup (string_value);

  emit_changed (META_PREF_BUTTON_LAYOUT);
}

static gboolean
button_layout_handler (GVariant *value,
                       gpointer *result,
                       gpointer  data)
{
  const gchar *string_value;

  *result = NULL; /* ignored */

  string_value = g_variant_get_string (value, NULL);

  if (string_value)
    update_button_layout (string_value);

  return TRUE;
}

const PangoFontDescription*
meta_prefs_get_titlebar_font (void)
{
  if (use_system_font)
    return NULL;
  else
    return titlebar_font;
}

int
meta_prefs_get_num_workspaces (void)
{
  return num_workspaces;
}

gboolean
meta_prefs_get_disable_workarounds (void)
{
  return disable_workarounds;
}

const char*
meta_preference_to_string (MetaPreference pref)
{
  /* TODO: better handled via GLib enum nicknames */
  switch (pref)
    {
    case META_PREF_MOUSE_BUTTON_MODS:
      return "MOUSE_BUTTON_MODS";

    case META_PREF_FOCUS_MODE:
      return "FOCUS_MODE";

    case META_PREF_FOCUS_NEW_WINDOWS:
      return "FOCUS_NEW_WINDOWS";

    case META_PREF_ATTACH_MODAL_DIALOGS:
      return "ATTACH_MODAL_DIALOGS";

    case META_PREF_RAISE_ON_CLICK:
      return "RAISE_ON_CLICK";

    case META_PREF_THEME_NAME:
      return "THEME_NAME";

    case META_PREF_THEME_TYPE:
      return "THEME_TYPE";

    case META_PREF_TITLEBAR_FONT:
      return "TITLEBAR_FONT";

    case META_PREF_NUM_WORKSPACES:
      return "NUM_WORKSPACES";

    case META_PREF_KEYBINDINGS:
      return "KEYBINDINGS";

    case META_PREF_DISABLE_WORKAROUNDS:
      return "DISABLE_WORKAROUNDS";

    case META_PREF_ACTION_DOUBLE_CLICK_TITLEBAR:
      return "ACTION_DOUBLE_CLICK_TITLEBAR";

    case META_PREF_ACTION_MIDDLE_CLICK_TITLEBAR:
      return "ACTION_MIDDLE_CLICK_TITLEBAR";

    case META_PREF_ACTION_RIGHT_CLICK_TITLEBAR:
      return "ACTION_RIGHT_CLICK_TITLEBAR";

    case META_PREF_AUTO_RAISE:
      return "AUTO_RAISE";

    case META_PREF_AUTO_RAISE_DELAY:
      return "AUTO_RAISE_DELAY";

    case META_PREF_BUTTON_LAYOUT:
      return "BUTTON_LAYOUT";

    case META_PREF_WORKSPACE_NAMES:
      return "WORKSPACE_NAMES";

    case META_PREF_VISUAL_BELL:
      return "VISUAL_BELL";

    case META_PREF_AUDIBLE_BELL:
      return "AUDIBLE_BELL";

    case META_PREF_VISUAL_BELL_TYPE:
      return "VISUAL_BELL_TYPE";

    case META_PREF_REDUCED_RESOURCES:
      return "REDUCED_RESOURCES";

    case META_PREF_GNOME_ACCESSIBILITY:
      return "GNOME_ACCESSIBILTY";

    case META_PREF_GNOME_ANIMATIONS:
      return "GNOME_ANIMATIONS";

    case META_PREF_CURSOR_THEME:
      return "CURSOR_THEME";

    case META_PREF_CURSOR_SIZE:
      return "CURSOR_SIZE";

    case META_PREF_COMPOSITING_MANAGER:
      return "COMPOSITING_MANAGER";

    case META_PREF_RESIZE_WITH_RIGHT_BUTTON:
      return "RESIZE_WITH_RIGHT_BUTTON";

    case META_PREF_EDGE_TILING:
      return "EDGE_TILING";

    case META_PREF_FORCE_FULLSCREEN:
      return "FORCE_FULLSCREEN";

    case META_PREF_PLACEMENT_MODE:
      return "PLACEMENT_MODE";

    case META_PREF_ALT_TAB_THUMBNAILS:
      return "ALT_TAB_THUMBNAILS";

    default:
      break;
    }

  return "(unknown)";
}

void
meta_prefs_set_num_workspaces (int n_workspaces)
{
  g_settings_set_int (SETTINGS (SCHEMA_GENERAL),
                      KEY_NUM_WORKSPACES,
                      n_workspaces);
}

static GHashTable *key_bindings;

static void
meta_key_pref_free (MetaKeyPref *pref)
{
  update_binding (pref, NULL);

  g_free (pref->name);
  g_free (pref->schema);

  g_free (pref);
}

static void
init_bindings (void)
{
  key_bindings = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                        (GDestroyNotify) meta_key_pref_free);
}

static void
init_workspace_names (void)
{
  update_workspace_names ();
}

static gboolean
update_binding (MetaKeyPref *binding,
                gchar      **strokes)
{
  unsigned int keysym;
  unsigned int keycode;
  MetaVirtualModifier mods;
  gboolean changed = FALSE;
  MetaKeyCombo *combo;
  int i;

  meta_topic (META_DEBUG_KEYBINDINGS,
              "Binding \"%s\" has new GSettings value\n",
              binding->name);

  /* Okay, so, we're about to provide a new list of key combos for this
   * action. Delete any pre-existing list.
   */
  g_slist_free_full (binding->bindings, g_free);
  binding->bindings = NULL;

  for (i = 0; strokes && strokes[i]; i++)
    {
      keysym = 0;
      keycode = 0;
      mods = 0;

      if (!meta_ui_parse_accelerator (strokes[i], &keysym, &keycode, &mods))
        {
          meta_topic (META_DEBUG_KEYBINDINGS,
                      "Failed to parse new GSettings value\n");
          g_warning ("\"%s\" found in configuration database is not a valid "
                     "value for keybinding \"%s\"", strokes[i], binding->name);

          /* Value is kept and will thus be removed next time we save the key.
           * Changing the key in response to a modification could lead to cyclic calls. */
          continue;
        }

      /* Bug 329676: Bindings which can be shifted must not have no modifiers,
       * nor only SHIFT as a modifier.
       */

      if (binding->add_shift &&
          0 != keysym &&
          (META_VIRTUAL_SHIFT_MASK == mods || 0 == mods))
        {
          g_warning ("Cannot bind \"%s\" to %s: it needs a modifier "
                     "such as Ctrl or Alt.", binding->name, strokes[i]);

          /* Value is kept and will thus be removed next time we save the key.
           * Changing the key in response to a modification could lead to cyclic calls. */
          continue;
        }

      changed = TRUE;

      combo = g_malloc0 (sizeof (MetaKeyCombo));
      combo->keysym = keysym;
      combo->keycode = keycode;
      combo->modifiers = mods;
      binding->bindings = g_slist_prepend (binding->bindings, combo);

      meta_topic (META_DEBUG_KEYBINDINGS,
                      "New keybinding for \"%s\" is keysym = 0x%x keycode = 0x%x mods = 0x%x\n",
                      binding->name, keysym, keycode, mods);
    }

  return changed;
}

static gboolean
update_key_binding (const char *key,
                    gchar     **strokes)
{
  MetaKeyPref *pref = g_hash_table_lookup (key_bindings, key);

  if (pref)
    return update_binding (pref, strokes);
  else
    return FALSE;
}

static gboolean
update_workspace_names (void)
{
  int i;
  char **names;
  int n_workspace_names, n_names;
  gboolean changed = FALSE;

  names = g_settings_get_strv (SETTINGS (SCHEMA_GENERAL), KEY_WORKSPACE_NAMES);
  n_names = g_strv_length (names);
  n_workspace_names = workspace_names ? g_strv_length (workspace_names) : 0;

  for (i = 0; i < n_names; i++)
    if (n_workspace_names < i + 1 || !workspace_names[i] ||
        g_strcmp0 (names[i], workspace_names[i]) != 0)
      {
        changed = TRUE;
        break;
      }

  if (n_workspace_names != n_names)
    changed = TRUE;

  if (changed)
    {
      if (workspace_names)
        g_strfreev (workspace_names);
      workspace_names = names;
    }
  else
    {
      g_strfreev (names);
    }

  return changed;
}

const char*
meta_prefs_get_workspace_name (int i)
{
  const char *name;

  g_return_val_if_fail (i >= 0, NULL);

  if (!workspace_names ||
      g_strv_length (workspace_names) < (guint)i + 1 ||
      !*workspace_names[i])
    {
      char *generated_name = g_strdup_printf (_("Workspace %d"), i + 1);
      name = g_intern_string (generated_name);
      g_free (generated_name);
    }
  else
    name = workspace_names[i];

  meta_topic (META_DEBUG_PREFS,
              "Getting name of workspace %d: \"%s\"\n", i, name);

  return name;
}

void
meta_prefs_change_workspace_name (int         num,
                                  const char *name)
{
  GVariantBuilder builder;
  int n_workspace_names, i;

  g_return_if_fail (num >= 0);

  meta_topic (META_DEBUG_PREFS,
              "Changing name of workspace %d to %s\n",
              num, name ? name : "none");

  /* NULL and empty string both mean "default" here,
   * and we also need to match the name against its default value
   * to avoid saving it literally. */
  if (g_strcmp0 (name, meta_prefs_get_workspace_name (num)) == 0)
    {
      if (!name || !*name)
        meta_topic (META_DEBUG_PREFS,
                    "Workspace %d already uses default name\n", num);
      else
        meta_topic (META_DEBUG_PREFS,
                    "Workspace %d already has name %s\n", num, name);
      return;
    }

  g_variant_builder_init (&builder, G_VARIANT_TYPE_STRING_ARRAY);
  n_workspace_names = workspace_names ? g_strv_length (workspace_names) : 0;

  for (i = 0; i < MAX (num + 1, n_workspace_names); i++)
    {
      const char *value;

      if (i == num)
        value = name ? name : "";
      else if (i < n_workspace_names)
        value = workspace_names[i] ? workspace_names[i] : "";
      else
        value = "";

      g_variant_builder_add (&builder, "s", value);
    }
  g_settings_set_value (SETTINGS (SCHEMA_GENERAL), KEY_WORKSPACE_NAMES,
                        g_variant_builder_end (&builder));
}

const gchar *
meta_prefs_get_button_layout (void)
{
  return button_layout;
}

gboolean
meta_prefs_get_visual_bell (void)
{
  return bell_is_visible;
}

gboolean
meta_prefs_bell_is_audible (void)
{
  return bell_is_audible;
}

GDesktopVisualBellType
meta_prefs_get_visual_bell_type (void)
{
  return visual_bell_type;
}

gboolean
meta_prefs_add_keybinding (const char           *name,
                           const char           *schema,
                           MetaKeyBindingAction  action,
                           MetaKeyBindingFlags   flags)
{
  MetaKeyPref  *pref;
  GSettings    *settings;
  char        **strokes;

  if (g_hash_table_lookup (key_bindings, name))
    {
      g_warning ("Trying to re-add keybinding \"%s\".", name);
      return FALSE;
    }

  settings = SETTINGS (schema);
  if (settings == NULL)
    {
      settings = g_settings_new (schema);
      g_signal_connect (settings, "changed",
                        G_CALLBACK (bindings_changed), NULL);
      g_hash_table_insert (settings_schemas, g_strdup (schema), settings);
    }

  pref = g_new0 (MetaKeyPref, 1);
  pref->name = g_strdup (name);
  pref->schema = g_strdup (schema);
  pref->action = action;
  pref->bindings = NULL;
  pref->add_shift = (flags & META_KEY_BINDING_REVERSES) != 0;
  pref->per_window = (flags & META_KEY_BINDING_PER_WINDOW) != 0;

  strokes = g_settings_get_strv (settings, name);
  update_binding (pref, strokes);
  g_strfreev (strokes);

  g_hash_table_insert (key_bindings, g_strdup (name), pref);

  return TRUE;
}

/**
 * meta_prefs_get_keybindings: (skip)
 * Return: (element-type MetaKeyPref) (transfer container):
 */
GList *
meta_prefs_get_keybindings (void)
{
  return g_hash_table_get_values (key_bindings);
}

GDesktopTitlebarAction
meta_prefs_get_action_double_click_titlebar (void)
{
  return action_double_click_titlebar;
}

GDesktopTitlebarAction
meta_prefs_get_action_middle_click_titlebar (void)
{
  return action_middle_click_titlebar;
}

GDesktopTitlebarAction
meta_prefs_get_action_right_click_titlebar (void)
{
  return action_right_click_titlebar;
}

gboolean
meta_prefs_get_auto_raise (void)
{
  return auto_raise;
}

int
meta_prefs_get_auto_raise_delay (void)
{
  return auto_raise_delay;
}

gboolean
meta_prefs_get_reduced_resources (void)
{
  return reduced_resources;
}

gboolean
meta_prefs_get_gnome_accessibility (void)
{
  return gnome_accessibility;
}

gboolean
meta_prefs_get_gnome_animations (void)
{
  return gnome_animations;
}

gboolean
meta_prefs_get_edge_tiling (void)
{
  return edge_tiling;
}

MetaKeyBindingAction
meta_prefs_get_keybinding_action (const char *name)
{
  MetaKeyPref *pref = g_hash_table_lookup (key_bindings, name);

  return pref ? pref->action : META_KEYBINDING_ACTION_NONE;
}

/* This is used by the menu system to decide what key binding
 * to display next to an option. We return the first non-disabled
 * binding, if any.
 */
void
meta_prefs_get_window_binding (const char          *name,
                               unsigned int        *keysym,
                               MetaVirtualModifier *modifiers)
{
  MetaKeyPref *pref = g_hash_table_lookup (key_bindings, name);

  if (pref->per_window)
    {
      GSList *s = pref->bindings;

      while (s)
        {
          MetaKeyCombo *c = s->data;

          if (c->keysym != 0 || c->modifiers != 0)
            {
              *keysym = c->keysym;
              *modifiers = c->modifiers;
              return;
            }

          s = s->next;
        }

      /* Not found; return the disabled value */
      *keysym = *modifiers = 0;
      return;
    }

  g_assert_not_reached ();
}

gboolean
meta_prefs_get_compositing_manager (void)
{
  return compositing_manager;
}

guint
meta_prefs_get_mouse_button_resize (void)
{
  return resize_with_right_button ? 3: 2;
}

guint
meta_prefs_get_mouse_button_menu (void)
{
  return resize_with_right_button ? 2: 3;
}

gboolean
meta_prefs_get_force_fullscreen (void)
{
  return force_fullscreen;
}

MetaPlacementMode
meta_prefs_get_placement_mode (void)
{
  return placement_mode;
}

gboolean
meta_prefs_get_alt_tab_thumbnails (void)
{
  return alt_tab_thumbnails;
}

void
meta_prefs_set_compositing_manager (gboolean whether)
{
  g_settings_set_boolean (SETTINGS (SCHEMA_METACITY), KEY_COMPOSITOR, whether);
}

void
meta_prefs_set_force_fullscreen (gboolean whether)
{
  force_fullscreen = whether;
}
