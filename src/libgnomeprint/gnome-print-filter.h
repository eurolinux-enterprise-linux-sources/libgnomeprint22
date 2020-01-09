#ifndef __GNOME_PRINT_FILTER_H__
#define __GNOME_PRINT_FILTER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#include <libgnomeprint/gnome-print.h>
#include <libgnomeprint/gnome-glyphlist.h>
#include <libart_lgpl/art_svp_intersect.h>

#define GNOME_TYPE_PRINT_FILTER (gnome_print_filter_get_type ())
#define GNOME_PRINT_FILTER(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GNOME_TYPE_PRINT_FILTER, GnomePrintFilter))
#define GNOME_PRINT_FILTER_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), GNOME_TYPE_PRINT_FILTER, GnomePrintFilterClass))
#define GNOME_IS_PRINT_FILTER(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GNOME_TYPE_PRINT_FILTER))
#define GNOME_IS_PRINT_FILTER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GNOME_TYPE_PRINT_FILTER))
#define GNOME_PRINT_FILTER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GNOME_TYPE_PRINT_FILTER, GnomePrintFilterClass))
 
typedef struct _GnomePrintFilter GnomePrintFilter;
typedef struct _GnomePrintFilterPrivate GnomePrintFilterPrivate;
typedef struct _GnomePrintFilterClass GnomePrintFilterClass;

struct _GnomePrintFilter {
	GObject parent;

	GnomePrintFilterPrivate *priv;
};

struct _GnomePrintFilterClass {
	GObjectClass parent_class;

	/* Signals */
	void (* changed)             (GnomePrintFilter *);
	void (* predecessor_added)   (GnomePrintFilter *, GnomePrintFilter *);
	void (* predecessor_removed) (GnomePrintFilter *, GnomePrintFilter *);
	void (* successor_added)     (GnomePrintFilter *, GnomePrintFilter *);
	void (* successor_removed)   (GnomePrintFilter *, GnomePrintFilter *);

	/* Methods */
	void (* reset) (GnomePrintFilter *);
	void (* flush) (GnomePrintFilter *);

	gint (* beginpage) (GnomePrintFilter *, GnomePrintContext *, const guchar *);
	gint (* showpage)  (GnomePrintFilter *);

	gint (* gsave)     (GnomePrintFilter *);
	gint (* grestore)  (GnomePrintFilter *);

	gint (* clip)      (GnomePrintFilter *, const ArtBpath *, ArtWindRule);
	gint (* fill)      (GnomePrintFilter *, const ArtBpath *, ArtWindRule);
	gint (* stroke)    (GnomePrintFilter *, const ArtBpath *);
	gint (* image)     (GnomePrintFilter *, const gdouble *, const guchar *, gint, gint, gint, gint);
	gint (* glyphlist) (GnomePrintFilter *, const gdouble *, GnomeGlyphList *);

	gint (* setrgbcolor)  (GnomePrintFilter *, gdouble, gdouble, gdouble);
	gint (* setopacity)   (GnomePrintFilter *, gdouble);
	gint (* setlinewidth) (GnomePrintFilter *, gdouble);
};

GType gnome_print_filter_get_type (void);

GnomePrintFilter *gnome_print_filter_new_from_module_name (const gchar *module_name,
		const gchar *first_property_name, ...);

gint gnome_print_filter_beginpage    (GnomePrintFilter *, GnomePrintContext *, const guchar *);
gint gnome_print_filter_showpage     (GnomePrintFilter *);
gint gnome_print_filter_gsave        (GnomePrintFilter *);
gint gnome_print_filter_grestore     (GnomePrintFilter *);
gint gnome_print_filter_clip         (GnomePrintFilter *, const ArtBpath *, ArtWindRule);
gint gnome_print_filter_fill         (GnomePrintFilter *, const ArtBpath *, ArtWindRule);
gint gnome_print_filter_stroke       (GnomePrintFilter *, const ArtBpath *);
gint gnome_print_filter_image        (GnomePrintFilter *, const gdouble *, const guchar *, gint, gint, gint, gint);
gint gnome_print_filter_glyphlist    (GnomePrintFilter *, const gdouble *, GnomeGlyphList *);
gint gnome_print_filter_setrgbcolor  (GnomePrintFilter *, gdouble, gdouble, gdouble);
gint gnome_print_filter_setopacity   (GnomePrintFilter *, gdouble);
gint gnome_print_filter_setlinewidth (GnomePrintFilter *, gdouble);

void gnome_print_filter_changed (GnomePrintFilter *);
void gnome_print_filter_reset   (GnomePrintFilter *);
void gnome_print_filter_flush   (GnomePrintFilter *);

/* Functions for reading and modifying the filter network */
void              gnome_print_filter_append_predecessor (GnomePrintFilter *, GnomePrintFilter *p);
guint             gnome_print_filter_count_predecessors (GnomePrintFilter *);
guint             gnome_print_filter_count_successors   (GnomePrintFilter *);
GnomePrintFilter *gnome_print_filter_get_predecessor    (GnomePrintFilter *, guint i);
GnomePrintFilter *gnome_print_filter_get_successor      (GnomePrintFilter *, guint i);
gboolean          gnome_print_filter_is_predecessor     (GnomePrintFilter *, GnomePrintFilter *p, gboolean indirect);
void              gnome_print_filter_remove_predecessor (GnomePrintFilter *, GnomePrintFilter *p);

guint             gnome_print_filter_count_filters  (GnomePrintFilter *);
GnomePrintFilter *gnome_print_filter_get_filter     (GnomePrintFilter *, guint);
void              gnome_print_filter_add_filter     (GnomePrintFilter *, GnomePrintFilter *);
void              gnome_print_filter_remove_filter  (GnomePrintFilter *, GnomePrintFilter *);
void              gnome_print_filter_remove_filters (GnomePrintFilter *);

#define GNOME_PRINT_FILTER_ERROR gnome_print_filter_error_quark ()
typedef enum {
	GNOME_PRINT_FILTER_ERROR_SYNTAX
} GnomePrintFilterError;
GQuark gnome_print_filter_error_quark (void);

gchar            *gnome_print_filter_description (GnomePrintFilter *);
GnomePrintFilter *gnome_print_filter_new_from_description (const gchar *, GError **);

G_END_DECLS

#endif /* __GNOME_PRINT_FILTER_H__ */
