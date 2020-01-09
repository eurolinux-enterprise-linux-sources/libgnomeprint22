INCLUDES = \
	-I$(top_srcdir) \
	$(LIBGNOMEPRINT_CFLAGS) $(WARN_CFLAGS) \
	-DGNOME_PRINT_MODULES_DIR="\"$(GNOME_PRINT_MODULES_DIR)\""

plugindir = $(GNOME_PRINT_MODULES_DIR)/filters
plugin_LTLIBRARIES =         \
	libgnomeprint-clip.la      \
	libgnomeprint-draft.la     \
	libgnomeprint-frgba.la     \
	libgnomeprint-multipage.la \
	libgnomeprint-position.la  \
	libgnomeprint-reorder.la   \
	libgnomeprint-reverse.la   \
	libgnomeprint-rotate.la    \
	libgnomeprint-select.la    \
	libgnomeprint-zoom.la

FILTER_LDFLAGS	= -avoid-version -module -no-undefined
FILTER_LIBS	= $(top_builddir)/libgnomeprint/libgnomeprint-2-2.la $(GP_F_LIBS)

libgnomeprint_clip_la_SOURCES		= gnome-print-filter-clip.c gnome-print-filter-clip.h
libgnomeprint_clip_la_LDFLAGS		= $(FILTER_LDFLAGS)
libgnomeprint_clip_la_LIBADD		= $(FILTER_LIBS)

libgnomeprint_draft_la_SOURCES		= gnome-print-filter-draft.c gnome-print-filter-draft.h
libgnomeprint_draft_la_LDFLAGS		= $(FILTER_LDFLAGS)
libgnomeprint_draft_la_LIBADD		= $(FILTER_LIBS)

libgnomeprint_frgba_la_SOURCES		= gnome-print-filter-frgba.c gnome-print-filter-frgba.h
libgnomeprint_frgba_la_LDFLAGS		= $(FILTER_LDFLAGS)
libgnomeprint_frgba_la_LIBADD		= $(FILTER_LIBS)

libgnomeprint_multipage_la_SOURCES	= gnome-print-filter-multipage.c gnome-print-filter-multipage.h
libgnomeprint_multipage_la_LDFLAGS	= $(FILTER_LDFLAGS)
libgnomeprint_multipage_la_LIBADD	= $(FILTER_LIBS)

libgnomeprint_position_la_SOURCES	= gnome-print-filter-position.c gnome-print-filter-position.h
libgnomeprint_position_la_LDFLAGS	= $(FILTER_LDFLAGS)
libgnomeprint_position_la_LIBADD	= $(FILTER_LIBS)

libgnomeprint_reorder_la_SOURCES	= gnome-print-filter-reorder.c gnome-print-filter-reorder.h
libgnomeprint_reorder_la_LDFLAGS	= $(FILTER_LDFLAGS)
libgnomeprint_reorder_la_LIBADD		= $(FILTER_LIBS)

libgnomeprint_reverse_la_SOURCES  = gnome-print-filter-reverse.c gnome-print-filter-reverse.h
libgnomeprint_reverse_la_LDFLAGS  = $(FILTER_LDFLAGS)
libgnomeprint_reverse_la_LIBADD   = $(FILTER_LIBS)

libgnomeprint_rotate_la_SOURCES		= gnome-print-filter-rotate.c gnome-print-filter-rotate.h
libgnomeprint_rotate_la_LDFLAGS		= $(FILTER_LDFLAGS)
libgnomeprint_rotate_la_LIBADD		= $(FILTER_LIBS)

libgnomeprint_select_la_SOURCES		= gnome-print-filter-select.c gnome-print-filter-select.h
libgnomeprint_select_la_LDFLAGS		= $(FILTER_LDFLAGS)
libgnomeprint_select_la_LIBADD		= $(FILTER_LIBS)

libgnomeprint_zoom_la_SOURCES		= gnome-print-filter-zoom.c gnome-print-filter-zoom.h
libgnomeprint_zoom_la_LDFLAGS		= $(FILTER_LDFLAGS)
libgnomeprint_zoom_la_LIBADD		= $(FILTER_LIBS)
