/*
 * Copyright (C) 2001 Havoc Pennington
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

#ifndef META_BUTTON_LAYOUT_PRIVATE_H
#define META_BUTTON_LAYOUT_PRIVATE_H

#include "meta-button-enums.h"

G_BEGIN_DECLS

typedef struct
{
  /* buttons in the group on the left side */
  MetaButtonType  left_buttons[META_BUTTON_TYPE_LAST];
  gboolean        left_buttons_has_spacer[META_BUTTON_TYPE_LAST];
  gint            n_left_buttons;

  /* buttons in the group on the right side */
  MetaButtonType  right_buttons[META_BUTTON_TYPE_LAST];
  gboolean        right_buttons_has_spacer[META_BUTTON_TYPE_LAST];
  gint            n_right_buttons;

  MetaButtonState button_states[META_BUTTON_TYPE_LAST];
} MetaButtonLayout;

MetaButtonLayout *meta_button_layout_new  (const gchar      *str,
                                           gboolean          invert);

void              meta_button_layout_free (MetaButtonLayout *layout);

G_END_DECLS

#endif
