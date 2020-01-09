#ifndef __GNOME_PRINT_FILTER_MULTIPAGE_H__
#define __GNOME_PRINT_FILTER_MULTIPAGE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GNOME_TYPE_PRINT_FILTER_MULTIPAGE (gnome_print_filter_multipage_get_type ())
#define GNOME_PRINT_FILTER_MULTIPAGE(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GNOME_TYPE_PRINT_FILTER_MULTIPAGE, GnomePrintFilterMultipage))
#define GNOME_PRINT_FILTER_MULTIPAGE_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), GNOME_TYPE_PRINT_FILTER_MULTIPAGE, GnomePrintFilterMultipageClass))
#define GNOME_IS_PRINT_FILTER_MULTIPAGE(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GNOME_TYPE_PRINT_FILTER_MULTIPAGE))
#define GNOME_IS_PRINT_FILTER_MULTIPAGE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GNOME_TYPE_PRINT_FILTER_MULTIPAGE))
#define GNOME_PRINT_FILTER_MULTIPAGE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GNOME_TYPE_PRINT_FILTER_MULTIPAGE, GnomePrintFilterMultipageClass))
 
typedef struct _GnomePrintFilterMultipage GnomePrintFilterMultipage;
typedef struct _GnomePrintFilterMultipageClass GnomePrintFilterMultipageClass;

GType gnome_print_filter_multipage_get_type (void);

G_END_DECLS

#endif /* __GNOME_PRINT_FILTER_MULTIPAGE_H__ */
