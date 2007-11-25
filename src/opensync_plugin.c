/* OpenSync plugin for Claws-Mail
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
#include "gettext.h"

#include "plugin.h"
#include "version.h"

#include <glib.h>

gint plugin_init(gchar **error)
{
	bindtextdomain(TEXTDOMAIN, LOCALEDIR);
	bind_textdomain_codeset(TEXTDOMAIN, "UTF-8");

	/* Version check */
	if(!check_plugin_version(MAKE_NUMERIC_VERSION(2,9,2,72),
	                         VERSION_NUMERIC, _("OpenSync"), error))
		return -1;

	opensync_init();

	return 0;
}

gboolean plugin_done(void)
{
  opensync_done();
  return TRUE;
}

const gchar *plugin_name(void)
{
	return _("OpenSync");
}

const gchar *plugin_desc(void)
{
	return _("This plugin offers an interface to "
	         "OpenSync. It does nothing user-visible "
	         "by itself, but has to be loaded in order to "
	         "use the Claws-Mail plugin for OpenSync.\nFeedback "
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
