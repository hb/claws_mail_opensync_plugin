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

#ifdef HAVE_GETUID
# include <unistd.h>
# include <sys/types.h>
#endif

#include <string.h>
#include <errno.h>

#include "vformat.h"

#define BUFFSIZE 8192

typedef struct
{
	ItemPerson *person;
	AddressDataSource *ds;
} ContactHashVal;

static gchar* vcard_get_from_ItemPerson(ItemPerson*);
static void update_ItemPerson_from_vcard(ItemPerson*, gchar*);

static gchar* opensync_get_socket_name(void);
static gint create_unix_socket(void);
static gint uxsock_remove(void);
static gboolean listen_channel_input_cb(GIOChannel*, GIOCondition, gpointer);

static void received_contacts_request(gint);
static void received_finished_notification(gint);
static void received_contact_modify_request(gint);
static void received_contact_delete_request(gint);

static gboolean sock_send(int, char*);

static gint addrbook_entry_send(ItemPerson*, const gchar*);

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
	addrindex_load_person_attribute(NULL, addrbook_entry_send);
	sock_send(answer_sock, ":done:\n");
	g_print("Sending of contacts done\n");
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
			while(!done) {
				if(fd_gets(answer_sock, buf, sizeof(buf)) == -1) {
					g_print("error receiving contact to modify\n");
					break;
				}
				if(g_str_has_prefix(buf,":done:"))
				done = TRUE;
				else {
					tmp = vcard;
					vcard = g_strconcat(tmp,buf,NULL);
					g_free(tmp);
				}
			}
			update_ItemPerson_from_vcard(hash_val->person, vcard);
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
	gboolean delete_successful = FALSE;

	if (fd_gets(fd, buf, sizeof(buf)) != -1) {
		gchar *id;
		ContactHashVal *hash_val;
		id = g_strchomp(buf);
		hash_val = g_hash_table_lookup(contact_hash, id);

		if (hash_val) {
			g_print("about to delete id: '%s'\n", id);
			delete_successful = TRUE;
		}
	}
	if(delete_successful)
	sock_send(fd, ":ok:\n");
	else
	sock_send(fd, ":failure:\n");
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
	return 0;
}

static gint create_unix_socket(void)
{
	gint uxsock;
	gchar *path;

	path = opensync_get_socket_name();
	uxsock = fd_connect_unix(path);

	if (uxsock < 0) {
		g_unlink(path);
		return fd_open_unix(path);
	}

	/* File already exists */
	g_unlink(path);
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

static gint addrbook_entry_send(ItemPerson *itemperson, const gchar *book)
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
	// TODO: Save AddressDataSource
	g_hash_table_insert(contact_hash, g_strdup(ADDRITEM_ID(itemperson)), val);

	return 0;
}

/* TODO: Make this more complete */
static gchar* vcard_get_from_ItemPerson(ItemPerson *item)
{
	VFormat *vformat;
	gchar *vcard;
	VFormatAttribute *attr;

	vformat = vformat_new();

	/* UID */
	attr = vformat_attribute_new(NULL,"UID");
	vformat_add_attribute_with_value(vformat, attr, ADDRITEM_ID(item));

	/* Name */
	attr = vformat_attribute_new(NULL,"N");
	vformat_add_attribute_with_values(vformat, attr,
			item->lastName ? item->lastName : "", item->firstName ? item->firstName
					: "",
			NULL);

	vcard = vformat_to_string(vformat, VFORMAT_CARD_21);
	vformat_free(vformat);

	return vcard;
}

/* TODO: Make this more complete */
static void update_ItemPerson_from_vcard(ItemPerson *item, gchar *vcard)
{
	VFormat *vformat;
	GList *attr_list, *walk;

	g_print("Update ItemPerson from vcard:\n");

	vformat = vformat_new_from_string(vcard);

	attr_list = vformat_get_attributes(vformat);
	for (walk = attr_list; walk; walk = walk->next) {
		VFormatAttribute *attr;
		const char *attr_name;

		attr = walk->data;
		attr_name = vformat_attribute_get_name(attr);

		/* UID */
		if (!strcmp(attr_name, "UID")) {
			if (!vformat_attribute_is_single_valued(attr))
				g_print("Error: UID is supposed to be single valued\n");
			else {
				g_print(" UID: '%s'\n", vformat_attribute_get_value(attr));
			}
		}

		/* Name */
		else if (!strcmp(attr_name, "N")) {
			g_print(" Last name: '%s'\n", vformat_attribute_get_nth_value(attr, 0));
			g_print(" First name: '%s'\n", vformat_attribute_get_nth_value(attr, 1));
		}

	} /* for all attributes */
}
