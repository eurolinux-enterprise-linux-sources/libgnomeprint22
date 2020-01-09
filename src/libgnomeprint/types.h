#ifndef __GNOME_PRINT_FILTER_PARSE_TYPES_H__
#define __GNOME_PRINT_FILTER_PARSE_TYPES_H__

#include <libgnomeprint/gnome-print-filter.h>

typedef struct {
	GPtrArray *a;
} pool_t;

typedef struct _graph_t graph_t;
struct _graph_t {
	GnomePrintFilter *filter;
	GError *error;
};

static inline void
gnome_print_filter_parse_unescape (gchar *str)
{
	gchar *walk = str;

	g_return_if_fail (walk);

	while (*walk) {
		if (*walk == '\\')
			walk++;
		*str = *walk;
		str++;
		walk++;
	}
	*str = '\0';
}

#endif /* __GNOME_PRINT_FILTER_PARSE_TYPES_H__ */
