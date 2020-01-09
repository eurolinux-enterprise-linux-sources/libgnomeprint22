#ifndef __GNOME_PRINT_FILTER_FRGBA_H__
#define __GNOME_PRINT_FILTER_FRGBA_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GNOME_TYPE_PRINT_FILTER_FRGBA (gnome_print_filter_frgba_get_type ())
#define GNOME_PRINT_FILTER_FRGBA(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GNOME_TYPE_PRINT_FILTER_FRGBA, GnomePrintFilterFrgba))
#define GNOME_PRINT_FILTER_FRGBA_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), GNOME_TYPE_PRINT_FILTER_FRGBA, GnomePrintFilterFrgbaClass))
#define GNOME_PRINT_IS_FILTER_FRGBA(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GNOME_TYPE_PRINT_FILTER_FRGBA))
#define GNOME_PRINT_IS_FILTER_FRGBA_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GNOME_TYPE_PRINT_FILTER_FRGBA))
#define GNOME_PRINT_FILTER_FRGBA_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GNOME_TYPE_PRINT_FILTER_FRGBA, GnomePrintFilterFrgbaClass))
 
typedef struct _GnomePrintFilterFrgba GnomePrintFilterFrgba;
typedef struct _GnomePrintFilterFrgbaClass GnomePrintFilterFrgbaClass;

GType gnome_print_filter_frgba_get_type (void);

G_END_DECLS

#endif /* __GNOME_PRINT_FILTER_FRGBA_H__ */
