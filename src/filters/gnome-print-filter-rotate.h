#ifndef __GNOME_PRINT_FILTER_ROTATE_H__
#define __GNOME_PRINT_FILTER_ROTATE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GNOME_TYPE_PRINT_FILTER_ROTATE (gnome_print_filter_rotate_get_type ())
#define GNOME_PRINT_FILTER_ROTATE(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GNOME_TYPE_PRINT_FILTER_ROTATE, GnomePrintFilterRotate))
#define GNOME_PRINT_FILTER_ROTATE_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), GNOME_TYPE_PRINT_FILTER_ROTATE, GnomePrintFilterRotateClass))
#define GNOME_PRINT_IS_FILTER_ROTATE(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GNOME_TYPE_PRINT_FILTER_ROTATE))
#define GNOME_PRINT_IS_FILTER_ROTATE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GNOME_TYPE_PRINT_FILTER_ROTATE))
#define GNOME_PRINT_FILTER_ROTATE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GNOME_TYPE_PRINT_FILTER_ROTATE, GnomePrintFilterRotateClass))
 
typedef struct _GnomePrintFilterRotate GnomePrintFilterRotate;
typedef struct _GnomePrintFilterRotateClass GnomePrintFilterRotateClass;

GType gnome_print_filter_rotate_get_type (void);

G_END_DECLS

#endif /* __GNOME_PRINT_FILTER_ROTATE_H__ */
