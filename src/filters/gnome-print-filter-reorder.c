#include <config.h>
#include "gnome-print-filter-reorder.h"

#include <gmodule.h>

#include <libgnomeprint/gnome-print-filter.h>
#include <libgnomeprint/gnome-print-i18n.h>
#include <libgnomeprint/gnome-print-meta.h>
#include <libgnomeprint/gnome-print-private.h>

#include <libart_lgpl/art_affine.h>

#define FILTER_NAME        N_("reorder")
#define FILTER_DESCRIPTION N_("The reorder-filter reorders pages.")

struct _GnomePrintFilterReorder {
	GnomePrintFilter parent;

	GArray *cache;
	GnomePrintContext *meta;

	GArray *order;
	guint in, out;
};
                                                                                
struct _GnomePrintFilterReorderClass {
	GnomePrintFilterClass parent_class;
};

static GnomePrintFilterClass *parent_class = NULL;

enum {
	PROP_0,
	PROP_ORDER,
	PROP_NAME,
	PROP_DESCRIPTION
};

static void
gnome_print_filter_reorder_get_property (GObject *object, guint n, GValue *v,
				GParamSpec *pspec)
{
	GnomePrintFilterReorder *filter = (GnomePrintFilterReorder *) object;

	switch (n) {
	case PROP_ORDER:
		{
			guint i;
			GValue vd = {0,};
			GValueArray *va;

			if (!filter->order) break;
			va = g_value_array_new (filter->order->len);
			g_value_init (&vd, G_TYPE_UINT);
			for (i = 0; i < filter->order->len; i++) {
				g_value_set_uint (&vd, g_array_index (filter->order, guint, i));
				g_value_array_append (va, &vd);
			}
			g_value_unset (&vd);
			g_value_set_boxed (v, va);
		}
		break;
		break;
	case PROP_NAME:
		g_value_set_string (v, _(FILTER_NAME));
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (v, _(FILTER_DESCRIPTION));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, n, pspec);
	}
}

static void
gnome_print_filter_reorder_set_property (GObject *object, guint n,
		const GValue *v, GParamSpec *pspec)
{
	GnomePrintFilterReorder *filter = (GnomePrintFilterReorder *) object;

	switch (n) {
	case PROP_ORDER:
		{
			GValueArray *a = g_value_get_boxed (v);

			if (!a || !a->n_values) {
				if (filter->order) {
					g_array_free (filter->order, TRUE);
					filter->order = NULL;
					gnome_print_filter_changed (GNOME_PRINT_FILTER (filter));
				}
				break;
			} else {
				guint i;
				gboolean b = FALSE;

				if (!filter->order && !a->n_values);
				else if (!filter->order && a->n_values) {
					filter->order = g_array_new (FALSE, TRUE, sizeof (guint));
					g_array_set_size (filter->order, a->n_values);
					b = TRUE;
				} else if (filter->order->len != a->n_values) {
					g_array_set_size (filter->order, a->n_values);
					b = TRUE;
				}
				for (i = 0; i < a->n_values; i++) {
					guint j = g_value_get_uint (g_value_array_get_nth (a, i));

					if (g_array_index (filter->order, guint, i) != j) {
						g_array_index (filter->order, guint, i) = j;
						b = TRUE;
					}
				}
				if (b)
					gnome_print_filter_changed (GNOME_PRINT_FILTER (filter));
			}
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, n, pspec);
	}
}

static void
gnome_print_filter_reorder_scan_cache (GnomePrintFilterReorder *r)
{
	guint i;
	GnomePrintContext *pc = NULL;
	GnomePrintFilter *filter = NULL;

	g_return_if_fail (GNOME_PRINT_IS_FILTER_REORDER (r));

	if (!r->cache || !r->cache->len) return;
	g_object_get (G_OBJECT (r), "context", &pc, NULL);
	g_object_get (G_OBJECT (pc), "filter", &filter, NULL);
	g_object_ref (G_OBJECT (filter));
	for (i = 0; i < r->cache->len; ) {
		if (!r->order || (r->order->len <= r->out) ||
				(g_array_index (r->cache, guint, i) ==
				 g_array_index (r->order, guint, r->out))) {
			guint j, n;
			GnomePrintContext *meta;

			n = gnome_print_filter_count_successors (GNOME_PRINT_FILTER (r));
			if (!n) {
				g_object_set (G_OBJECT (pc), "filter", NULL, NULL);
				gnome_print_meta_render_page (GNOME_PRINT_META (r->meta), pc, i, TRUE);
			} else
				for (j = 0; j < n; j++) {
					g_object_set (G_OBJECT (pc), "filter",
							gnome_print_filter_get_successor (GNOME_PRINT_FILTER (r), j),
							NULL);
					gnome_print_meta_render_page (GNOME_PRINT_META (r->meta), pc, i, TRUE);
				}
			r->out++;
			meta = g_object_new (GNOME_TYPE_PRINT_META, NULL);
			for (j = 0; j < i; j++)
				gnome_print_meta_render_page (GNOME_PRINT_META (r->meta), meta, j, TRUE);
			for (j = i + 1; j < gnome_print_meta_get_pages (GNOME_PRINT_META (r->meta)); j++)
				gnome_print_meta_render_page (GNOME_PRINT_META (r->meta), meta, j, TRUE);
			g_object_unref (G_OBJECT (r->meta));
			r->meta = meta;
			g_array_remove_index (r->cache, i);
			i = 0;
		} else
			i++;
	}
	g_object_set (G_OBJECT (pc), "filter", filter, NULL);
	g_object_unref (G_OBJECT (filter));
}

static gboolean
gnome_print_filter_reorder_pass_through (GnomePrintFilterReorder *r)
{
	g_return_val_if_fail (GNOME_PRINT_IS_FILTER_REORDER (r), FALSE);

	return !r->order ||
			(r->order->len <= r->out) ||
			(g_array_index (r->order, guint, r->out) == r->in - 1);
}

static gint
gnome_print_filter_reorder_beginpage (GnomePrintFilter *f,
		GnomePrintContext *pc, const guchar *n)
{
	GnomePrintFilterReorder *r = GNOME_PRINT_FILTER_REORDER (f);
	guint c = r->in;

	gnome_print_filter_reorder_scan_cache (r);
	r->in++;
	if (gnome_print_filter_reorder_pass_through (r)) {
		return parent_class->beginpage (f, pc, n);
	}
	if (!r->cache)
		r->cache = g_array_new (FALSE, TRUE, sizeof (guint));
	g_array_append_val (r->cache, c);
	return gnome_print_beginpage_real (r->meta, n);
}

static gint
gnome_print_filter_reorder_showpage (GnomePrintFilter *f)
{
	GnomePrintFilterReorder *r = GNOME_PRINT_FILTER_REORDER (f);

	if (gnome_print_filter_reorder_pass_through (r)) {
		parent_class->showpage (f);
		r->out++;
	} else {
		gnome_print_showpage_real (r->meta);
	}
	gnome_print_filter_reorder_scan_cache (r);
	return GNOME_PRINT_OK;
}

static gint
gnome_print_filter_reorder_gsave (GnomePrintFilter *f)
{
	GnomePrintFilterReorder *r = GNOME_PRINT_FILTER_REORDER (f);

	if (gnome_print_filter_reorder_pass_through (r))
		return parent_class->gsave (f);
	return gnome_print_gsave_real (r->meta);
}

static gint
gnome_print_filter_reorder_grestore (GnomePrintFilter *f)
{
	GnomePrintFilterReorder *r = GNOME_PRINT_FILTER_REORDER (f);

	if (gnome_print_filter_reorder_pass_through (r))
		return parent_class->grestore (f);
	return gnome_print_grestore_real (r->meta);
}

static gint
gnome_print_filter_reorder_stroke (GnomePrintFilter *f, const ArtBpath *b)
{
	GnomePrintFilterReorder *r = GNOME_PRINT_FILTER_REORDER (f);

	if (gnome_print_filter_reorder_pass_through (r))
		return parent_class->stroke (f, b);
	return gnome_print_stroke_bpath_real (r->meta, b);
}

static gint
gnome_print_filter_reorder_clip (GnomePrintFilter *f, const ArtBpath *b,
		ArtWindRule rule)
{
	GnomePrintFilterReorder *r = GNOME_PRINT_FILTER_REORDER (f);

	if (gnome_print_filter_reorder_pass_through (r))
		return parent_class->clip (f, b, rule);
	return gnome_print_clip_bpath_rule_real (r->meta, b, rule);
}

static gint
gnome_print_filter_reorder_fill (GnomePrintFilter *f, const ArtBpath *b,
		ArtWindRule rule)
{
	GnomePrintFilterReorder *r = GNOME_PRINT_FILTER_REORDER (f);

	if (gnome_print_filter_reorder_pass_through (r))
		return parent_class->fill (f, b, rule);
	return gnome_print_fill_bpath_rule_real (r->meta, b, rule);
}

static gint
gnome_print_filter_reorder_glyphlist (GnomePrintFilter *f,
		const gdouble *a, GnomeGlyphList *gl)
{
	GnomePrintFilterReorder *r = GNOME_PRINT_FILTER_REORDER (f);

	if (gnome_print_filter_reorder_pass_through (r))
		return parent_class->glyphlist (f, a, gl);
	return gnome_print_glyphlist_transform_real (r->meta, a, gl);
}

static gint
gnome_print_filter_reorder_image (GnomePrintFilter *f,
		const gdouble *a, const guchar *p, gint w, gint h, gint rowstride, gint c)
{
	GnomePrintFilterReorder *r = GNOME_PRINT_FILTER_REORDER (f);

	if (gnome_print_filter_reorder_pass_through (r))
		return parent_class->image (f, a, p, w, h, rowstride, c);
	return gnome_print_image_transform_real (r->meta, a, p, w, h, rowstride, c);
}

static gint
gnome_print_filter_reorder_setrgbcolor (GnomePrintFilter *f,
		gdouble red, gdouble g, gdouble b)
{
	GnomePrintFilterReorder *r = GNOME_PRINT_FILTER_REORDER (f);

	if (gnome_print_filter_reorder_pass_through (r))
		return parent_class->setrgbcolor (f, red, g, b);
	return gnome_print_setrgbcolor_real (r->meta, red, g, b);
}

static gint
gnome_print_filter_reorder_setopacity (GnomePrintFilter *f,
		gdouble o)
{
	GnomePrintFilterReorder *r = GNOME_PRINT_FILTER_REORDER (f);

	if (gnome_print_filter_reorder_pass_through (r))
		return parent_class->setopacity (f, o);
	return gnome_print_setopacity_real (r->meta, o);
}

static gint
gnome_print_filter_reorder_setlinewidth (GnomePrintFilter *f,
		gdouble w)
{
	GnomePrintFilterReorder *r = GNOME_PRINT_FILTER_REORDER (f);

	if (gnome_print_filter_reorder_pass_through (r))
		return parent_class->setlinewidth (f, w);
	return gnome_print_setlinewidth_real (r->meta, w);
}

static void
gnome_print_filter_reorder_reset (GnomePrintFilter *f)
{
	GnomePrintFilterReorder *r = GNOME_PRINT_FILTER_REORDER (f);

	r->in = r->out = 0;
	gnome_print_meta_reset (GNOME_PRINT_META (r->meta));
	if (r->cache) {
		g_array_free (r->cache, TRUE);
		r->cache = NULL;
	}

	parent_class->reset (f);
}

static void
gnome_print_filter_reorder_flush (GnomePrintFilter *f)
{
	GnomePrintFilterReorder *r = GNOME_PRINT_FILTER_REORDER (f);
	GnomePrintFilter *filter = NULL;
	GnomePrintContext *pc = NULL;
	guint i, n;

	g_object_get (G_OBJECT (r), "context", &pc, NULL);
	g_object_get (G_OBJECT (pc), "filter", &filter, NULL);
	g_object_ref (G_OBJECT (filter));
	n = gnome_print_filter_count_successors (GNOME_PRINT_FILTER (r));
	while (r->cache && r->cache->len) {
		if (!n) {
			g_object_set (G_OBJECT (pc), "filter", NULL, NULL);
			gnome_print_beginpage (pc, (const guchar *) "");
			gnome_print_showpage (pc);
		} else
			for (i = 0; i < n; i++) {
				g_object_set (G_OBJECT (pc), "filter", gnome_print_filter_get_successor (f, i), NULL);
				gnome_print_beginpage (pc, (const guchar *) "");
				gnome_print_showpage (pc);
				r->out++;
			}
		g_object_set (G_OBJECT (pc), "filter", filter, NULL);
		r->out++;
		gnome_print_filter_reorder_scan_cache (r);
	}
	g_object_unref (G_OBJECT (filter));
}

static void
gnome_print_filter_reorder_finalize (GObject *object)
{
	GnomePrintFilterReorder *r = GNOME_PRINT_FILTER_REORDER (object);

	if (r->cache) {
		g_array_free (r->cache, TRUE);
		r->cache = NULL;
	}
	if (r->order) {
		g_array_free (r->order, TRUE);
		r->order = NULL;
	}
	if (r->meta) {
		g_object_unref (G_OBJECT (r->meta));
		r->meta = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gnome_print_filter_reorder_class_init (GnomePrintFilterReorderClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;
	GnomePrintFilterClass *filter_class = (GnomePrintFilterClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	filter_class->reset        = gnome_print_filter_reorder_reset;
	filter_class->flush        = gnome_print_filter_reorder_flush;
	filter_class->beginpage    = gnome_print_filter_reorder_beginpage;
	filter_class->showpage     = gnome_print_filter_reorder_showpage;
	filter_class->gsave        = gnome_print_filter_reorder_gsave;
	filter_class->grestore     = gnome_print_filter_reorder_grestore;
	filter_class->stroke       = gnome_print_filter_reorder_stroke;
	filter_class->clip         = gnome_print_filter_reorder_clip;
	filter_class->fill         = gnome_print_filter_reorder_fill;
	filter_class->glyphlist    = gnome_print_filter_reorder_glyphlist;
	filter_class->image        = gnome_print_filter_reorder_image;
	filter_class->setrgbcolor  = gnome_print_filter_reorder_setrgbcolor;
	filter_class->setopacity   = gnome_print_filter_reorder_setopacity;
	filter_class->setlinewidth = gnome_print_filter_reorder_setlinewidth;

	object_class->finalize     = gnome_print_filter_reorder_finalize;
	object_class->get_property = gnome_print_filter_reorder_get_property;
	object_class->set_property = gnome_print_filter_reorder_set_property;

	g_object_class_override_property (object_class, PROP_NAME, "name");
	g_object_class_override_property (object_class, PROP_DESCRIPTION, "description");
	g_object_class_install_property (object_class, PROP_ORDER,
		g_param_spec_value_array ("order", _("Order"), _("Order"),
			g_param_spec_uint ("order_item", "Order item", "Order item",
				0, G_MAXUINT, 0, G_PARAM_READWRITE), G_PARAM_READWRITE));
}

static void
gnome_print_filter_reorder_init (GnomePrintFilterReorder *r)
{
	r->meta = g_object_new (GNOME_TYPE_PRINT_META, NULL);
}

GType
gnome_print_filter_reorder_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GnomePrintFilterReorderClass), NULL, NULL,
			(GClassInitFunc) gnome_print_filter_reorder_class_init,
			NULL, NULL, sizeof (GnomePrintFilterReorder), 0,
			(GInstanceInitFunc) gnome_print_filter_reorder_init
		};
		type = g_type_register_static (GNOME_TYPE_PRINT_FILTER,
				"GnomePrintFilterReorder", &info, 0);
	}
	return type;
}

G_MODULE_EXPORT GType gnome_print__filter_get_type (void);

G_MODULE_EXPORT GType
gnome_print__filter_get_type (void)
{
	return GNOME_TYPE_PRINT_FILTER_REORDER;
}
