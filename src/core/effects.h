/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001 Anders Carlsson, Havoc Pennington
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

#ifndef META_EFFECTS_H
#define META_EFFECTS_H

#include "screen-private.h"

/**
 * Performs the minimize effect.
 *
 * \param window       The window we're moving
 * \param window_rect  Its current state
 * \param target       Where it should end up
 */
void meta_effect_run_minimize (MetaWindow    *window,
                               MetaRectangle *window_rect,
                               MetaRectangle *target);

#endif /* META_EFFECTS_H */
