#ifndef __GNOME_PRINT_FILTER_DRAFT_H__
#define __GNOME_PRINT_FILTER_DRAFT_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GNOME_TYPE_PRINT_FILTER_DRAFT (gnome_print_filter_draft_get_type ())
#define GNOME_PRINT_FILTER_DRAFT(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GNOME_TYPE_PRINT_FILTER_DRAFT, GnomePrintFilterDraft))
#define GNOME_PRINT_FILTER_DRAFT_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), GNOME_TYPE_PRINT_FILTER_DRAFT, GnomePrintFilterDraftClass))
#define GNOME_PRINT_IS_FILTER_DRAFT(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GNOME_TYPE_PRINT_FILTER_DRAFT))
#define GNOME_PRINT_IS_FILTER_DRAFT_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GNOME_TYPE_PRINT_FILTER_DRAFT))
#define GNOME_PRINT_FILTER_DRAFT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GNOME_TYPE_PRINT_FILTER_DRAFT, GnomePrintFilterDraftClass))
 
typedef struct _GnomePrintFilterDraft GnomePrintFilterDraft;
typedef struct _GnomePrintFilterDraftClass GnomePrintFilterDraftClass;

GType gnome_print_filter_draft_get_type (void);

G_END_DECLS

#endif /* __GNOME_PRINT_FILTER_DRAFT_H__ */
