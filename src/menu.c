/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity window menu */

/* 
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2004 Rob Adams
 * Copyright (C) 2005 Elijah Newren
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

#include <config.h>
#include <stdio.h>
#include <string.h>
#include "menu.h"
#include "main.h"
#include "util.h"
#include "core.h"
#include "themewidget.h"
#include "metaaccellabel.h"
#include "ui.h"

typedef struct _MenuItem MenuItem;
typedef struct _MenuData MenuData;

typedef enum
{
  MENU_ITEM_SEPARATOR = 0,
  MENU_ITEM_NORMAL,
  MENU_ITEM_IMAGE,
  MENU_ITEM_CHECKBOX,
  MENU_ITEM_RADIOBUTTON,
} MetaMenuItemType;

struct _MenuItem
{
  MetaMenuOp op;
  MetaMenuItemType type;
  const char *stock_id;
  const gboolean checked;
  const char *label;
};


struct _MenuData
{
  MetaWindowMenu *menu;
  MetaMenuOp op;
};

static void activate_cb (GtkWidget *menuitem, gpointer data);

static MenuItem menuitems[] = {
  /* Translators: Translate this string the same way as you do in libwnck! */
  { META_MENU_OP_MINIMIZE, MENU_ITEM_IMAGE, METACITY_STOCK_MINIMIZE, FALSE, N_("Mi_nimize") },
  /* Translators: Translate this string the same way as you do in libwnck! */
  { META_MENU_OP_MAXIMIZE, MENU_ITEM_IMAGE, METACITY_STOCK_MAXIMIZE, FALSE, N_("Ma_ximize") },
  /* Translators: Translate this string the same way as you do in libwnck! */
  { META_MENU_OP_UNMAXIMIZE, MENU_ITEM_NORMAL, NULL, FALSE, N_("Unma_ximize") },
  /* Translators: Translate this string the same way as you do in libwnck! */
  { META_MENU_OP_SHADE, MENU_ITEM_NORMAL, NULL, FALSE, N_("Roll _Up") },
  /* Translators: Translate this string the same way as you do in libwnck! */
  { META_MENU_OP_UNSHADE, MENU_ITEM_NORMAL, NULL, FALSE, N_("_Unroll") },
  /* Translators: Translate this string the same way as you do in libwnck! */
  { META_MENU_OP_MOVE, MENU_ITEM_NORMAL, NULL, FALSE, N_("_Move") },
  /* Translators: Translate this string the same way as you do in libwnck! */
  { META_MENU_OP_RESIZE, MENU_ITEM_NORMAL, NULL, FALSE, N_("_Resize") },
  /* Translators: Translate this string the same way as you do in libwnck! */
  { META_MENU_OP_RECOVER, MENU_ITEM_NORMAL, NULL, FALSE, N_("Move Titlebar On_screen") },
  { 0, MENU_ITEM_SEPARATOR, NULL, FALSE, NULL }, /* separator */
  /* Translators: Translate this string the same way as you do in libwnck! */
  { META_MENU_OP_DELETE, MENU_ITEM_IMAGE, METACITY_STOCK_DELETE, FALSE, N_("_Close") },
  { META_MENU_OP_WORKSPACES, MENU_ITEM_SEPARATOR, NULL, FALSE, NULL }, /* separator */
  /* Translators: Translate this string the same way as you do in libwnck! */
  { META_MENU_OP_ABOVE, MENU_ITEM_CHECKBOX, NULL, FALSE, N_("Always on _Top") },
  /* Translators: Translate this string the same way as you do in libwnck! */
  { META_MENU_OP_UNABOVE, MENU_ITEM_CHECKBOX, NULL, TRUE, N_("Always on _Top") },
  /* Translators: Translate this string the same way as you do in libwnck! */
  { META_MENU_OP_STICK, MENU_ITEM_RADIOBUTTON, NULL, FALSE, N_("_Always on Visible Workspace") },
  /* Translators: Translate this string the same way as you do in libwnck! */
  { META_MENU_OP_UNSTICK, MENU_ITEM_RADIOBUTTON, NULL, FALSE,  N_("_Only on This Workspace") },
  /* Translators: Translate this string the same way as you do in libwnck! */
  { META_MENU_OP_MOVE_LEFT, MENU_ITEM_NORMAL, NULL, FALSE, N_("Move to Workspace _Left") },
  /* Translators: Translate this string the same way as you do in libwnck! */
  { META_MENU_OP_MOVE_RIGHT, MENU_ITEM_NORMAL, NULL, FALSE, N_("Move to Workspace R_ight") },
  /* Translators: Translate this string the same way as you do in libwnck! */
  { META_MENU_OP_MOVE_UP, MENU_ITEM_NORMAL, NULL, FALSE, N_("Move to Workspace _Up") },
  /* Translators: Translate this string the same way as you do in libwnck! */
  { META_MENU_OP_MOVE_DOWN, MENU_ITEM_NORMAL, NULL, FALSE, N_("Move to Workspace _Down") }
};

static void
popup_position_func (GtkMenu   *menu,
                     gint      *x,
                     gint      *y,
                     gboolean  *push_in,
                     gpointer  user_data)
{
  GtkRequisition req;      
  GdkPoint *pos;

  pos = user_data;
  
  gtk_widget_size_request (GTK_WIDGET (menu), &req);

  *x = pos->x;
  *y = pos->y;
  
  if (meta_ui_get_direction() == META_UI_DIRECTION_RTL)
    *x = MAX (0, *x - req.width); 

  /* Ensure onscreen */
  *x = CLAMP (*x, 0, MAX (0, gdk_screen_width () - req.width));
  *y = CLAMP (*y, 0, MAX (0, gdk_screen_height () - req.height));
}

static void
menu_closed (GtkMenu *widget,
             gpointer data)
{
  MetaWindowMenu *menu;
  
  menu = data;

  meta_frames_notify_menu_hide (menu->frames);
#ifdef MPX
  (* menu->func) (menu, gdk_display,
                  menu->client_xwindow,
                  gtk_get_current_event_time (),
                  0, 0, 0,
                  menu->data);

#else
  (* menu->func) (menu, gdk_display,
                  menu->client_xwindow,
                  gtk_get_current_event_time (),
                  0, 0,
                  menu->data);
#endif
  
  /* menu may now be freed */
}

static void
activate_cb (GtkWidget *menuitem, gpointer data)
{
  MenuData *md;
  
  g_return_if_fail (GTK_IS_WIDGET (menuitem));
  
  md = data;

  meta_frames_notify_menu_hide (md->menu->frames);
#ifdef MPX
  (* md->menu->func) (md->menu, gdk_display,
                      md->menu->client_xwindow,
                      gtk_get_current_event_time (),
                      md->op,
                      GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menuitem),
                                                          "workspace")),
		      g_object_get_data (G_OBJECT (menuitem), "device"),
                      md->menu->data);

#else
  (* md->menu->func) (md->menu, gdk_display,
                      md->menu->client_xwindow,
                      gtk_get_current_event_time (),
                      md->op,
                      GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menuitem),
                                                          "workspace")),
                      md->menu->data);
#endif

  /* menu may now be freed */
}

/*
 * Given a Display and an index, get the workspace name and add any
 * accelerators. At the moment this means adding a _ if the name is of
 * the form "Workspace n" where n is less than 10, and escaping any
 * other '_'s so they do not create inadvertant accelerators.
 * 
 * The calling code owns the string, and is reponsible to free the
 * memory after use.
 */
static char*
get_workspace_name_with_accel (Display *display,
                               Window   xroot,
                               int      index)
{
  const char *name;
  int number;

  name = meta_core_get_workspace_name_with_index (display, xroot, index);

  g_assert (name != NULL);
  
  /*
   * If the name is of the form "Workspace x" where x is an unsigned
   * integer, insert a '_' before the number if it is less than 10 and
   * return it
   */
  number = 0;
  if (sscanf (name, _("Workspace %d"), &number) == 1)
    {
      char *new_name;
      
      /*
       * Above name is a pointer into the Workspace struct. Here we make
       * a copy copy so we can have our wicked way with it.
       */
      if (number == 10)
        new_name = g_strdup_printf (_("Workspace 1_0"));
      else
        new_name = g_strdup_printf (_("Workspace %s%d"),
                                    number < 10 ? "_" : "",
                                    number);
      return new_name;
    }
  else
    {
      /*
       * Otherwise this is just a normal name. Escape any _ characters so that
       * the user's workspace names do not get mangled.  If the number is less
       * than 10 we provide an accelerator.
       */
      char *new_name;
      const char *source;
      char *dest;

      /*
       * Assume the worst case, that every character is a _.  We also
       * provide memory for " (_#)"
       */
      new_name = g_malloc0 (strlen (name) * 2 + 6 + 1);

      /*
       * Now iterate down the strings, adding '_' to escape as we go
       */
      dest = new_name;
      source = name;
      while (*source != '\0')
        {
          if (*source == '_')
            *dest++ = '_';
          *dest++ = *source++;
        }

      /* People don't start at workstation 0, but workstation 1 */
      if (index < 9)
        {
          g_snprintf (dest, 6, " (_%d)", index + 1);
        }
      else if (index == 9)
        {
          g_snprintf (dest, 6, " (_0)");
        }

      return new_name;
    }
}

static GtkWidget *
menu_item_new (MenuItem *menuitem, int workspace_id)
{
  unsigned int key;
  MetaVirtualModifier mods;
  const char *i18n_label;
  GtkWidget *mi;
  GtkWidget *accel_label;

  if (menuitem->type == MENU_ITEM_NORMAL)
    {
      mi = gtk_menu_item_new ();
    }
  else if (menuitem->type == MENU_ITEM_IMAGE)
    {
      GtkWidget *image;
      
      image = gtk_image_new_from_stock (menuitem->stock_id, GTK_ICON_SIZE_MENU);
      mi = gtk_image_menu_item_new ();
     
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi), image);
      gtk_widget_show (image);
    }
  else if (menuitem->type == MENU_ITEM_CHECKBOX)
    {
      mi = gtk_check_menu_item_new ();
      
      gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mi),
                                      menuitem->checked);
    }    
  else if (menuitem->type == MENU_ITEM_RADIOBUTTON)
    {
      mi = gtk_check_menu_item_new ();

      gtk_check_menu_item_set_draw_as_radio (GTK_CHECK_MENU_ITEM (mi),
                                             TRUE);
      gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mi),
                                      menuitem->checked);
    }
  else
    return gtk_separator_menu_item_new ();

  i18n_label = _(menuitem->label);
  meta_core_get_menu_accelerator (menuitem->op, workspace_id, &key, &mods);

  accel_label = meta_accel_label_new_with_mnemonic (i18n_label);
  gtk_misc_set_alignment (GTK_MISC (accel_label), 0.0, 0.5);

  gtk_container_add (GTK_CONTAINER (mi), accel_label);
  gtk_widget_show (accel_label);

  meta_accel_label_set_accelerator (META_ACCEL_LABEL (accel_label),
                                    key, mods);
  
  return mi;
}

MetaWindowMenu*
#ifdef MPX
meta_window_menu_new   (MetaFrames         *frames,
                        MetaMenuOp          ops,
                        MetaMenuOp          insensitive,
			MetaDevices	   *devices,
                        Window              client_xwindow,
                        unsigned long       active_workspace,
                        int                 n_workspaces,
                        MetaWindowMenuFunc  func,
                        gpointer            data)
#else
meta_window_menu_new   (MetaFrames         *frames,
                        MetaMenuOp          ops,
                        MetaMenuOp          insensitive,
                        Window              client_xwindow,
                        unsigned long       active_workspace,
                        int                 n_workspaces,
                        MetaWindowMenuFunc  func,
                        gpointer            data)
#endif
{
  int i;
  MetaWindowMenu *menu;

  /* FIXME: Modifications to 'ops' should happen in meta_window_show_menu */
  if (n_workspaces < 2)
    ops &= ~(META_MENU_OP_STICK | META_MENU_OP_UNSTICK | META_MENU_OP_WORKSPACES);
  else if (n_workspaces == 2) 
    /* #151183: If we only have two workspaces, disable the menu listing them. */
    ops &= ~(META_MENU_OP_WORKSPACES);
  
  menu = g_new (MetaWindowMenu, 1);
  menu->frames = frames;
  menu->client_xwindow = client_xwindow;
  menu->func = func;
  menu->data = data;
  menu->ops = ops;
  menu->insensitive = insensitive;  
  
  menu->menu = gtk_menu_new ();

  gtk_menu_set_screen (GTK_MENU (menu->menu),
                       gtk_widget_get_screen (GTK_WIDGET (frames)));

  for (i = 0; i < (int) G_N_ELEMENTS (menuitems); i++)
    {
      MenuItem menuitem = menuitems[i];
      if (ops & menuitem.op || menuitem.op == 0)
        {
          GtkWidget *mi;
          MenuData *md;
          unsigned int key;
          MetaVirtualModifier mods;

          mi = menu_item_new (&menuitem, -1);

          /* Set the activeness of radiobuttons. */
          switch (menuitem.op)
            {
            case META_MENU_OP_STICK:
              gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mi),
                                              active_workspace == 0xFFFFFFFF);
              break;
            case META_MENU_OP_UNSTICK:
              gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mi),
                                              active_workspace != 0xFFFFFFFF);
              break;
            default:
              break;
            }

          if (menuitem.type != MENU_ITEM_SEPARATOR)
            {
              meta_core_get_menu_accelerator (menuitems[i].op, -1,
                                              &key, &mods);

              if (insensitive & menuitem.op)
                gtk_widget_set_sensitive (mi, FALSE);
              
              md = g_new (MenuData, 1);
              
              md->menu = menu;
              md->op = menuitem.op;
              
              gtk_signal_connect_full (GTK_OBJECT (mi),
                                       "activate",
                                       GTK_SIGNAL_FUNC (activate_cb),
                                       NULL,
                                       md,
                                       g_free, FALSE, FALSE);
            }
          
          gtk_menu_shell_append (GTK_MENU_SHELL (menu->menu), mi);
          
          gtk_widget_show (mi);
        }
    }

  if (ops & META_MENU_OP_WORKSPACES)
    {
      Display *display;
      Window xroot;
      GdkScreen *screen;
      GtkWidget *submenu;
      GtkWidget *submenuitem;

      MenuItem to_another_workspace = {
        0, MENU_ITEM_NORMAL,
        NULL, FALSE,
        N_("Move to Another _Workspace")
      };

      meta_verbose ("Creating %d-workspace menu current space %lu\n",
                    n_workspaces, active_workspace);
          
      display = gdk_x11_drawable_get_xdisplay (GTK_WIDGET (frames)->window);
        
      screen = gdk_drawable_get_screen (GTK_WIDGET (frames)->window);
      xroot = GDK_DRAWABLE_XID (gdk_screen_get_root_window (screen));

      submenu = gtk_menu_new ();
      submenuitem = menu_item_new (&to_another_workspace, -1);
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (submenuitem), submenu);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu->menu), submenuitem);
      gtk_widget_show (submenuitem);
           
      for (i = 0; i < n_workspaces; i++)
        {
          char *label;
          MenuData *md;
          unsigned int key;
          MetaVirtualModifier mods;
          MenuItem moveitem;
          GtkWidget *mi;
          
          meta_core_get_menu_accelerator (META_MENU_OP_WORKSPACES,
                                          i + 1,
                                          &key, &mods);
              
          label = get_workspace_name_with_accel (display, xroot, i);

          moveitem.type = MENU_ITEM_NORMAL;
          moveitem.op = META_MENU_OP_WORKSPACES;
          moveitem.label = label;
          mi = menu_item_new (&moveitem, i + 1);

          g_free (label);

          if ((active_workspace == (unsigned)i) && (ops & META_MENU_OP_UNSTICK))
            gtk_widget_set_sensitive (mi, FALSE);
            
          md = g_new (MenuData, 1);

          md->menu = menu;
          md->op = META_MENU_OP_WORKSPACES;

          g_object_set_data (G_OBJECT (mi),
                             "workspace",
                             GINT_TO_POINTER (i));
          
          gtk_signal_connect_full (GTK_OBJECT (mi),
                                   "activate",
                                   GTK_SIGNAL_FUNC (activate_cb),
                                   NULL,
                                   md,
                                   g_free, FALSE, FALSE);

          gtk_menu_shell_append (GTK_MENU_SHELL (submenu), mi);
          
          gtk_widget_show (mi);
        }
    }
  else
    meta_verbose ("not creating workspace menu\n");
  
  g_signal_connect (menu->menu, "selection_done",
                    G_CALLBACK (menu_closed), menu);  

#ifdef MPX

  {
    GtkWidget *submenu;
    GtkWidget *submenuitem;

    MenuItem sep = { 0, MENU_ITEM_SEPARATOR, NULL, FALSE, NULL };
    submenu = gtk_menu_new();
    submenuitem = menu_item_new (&sep, -1);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu->menu), submenuitem);
    gtk_widget_show (submenuitem);
  }

  {
    /* SetClientPointer menu */
    Display *display;

    GtkWidget *submenu;
    GtkWidget *submenuitem;
    XID clientPtr;

    MenuItem select_client_pointer = {
      0, MENU_ITEM_NORMAL, NULL,
      FALSE, N_("Select Client Pointer")
    };

    submenu = gtk_menu_new ();
    submenuitem = menu_item_new (&select_client_pointer, -1);
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (submenuitem), submenu);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu->menu), submenuitem);
    gtk_widget_show (submenuitem);

    display = gdk_x11_drawable_get_xdisplay (GTK_WIDGET (frames)->window);

    /* Someone might have changed the clientPointer! */
    XGetClientPointer(display, client_xwindow, (int *)&clientPtr);
    meta_warning("my mouse is %d\n", (int) clientPtr); /* XXX */

    for (i = 0; i < devices->miceUsed; i++) {

      MenuItem moveitem;
      MenuData *md;
      GtkWidget *mi;

      moveitem.type = MENU_ITEM_RADIOBUTTON;
      moveitem.op = META_MENU_OP_CLIENT_POINTER;
      moveitem.label = devices->mice[i].name;
      meta_warning("name = %s\n", moveitem.label);
      /* XXX gotta make radiobuttons appear as checked!
      moveitem.checked = (clientPtr == `this guy`) ? 1 : 0; */
      mi = menu_item_new (&moveitem, i+1);

      md = g_new (MenuData, 1);
      md->menu = menu;
      md->op = META_MENU_OP_CLIENT_POINTER;
      g_object_set_data (G_OBJECT(mi),
                         "device",
                         &devices->mice[i]);
      gtk_signal_connect_full (GTK_OBJECT (mi),
                               "activate",
                               GTK_SIGNAL_FUNC (activate_cb),
                               NULL,
                               md,
                               g_free, FALSE, FALSE);

      gtk_menu_shell_append (GTK_MENU_SHELL (submenu), mi);
      gtk_widget_show (mi);
    }
  }
#endif

  return menu;
}

void
meta_window_menu_popup (MetaWindowMenu     *menu,
                        int                 root_x,
                        int                 root_y,
                        int                 button,
                        guint32             timestamp)
{
  GdkPoint *pt;
  
  pt = g_new (GdkPoint, 1);

  g_object_set_data_full (G_OBJECT (menu->menu),
                          "destroy-point",
                          pt,
                          g_free);

  pt->x = root_x;
  pt->y = root_y;
  
  gtk_menu_popup (GTK_MENU (menu->menu),
                  NULL, NULL,
                  popup_position_func, pt,
                  button,
                  timestamp);

  if (!GTK_MENU_SHELL (menu->menu)->have_xgrab)
    meta_warning ("GtkMenu failed to grab the pointer\n");
}

void
meta_window_menu_free (MetaWindowMenu *menu)
{
  gtk_widget_destroy (menu->menu);
  g_free (menu);
}
