#ifndef __GNOME_PRINT_FILTER_SELECT_H__
#define __GNOME_PRINT_FILTER_SELECT_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GNOME_TYPE_PRINT_FILTER_SELECT (gnome_print_filter_select_get_type ())
#define GNOME_PRINT_FILTER_SELECT(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GNOME_TYPE_PRINT_FILTER_SELECT, GnomePrintFilterSelect))
#define GNOME_PRINT_FILTER_SELECT_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), GNOME_TYPE_PRINT_FILTER_SELECT, GnomePrintFilterSelectClass))
#define GNOME_PRINT_IS_FILTER_SELECT(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GNOME_TYPE_PRINT_FILTER_SELECT))
#define GNOME_PRINT_IS_FILTER_SELECT_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GNOME_TYPE_PRINT_FILTER_SELECT))
#define GNOME_PRINT_FILTER_SELECT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GNOME_TYPE_PRINT_FILTER_SELECT, GnomePrintFilterSelectClass))
 
typedef struct _GnomePrintFilterSelect GnomePrintFilterSelect;
typedef struct _GnomePrintFilterSelectClass GnomePrintFilterSelectClass;

GType gnome_print_filter_select_get_type (void);

G_END_DECLS

#endif /* __GNOME_PRINT_FILTER_SELECT_H__ */
