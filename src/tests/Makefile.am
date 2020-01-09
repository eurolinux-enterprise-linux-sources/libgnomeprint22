SUBDIRS = files output tools

AM_CFLAGS=$(WARN_CFLAGS)

noinst_PROGRAMS = generate gpa-test fonts simple pango test-print test-filter test-meta

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgnomeprint/gpa \
	$(LIBGNOMEPRINT_CFLAGS) \
	$(LIBART_CFLAGS)

print_libs = \
	$(top_builddir)/libgnomeprint/libgnomeprint-2-2.la \
	$(LIBGNOMEPRINT_LIBS)

generate_SOURCES = generate.c
generate_LDADD = $(print_libs)

test_meta_LDADD = $(print_libs)

test_filter_SOURCES = test-filter.c test-common.c test-common.h
test_filter_LDADD = $(print_libs)

simple_SOURCES = simple.c
simple_LDADD = $(print_libs)

pango_SOURCES = pango.c
pango_LDADD = $(print_libs)

EXTRA_DIST = quit.txt run-test.pl quit.txt

gpa_test_SOURCES = gpa-test.c
gpa_test_DEPENDENCIES = $(DEPS)
gpa_test_LDADD = $(print_libs)

fonts_SOURCES = fonts.c
fonts_LDADD = $(print_libs)

test_print_SOURCES = test-print.c test-common.c test-common.h
test_print_LDADD = $(print_libs)
