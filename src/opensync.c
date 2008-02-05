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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif
#include "pluginconfig.h"

#include "gettext.h"

#include <glib.h>
#include <glib/gstdio.h>

#include "socket.h"
#include "addrindex.h"
#include "addritem.h"
#include "addrbook.h"
#include "addressbook.h"
#include "addrduplicates.h"
#include "alertpanel.h"

#ifdef HAVE_GETUID
# include <unistd.h>
# include <sys/types.h>
#endif

#include <string.h>
#include <errno.h>

#include "vformat.h"
#include "opensync_prefs.h"

#define BUFFSIZE 8192

typedef struct
{
	ItemPerson *person;
	AddressDataSource *ds;
} ContactHashVal;

static gchar* vcard_get_from_ItemPerson(ItemPerson*);
static void update_ItemPerson_from_vcard(AddressBookFile*, ItemPerson*, gchar*);

static char* sock_get_next_line(int);

static gchar* opensync_get_socket_name(void);
static gint create_unix_socket(void);
static gint uxsock_remove(void);
static gboolean listen_channel_input_cb(GIOChannel*, GIOCondition, gpointer);

static void received_contacts_request(gint);
static void received_finished_notification(gint);
static void received_contact_modify_request(gint);
static void received_contact_delete_request(gint);
static void received_contact_add_request(gint);
static gchar* get_next_contact(void);

static gboolean sock_send(int, char*);

static gint addrbook_entry_send(ItemPerson*, AddressDataSource *ds);

static GList* restore_or_add_email_address(AddressBookFile*, ItemPerson*,
																					 GList*, const gchar*);

static gint uxsock = -1;
static gint answer_sock = -1;
static GIOChannel *listen_channel= NULL;

static GHashTable *contact_hash= NULL;

void opensync_init(void)
{
	/* created unix socket to listen on */
	uxsock = create_unix_socket();
	if (uxsock < 0) {
		g_print("failed to create unix socket for opensync\n");
		return;
	}
	listen_channel = g_io_channel_unix_new(uxsock);
	g_io_add_watch(listen_channel, G_IO_IN, listen_channel_input_cb, NULL);
	g_io_channel_unref(listen_channel);
}

void opensync_done(void)
{
	GError *error= NULL;
	if (listen_channel) {
		g_io_channel_shutdown(listen_channel, TRUE, &error);
		if (error) {
			g_print("Error shutting down channel: %s\n", error->message);
			g_error_free(error);
		}
		g_io_channel_unref(listen_channel);
	}

	uxsock_remove();
}

static gboolean sock_send(int fd, char *msg)
{
	int bytes_to_write, bytes_written;

	bytes_to_write = strlen(msg);
	bytes_written = fd_write_all(fd, msg, bytes_to_write);
	if (bytes_written != bytes_to_write) {
		g_print("could not write all bytes to socket\n");
		return FALSE;
	}
	return TRUE;
}

static void received_finished_notification(gint answer_sock)
{
	/* cleanup */
	if (contact_hash) {
		g_hash_table_destroy(contact_hash);
		contact_hash = NULL;
	}
}

static void received_contacts_request(gint fd)
{
	g_print("Sending contacts\n");
	addrindex_load_person_ds(addrbook_entry_send);
	sock_send(answer_sock, ":done:\n");
	g_print("Sending of contacts done: %d\n",	g_hash_table_size(contact_hash));
}

static void received_contact_modify_request(gint fd)
{
	gchar buf[BUFFSIZE];

	if (fd_gets(fd, buf, sizeof(buf)) != -1) {
		gchar *id;
		ContactHashVal *hash_val;
		id = g_strchomp(buf);
		g_print("id to change: '%s'\n",id);
		hash_val = g_hash_table_lookup(contact_hash, id);
		if(hash_val) {
			gchar *vcard = g_strdup("");
			gchar *tmp;
			gboolean done = FALSE;
			AddressBookFile *abf;

			while(!done) {
				if(fd_gets(answer_sock, buf, sizeof(buf)) == -1) {
					g_print("error receiving contact to modify\n");
					break;
				}
				g_print("buf: %s\n",buf);
				if(g_str_has_prefix(buf,":done:"))
					done = TRUE;
				else {
					tmp = vcard;
					vcard = g_strconcat(tmp,buf,NULL);
					g_free(tmp);
				}
			}
			abf = hash_val->ds->rawDataSource;
			g_print("Modification to: '%s'\n",vcard);
			update_ItemPerson_from_vcard(abf, hash_val->person, vcard);
			sock_send(fd, ":ok:\n");
			g_free(vcard);
		}
		else {
			g_printf("warning: tried to modify non-existent contact\n");
			sock_send(fd, ":failure:\n");
		}
	}
}

static void received_contact_delete_request(gint fd)
{
	gchar buf[BUFFSIZE];
	gboolean delete_successful= FALSE;

	if (fd_gets(fd, buf, sizeof(buf)) != -1) {
		gchar *id;
		ContactHashVal *hash_val;
		id = g_strchomp(buf);
		hash_val = g_hash_table_lookup(contact_hash, id);

		if (hash_val) {
			AlertValue val;
			val = G_ALERTALTERNATE;
			if(opensync_config.contact_ask_delete) {
				gchar *msg;
				msg = g_strdup_printf(_("Really delete contact for '%s'?"),
															ADDRITEM_NAME(hash_val->person));
				val = alertpanel(_("OpenSync plugin"),msg,
												 GTK_STOCK_CANCEL,GTK_STOCK_DELETE,NULL);
				g_free(msg);
			}
			if(((!opensync_config.contact_ask_delete) || (val != G_ALERTDEFAULT)) &&
				 (addrduplicates_delete_item_person(hash_val->person,hash_val->ds))) {
				g_print("Deleted id: '%s'\n", id);
				delete_successful = TRUE;
			}
		}
	}
	if(delete_successful) {
	  sock_send(fd, ":ok:\n");
	}
	else {
	  sock_send(fd, ":failure:\n");
	}
}

static void received_contact_add_request(gint fd)
{
	gchar *vcard;
	gchar *msg;
	gboolean add_successful;
	ItemPerson *person;

	add_successful = FALSE;
	vcard = get_next_contact();

	if (vcard) {
		AlertValue val;
		val = G_ALERTALTERNATE;
		if (opensync_config.contact_ask_add) {
			msg = g_strdup_printf(_("Really add contact:\n%s?"),vcard);
			val = alertpanel(_("OpenSync plugin"),msg,
											 GTK_STOCK_CANCEL,GTK_STOCK_ADD,NULL);
			g_free(msg);
		}
		if (!opensync_config.contact_ask_add || (val != G_ALERTDEFAULT)) {
			gchar *path = NULL;
			AddressDataSource *book = NULL;
			ItemFolder *folder = NULL;
			AddressBookFile *abf = NULL;

			if(opensync_config.addrbook_choice == OPENSYNC_ADDRESS_BOOK_INDIVIDUAL)
				path = addressbook_folder_selection(NULL);
			if(!path)
				path = g_strdup(opensync_config.addrbook_folderpath);

			if (addressbook_peek_folder_exists(path, &book, &folder) && book) {
				abf = book->rawDataSource;
				person = addrbook_add_contact(abf, folder, "", "", "");
				person->status = ADD_ENTRY;
				update_ItemPerson_from_vcard(abf, person, vcard);
				add_successful = TRUE;
			}
			else
				g_warning("addressbook folder not found '%s'\n", path);
			g_free(path);
		}
		else {
			g_print("Error: User refused to add contact '%s'\n", vcard);
		}
	}
	else {
		g_print("Error: Not able to get the contact to add\n");
	}
	if(add_successful) {
		gchar *return_vcard;
		return_vcard = vcard_get_from_ItemPerson(person);
		
	  sock_send(fd, ":start_contact:\n");
	  msg = g_strdup_printf("%s\n", return_vcard);
		g_free(return_vcard);
	  sock_send(fd, msg);
	  g_free(msg);
	  sock_send(fd, ":end_contact:\n");	  
	}
	else {
	  sock_send(fd, ":failure:\n");
	}
	g_free(vcard);
}

static gboolean listen_channel_input_cb(GIOChannel *chan, GIOCondition cond,
																				gpointer data)
{
	gint sock;
	gchar buf[BUFFSIZE];

	if (answer_sock != -1)
		return TRUE;

	sock = g_io_channel_unix_get_fd(chan);
	answer_sock = fd_accept(sock);

	while (fd_gets(answer_sock, buf, sizeof(buf)) != -1) {
		g_print("Received request: %s", buf);
		if(g_str_has_prefix(buf,":request_contacts:"))
			received_contacts_request(answer_sock);
		else if(g_str_has_prefix(buf, ":modify_contact:"))
			received_contact_modify_request(answer_sock);
		else if(g_str_has_prefix(buf, ":delete_contact:"))
			received_contact_delete_request(answer_sock);
		else if(g_str_has_prefix(buf, ":add_contact:"))
			received_contact_add_request(answer_sock);
		else if(g_str_has_prefix(buf,":finished:")) {
			received_finished_notification(answer_sock);
			break;
		}
	}

	fd_close(answer_sock);
	answer_sock = -1;
	g_print("closed answer sock\n");

	return TRUE;
}

static gint uxsock_remove(void)
{
	gchar *filename;

	if (uxsock < 0)
		return -1;

	fd_close(uxsock);
	filename = opensync_get_socket_name();
	g_unlink(filename);
	g_free(filename);
	return 0;
}

static gint create_unix_socket(void)
{
	gint uxsock;
	gchar *path;

	path = opensync_get_socket_name();
	uxsock = fd_connect_unix(path);

	if (uxsock < 0) {
		gint retVal;
		g_unlink(path);
		retVal = fd_open_unix(path);
		g_free(path);
		return retVal;
	}

	/* File already exists */
	g_unlink(path);
	g_free(path);
	return -1;
}

static gchar* opensync_get_socket_name(void)
{
	static gchar *filename= NULL;

	if (filename == NULL) {
		filename = g_strdup_printf("%s%cclaws-mail-opensync-%d", g_get_tmp_dir(),
															 G_DIR_SEPARATOR,
#if HAVE_GETUID
															 getuid()
#else
															 0
#endif
															 );
	}

	return filename;
}

static gint addrbook_entry_send(ItemPerson *itemperson, AddressDataSource *ds)
{
	gchar *vcard;
	ContactHashVal *val;

	vcard = vcard_get_from_ItemPerson(itemperson);
	sock_send(answer_sock, ":start_contact:\n");
	sock_send(answer_sock, vcard);
	sock_send(answer_sock, ":end_contact:\n");
	g_free(vcard);

	/* Remember contacts for easier changing */
	if (!contact_hash)
		contact_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
																				 g_free);

	val = g_new0(ContactHashVal,1);
	val->person = itemperson;
	val->ds = ds;
	g_hash_table_insert(contact_hash, g_strdup(ADDRITEM_ID(itemperson)), val);

	return 0;
}

static gchar* vcard_get_from_ItemPerson(ItemPerson *item)
{
	VFormat *vformat;
	gchar *vcard;
	VFormatParam *param;
	VFormatAttribute *attr;
	GList *walk;

	vformat = vformat_new();

	/* UID */
	attr = vformat_attribute_new(NULL,"UID");
	vformat_add_attribute_with_value(vformat, attr, ADDRITEM_ID(item));

	/* Name */
	if(item->lastName || item->firstName) {
		attr = vformat_attribute_new(NULL,"N");
		vformat_add_attribute_with_values(vformat, attr,
																			item->lastName ? item->lastName : "",
																			item->firstName ? item->firstName
																			: "",
																			NULL);
	}

	/* Formatted name */
	if(ADDRITEM_NAME(item)) {
		attr = vformat_attribute_new(NULL,"FN");
		vformat_add_attribute_with_value(vformat, attr, ADDRITEM_NAME(item));
	}

	/* EMail addresses */
	for (walk = item->listEMail; walk; walk = walk->next) {
		gchar *email;
		email = ((ItemEMail*)walk->data)->address;
		attr = vformat_attribute_new(NULL,"EMAIL");
		param = vformat_attribute_param_new("INTERNET");
		vformat_attribute_add_param(attr, param);
		vformat_add_attribute_with_value(vformat, attr, email);
	}

	vcard = vformat_to_string(vformat, VFORMAT_CARD_21);
	vformat_free(vformat);

	return vcard;
}

static void update_ItemPerson_from_vcard(AddressBookFile *abf,
																				 ItemPerson *item, gchar *vcard)
{
	VFormat *vformat;
	GList *attr_list, *walk;
	gint numEmail;
	GList *emailWalk;
	GList *savedMailList;

	numEmail = 0;

	vformat = vformat_new_from_string(vcard);

	/* steal email list */
	savedMailList = NULL;
	for (emailWalk = item->listEMail; emailWalk; emailWalk = emailWalk->next) {
		ItemEMail *itemEMail;
		itemEMail = emailWalk->data;
		itemEMail = addritem_person_remove_email(item, itemEMail);
		if (itemEMail)
			savedMailList = g_list_append(savedMailList, itemEMail);
	}

	attr_list = vformat_get_attributes(vformat);
	for (walk = attr_list; walk; walk = walk->next) {
		VFormatAttribute *attr;
		const char *attr_name;

		attr = walk->data;
		attr_name = vformat_attribute_get_name(attr);

		/* We won't be treating the UID here. */

		/* Name */
		if (!strcmp(attr_name, "N")) {
			const gchar *first_name;
			const gchar *last_name;
			gchar *display_name;

			last_name = vformat_attribute_get_nth_value(attr,0);
			first_name = vformat_attribute_get_nth_value(attr, 1);

			if(last_name)
				addritem_person_set_last_name(item, last_name);
			if(first_name)
				addritem_person_set_first_name(item, first_name);

			if(last_name || first_name) {
				display_name = g_strdup_printf("%s %s",
																			 first_name?first_name:"",
																			 last_name?last_name:"");
				addritem_person_set_common_name(item, display_name);
				g_free(display_name);
			}
		}

		/* display name */
		else if (!strcmp(attr_name, "FN")) {
			const gchar *display_name;

			display_name = vformat_attribute_get_nth_value(attr,0);
			addritem_person_set_common_name(item, display_name?display_name:"");
		}

		/* Internet EMail addresses */
		else if (!strcmp(attr_name, "EMAIL")) {
			if (!vformat_attribute_is_single_valued(attr))
				g_print("Error: EMAIL is supposed to be single valued\n");
			else {
				GList *paramList, *paramWalk;
				numEmail++;
				paramList = vformat_attribute_get_params(attr);
				if (!paramList) {
					/* INTERNET is default */
					const gchar *email;
					email = vformat_attribute_get_nth_value(attr, 0);
					savedMailList = restore_or_add_email_address(abf, item, savedMailList,email);
				}
				else {
					for (paramWalk = paramList; paramWalk; paramWalk = paramWalk->next) {
						VFormatParam *param;
						param = paramWalk->data;
						if (param && param->name && param->values && param->values->data &&
								!strcmp(param->name, "TYPE") &&
								!strcmp((char*)param->values->data, "INTERNET")) {
							const gchar *email;
							email = vformat_attribute_get_nth_value(attr, 0);
							savedMailList = restore_or_add_email_address(abf, item, savedMailList,
																													 email);
						}
					}
				}
			}
		} /* INTERNET Email addresses */

	} /* for all attributes */

	/* if no mails were included, keep the old email list */
	if(numEmail > 0) {
		while(savedMailList) {
			ItemEMail *emailItem;
			emailItem = savedMailList->data;
			savedMailList = restore_or_add_email_address(abf, item, savedMailList,
																									 emailItem->address);
		}
	}

	/* savedMailList now contains the left-overs. Free it.
		 (if the sync went well, those are deleted entries) */
	for (emailWalk = savedMailList; emailWalk; emailWalk = emailWalk->next) {
		ItemEMail *itemEMail;
		itemEMail = emailWalk->data;
		addritem_free_item_email(itemEMail);
	}
	g_list_free(savedMailList);

	item->status = UPDATE_ENTRY;
	addrbook_set_dirty(abf,TRUE);
}

static gchar* get_next_contact(void)
{
	char *line;
	char *vcard;
	char *vcard_tmp;
	gboolean complete= FALSE;

	vcard = g_strdup("");
	while (!complete && ((line = sock_get_next_line(answer_sock)) != NULL)) {
		if (g_str_has_prefix(line, ":done:")) {
			g_free(vcard);
			vcard = NULL;
			break;
		}
		else if (g_str_has_prefix(line, ":start_contact:")) {
			continue;
		}
		else if (g_str_has_prefix(line, ":end_contact:")) {
			complete = TRUE;
			continue;
		}

		/* append line to vcard string */
		vcard_tmp = vcard;
		vcard = g_strconcat(vcard_tmp, line, NULL);
		g_free(vcard_tmp);
	};

	return vcard;
}

static char* sock_get_next_line(int fd)
{
	int n;
	static char buf[BUFFSIZE];
	char *newline, *bp, *nl;
	int len;

	len = BUFFSIZE-1;
	bp = buf;
	do {
		if ((n = recv(fd, bp, len, MSG_PEEK)) <= 0)
			return NULL;
		else {
			if ((newline = memchr(bp, '\n', n)) != NULL)
				n = newline - bp + 1;
			if ((n = read(fd, bp, n)) < 0)
				return NULL;
			bp += n;
			len -= n;
		}
	} while (!newline && len);
	nl = strchr(buf, '\n');
	if (nl)
		*(nl+1) = '\0';
	return buf;
}

static GList* restore_or_add_email_address(AddressBookFile *abf,
																					 ItemPerson *item, GList *savedList,
																					 const gchar *email)
{
	ItemEMail *itemMail = NULL;
	GList *walk;
	gboolean found;
	gchar *addr;

	if(!email)
		return savedList;

	found = FALSE;
	for (walk = savedList; walk; walk = walk->next) {
		itemMail = walk->data;
		if(itemMail && itemMail->address) {
			addr = g_strchomp(g_strdup(itemMail->address));
			if (!strcmp(addr, email))
				found = TRUE;
			g_free(addr);
			if (found)
				break;
		}
	}

	if (found) {
		addritem_person_add_email(item, itemMail);
		savedList = g_list_delete_link(savedList, walk);
	}
	/* something's fishy here */
	else if(1){
		ItemEMail *newEMail;
		newEMail = addritem_create_item_email();
		addritem_email_set_address(newEMail, email);
		addrcache_id_email(abf->addressCache, newEMail);
		addritem_person_add_email(item, newEMail);
		addrbook_set_dirty(abf,TRUE);
	}

	return savedList;
}
