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

#ifndef META_UI_PRIVATE_H
#define META_UI_PRIVATE_H

#include <libmetacity/meta-theme.h>
#include "ui.h"

G_BEGIN_DECLS

MetaTheme *meta_ui_get_theme     (MetaUI *ui);

gboolean   meta_ui_is_composited (MetaUI *ui);

G_END_DECLS

#endif
