/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#include <config.h>

#include <libgnomeprint/gnome-print-filter.h>
#include <libgnomeprint/gnome-print-private.h>
#include <libgnomeprint/gnome-print-meta.h>
#include <libgnomeprint/gp-gc-private.h>
#include <libgnomeprint/gnome-print-i18n.h>

#include <libart_lgpl/art_affine.h>

#include <string.h>
#include <ctype.h>

#include <gmodule.h>

#define FILTER_NAME        N_("generic")
#define FILTER_DESCRIPTION N_("The 'generic'-filter can be used to print several pages onto one page.")

#define C(r) { gint res = (r); if (res < 0) return res; }

static GObjectClass *parent_class = NULL;

struct _GnomePrintFilterPrivate {

	GPtrArray *predecessors;
	GPtrArray *successors;
	GPtrArray *filters;

	gdouble transform[6];

	GnomePrintContext *pc;
	GnomePrintFilter *bin;

	GPtrArray *meta;
	GPtrArray *meta_bin;
	GnomePrintContext *meta_bin_last;

	gboolean suppress_empty_pages;
};

enum {
	PROP_0,
	PROP_NAME,
	PROP_DESCRIPTION,
	PROP_SUPPRESS_EMPTY_PAGES,
	PROP_CONTEXT,
	PROP_TRANSFORM,
	PROP_FILTERS
};

enum {
	CHANGED,
	PREDECESSOR_ADDED,
	PREDECESSOR_REMOVED,
	SUCCESSOR_ADDED,
	SUCCESSOR_REMOVED,
	LAST_SIGNAL
};

#define gnome_print_filter_haspage(f)  (GNOME_IS_PRINT_FILTER (f) && GNOME_IS_PRINT_CONTEXT ((f)->priv->pc))

static guint signals[LAST_SIGNAL] = { 0 };

static void
gnome_print_filter_get_property (GObject *object, guint n, GValue *v, GParamSpec *pspec)
{
	GnomePrintFilter *f = (GnomePrintFilter *) object;
	guint i;
	GValueArray *va;
	GValue vd = {0,};

	switch (n) {
	case PROP_NAME:		g_value_set_string  (v, _(FILTER_NAME));		break;
	case PROP_DESCRIPTION:	g_value_set_string  (v, _(FILTER_DESCRIPTION));		break;
	case PROP_SUPPRESS_EMPTY_PAGES:
		g_value_set_boolean (v, f->priv->suppress_empty_pages);
		break;
	case PROP_CONTEXT: g_value_set_object (v, f->priv->pc ); break;
	case PROP_TRANSFORM: 
		va = g_value_array_new (6);
		g_value_init (&vd, G_TYPE_DOUBLE);
		for (i = 0; i < 6; i++) {
			g_value_set_double (&vd, f->priv->transform[i]);
			g_value_array_append (va, &vd);
		}
		g_value_unset (&vd);
		g_value_set_boxed (v, va);
		g_value_array_free (va);
		break;
	case PROP_FILTERS:
		va = g_value_array_new (0);
		g_value_init (&vd, G_TYPE_OBJECT);
		for (i = 0; i < (f->priv->filters ? f->priv->filters->len : 0); i++) {
			g_value_set_object (&vd, g_ptr_array_index (f->priv->filters, i));
			g_value_array_append (va, &vd);
		}
		g_value_unset (&vd);
		g_value_set_boxed (v, va);
		g_value_array_free (va);
		break;
	default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, n, pspec);
	}
}

static void
gnome_print_filter_set_property (GObject *object, guint n, const GValue *v,
			GParamSpec *pspec)
{
	GnomePrintFilter *f = (GnomePrintFilter *) object;
	gboolean b;
	GValueArray *va = NULL;
	guint i;

	switch (n) {
	case PROP_SUPPRESS_EMPTY_PAGES:
		b = g_value_get_boolean (v);
		if (b != f->priv->suppress_empty_pages) {
			f->priv->suppress_empty_pages = b;
			gnome_print_filter_changed (f);
		}
		break;
	case PROP_TRANSFORM:
		va = g_value_get_boxed (v);
		if (!va || !va->n_values)
			art_affine_identity (f->priv->transform);
		else
			for (i = 0; (i < va->n_values) && (i < 6); i++)
				f->priv->transform[i] = g_value_get_double (
						g_value_array_get_nth (va, i));
		gnome_print_filter_changed (f);
		break;
	case PROP_FILTERS:
		va = g_value_get_boxed (v);
		while (gnome_print_filter_count_filters (f))
			gnome_print_filter_remove_filter (f, NULL);
		for (i = 0; i < (va ? va->n_values : 0); i++)
			gnome_print_filter_add_filter (f,
				GNOME_PRINT_FILTER (g_value_get_object (
					g_value_array_get_nth (va, i))));
		break;
	case PROP_CONTEXT:
		if (f->priv->pc)
			g_object_remove_weak_pointer (G_OBJECT (f->priv->pc),
				(gpointer *)&(f->priv->pc));
		f->priv->pc = g_value_get_object (v);
		if (f->priv->pc)
			g_object_add_weak_pointer (G_OBJECT (f->priv->pc),
				(gpointer *)&(f->priv->pc));
		break;
	default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, n, pspec);
	}
}

#define GPFGC(f) GNOME_PRINT_FILTER_GET_CLASS(f)

void
gnome_print_filter_clear_data (GnomePrintFilter *f)
{
	guint i;

	g_return_if_fail (GNOME_IS_PRINT_FILTER (f));

	if (f->priv->meta_bin_last) {
		g_object_unref (f->priv->meta_bin_last);
		f->priv->meta_bin_last = NULL;
	}
	if (f->priv->meta_bin) {
		for (i = 0; i < f->priv->meta_bin->len; i++)
			g_object_unref (G_OBJECT (f->priv->meta_bin->pdata[i]));
		g_ptr_array_free (f->priv->meta_bin, TRUE);
		f->priv->meta_bin = NULL;
	}
}

static gint
beginpage_impl (GnomePrintFilter *f, GnomePrintContext *pc, const guchar *name)
{
	GnomePrintFilter *s;
	guint i, n;

	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), GNOME_PRINT_ERROR_BADVALUE);
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADVALUE);
	g_return_val_if_fail (!gnome_print_filter_haspage (f) || (f->priv->pc == pc), GNOME_PRINT_ERROR_BADVALUE);

	/* Check if there are filters inside this filter. */
	n = gnome_print_filter_count_filters (f);
	if (n) {
		C (gnome_print_filter_beginpage (
					gnome_print_filter_get_filter (f, 0), pc, name));
		if (f->priv->meta_bin)
			while (f->priv->meta_bin->len)
				g_ptr_array_remove_index (f->priv->meta_bin, 0);
		if (!f->priv->meta_bin)
			f->priv->meta_bin = g_ptr_array_new ();
		for (i = 1; i < n; i++) {
			GnomePrintContext *meta = g_object_new (GNOME_TYPE_PRINT_META, NULL);

			g_ptr_array_add (f->priv->meta_bin, meta);
			C (gnome_print_beginpage_real (meta, name));
		}
		return GNOME_PRINT_OK;
	}

	/* Check if this filter doesn't have successors. */
	n = gnome_print_filter_count_successors (f);
	if (!n) {
		if (!f->priv->bin || !gnome_print_filter_count_successors (f->priv->bin))
			return gnome_print_beginpage_real (pc, name);
		if (!f->priv->bin->priv->meta_bin_last)
			f->priv->bin->priv->meta_bin_last =
				g_object_new (GNOME_TYPE_PRINT_META, NULL);
		return gnome_print_beginpage_real (
							f->priv->bin->priv->meta_bin_last, name);
	}

	/* This filter has successors */
	s = gnome_print_filter_get_successor (f, 0);
	s->priv->bin = f->priv->bin;
	C (gnome_print_filter_beginpage (s, f->priv->pc, name));
	for (i = 1; i < n; i++)
		C (gnome_print_beginpage_real (
					GNOME_PRINT_CONTEXT (g_ptr_array_index (f->priv->meta, i - 1)),
					name));
	return GNOME_PRINT_OK;
}

static gint
showpage_impl (GnomePrintFilter *f)
{
	GnomePrintFilter *f_old = NULL;
	guint i, n;

	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), GNOME_PRINT_ERROR_BADVALUE);

	if (!gnome_print_filter_haspage (f))
		return GNOME_PRINT_OK;

	/* Check if this filter has filters inside. */
	g_object_get (G_OBJECT (f->priv->pc), "filter", &f_old, NULL);
	if (f_old)
		g_object_ref (G_OBJECT (f_old));
	n = gnome_print_filter_count_filters (f);
	if (n) {
		gnome_print_filter_showpage (
					gnome_print_filter_get_filter (f, 0));
		for (i = 1; i < n; i++) {
			GnomePrintMeta *meta = g_ptr_array_index (f->priv->meta_bin, 0);

			gnome_print_showpage_real (GNOME_PRINT_CONTEXT (meta));
			g_object_set (G_OBJECT (f->priv->pc), "filter",
					gnome_print_filter_get_filter (f, i), NULL);
			gnome_print_meta_render (meta, f->priv->pc);
			g_object_unref (G_OBJECT (meta));
			g_ptr_array_remove_index (f->priv->meta_bin, 0);
		}
		g_ptr_array_free (f->priv->meta_bin, TRUE);
		f->priv->meta_bin = NULL;
		goto reset_filter;
	}

	/* Check if this filter has successors */
	n = gnome_print_filter_count_successors (f);
	if (!n) {
		if (!f->priv->bin) {
			gnome_print_showpage_real (f->priv->pc);
			goto reset_filter;
		}
		n = gnome_print_filter_count_successors (f->priv->bin);
		if (!n) {
			gnome_print_showpage_real (f->priv->pc);
			goto reset_filter;
		}
		gnome_print_showpage_real (f->priv->bin->priv->meta_bin_last);
		for (i = 0; i < n; i++) {
			g_object_set (G_OBJECT (f->priv->pc), "filter",
					gnome_print_filter_get_successor (f->priv->bin, i), NULL);
			gnome_print_meta_render (
				GNOME_PRINT_META (f->priv->bin->priv->meta_bin_last), f->priv->pc);
		}
		g_object_unref (G_OBJECT (f->priv->bin->priv->meta_bin_last));
		f->priv->bin->priv->meta_bin_last = NULL;
		goto reset_filter;
	}

	/* This filter has successors */
	gnome_print_filter_showpage (gnome_print_filter_get_successor (f, 0));
	for (i = 1; i < n; i++) {
		GnomePrintMeta *meta = g_ptr_array_index (f->priv->meta, i - 1);
		GnomePrintFilter *s = gnome_print_filter_get_successor (f, i);

		s->priv->bin = f->priv->bin;
		gnome_print_showpage_real (GNOME_PRINT_CONTEXT (meta));
		g_object_set (G_OBJECT (f->priv->pc), "filter", s, NULL);
		gnome_print_meta_render (meta, f->priv->pc);
		gnome_print_meta_reset (meta);
	}

reset_filter :
	g_object_set (G_OBJECT (f->priv->pc), "filter", f_old, NULL);
	if (f_old)
		g_object_unref (G_OBJECT (f_old));
	return GNOME_PRINT_OK;
}

static gint
gsave_impl (GnomePrintFilter *f)
{
	guint i, n;

	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), GNOME_PRINT_ERROR_BADVALUE);

	if (!gnome_print_filter_haspage (f))
		return GNOME_PRINT_OK;

	/* Check if this filter has filters inside. */
	n = gnome_print_filter_count_filters (f);
	if (n) {
		C (gnome_print_filter_gsave (gnome_print_filter_get_filter (f, 0)));
		for (i = 1; i < n; i++)
			C (gnome_print_gsave_real (GNOME_PRINT_CONTEXT (
							g_ptr_array_index (f->priv->meta_bin, i - 1))));
		return GNOME_PRINT_OK;
	}

	/* Check if this filter has successors */
	n = gnome_print_filter_count_successors (f);
	if (!n) {
		if (!f->priv->bin || !gnome_print_filter_count_successors (f->priv->bin))
			return gnome_print_gsave_real (f->priv->pc);
		return gnome_print_gsave_real (f->priv->bin->priv->meta_bin_last);
	}

	/* This filter has successors */
	C (gnome_print_filter_gsave (gnome_print_filter_get_successor (f, 0)));
	for (i = 1; i < n; i++)
		C (gnome_print_gsave_real (GNOME_PRINT_CONTEXT (
						g_ptr_array_index (f->priv->meta, i - 1))));
	return GNOME_PRINT_OK;
}

static gint
grestore_impl (GnomePrintFilter *f)
{
	guint i, n;

	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), GNOME_PRINT_ERROR_BADVALUE);

	if (!gnome_print_filter_haspage (f))
		return GNOME_PRINT_OK;

	/* Check if this filter has filters inside. */
	n = gnome_print_filter_count_filters (f);
	if (n) {
		C (gnome_print_filter_grestore (gnome_print_filter_get_filter (f, 0)));
		for (i = 1; i < n; i++)
			C (gnome_print_grestore_real (GNOME_PRINT_CONTEXT (
							g_ptr_array_index (f->priv->meta_bin, i - 1))));
		return GNOME_PRINT_OK;
	}

	/* Check if this filter has successors */
	n = gnome_print_filter_count_successors (f);
	if (!n) {
		if (!f->priv->bin || !gnome_print_filter_count_successors (f->priv->bin))
			return gnome_print_grestore_real (f->priv->pc);
		return gnome_print_grestore_real (f->priv->bin->priv->meta_bin_last);
	}

	/* This filter has successors */
	C (gnome_print_filter_grestore (gnome_print_filter_get_successor (f, 0)));
	for (i = 1; i < n; i++)
		C (gnome_print_grestore_real (GNOME_PRINT_CONTEXT (
						g_ptr_array_index (f->priv->meta, i - 1))));
	return GNOME_PRINT_OK;
}

static gint
clip_impl (GnomePrintFilter *f, const ArtBpath *bpath, ArtWindRule r)
{
	ArtBpath *b;
	guint i, n;

	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), GNOME_PRINT_ERROR_BADVALUE);

	if (!gnome_print_filter_haspage (f))
		return GNOME_PRINT_OK;

	b = art_bpath_affine_transform (bpath, f->priv->transform);

	/* Check if this filter has filters inside. */
	n = gnome_print_filter_count_filters (f);
	if (n) {
		gnome_print_filter_clip (gnome_print_filter_get_filter (f, 0), b, r);
		for (i = 1; i < n; i++)
			gnome_print_clip_bpath_rule_real (GNOME_PRINT_CONTEXT (
							g_ptr_array_index (f->priv->meta_bin, i - 1)), b, r);
		art_free (b);
		return GNOME_PRINT_OK;
	}

	/* Check if this filter has successors */
	n = gnome_print_filter_count_successors (f);
	if (!n) {
		if (!f->priv->bin || !gnome_print_filter_count_successors (f->priv->bin))
			gnome_print_clip_bpath_rule_real (f->priv->pc, b, r);
		else
			gnome_print_clip_bpath_rule_real (
											f->priv->bin->priv->meta_bin_last, b, r);
		art_free (b);
		return GNOME_PRINT_OK;
	}

	/* This filter has successors */
	gnome_print_filter_clip (gnome_print_filter_get_successor (f, 0), b, r);
	for (i = 1; i < n; i++)
		gnome_print_clip_bpath_rule_real (GNOME_PRINT_CONTEXT (
						g_ptr_array_index (f->priv->meta, i - 1)), b, r);
	art_free (b);
	return GNOME_PRINT_OK;
}

static gint
fill_impl (GnomePrintFilter *f, const ArtBpath *bpath, ArtWindRule r)
{
	ArtBpath *b;
	guint i, n;

	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), GNOME_PRINT_ERROR_BADVALUE);

	if (!gnome_print_filter_haspage (f))
		return GNOME_PRINT_OK;

	b = art_bpath_affine_transform (bpath, f->priv->transform);

	/* Check if this filter has filters inside. */
	n = gnome_print_filter_count_filters (f);
	if (n) {
		gnome_print_filter_fill (gnome_print_filter_get_filter (f, 0), b, r);
		for (i = 1; i < n; i++)
			gnome_print_fill_bpath_rule_real (GNOME_PRINT_CONTEXT (
							g_ptr_array_index (f->priv->meta_bin, i - 1)), b, r);
		art_free (b);
		return GNOME_PRINT_OK;
	}

	/* Check if this filter has successors */
	n = gnome_print_filter_count_successors (f);
	if (!n) {
		if (!f->priv->bin || !gnome_print_filter_count_successors (f->priv->bin))
			gnome_print_fill_bpath_rule_real (f->priv->pc, b, r);
		else
			gnome_print_fill_bpath_rule_real (f->priv->bin->priv->meta_bin_last, b, r);
		art_free (b);
		return GNOME_PRINT_OK;
	}

	/* This filter has successors */
	gnome_print_filter_fill (gnome_print_filter_get_successor (f, 0), b, r);
	for (i = 1; i < n; i++)
		gnome_print_fill_bpath_rule_real (GNOME_PRINT_CONTEXT (
						g_ptr_array_index (f->priv->meta, i - 1)), b, r);
	art_free (b);
	return GNOME_PRINT_OK;
}

static gint
stroke_impl (GnomePrintFilter *f, const ArtBpath *bpath)
{
	ArtBpath *b;
	guint i, n;

	if (!gnome_print_filter_haspage (f))
		return GNOME_PRINT_OK;

	b = art_bpath_affine_transform (bpath, f->priv->transform);

	/* Check if this filter has filters inside. */
	n = gnome_print_filter_count_filters (f);
	if (n) {
		gnome_print_filter_stroke (gnome_print_filter_get_filter (f, 0), b);
		for (i = 1; i < n; i++)
			gnome_print_stroke_bpath_real (GNOME_PRINT_CONTEXT (
							g_ptr_array_index (f->priv->meta_bin, i - 1)), b);
		art_free (b);
		return GNOME_PRINT_OK;
	}

	/* Check if this filter has successors */
	n = gnome_print_filter_count_successors (f);
	if (!n) {
		if (!f->priv->bin || !gnome_print_filter_count_successors (f->priv->bin))
			gnome_print_stroke_bpath_real (f->priv->pc, b);
		else
			gnome_print_stroke_bpath_real (f->priv->bin->priv->meta_bin_last, b);
		art_free (b);
		return GNOME_PRINT_OK;
	}

	/* This filter has successors */
	gnome_print_filter_stroke (gnome_print_filter_get_successor (f, 0), b);
	for (i = 1; i < n; i++)
		gnome_print_stroke_bpath_real (GNOME_PRINT_CONTEXT (
						g_ptr_array_index (f->priv->meta, i - 1)), b);
	art_free (b);
	return GNOME_PRINT_OK;
}

static gint
image_impl (GnomePrintFilter *f, const gdouble *affine, const guchar *p, gint w, gint h, gint r, gint c)
{
	gdouble a[6];
	guint i, n;

	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), GNOME_PRINT_ERROR_BADVALUE);

	if (!gnome_print_filter_haspage (f))
		return GNOME_PRINT_OK;

	art_affine_multiply (a, affine, f->priv->transform);

	/* Check if this filter has filters inside. */
	n = gnome_print_filter_count_filters (f);
	if (n) {
		C (gnome_print_filter_image (gnome_print_filter_get_filter (f, 0), a, p, w, h, r, c));
		for (i = 1; i < n; i++)
			C (gnome_print_image_transform_real (GNOME_PRINT_CONTEXT (
							g_ptr_array_index (f->priv->meta_bin, i - 1)),
						a, p, w, h, r, c));
		return GNOME_PRINT_OK;
	}

	/* Check if this filter has successors */
	n = gnome_print_filter_count_successors (f);
	if (!n) {
		if (!f->priv->bin || !gnome_print_filter_count_successors (f->priv->bin))
			return gnome_print_image_transform_real (f->priv->pc, a, p, w, h, r, c);
		return gnome_print_image_transform_real (
										f->priv->bin->priv->meta_bin_last, a, p, w, h, r, c);
	}

	/* This filter has successors */
	C (gnome_print_filter_image (gnome_print_filter_get_successor (f, 0), a, p, w, h, r, c));
	for (i = 1; i < n; i++)
		C (gnome_print_image_transform_real (GNOME_PRINT_CONTEXT (
						g_ptr_array_index (f->priv->meta, i - 1)), a, p, w, h, r, c));
	return GNOME_PRINT_OK;
}

static gint
glyphlist_impl (GnomePrintFilter *f, const gdouble *affine, GnomeGlyphList *l)
{
	gdouble a[6];
	guint i, n;

	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), GNOME_PRINT_ERROR_BADVALUE);

	if (!gnome_print_filter_haspage (f))
		return GNOME_PRINT_OK;

	art_affine_multiply (a, affine, f->priv->transform);

	/* Check if this filter has filters inside. */
	n = gnome_print_filter_count_filters (f);
	if (n) {
		C (gnome_print_filter_glyphlist (gnome_print_filter_get_filter (f, 0), a, l));
		for (i = 1; i < n; i++)
			C (gnome_print_glyphlist_transform_real (GNOME_PRINT_CONTEXT (
							g_ptr_array_index (f->priv->meta_bin, i - 1)), a, l));
		return GNOME_PRINT_OK;
	}

	/* Check if this filter has successors */
	n = gnome_print_filter_count_successors (f);
	if (!n) {
		if (!f->priv->bin || !gnome_print_filter_count_successors (f->priv->bin))
			return gnome_print_glyphlist_transform_real (f->priv->pc, a, l);
		return gnome_print_glyphlist_transform_real (
														f->priv->bin->priv->meta_bin_last, a, l);
	}

	/* This filter has successors */
	C (gnome_print_filter_glyphlist (gnome_print_filter_get_successor (f, 0), a, l));
	for (i = 1; i < n; i++)
		C (gnome_print_glyphlist_transform_real (GNOME_PRINT_CONTEXT (
						g_ptr_array_index (f->priv->meta, i - 1)), a, l));
	return GNOME_PRINT_OK;
}

static gint
setrgbcolor_impl (GnomePrintFilter *f, gdouble r, gdouble g, gdouble b)
{
	guint i, n;

	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), GNOME_PRINT_ERROR_BADVALUE);

	if (!gnome_print_filter_haspage (f))
		return GNOME_PRINT_OK;

	/* Check if this filter has filters inside. */
	n = gnome_print_filter_count_filters (f);
	if (n) {
		C (gnome_print_filter_setrgbcolor (gnome_print_filter_get_filter (f, 0), r, g, b));
		for (i = 1; i < n; i++)
			C (gnome_print_setrgbcolor_real (GNOME_PRINT_CONTEXT (
							g_ptr_array_index (f->priv->meta_bin, i - 1)), r, g, b));
		return GNOME_PRINT_OK;
	}

	/* Check if this filter has successors */
	n = gnome_print_filter_count_successors (f);
	if (!n) {
		if (!f->priv->bin || !gnome_print_filter_count_successors (f->priv->bin))
			return gnome_print_setrgbcolor_real (f->priv->pc, r, g, b);
		return gnome_print_setrgbcolor_real (
						f->priv->bin->priv->meta_bin_last, r, g, b);
	}

	/* This filter has successors */
	C (gnome_print_filter_setrgbcolor (gnome_print_filter_get_successor (f, 0), r, g, b));
	for (i = 1; i < n; i++)
		C (gnome_print_setrgbcolor_real (GNOME_PRINT_CONTEXT (
						g_ptr_array_index (f->priv->meta, i - 1)), r, g, b));
	return GNOME_PRINT_OK;
}

static gint
setopacity_impl (GnomePrintFilter *f, gdouble o)
{
	guint i, n;
	
	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), GNOME_PRINT_ERROR_BADVALUE);

	if (!gnome_print_filter_haspage (f))
		return GNOME_PRINT_OK;

	/* Check if this filter has filters inside. */
	n = gnome_print_filter_count_filters (f);
	if (n) {
		C (gnome_print_filter_setopacity (gnome_print_filter_get_filter (f, 0), o));
		for (i = 1; i < n; i++)
			C (gnome_print_setopacity_real (GNOME_PRINT_CONTEXT (
							g_ptr_array_index (f->priv->meta_bin, i - 1)), o));
		return GNOME_PRINT_OK;
	}

	/* Check if this filter has successors */
	n = gnome_print_filter_count_successors (f);
	if (!n) {
		if (!f->priv->bin || !gnome_print_filter_count_successors (f->priv->bin))
			return gnome_print_setopacity_real (f->priv->pc, o);
		return gnome_print_setopacity_real (f->priv->bin->priv->meta_bin_last, o);
	}

	/* This filter has successors */
	C (gnome_print_filter_setopacity (gnome_print_filter_get_successor (f, 0), o));
	for (i = 1; i < n; i++)
		C (gnome_print_setopacity_real (GNOME_PRINT_CONTEXT (
						g_ptr_array_index (f->priv->meta, i - 1)), o));
	return GNOME_PRINT_OK;
}

static gint
setlinewidth_impl (GnomePrintFilter *f, gdouble w)
{
	guint i, n;

	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), GNOME_PRINT_ERROR_BADVALUE);

	if (!gnome_print_filter_haspage (f))
		return GNOME_PRINT_OK;

	/* Check if this filter has filters inside. */
	n = gnome_print_filter_count_filters (f);
	if (n) {
		C (gnome_print_filter_setlinewidth (gnome_print_filter_get_filter (f, 0), w));
		for (i = 1; i < n; i++)
			C (gnome_print_setlinewidth_real (GNOME_PRINT_CONTEXT (
							g_ptr_array_index (f->priv->meta_bin, i - 1)), w));
		return GNOME_PRINT_OK;
	}

	/* Check if this filter has successors */
	n = gnome_print_filter_count_successors (f);
	if (!n) {
		if (!f->priv->bin || !gnome_print_filter_count_successors (f->priv->bin))
			return gnome_print_setlinewidth_real (f->priv->pc, w);
		return gnome_print_setlinewidth_real (f->priv->bin->priv->meta_bin_last, w);
	}

	/* This filter has successors */
	C (gnome_print_filter_setlinewidth (gnome_print_filter_get_successor (f, 0), w));
	for (i = 1; i < n; i++)
		C (gnome_print_setlinewidth_real (GNOME_PRINT_CONTEXT (
						g_ptr_array_index (f->priv->meta, i - 1)), w));
	return GNOME_PRINT_OK;
}

static void
flush_impl (GnomePrintFilter *f)
{
	guint i, n;

	n = gnome_print_filter_count_filters (f);
	if (n) {
		for (i = 0; i < n; i++)
			gnome_print_filter_flush (gnome_print_filter_get_filter (f, i));
	}
	n = gnome_print_filter_count_successors (f);
	if (n) {
		for (i = 0; i < n; i++)
			gnome_print_filter_flush (gnome_print_filter_get_successor (f, i));
	}
	if (f->priv->bin) {
		n = gnome_print_filter_count_successors (f->priv->bin);
		for (i = 0; i < n; i++)
			gnome_print_filter_flush (
					gnome_print_filter_get_successor (f->priv->bin, i));
	}
}

static void
reset_impl (GnomePrintFilter *f)
{
	guint i;

	gnome_print_filter_clear_data (f);

	for (i = gnome_print_filter_count_filters (f); i > 0; i--)
		gnome_print_filter_reset (gnome_print_filter_get_filter (f, i - 1));

	for (i = gnome_print_filter_count_successors (f); i > 0; i--)
		gnome_print_filter_reset (gnome_print_filter_get_successor (f, i - 1));
	for (i = gnome_print_filter_count_successors (f); i > 1; i--)
		gnome_print_meta_reset (g_ptr_array_index (f->priv->meta, i - 2));
	if (f->priv->bin)
		for (i = gnome_print_filter_count_successors (f->priv->bin); i > 0; i--)
			gnome_print_filter_reset (gnome_print_filter_get_successor (f->priv->bin,
															i - 1));
	g_object_set (G_OBJECT (f), "context", NULL, NULL);
}

void
gnome_print_filter_remove_predecessor (GnomePrintFilter *f, GnomePrintFilter *p)
{
	guint i;

	g_return_if_fail (GNOME_IS_PRINT_FILTER (f));
	g_return_if_fail (GNOME_IS_PRINT_FILTER (p));
	g_return_if_fail (gnome_print_filter_is_predecessor (f, p, FALSE));

	g_ptr_array_remove (f->priv->predecessors, p);
	if (!f->priv->predecessors->len) {
		g_ptr_array_free (f->priv->predecessors, TRUE);
		f->priv->predecessors = NULL;
	}
	for (i = 0; i < p->priv->successors->len; i++)
		if (f == g_ptr_array_index (p->priv->successors, i))
			break;
	g_ptr_array_remove_index (p->priv->successors, i);
	if (!p->priv->successors->len) {
		g_ptr_array_free (p->priv->successors, TRUE);
		p->priv->successors = NULL;
	}
	if (i > 0) {
		g_ptr_array_remove_index (p->priv->meta, i - 1);
		if (!p->priv->meta->len) {
			g_ptr_array_free (p->priv->meta, TRUE);
			p->priv->meta = NULL;
		}
	}
	g_signal_emit (G_OBJECT (f), signals[PREDECESSOR_REMOVED], 0, p);
	g_signal_emit (G_OBJECT (p), signals[SUCCESSOR_REMOVED  ], 0, f);
	g_object_unref (G_OBJECT (f));
	g_object_unref (G_OBJECT (p));
}

static void
gnome_print_filter_finalize (GObject *object)
{
	GnomePrintFilter *f = (GnomePrintFilter *) object;

	gnome_print_filter_clear_data (f);
	gnome_print_filter_remove_filters (f);

	if (f->priv) {
		while (f->priv->predecessors)
			gnome_print_filter_remove_predecessor (f,
				g_ptr_array_index (f->priv->predecessors, 0));
		while (f->priv->successors)
			gnome_print_filter_remove_predecessor (
				g_ptr_array_index (f->priv->successors, 0), f);
		if (f->priv->pc) {
			g_object_remove_weak_pointer (G_OBJECT (f->priv->pc),
				(gpointer *)&(f->priv->pc));
			f->priv->pc = NULL;
		}
		g_free (f->priv);
		f->priv = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
param_transform_set_default (GParamSpec *pspec, GValue *v)
{
	GValueArray *va = g_value_array_new (6);
	GValue vd = {0,};
	gdouble a[6];
	guint i;

	art_affine_identity (a);
	g_value_init (&vd, G_TYPE_DOUBLE);
	for (i = 0; i < 6; i++) {
		g_value_set_double (&vd, a[i]);
		g_value_array_append (va, &vd);
	}
	g_value_unset (&vd);
	g_value_set_boxed (v, va);
	g_value_array_free (va);
}

struct {
	GParamSpecValueArray parent_instance;
} GnomePrintFilterParamTransform;

static void
param_transform_init (GParamSpec *pspec)
{
	GParamSpecValueArray *pspec_va = (GParamSpecValueArray *) pspec;

	pspec_va->element_spec = g_param_spec_double ("transform_item",
		_("Transform item"), _("Transform item"), -G_MAXDOUBLE, G_MAXDOUBLE, 0.,
		G_PARAM_READWRITE);
	g_param_spec_ref (pspec_va->element_spec);
	g_param_spec_sink (pspec_va->element_spec);
}

static gint
param_transform_cmp (GParamSpec *pspec, const GValue *value1,
								const GValue *value2)
{
	GValueArray *value_array1 = g_value_get_boxed (value1);
	GValueArray *value_array2 = g_value_get_boxed (value2);

	if (!value_array1 || !value_array2)
		return value_array2 ? -1 : value_array1 != value_array2;
	if (value_array1->n_values != value_array2->n_values)
		return value_array1->n_values < value_array2->n_values ? -1 : 1;
	else {
		guint i;
		
		for (i = 0; i < value_array1->n_values; i++) {
			GValue *element1 = value_array1->values + i;
			GValue *element2 = value_array2->values + i;
			gint cmp;

			if (G_VALUE_TYPE (element1) != G_VALUE_TYPE (element2))
				return G_VALUE_TYPE (element1) < G_VALUE_TYPE (element2) ? -1 : 1;
			cmp = g_param_values_cmp (((GParamSpecValueArray *) pspec)->element_spec,
				element1, element2);
			if (cmp)
				return cmp;
		}
		return 0;
	}
}

static GType
gnome_print_filter_param_transform_get_type (void)
{
	static GType type;
	if (G_UNLIKELY (type) == 0) {
		static GParamSpecTypeInfo pspec_info = {
			sizeof (GnomePrintFilterParamTransform), 0,
			param_transform_init,
			0xdeadbeef, NULL, param_transform_set_default, NULL,
			param_transform_cmp
		};
		pspec_info.value_type = G_TYPE_VALUE_ARRAY;
		type = g_param_type_register_static ("GnomePrintFilterParamTransform", &pspec_info);
	}
	return type;
}

static void
gnome_print_filter_class_init (GnomePrintFilterClass *filter_class)
{
	GObjectClass *object_class = (GObjectClass *) filter_class;
	GParamSpec *pspec;

	parent_class = g_type_class_peek_parent (filter_class);

	filter_class->beginpage    = beginpage_impl;
	filter_class->showpage     = showpage_impl;
	filter_class->gsave        = gsave_impl;
	filter_class->grestore     = grestore_impl;
	filter_class->clip         = clip_impl;
	filter_class->fill         = fill_impl;
	filter_class->stroke       = stroke_impl;
	filter_class->image        = image_impl;
	filter_class->glyphlist    = glyphlist_impl;
	filter_class->setrgbcolor  = setrgbcolor_impl;
	filter_class->setopacity   = setopacity_impl;
	filter_class->setlinewidth = setlinewidth_impl;
	filter_class->flush        = flush_impl;
	filter_class->reset        = reset_impl;

	object_class->finalize     = gnome_print_filter_finalize;
	object_class->get_property = gnome_print_filter_get_property;
	object_class->set_property = gnome_print_filter_set_property;

	g_object_class_install_property (object_class,
		PROP_SUPPRESS_EMPTY_PAGES,
		g_param_spec_boolean ("suppress_empty_pages",
			_("Suppress empty pages"), _("Suppress empty pages"),
			FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_NAME,
		g_param_spec_string ("name", _("Name"), _("Name"),
			_(FILTER_NAME), G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_DESCRIPTION,
		g_param_spec_string ("description", _("Description"),
			_("Description"), _(FILTER_DESCRIPTION),
			G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_CONTEXT,
		g_param_spec_object ("context", _("Context"), _("Context"),
			GNOME_TYPE_PRINT_CONTEXT, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_FILTERS,
		g_param_spec_value_array ("filters", _("Filters"), _("Filters"),
			g_param_spec_object ("filter", _("Filter"), _("Filter"),
				GNOME_TYPE_PRINT_FILTER, G_PARAM_READWRITE), G_PARAM_READWRITE));
	pspec = g_param_spec_internal (
		gnome_print_filter_param_transform_get_type (),
		"transform", _("Transform"), _("Transform"), G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_TRANSFORM, pspec);

	signals[CHANGED] = g_signal_new ("changed",
		G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GnomePrintFilterClass, changed), NULL, NULL,
		g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0, G_TYPE_NONE);
	signals[PREDECESSOR_ADDED] = g_signal_new ("predecessor_added",
		G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GnomePrintFilterClass, predecessor_added), NULL,
		NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
		GNOME_TYPE_PRINT_FILTER);
	signals[PREDECESSOR_REMOVED] = g_signal_new ("predecessor_removed",
		G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GnomePrintFilterClass, predecessor_removed), NULL,
		NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
		GNOME_TYPE_PRINT_FILTER);
	signals[SUCCESSOR_ADDED] = g_signal_new ("successor_added",
		G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GnomePrintFilterClass, successor_added), NULL,
		NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
		GNOME_TYPE_PRINT_FILTER);
	signals[SUCCESSOR_REMOVED] = g_signal_new ("successor_removed",
		G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GnomePrintFilterClass, successor_removed), NULL,
		NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
		GNOME_TYPE_PRINT_FILTER);
}

static void
gnome_print_filter_init (GnomePrintFilter *f)
{
	f->priv = g_new0 (GnomePrintFilterPrivate, 1);

	art_affine_identity (f->priv->transform);
}

GType
gnome_print_filter_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GnomePrintFilterClass), NULL, NULL,
			(GClassInitFunc) gnome_print_filter_class_init,
			NULL, NULL, sizeof (GnomePrintFilter), 0,
			(GInstanceInitFunc) gnome_print_filter_init
		};
		type = g_type_register_static (G_TYPE_OBJECT,
				"GnomePrintFilter", &info, 0);
	}
	return type;
}

void
gnome_print_filter_changed (GnomePrintFilter *filter)
{
	g_return_if_fail (GNOME_IS_PRINT_FILTER (filter));

	g_signal_emit (G_OBJECT (filter), signals[CHANGED], 0);
}

void
gnome_print_filter_append_predecessor (GnomePrintFilter *f, GnomePrintFilter *p)
{
	g_return_if_fail (GNOME_IS_PRINT_FILTER (f));
	g_return_if_fail (GNOME_IS_PRINT_FILTER (p));
	g_return_if_fail (!gnome_print_filter_is_predecessor (f, p, FALSE));
	g_return_if_fail (f != p);

	if (!f->priv->predecessors) f->priv->predecessors = g_ptr_array_new ();
	g_ptr_array_add (f->priv->predecessors, p);
	if (!p->priv->successors) p->priv->successors = g_ptr_array_new ();
	g_ptr_array_add (p->priv->successors, f);
	if (gnome_print_filter_count_successors (p) > 1) {
		if (!p->priv->meta) p->priv->meta = g_ptr_array_new ();
		g_ptr_array_add (p->priv->meta, g_object_new (GNOME_TYPE_PRINT_META, NULL));
	}
	g_object_ref (G_OBJECT (f));
	g_object_ref (G_OBJECT (p));
	g_signal_emit (G_OBJECT (p), signals[SUCCESSOR_ADDED], 0, f);
	g_signal_emit (G_OBJECT (f), signals[PREDECESSOR_ADDED], 0, p);
}

gboolean
gnome_print_filter_is_predecessor (GnomePrintFilter *f, GnomePrintFilter *p, gboolean indirect)
{
	guint i;

	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), FALSE);
	g_return_val_if_fail (!p || GNOME_IS_PRINT_FILTER (p), FALSE);

	if (!f->priv->predecessors) return FALSE;
	for (i = 0; i < f->priv->predecessors->len; i++) {
		GnomePrintFilter *t = g_ptr_array_index (f->priv->predecessors, i);
		if (t == p) return TRUE;
		if (indirect && t && gnome_print_filter_is_predecessor (t, p, TRUE))
			return TRUE;
	}
	return FALSE;
}

guint
gnome_print_filter_count_predecessors (GnomePrintFilter *f)
{
	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), 0);

	return f->priv->predecessors ? f->priv->predecessors->len : 0;
}

guint
gnome_print_filter_count_successors (GnomePrintFilter *f)
{
	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), 0);

	return f->priv->successors ? f->priv->successors->len : 0;
}

GnomePrintFilter *
gnome_print_filter_get_predecessor (GnomePrintFilter *f, guint n)
{
	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), NULL);
	g_return_val_if_fail (f->priv->predecessors, NULL);
	g_return_val_if_fail (n < f->priv->predecessors->len, NULL);

	return g_ptr_array_index (f->priv->predecessors, n);
}

GnomePrintFilter *
gnome_print_filter_get_successor (GnomePrintFilter *f, guint n)
{
	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), NULL);
	g_return_val_if_fail (n < gnome_print_filter_count_successors (f), NULL);

	return f->priv->successors ? g_ptr_array_index (f->priv->successors, n) : NULL;
}

static GnomePrintFilter *
gnome_print_filter_new_from_module_name_valist (const gchar *module_name,
		const gchar *first_property_name, va_list var_args)
{
	static GHashTable *modules;
	GModule *module;
	gpointer get_type;
	GType (* filter_get_type) (void);

	if (!strcmp (module_name, "GnomePrintFilter"))
		return GNOME_PRINT_FILTER (g_object_new_valist (GNOME_TYPE_PRINT_FILTER,
				first_property_name, var_args));

	if (!modules)
		modules = g_hash_table_new (g_str_hash, g_str_equal);
	module = g_hash_table_lookup (modules, module_name);
	if (!module) {
		gchar *dir, *path, *m;
		
		dir = g_build_filename (GNOME_PRINT_MODULES_DIR, "filters", NULL);
		m = g_strdup_printf ("gnomeprint-%s", module_name);
		path = g_module_build_path (dir, m);
		g_free (dir);
		g_free (m);
		module = g_module_open (path, G_MODULE_BIND_LAZY);
		g_free (path);
		if (module)
			g_hash_table_insert (modules, g_strdup (module_name), module);
	}
	if (!module)
		return NULL;
	if (!g_module_symbol (module, "gnome_print__filter_get_type", &get_type)) {
		g_hash_table_remove (modules, module_name);
		g_module_close (module);
		return NULL;
	}
	filter_get_type = get_type;
	return GNOME_PRINT_FILTER (g_object_new_valist (filter_get_type (),
				first_property_name, var_args));
}

GnomePrintFilter *
gnome_print_filter_new_from_module_name (const gchar *module_name,
		const gchar *first_property_name, ...)
{
	GnomePrintFilter *f;
	va_list var_args;

	g_return_val_if_fail (module_name, NULL);

	/*
	 * We are kind to our users: We accept GnomePrintFilterZoom, Zoom, zoom.
	 */
	va_start (var_args, first_property_name);
	f = GNOME_PRINT_FILTER (gnome_print_filter_new_from_module_name_valist (
				module_name, first_property_name, var_args));
	if (!f) {
		gchar *m, *ml;

		if (!strncmp (module_name, "GnomePrintFilter", 16))
			ml = g_strdup (module_name + 16);
		else
			ml = g_strdup (module_name);
		for (m = ml; *m; m++) *m = tolower (*m);
		f = GNOME_PRINT_FILTER (gnome_print_filter_new_from_module_name_valist (ml,
					first_property_name, var_args));
		g_free (ml);
	}
	va_end (var_args);

	return f;
}

static gchar *
gnome_print_filter_description_rec (GnomePrintFilter *f, const gchar *in, 
		gboolean brackets)
{
	gchar *u, *t = NULL, *buf;
	guint i, n, n_props, j;
	GParamSpec **props;

	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), NULL);

	n = gnome_print_filter_count_successors (f);
	if (brackets)
		buf = g_strdup_printf ("%s { %s", in, G_OBJECT_TYPE_NAME (f));
	else
		if (!strlen (in))
			buf = g_strdup_printf ("%s", G_OBJECT_TYPE_NAME (f));
		else
			buf = g_strdup_printf ("%s %s", in, G_OBJECT_TYPE_NAME (f));
	props = g_object_class_list_properties (G_OBJECT_GET_CLASS (f), &n_props);
	for (j = 0; j < n_props; j++) {
		GValue v = {0,};

		/*
		 * We only save writable parameters. We skip the parameter 
		 * 'filters'.
		 */
		if (!(props[j]->flags & G_PARAM_WRITABLE))
			continue;
		if (!strcmp (props[j]->name, "filters"))
			continue;

		g_value_init (&v, G_PARAM_SPEC_VALUE_TYPE (props[j]));
		g_object_get_property (G_OBJECT (f), props[j]->name, &v);
		if (g_param_value_defaults (props[j], &v)) {
			g_value_unset (&v);
			continue;
		}
		if (G_IS_PARAM_SPEC_STRING (props[j])) {
			t = g_strdup_printf ("%s %s='%s'", buf, props[j]->name,
					g_value_get_string (&v));
			g_free (buf); buf = t;
		} else if (G_IS_PARAM_SPEC_INT (props[j])) {
			t = g_strdup_printf ("%s %s=%i", buf, props[j]->name,
					g_value_get_int (&v));
			g_free (buf); buf = t;
		} else if (G_IS_PARAM_SPEC_UINT (props[j])) {
			t = g_strdup_printf ("%s %s=%i", buf, props[j]->name,
					g_value_get_uint (&v));
			g_free (buf); buf = t;
		} else if (G_IS_PARAM_SPEC_DOUBLE (props[j])) {
			t = g_strdup_printf ("%s %s=%f", buf, props[j]->name,
					g_value_get_double (&v));
			g_free (buf); buf = t;
		} else if (G_IS_PARAM_SPEC_ULONG (props[j])) {
			t = g_strdup_printf ("%s %s=%li", buf, props[j]->name,
					g_value_get_ulong (&v));
			g_free (buf); buf = t;
		} else if (G_IS_PARAM_SPEC_ENUM (props[j])) {
			t = g_strdup_printf ("%s %s=%i", buf, props[j]->name,
					g_value_get_enum (&v));
			g_free (buf); buf = t;
		} else if (G_IS_PARAM_SPEC_BOOLEAN (props[j])) {
			t = g_strdup_printf ("%s %s=%s", buf, props[j]->name,
					g_value_get_boolean (&v) ? "true" : "false");
			g_free (buf); buf = t;
		} else if (G_IS_PARAM_SPEC_OBJECT (props[j])) {
			/* Do nothing */
		} else if (props[j]->value_type == G_TYPE_VALUE_ARRAY) {
			GValueArray *va = g_value_get_boxed (&v);

			if (va) {
				gchar *s = g_strdup ("");
				guint i;

				for (i = 0; i < va->n_values; i++) {
					GValue *val = g_value_array_get_nth (va, i);

					if (G_VALUE_HOLDS_INT (val))
						t = g_strdup_printf ("%s%i", s, g_value_get_int (val));
					else if (G_VALUE_HOLDS_UINT (val))
						t = g_strdup_printf ("%s%i", s, g_value_get_uint (val));
					else if (G_VALUE_HOLDS_LONG (val))
						t = g_strdup_printf ("%s%li", s, g_value_get_long (val));
					else if (G_VALUE_HOLDS_ULONG (val))
						t = g_strdup_printf ("%s%li", s, g_value_get_ulong (val));
					else if (G_VALUE_HOLDS_BOOLEAN (val))
						t = g_strdup_printf ("%s%s", s,
								g_value_get_boolean (val) ? "true" : "false");
					else if (G_VALUE_HOLDS_DOUBLE (val))
						t = g_strdup_printf ("%s%f", s, g_value_get_double (val));
					else if (G_VALUE_HOLDS_STRING (val))
						t = g_strdup_printf ("%s%s", s, g_value_get_string (val));
					else if (G_VALUE_HOLDS_OBJECT (val)) {
						/* Do nothing */
					} else {
						t = g_strdup_printf ("%s?", s);
						g_warning ("Implement this!");
					}
					g_free (s); s = t;
					if (i < va->n_values - 1) {
						t = g_strdup_printf ("%s,", s);
						g_free (s); s = t;
					}
				}
				if (s) {
					t = g_strdup_printf ("%s %s=%s", buf, props[j]->name, s);
					g_free (buf); g_free (s); buf = t;
				}
			}
		} else
			g_warning ("Implement %s!", g_type_name (props[j]->value_type));
		g_value_unset (&v);
	}
	g_free (props);

	/* Bin */
	if (gnome_print_filter_count_filters (f)) {
		guint n_bin;

		n_bin = gnome_print_filter_count_filters (f);
		t = g_strdup_printf ("%s [", buf);
		g_free (buf); buf = t;
		for (i = 0; i < n_bin; i++) {
			GnomePrintFilter *s = gnome_print_filter_get_filter (f, i);

			t = gnome_print_filter_description_rec (s, "", n_bin > 1);
			u = g_strdup_printf ("%s%s", buf, t);
			g_free (buf); g_free (t); buf = u;
		}
		t = g_strdup_printf ("%s ]", buf);
		g_free (buf); buf = t;
	}

	/* Regular successors */
	if (n) {
		t = g_strdup_printf ("%s !", buf);
		g_free (buf); buf = t;
		for (i = 0; i < n; i++) {
			GnomePrintFilter *s = gnome_print_filter_get_successor (f, i);

			t = gnome_print_filter_description_rec (s, "", n > 1);
			u = g_strdup_printf ("%s %s", buf, t);
			g_free (buf); g_free (t); buf = u;
		}
	}

	if (brackets) {
		t = g_strdup_printf ("%s }", buf);
		g_free (buf); buf = t;
	}

	return buf;
}

gchar *
gnome_print_filter_description (GnomePrintFilter *f)
{
	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), NULL);

	return gnome_print_filter_description_rec (f, "", FALSE);
}

extern GnomePrintFilter *_gnome_print_filter_parse_launch (const gchar *, GError **);

GnomePrintFilter *
gnome_print_filter_new_from_description (const gchar *description, GError **e)
{
	GError *e_int = NULL;
	GnomePrintFilter *f;
	
	f = _gnome_print_filter_parse_launch (description, e ? e : &e_int);
	if (e_int) {
		g_warning ("Could not create filter from description '%s': %s",
				description, e_int->message);
		g_error_free (e_int);
	}
	return f;
}

gint
gnome_print_filter_beginpage (GnomePrintFilter *f, GnomePrintContext *pc,
		const guchar *name)
{
	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), GNOME_PRINT_ERROR_UNKNOWN);
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_UNKNOWN);
	g_return_val_if_fail (!gnome_print_filter_haspage (f) || (pc == f->priv->pc), GNOME_PRINT_ERROR_UNKNOWN);

	if (!f->priv->pc)
		g_object_set (G_OBJECT (f), "context", pc, NULL);
	return GPFGC (f)->beginpage ?
		GPFGC (f)->beginpage (f, pc, name) : GNOME_PRINT_OK;
}

gint
gnome_print_filter_showpage (GnomePrintFilter *f)
{
	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), GNOME_PRINT_ERROR_UNKNOWN);

	return (GPFGC (f)->showpage) ?
		GPFGC (f)->showpage (f) : GNOME_PRINT_OK;
}

gint
gnome_print_filter_gsave (GnomePrintFilter *f)
{
	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), GNOME_PRINT_ERROR_UNKNOWN);

	return GPFGC (f)->gsave ? GPFGC (f)->gsave (f) : GNOME_PRINT_OK;
}

gint
gnome_print_filter_grestore (GnomePrintFilter *f)
{
	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), GNOME_PRINT_ERROR_UNKNOWN);

	return GPFGC (f)->grestore ? GPFGC (f)->grestore (f) : GNOME_PRINT_OK;
}

gint
gnome_print_filter_stroke (GnomePrintFilter *f, const ArtBpath *b)
{
	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), GNOME_PRINT_ERROR_UNKNOWN);

	return GPFGC (f)->stroke ? GPFGC (f)->stroke (f, b) : GNOME_PRINT_OK;
}

gint
gnome_print_filter_fill (GnomePrintFilter *f, const ArtBpath *b, ArtWindRule r)
{
	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), GNOME_PRINT_ERROR_UNKNOWN);

	return GPFGC (f)->fill ? GPFGC (f)->fill (f, b, r) : GNOME_PRINT_OK;
}

gint
gnome_print_filter_clip (GnomePrintFilter *f, const ArtBpath *b, ArtWindRule r)
{
	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), GNOME_PRINT_ERROR_UNKNOWN);

	return GPFGC (f)->clip ? GPFGC (f)->clip (f, b, r) : GNOME_PRINT_OK;
}

gint
gnome_print_filter_image (GnomePrintFilter *f, const gdouble *a, const guchar *p,
		gint w, gint h, gint r, gint ch)
{
	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), GNOME_PRINT_ERROR_UNKNOWN);

	return GPFGC (f)->image ? GPFGC (f)->image (f, a, p, w, h, r, ch) :
		GNOME_PRINT_OK;
}

gint
gnome_print_filter_glyphlist (GnomePrintFilter *f, const gdouble *a, GnomeGlyphList *gl)
{
	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), GNOME_PRINT_ERROR_UNKNOWN);

	return GPFGC (f)->glyphlist ?
		GPFGC (f)->glyphlist (f, a, gl) : GNOME_PRINT_OK;
}

gint
gnome_print_filter_setrgbcolor (GnomePrintFilter *f, gdouble r, gdouble g, gdouble b)
{
	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), GNOME_PRINT_ERROR_UNKNOWN);

	return GPFGC (f)->setrgbcolor ?
		GPFGC (f)->setrgbcolor (f, r, g, b) : GNOME_PRINT_OK;
}

gint
gnome_print_filter_setopacity (GnomePrintFilter *f, gdouble o)
{
	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), GNOME_PRINT_ERROR_UNKNOWN);

	return GPFGC (f)->setopacity ?
		GPFGC (f)->setopacity (f, o) : GNOME_PRINT_OK;
}

gint
gnome_print_filter_setlinewidth (GnomePrintFilter *f, gdouble w)
{
	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), GNOME_PRINT_ERROR_UNKNOWN);

	return GPFGC (f)->setlinewidth ?
		GPFGC (f)->setlinewidth (f, w) : GNOME_PRINT_OK;
}

void
gnome_print_filter_reset (GnomePrintFilter *f)
{
	g_return_if_fail (GNOME_IS_PRINT_FILTER (f));

	if (GPFGC (f)->reset)
		GPFGC (f)->reset (f);
}

void
gnome_print_filter_flush (GnomePrintFilter *f)
{
	g_return_if_fail (GNOME_IS_PRINT_FILTER (f));

	if (GPFGC (f)->flush)
		GPFGC (f)->flush (f);
}

guint
gnome_print_filter_count_filters (GnomePrintFilter *f)
{
	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), 0);

	return f->priv->filters ? f->priv->filters->len : 0;
}

GnomePrintFilter *
gnome_print_filter_get_filter (GnomePrintFilter *f, guint n)
{
	g_return_val_if_fail (GNOME_IS_PRINT_FILTER (f), NULL);
	g_return_val_if_fail (n < gnome_print_filter_count_filters (f), NULL);

	return GNOME_PRINT_FILTER (g_ptr_array_index (f->priv->filters, n));
}

void
gnome_print_filter_remove_filters (GnomePrintFilter *f)
{
	g_return_if_fail (GNOME_IS_PRINT_FILTER (f));

	g_object_freeze_notify (G_OBJECT (f));
	while (gnome_print_filter_count_filters (f))
		gnome_print_filter_remove_filter (f, gnome_print_filter_get_filter (f, 0));
	g_object_thaw_notify (G_OBJECT (f));
}

void
gnome_print_filter_remove_filter (GnomePrintFilter *f, GnomePrintFilter *fs)
{
	guint i;

	g_return_if_fail (GNOME_IS_PRINT_FILTER (f));
	g_return_if_fail (GNOME_IS_PRINT_FILTER (fs));

	if (!f->priv->filters) return;
	for (i = gnome_print_filter_count_filters (f); i > 0; i--)
		if (gnome_print_filter_get_filter (f, i - 1) == fs) break;
	if (!i) return;
	g_ptr_array_remove_index (f->priv->filters, i - 1);
	g_object_unref (G_OBJECT (fs));
	if (!gnome_print_filter_count_filters (f)) {
		g_ptr_array_free (f->priv->filters, TRUE);
		f->priv->filters = NULL;
	}
	g_object_notify (G_OBJECT (f), "filters");
}

void
gnome_print_filter_add_filter (GnomePrintFilter *f, GnomePrintFilter *fs)
{
	guint i;

	g_return_if_fail (GNOME_IS_PRINT_FILTER (f));
	g_return_if_fail (GNOME_IS_PRINT_FILTER (fs));

	for (i = gnome_print_filter_count_filters (f); i > 0; i--)
		if (gnome_print_filter_get_filter (f, i - 1) == fs) return;
	g_object_ref (G_OBJECT (fs));
	if (fs->priv->bin) gnome_print_filter_remove_filter (fs->priv->bin, fs);
	fs->priv->bin = f;
	if (!f->priv->filters)
		f->priv->filters = g_ptr_array_new ();
	g_ptr_array_add (f->priv->filters, fs);
	g_object_notify (G_OBJECT (f), "filters");
}

GQuark
gnome_print_filter_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("gnome-print-filter-error-quark");
	return quark; 
}
