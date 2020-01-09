#ifndef __GNOME_PRINT_FILTER_REORDER_H__
#define __GNOME_PRINT_FILTER_REORDER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GNOME_TYPE_PRINT_FILTER_REORDER (gnome_print_filter_reorder_get_type ())
#define GNOME_PRINT_FILTER_REORDER(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GNOME_TYPE_PRINT_FILTER_REORDER, GnomePrintFilterReorder))
#define GNOME_PRINT_FILTER_REORDER_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), GNOME_TYPE_PRINT_FILTER_REORDER, GnomePrintFilterReorderClass))
#define GNOME_PRINT_IS_FILTER_REORDER(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GNOME_TYPE_PRINT_FILTER_REORDER))
#define GNOME_PRINT_IS_FILTER_REORDER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GNOME_TYPE_PRINT_FILTER_REORDER))
#define GNOME_PRINT_FILTER_REORDER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GNOME_TYPE_PRINT_FILTER_REORDER, GnomePrintFilterReorderClass))
 
typedef struct _GnomePrintFilterReorder GnomePrintFilterReorder;
typedef struct _GnomePrintFilterReorderClass GnomePrintFilterReorderClass;

GType gnome_print_filter_reorder_get_type (void);

G_END_DECLS

#endif /* __GNOME_PRINT_FILTER_REORDER_H__ */
