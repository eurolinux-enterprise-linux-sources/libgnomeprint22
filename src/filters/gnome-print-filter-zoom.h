#ifndef __GNOME_PRINT_FILTER_ZOOM_H__
#define __GNOME_PRINT_FILTER_ZOOM_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GNOME_TYPE_PRINT_FILTER_ZOOM (gnome_print_filter_zoom_get_type ())
#define GNOME_PRINT_FILTER_ZOOM(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GNOME_TYPE_PRINT_FILTER_ZOOM, GnomePrintFilterZoom))
#define GNOME_PRINT_FILTER_ZOOM_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), GNOME_TYPE_PRINT_FILTER_ZOOM, GnomePrintFilterZoomClass))
#define GNOME_PRINT_IS_FILTER_ZOOM(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GNOME_TYPE_PRINT_FILTER_ZOOM))
#define GNOME_PRINT_IS_FILTER_ZOOM_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GNOME_TYPE_PRINT_FILTER_ZOOM))
#define GNOME_PRINT_FILTER_ZOOM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GNOME_TYPE_PRINT_FILTER_ZOOM, GnomePrintFilterZoomClass))
 
typedef struct _GnomePrintFilterZoom GnomePrintFilterZoom;
typedef struct _GnomePrintFilterZoomClass GnomePrintFilterZoomClass;

GType gnome_print_filter_zoom_get_type (void);

G_END_DECLS

#endif /* __GNOME_PRINT_FILTER_ZOOM_H__ */
