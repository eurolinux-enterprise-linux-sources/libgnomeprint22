#ifndef __GNOME_PRINT_FILTER_REVERSE_H__
#define __GNOME_PRINT_FILTER_REVERSE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GNOME_TYPE_PRINT_FILTER_REVERSE (gnome_print_filter_reverse_get_type ())
#define GNOME_PRINT_FILTER_REVERSE(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GNOME_TYPE_PRINT_FILTER_REVERSE, GnomePrintFilterReverse))
#define GNOME_PRINT_FILTER_REVERSE_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), GNOME_TYPE_PRINT_FILTER_REVERSE, GnomePrintFilterReverseClass))
#define GNOME_PRINT_IS_FILTER_REVERSE(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GNOME_TYPE_PRINT_FILTER_REVERSE))
#define GNOME_PRINT_IS_FILTER_REVERSE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GNOME_TYPE_PRINT_FILTER_REVERSE))
#define GNOME_PRINT_FILTER_REVERSE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GNOME_TYPE_PRINT_FILTER_REVERSE, GnomePrintFilterReverseClass))
 
typedef struct _GnomePrintFilterReverse GnomePrintFilterReverse;
typedef struct _GnomePrintFilterReverseClass GnomePrintFilterReverseClass;

GType gnome_print_filter_reverse_get_type (void);

G_END_DECLS

#endif /* __GNOME_PRINT_FILTER_REVERSE_H__ */
