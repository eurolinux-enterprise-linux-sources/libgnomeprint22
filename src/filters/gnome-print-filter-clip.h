#ifndef __GNOME_PRINT_FILTER_CLIP_H__
#define __GNOME_PRINT_FILTER_CLIP_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GNOME_TYPE_PRINT_FILTER_CLIP (gnome_print_filter_clip_get_type ())
#define GNOME_PRINT_FILTER_CLIP(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GNOME_TYPE_PRINT_FILTER_CLIP, GnomePrintFilterClip))
#define GNOME_PRINT_FILTER_CLIP_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), GNOME_TYPE_PRINT_FILTER_CLIP, GnomePrintFilterClipClass))
#define GNOME_PRINT_IS_FILTER_CLIP(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GNOME_TYPE_PRINT_FILTER_CLIP))
#define GNOME_PRINT_IS_FILTER_CLIP_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GNOME_TYPE_PRINT_FILTER_CLIP))
#define GNOME_PRINT_FILTER_CLIP_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GNOME_TYPE_PRINT_FILTER_CLIP, GnomePrintFilterClipClass))
 
typedef struct _GnomePrintFilterClip GnomePrintFilterClip;
typedef struct _GnomePrintFilterClipClass GnomePrintFilterClipClass;

GType gnome_print_filter_clip_get_type (void);

G_END_DECLS

#endif /* __GNOME_PRINT_FILTER_CLIP_H__ */
