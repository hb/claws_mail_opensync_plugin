bin_PROGRAMS = sock_test

sock_test_SOURCES = \
	sock_test.c
	
sock_test_LDFLAGS = \
	-avoid-version -module \
	$(GLIB_LIBS)

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-DLOCALEDIR=\""$(localedir)"\" 