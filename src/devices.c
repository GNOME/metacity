/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity device functions */

/* 
 * Copyright (C) 2007 Paulo Ricardo Zanoni
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

#include "devices.h"
#include "common.h"
#include "util.h"
#include "display.h"

#include <string.h>

#ifdef MPX
/* TODO: if these were macros, metacity would be faster! */

/* Should we :%s/find/get/g ??? */

MetaDevInfo*
meta_devices_find_mouse_by_name (MetaDisplay *display, 
				 gchar       *name)
{
  int i;

  for (i = 0; i < display->devices->miceUsed; i++)
    if (strcmp(name, display->devices->mice[i].name) == 0)
      return &display->devices->mice[i];

  meta_warning ("Error! Could not find mouse named %s!\n", name);

  return NULL;
}

MetaDevInfo*
meta_devices_find_mouse_by_id (MetaDisplay *display,
			       XID id)
{
  int i;

  for (i = 0; i < display->devices->miceUsed; i++)
    if (id == display->devices->mice[i].xdev->device_id)
      return &display->devices->mice[i];
  meta_warning ("Error! Could not find mouse XID = %d!\n", (int) id);
  
  return NULL;
}

MetaDevInfo*
meta_devices_find_keyboard_by_id (MetaDisplay *display,
				  XID id)
{
  int i;

  for (i = 0; i < display->devices->keybsUsed; i++)
    if (id == display->devices->keyboards[i].xdev->device_id)
      return &display->devices->keyboards[i];

  meta_warning ("Error! Could not find keyboard XID = %d\n", (int) id);

  return NULL;
}

MetaDevInfo*
meta_devices_find_paired_mouse (MetaDisplay *display, XID id)
{
  int i, j;

  for (i = 0; i < display->devices->keybsUsed; i++)
    if (id == display->devices->keyboards[i].xdev->device_id)
      {
	for (j = 0; j < display->devices->miceUsed; j++)
	  if (display->devices->pairedPointers[i] ==
	      display->devices->mice[j].xdev->device_id)
            return &display->devices->mice[j];
      }

  meta_warning("Error! Could not find mouse paired with keyboard XID = %d."
  	       " This should never happen!!!.\n",
               (int) id);

  return NULL;
}

MetaDevInfo*
meta_devices_find_paired_keyboard (MetaDisplay *display, XID id)
{

  /* FIXME: there can be more than one keyboard paired with the mouse... */
  int i;

  for (i = 0; i < display->devices->keybsUsed; i++)
    if (id == display->devices->pairedPointers[i])
      return &display->devices->keyboards[i];

  meta_warning("Could not find keyboard paired with mouse XID = %d."
  	       " Using another device.\n",
               (int) id);

  return &display->devices->keyboards[0];
}

/* This is used to manage lists of devices...
 * XXX We should decide a definitive way to do this...
 * I don't like this one... */

/* XXX We need performance!! Change these into macros?? */

void
meta_devices_list_create (MetaDevList *list)
{
  list->devices = g_new (MetaDevInfo*, DEFAULT_INPUT_ARRAY_SIZE);
  list->used = 0;
  list->size = DEFAULT_INPUT_ARRAY_SIZE;
}

void
meta_devices_list_destroy (MetaDevList *list)
{
  g_free (list->devices);
}

gboolean
meta_devices_list_add (MetaDevList *list, MetaDevInfo *dev)
{
  int i;

  for (i = 0; i < list->used; i++)
    if (list->devices[i]->xdev->device_id == dev->xdev->device_id)
      return FALSE;

  if (list->used == list->size)
    {
      list->devices = g_renew(MetaDevInfo*,
    			      list->devices,
			      list->size + DEFAULT_INPUT_ARRAY_SIZE);
      list->size += DEFAULT_INPUT_ARRAY_SIZE;
    }


  list->devices[list->used] = dev;
  list->used++;

  return TRUE;
}

gboolean
meta_devices_list_remove (MetaDevList *list, MetaDevInfo *dev)
{
  int i, j;

  for (i = 0; i < list->used; i++)
    if (list->devices[i]->xdev->device_id == dev->xdev->device_id)
      {
	for (j = i; j < (list->used -1); j++)
	  list->devices[j] = list->devices[j+1];
	list->used--;
	return TRUE;
      }

  return FALSE;
}

gboolean
meta_devices_list_is_member (MetaDevList *list, MetaDevInfo *dev)
{
  int i;

  for (i = 0; i < list->used; i++)
    if (list->devices[i]->xdev->device_id == dev->xdev->device_id)
      return TRUE;

  return FALSE;
}

#endif
