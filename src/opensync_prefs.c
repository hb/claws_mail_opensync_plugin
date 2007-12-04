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

#include "opensync_prefs.h"

#include "gettext.h"

#include "prefs.h"
#include "main.h"
#include "common/defs.h"


typedef struct {
  PrefsPage page;
} OpenSyncPage;

OpenSyncPrefs opensync_config;
OpenSyncPage  opensync_page;

PrefParam opensync_param[] = {
  {NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

static void opensync_create_prefs_page(PrefsPage*, GtkWindow*, gpointer);
static void opensync_destroy_prefs_page(PrefsPage*);
static void opensync_save_prefs(PrefsPage*);

void opensync_gtk_init(void)
{
  static gchar *path[3];

  path[0] = _("Plugins");
  path[1] = _("OpenSync");
  path[2] = NULL;

  opensync_page.page.path = path;
  opensync_page.page.create_widget  = opensync_create_prefs_page;
  opensync_page.page.destroy_widget = opensync_destroy_prefs_page;
  opensync_page.page.save_page      = opensync_save_prefs;
  prefs_gtk_register_page((PrefsPage*) &opensync_page);
}

void opensync_gtk_done(void)
{
  if(claws_is_exiting())
    return;
  prefs_gtk_unregister_page((PrefsPage*) &opensync_page);
}

void opensync_save_config(void)
{
  PrefFile *pfile;
  gchar *rcpath;
  
  debug_print("Saving OpenSync plugin configuration...\n");

  rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
  pfile = prefs_write_open(rcpath);
  g_free(rcpath);
  if(!pfile || (prefs_set_block_label(pfile, "OpenSyncPlugin") < 0))
    return;
  
  if(prefs_write_param(opensync_param, pfile->fp) < 0) {
    debug_print("failed!\n");
    g_warning(_("\nOpenSync plugin: Failed to write plugin configuration "
		"to file\n"));
    prefs_file_close_revert(pfile);
    return;
  }
  if (fprintf(pfile->fp, "\n") < 0) {
    FILE_OP_ERROR(rcpath, "fprintf");
    prefs_file_close_revert(pfile);
  } else
    prefs_file_close(pfile);
  debug_print("done.\n");
}

static void opensync_create_prefs_page(PrefsPage *page, GtkWindow *window,
				       gpointer data)
{
  GtkWidget *pvbox;
  
  /* Page vbox */
  pvbox = gtk_vbox_new(FALSE, 0);

  /* Done. */
  gtk_widget_show(pvbox);
  page->widget = pvbox;
}

static void opensync_destroy_prefs_page(PrefsPage *page)
{
}

static void opensync_save_prefs(PrefsPage *page)
{
}
