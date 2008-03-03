CFLAGS += "-Wall"

EXTRA_DIST = opensync.deps

plugindir = $(CLAWS_MAIL_PLUGINDIR)

plugin_DATA = opensync.deps

plugin_LTLIBRARIES = opensync.la


opensync_la_SOURCES = \
	opensync_plugin.c \
	opensync.c \
	vformat.c vformat.h \
	opensync_prefs.c opensync_prefs.h \
	gettext.h

opensync_la_LDFLAGS = \
	-avoid-version -module \
	$(GTK_LIBS)

AM_CPPFLAGS = \
	$(CLAWS_MAIL_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	-DLOCALEDIR=\""$(localedir)"\" 
