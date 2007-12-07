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
#include "addressbook.h"
#include "common/defs.h"

typedef struct
{
	PrefsPage page;
	GtkWidget *ask_add;
	GtkWidget *ask_delete;
	GtkWidget *ask_modify;
	GtkWidget *addrbook_choice_individual;
	GtkWidget *addrbook_choice_default;
	GtkWidget *addrbook_default_choice_cont;
	GtkWidget *addrbook_folderpath;
} OpenSyncPage;

OpenSyncPrefs opensync_config;
OpenSyncPage opensync_page;

PrefParam opensync_param[] =
{
	{ "ask_add", "TRUE", &opensync_config.ask_add, P_BOOL, NULL, NULL, NULL },
	{ "ask_delete", "TRUE", &opensync_config.ask_delete, P_BOOL, NULL, NULL,
		NULL },
	{ "ask_modify", "TRUE", &opensync_config.ask_modify, P_BOOL, NULL, NULL,
		NULL },
	{	"addrbook_choice", "0", &opensync_config.addrbook_choice, P_INT, NULL, NULL, NULL},
	{	"addrbook_folderpath", "", &opensync_config.addrbook_folderpath, P_STRING,
		NULL, NULL, NULL},

	{ NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL }
};

static void opensync_create_prefs_page(PrefsPage*, GtkWindow*, gpointer);
static void opensync_destroy_prefs_page(PrefsPage*);
static void opensync_save_prefs(PrefsPage*);

static void select_default_addressbook_clicked(void);
static void radio_addressbook_choice_toggle(GtkToggleButton*,gpointer);

void opensync_gtk_init(void)
{
	static gchar *path[3];

	path[0] = _("Plugins");
	path[1] = _("OpenSync");
	path[2] = NULL;

	opensync_page.page.path = path;
	opensync_page.page.create_widget = opensync_create_prefs_page;
	opensync_page.page.destroy_widget = opensync_destroy_prefs_page;
	opensync_page.page.save_page = opensync_save_prefs;
	prefs_gtk_register_page((PrefsPage*) &opensync_page);
}

void opensync_gtk_done(void)
{
	if (claws_is_exiting())
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
	if (!pfile || (prefs_set_block_label(pfile, "OpenSyncPlugin") < 0))
		return;

	if (prefs_write_param(opensync_param, pfile->fp) < 0) {
		debug_print("failed!\n");
		g_warning(_("\nOpenSync plugin: Failed to write plugin configuration "
						"to file\n"));
		prefs_file_close_revert(pfile);
		return;
	}
	if (fprintf(pfile->fp, "\n") < 0) {
		FILE_OP_ERROR(rcpath, "fprintf");
		prefs_file_close_revert(pfile);
	}
	else
		prefs_file_close(pfile);
	debug_print("done.\n");
}

static void opensync_create_prefs_page(PrefsPage *page, GtkWindow *window,
		gpointer data)
{
	GtkWidget *pvbox;
	GtkWidget *vbox;
	GtkWidget *frame;
	GtkWidget *checkbox;
	GtkWidget *radio;
	GtkWidget *hbox;
	GtkWidget *hbox2;
	GtkWidget *entry;
	GtkWidget *button;

	/* Page vbox */
	pvbox = gtk_vbox_new(FALSE, 0);

	/* Frame */
	frame = gtk_frame_new(_("Ask before modifying the addressbook"));
	gtk_container_set_border_width(GTK_CONTAINER(frame), 10);
	gtk_box_pack_start(GTK_BOX(pvbox), frame, FALSE, FALSE, 0);

	/* Frame vbox */
	vbox = gtk_vbox_new(FALSE, 4);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

	/* Adding contacts */
	checkbox = gtk_check_button_new_with_label(_("Ask before adding contacts"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox),
	opensync_config.ask_add);
	gtk_box_pack_start(GTK_BOX(vbox), checkbox, FALSE, FALSE, 0);
	opensync_page.ask_add = checkbox;

	/* Deleting contacts */
	checkbox = gtk_check_button_new_with_label(_("Ask before deleting contacts"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox),
	opensync_config.ask_delete);
	gtk_box_pack_start(GTK_BOX(vbox), checkbox, FALSE, FALSE, 0);
	opensync_page.ask_delete = checkbox;

	/* Modifying contacts */
	checkbox = gtk_check_button_new_with_label(_("Ask before modifying contacts"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox),
	opensync_config.ask_modify);
	gtk_box_pack_start(GTK_BOX(vbox), checkbox, FALSE, FALSE, 0);
	opensync_page.ask_modify = checkbox;

	/* Frame */
	frame = gtk_frame_new(_("Adding of contacts to Claws Mail's address book"));
	gtk_container_set_border_width(GTK_CONTAINER(frame), 10);
	gtk_box_pack_start(GTK_BOX(pvbox), frame, FALSE, FALSE, 0);

	/* Frame vbox */
	vbox = gtk_vbox_new(FALSE, 4);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

	/* Choice of addressbook */
	radio = gtk_radio_button_new_with_label(NULL, _("Let me choose for each "
			"contact individually"));
  g_signal_connect(G_OBJECT(radio), "toggled",
									 G_CALLBACK(radio_addressbook_choice_toggle),
									 GINT_TO_POINTER(OPENSYNC_ADDRESS_BOOK_INDIVIDUAL));
	gtk_box_pack_start(GTK_BOX(vbox), radio, FALSE, FALSE, 0);
	if(opensync_config.addrbook_choice == OPENSYNC_ADDRESS_BOOK_INDIVIDUAL)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), TRUE);
	opensync_page.addrbook_choice_individual = radio;

	hbox = gtk_hbox_new(FALSE, 20);

	radio = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio),
	_("Use default folder:"));
  g_signal_connect(G_OBJECT(radio), "toggled",
									 G_CALLBACK(radio_addressbook_choice_toggle),
									 GINT_TO_POINTER(OPENSYNC_ADDRESS_BOOK_DEFAULT));
	gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0);
	if(opensync_config.addrbook_choice == OPENSYNC_ADDRESS_BOOK_DEFAULT)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), TRUE);
	opensync_page.addrbook_choice_default = radio;

	hbox2 = gtk_hbox_new(FALSE, 20);
	opensync_page.addrbook_default_choice_cont = hbox2;
	entry = gtk_entry_new();
	if(opensync_config.addrbook_folderpath)
		gtk_entry_set_text(GTK_ENTRY(entry), opensync_config.addrbook_folderpath);
	gtk_box_pack_start(GTK_BOX(hbox2), entry, FALSE, FALSE, 0);
	opensync_page.addrbook_folderpath = entry;
	button = gtk_button_new_with_label(_("Select ..."));
  g_signal_connect(G_OBJECT(button), "clicked",
									 G_CALLBACK(select_default_addressbook_clicked), NULL);
	gtk_box_pack_start(GTK_BOX(hbox2), button, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), hbox2, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	/* sensitivity */
	radio_addressbook_choice_toggle(GTK_TOGGLE_BUTTON(radio),
																	GINT_TO_POINTER(OPENSYNC_ADDRESS_BOOK_DEFAULT));

	/* Done. */
	gtk_widget_show_all(pvbox);
	page->widget = pvbox;
}

static void opensync_destroy_prefs_page(PrefsPage *page)
{
}

static void opensync_save_prefs(PrefsPage *page)
{
	const gchar *tmp_str;

	opensync_config.ask_add =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(opensync_page.ask_add));
	opensync_config.ask_delete =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(opensync_page.ask_delete));
	opensync_config.ask_modify =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(opensync_page.ask_modify));

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
																	(opensync_page.addrbook_choice_individual)))
		opensync_config.addrbook_choice = OPENSYNC_ADDRESS_BOOK_INDIVIDUAL;
	else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
																	(opensync_page.addrbook_choice_default)))
		opensync_config.addrbook_choice = OPENSYNC_ADDRESS_BOOK_DEFAULT;

	tmp_str = gtk_entry_get_text(GTK_ENTRY(opensync_page.addrbook_folderpath));
	g_free(opensync_config.addrbook_folderpath);
	opensync_config.addrbook_folderpath = g_strdup(tmp_str);
}

static void select_default_addressbook_clicked(void)
{
  const gchar *folderpath;
  gchar *new_path;

  folderpath = gtk_entry_get_text(GTK_ENTRY(opensync_page.addrbook_folderpath));
	new_path = addressbook_folder_selection(folderpath);
	if(new_path) {
		gtk_entry_set_text(GTK_ENTRY(opensync_page.addrbook_folderpath), folderpath);
		g_free(new_path);
	}
}

/* This is just for sensitivity to stay conform with canceling the dialog */
static void radio_addressbook_choice_toggle(GtkToggleButton *togglebutton,
																						gpointer         user_data)
{
	gint type;
	
	type = GPOINTER_TO_INT(user_data);
	if(type == OPENSYNC_ADDRESS_BOOK_DEFAULT) {
		gtk_widget_set_sensitive(opensync_page.addrbook_default_choice_cont,
														 gtk_toggle_button_get_active(togglebutton));
	}
}
