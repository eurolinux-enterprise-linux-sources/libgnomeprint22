plugindir = $(GNOME_PRINT_MODULES_DIR)

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgnomeprint/gpa \
	$(LIBGNOMEPRINT_CFLAGS) \
	-DG_LOG_DOMAIN=\"GnomePrintWin32Plugin\"

plugin_LTLIBRARIES = libgnomeprintwin32.la

libgnomeprintwin32_la_SOURCES = gnome-print-win32.c gnome-print-win32-transport.c
libgnomeprintwin32_la_LDFLAGS = $(TOOLS_LIBS) -avoid-version -module -no-undefined
libgnomeprintwin32_la_LIBADD  = ../../libgnomeprint-2-2.la -lwinspool
