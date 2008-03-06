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

#include "opensync.h"
#include "opensync_prefs.h"

#include "gettext.h"

#include "plugin.h"
#include "version.h"
#include "common/defs.h"

#include <glib.h>

gint plugin_init(gchar **error)
{
  gchar *rcpath;

  bindtextdomain(TEXTDOMAIN, LOCALEDIR);
  bind_textdomain_codeset(TEXTDOMAIN, "UTF-8");
  
  /* Version check */
  if(!check_plugin_version(MAKE_NUMERIC_VERSION(3,1,0,62),
			   VERSION_NUMERIC, _("OpenSync"), error))
    return -1;
  
  /* Configuration */
  prefs_set_default(opensync_param);
  rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
  prefs_read_config(opensync_param, "OpenSyncPlugin", rcpath, NULL);
  g_free(rcpath);
  
  opensync_init();

  opensync_gtk_init();

  debug_print("OpenSync plugin loaded\n");
  return 0;
}

gboolean plugin_done(void)
{
  opensync_done();
  opensync_save_config();
  opensync_gtk_done();

  return TRUE;
}

const gchar *plugin_name(void)
{
	return _("OpenSync");
}

const gchar *plugin_desc(void)
{
	return _("This plugin offers an interface to "
	         "OpenSync for addressbook and "
					 "calendar synchronisation. The synchronisation "
					 "runs themselves have to be triggered with the "
					 "usual OpenSync tools.\n\nFeedback "
	         "to <berndth@gmx.de> is welcome.");
}

const gchar *plugin_type(void)
{
	return "GTK2";
}

const gchar *plugin_licence(void)
{
	return "GPL3+";
}

const gchar *plugin_version(void)
{
	return PLUGINVERSION;
}

struct PluginFeature *plugin_provides(void)
{
	static struct PluginFeature features[] =
	    {
		    {
			    PLUGIN_UTILITY, N_("OpenSync interface")
		    },
		    {PLUGIN_NOTHING, NULL}
	    };
	return features;
}
