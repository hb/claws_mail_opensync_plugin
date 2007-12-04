/* OpenSync plugin for Claws Mail
 * Copyright (C) 2007 Holger Berndt
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPENSYNC_PREFS_H
#define OPENSYNC_PREFS_H OPENSYNC_PREFS_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif
#include "pluginconfig.h"

#include "prefs_gtk.h"

typedef struct {
} OpenSyncPrefs;

extern OpenSyncPrefs opensync_config;
extern PrefParam     opensync_param[];

void opensync_gtk_init(void);
void opensync_gtk_done(void);
void opensync_save_config(void);

#endif /* OPENSYNC_PREFS_H */
