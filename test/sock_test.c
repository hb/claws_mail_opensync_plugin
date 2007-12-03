/* Claws-Mail plugin for OpenSync
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

#include "pluginconfig.h"

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <glib.h>

#define BUFFSIZE 8192

static char*    opensync_get_socket_name(void);
static int      sock_write_all(int, const char*, int);
static gboolean sock_send(int fd, char *msg);
static void     sock_eval_answer(int);
static void     sock_eval_contact(int);

static gchar*   answer_check_val(const gchar*,gchar*);

static int uxsock = -1;

static gchar* answer_check_val(const gchar *prefix, gchar *msg)
{
	gchar *retVal;
	if(!g_str_has_prefix(msg,prefix))
		retVal =  NULL;
	else {
		retVal = msg;
		retVal += strlen(prefix);
	}
	return retVal;
}

static gboolean sock_send(int fd, char *msg)
{
	int bytes_to_write, bytes_written;

	bytes_to_write = strlen(msg);
	bytes_written = sock_write_all(fd, msg, bytes_to_write);
	if(bytes_written != bytes_to_write) {
		g_print("could not write all bytes to socket\n");
		return FALSE;
	}
	/*g_print("Successfully sent: %s", msg);*/
	return TRUE;
}

static char* sock_answer_get_next_line(int fd)
{
	int n;
	static char buf[BUFFSIZE];
	char *newline, *bp, *nl;
	int len;

	len = BUFFSIZE;
	bp = buf;
	do {
		if((n = recv(fd, bp, len, MSG_PEEK)) <= 0)
			return NULL;
		else {
			if((newline = memchr(bp, '\n', n)) != NULL)
				n = newline - bp + 1;
			if((n = read(fd, bp, n)) < 0)
				return NULL;
			bp += n;
			len -= n;
		}
	} while(!newline && len);
	nl = strchr(buf,'\n');
	if(nl)
		*nl = '\0';
	return buf;
}

static void sock_eval_contact(int fd)
{
	gchar *line;
	gchar *val;

	gboolean done = FALSE;
	do {
		line = sock_answer_get_next_line(fd);
		if(line) {
			if(g_str_has_prefix(line, ":end_contact:")) {
				g_print("complete contact received\n");
				done = TRUE;
			}
		}
	} while(!done);
}

static void sock_eval_answer(int fd)
{
	char *line;
	gboolean complete = FALSE;
	do {
		line = sock_answer_get_next_line(fd);
		if(line) {
			//g_print("Received line %s\n", line);
			if(g_str_has_prefix(line,":done:")) {
				complete = TRUE;
					g_print("Claws Mail is done\n");
			} else if(g_str_has_prefix(line,":ok:")) {
				complete = TRUE;
				g_print("Claws Mail reported success\n");
			}
			else if(g_str_has_prefix(line,":failure:")) {
				complete = TRUE;
				g_print("Claws Mail reported failure\n");
			}
			else if(g_str_has_prefix(line, ":start_contact:")) {
				sock_eval_contact(fd);
			}
		}
	} while(!complete);
}

static int sock_write_all(int fd, const char *buf, int len)
{
	int n, wrlen = 0;

	while(len) {
		n = write(fd, buf, len);
		if (n <= 0) {
			g_print("Error writing on fd%d: %s\n", fd, strerror(errno));
			return -1;
		}
		len -= n;
		wrlen += n;
		buf += n;
	}
	return wrlen;
}

static char* opensync_get_socket_name(void)
{
	static char *filename = NULL;

	if(filename == NULL) {
		filename = g_strdup_printf("%s%cclaws-mail-opensync-%d",
		                           g_get_tmp_dir(), G_DIR_SEPARATOR,
#if HAVE_GETUID
		                           getuid()
#else
		                           0
#endif
		                          );
	}

	return filename;
}

static gint connect_unix_socket(void)
{
	gint uxsock;
	gchar *path;
	struct sockaddr_un addr;

	path = opensync_get_socket_name();

	uxsock = socket(PF_UNIX, SOCK_STREAM, 0);
	if(uxsock < 0)
		g_print("Could not create unix domain socket.\n");

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	if(connect(uxsock,(struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(uxsock);
		g_print("Could not connect to unix socket. Check that Claws-Mail "
		        "is running and the OpenSync plugin is loaded.\n");
		return -1;
	}

	g_print("successful connection to socket\n");
	return uxsock;
}

int main(void)
{
	int bytes_to_write, bytes_written;
	char *msg;

	uxsock = connect_unix_socket();
	if(uxsock < 0) {
		g_print("bailing out\n");
		return 1;
	}

	if(sock_send(uxsock, ":request_contacts:\n"))
		sock_eval_answer(uxsock);

	sock_send(uxsock, ":modify_contact:\n");
	sock_send(uxsock, "194022980\n");
	sock_send(uxsock, "BEGIN:VCARD\nVERSION:2.1\nN:Mustermann;Hans\nADR;TYPE=home:;;Musterstraße 1;Musterstadt;;12345;Deutschland\nTEL;HOME;VOICE:+49 1234 56788\nTEL;TYPE=CELL:+49 1234 56789\nTEL;HOME;FAX:+49 1234 12345\nEND:VCARD\n");
	sock_send(uxsock, ":done:\n");
	sock_eval_answer(uxsock);

	sock_send(uxsock, ":delete_contact:\n");
	sock_send(uxsock, "194022980\n");
	sock_eval_answer(uxsock);

	sock_send(uxsock, ":finished:\n");

	close(uxsock);
	return 0;
}
