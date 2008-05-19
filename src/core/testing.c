/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* 
 * Copyright (C) 2008 Thomas Thurman
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

/**
 * \file testing.c   The window manager's part of the test subsystem
 */

#include "config.h"

#ifdef USING_TESTING

#include "testing.h"
#include <glib.h>

static GSList *handlers = NULL;

void
meta_testing_register (MetaTestingHandler *handler)
{
  handlers = g_slist_prepend (handlers, handler);
}

char *
meta_testing_notify (char type, char *details)
{
  /*
   * We could be all efficient and have some way of letting the registration
   * function specify which types you're interested in, and then only notifying
   * the relevant handlers, but really the tiny amount of extra efficiency
   * isn't worth the extra complexity of the code for something that's run
   * so rarely.
   */
  
  GSList *cursor = handlers;

  while (cursor)
    {
      char *possible_result;

      possible_result = (*((MetaTestingHandler*)cursor->data)) (type, details);

      if (possible_result)
        {
          return possible_result;
        }

      cursor = g_slist_next (cursor);
    }

  return NULL; /* Give up. */

}

#else /* USING_TESTING */

/* Nothing happens. */

#endif /* USING_TESTING */

/* eof testing.c */



