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
 * \file testing.h   The window manager's part of the test subsystem
 */

#ifndef META_TESTING_H
#define META_TESTING_H

#ifdef USING_TESTING

/**
 * A handler for a certain kind of testing request.  See the testing
 * documentation in the "doc" directory for more details on the format
 * of the requests and responses here.
 *
 * \param type     The type of request.
 * \param details  Details of the request (interpretation depends on the type)
 * \return  A string to be returned to the client.  If this handler
 *          does not wish to handle this request, it should return NULL;
 *          if it returns a non-NULL value, processing of this request
 *          will cease immediately; no other handlers will be called.
 */
typedef char* (* MetaTestingHandler) (char type, char *details);

/**
 * Registers a handler for some kind of testing request.  This handler
 * will be called by meta_testing_notify on receipt of __METACITY_TESTING.
 *
 * \param handler  The handler.
 */
void meta_testing_register (MetaTestingHandler handler);

/**
 * After a __METACITY_TESTING property has been set, this function runs
 * through all the handlers which have been registered, looking for one
 * which can deal with that particular property.
 *
 * \param type     The type of request.
 * \param details  Details of the request (interpretation depends on the type)
 * \return  A string to be returned to the client.  (Note that if the
 *          client is to see "A=1234", the string returned should be "1234";
 *          the caller adds the "A=".)  It is the caller's responsibility to
 *          g_free the string.  If no other string can be found, returns NULL.
 */
char* meta_testing_notify (char type, char *details);

#endif /* USING_TESTING */

#endif /* META_TESTING_H */

/* eof testing.h */
