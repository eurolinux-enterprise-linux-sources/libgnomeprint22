#ifndef __GNOME_PRINT_FILTER_POSITION_H__
#define __GNOME_PRINT_FILTER_POSITION_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GNOME_TYPE_PRINT_FILTER_POSITION (gnome_print_filter_position_get_type ())
#define GNOME_PRINT_FILTER_POSITION(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GNOME_TYPE_PRINT_FILTER_POSITION, GnomePrintFilterPosition))
#define GNOME_PRINT_FILTER_POSITION_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), GNOME_TYPE_PRINT_FILTER_POSITION, GnomePrintFilterPositionClass))
#define GNOME_PRINT_IS_FILTER_POSITION(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GNOME_TYPE_PRINT_FILTER_POSITION))
#define GNOME_PRINT_IS_FILTER_POSITION_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GNOME_TYPE_PRINT_FILTER_POSITION))
#define GNOME_PRINT_FILTER_POSITION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GNOME_TYPE_PRINT_FILTER_POSITION, GnomePrintFilterPositionClass))
 
typedef struct _GnomePrintFilterPosition GnomePrintFilterPosition;
typedef struct _GnomePrintFilterPositionClass GnomePrintFilterPositionClass;

GType gnome_print_filter_position_get_type (void);

G_END_DECLS

#endif /* __GNOME_PRINT_FILTER_POSITION_H__ */
