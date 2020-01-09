#include <config.h>
#include "gnome-print-filter-reverse.h"

#include <math.h>
#include <string.h>

#include <gmodule.h>

#include <libgnomeprint/gnome-print-filter.h>
#include <libgnomeprint/gnome-print-i18n.h>
#include <libgnomeprint/gnome-print-meta.h>
#include <libgnomeprint/gnome-print-private.h>
#include <libgnomeprint/gp-gc-private.h>

#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_vpath_dash.h>
#include <libart_lgpl/art_vpath_bpath.h>

#define FILTER_NAME        N_("reverse")
#define FILTER_DESCRIPTION N_("The reverse-filter prints in reverse order.")

struct _GnomePrintFilterReverse {
	GnomePrintFilter parent;

	GnomePrintContext *pc;
	GnomePrintContext *meta;
};

struct _GnomePrintFilterReverseClass {
	GnomePrintFilterClass parent_class;
};

static GnomePrintFilterClass *parent_class = NULL;

enum {
	PROP_0,
	PROP_NAME,
	PROP_DESCRIPTION
};

static void
gnome_print_filter_reverse_get_property (GObject *object, guint n, GValue *v,
				GParamSpec *pspec)
{
	switch (n) {
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
gnome_print_filter_reverse_set_property (GObject *object, guint n,
		const GValue *v, GParamSpec *pspec)
{
	switch (n) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, n, pspec);
	}
}

#define C(res) { gint ret = (res); if (ret < 0) return ret; }

static gint
beginpage_impl (GnomePrintFilter *filter,
		GnomePrintContext *pc, const guchar *name)
{
	GnomePrintFilterReverse *f = GNOME_PRINT_FILTER_REVERSE (filter);

	if (!f->meta)
		f->meta = gnome_print_meta_new ();
	if (f->pc)
		g_object_unref (G_OBJECT (f->pc));
	f->pc = pc;
	g_object_ref (G_OBJECT (f->pc));

	return gnome_print_beginpage_real (f->meta, name);
}

static gint
showpage_impl (GnomePrintFilter *filter)
{
	GnomePrintFilterReverse *f = GNOME_PRINT_FILTER_REVERSE (filter);

	return gnome_print_showpage_real (f->meta);
}

static gint
gsave_impl (GnomePrintFilter *filter)
{
	GnomePrintFilterReverse *f = GNOME_PRINT_FILTER_REVERSE (filter);

	return gnome_print_gsave_real (f->meta);
}

static gint
grestore_impl (GnomePrintFilter *filter)
{
	GnomePrintFilterReverse *f = GNOME_PRINT_FILTER_REVERSE (filter);

	return gnome_print_grestore_real (f->meta);
}

static gint
clip_impl (GnomePrintFilter *filter,
	   const ArtBpath *bpath, ArtWindRule rule)
{
	GnomePrintFilterReverse *f = GNOME_PRINT_FILTER_REVERSE (filter);

	return gnome_print_clip_bpath_rule_real (f->meta, bpath, rule);
}

static gint
stroke_impl (GnomePrintFilter *filter, const ArtBpath *bpath)
{
	GnomePrintFilterReverse *f = GNOME_PRINT_FILTER_REVERSE (filter);

	return gnome_print_stroke_bpath_real  (f->meta, bpath);
}

static gint
fill_impl (GnomePrintFilter *filter,
	   const ArtBpath *bpath, ArtWindRule rule)
{
	GnomePrintFilterReverse *f = GNOME_PRINT_FILTER_REVERSE (filter);

	return gnome_print_fill_bpath_rule_real (f->meta, bpath, rule);
}

static gint
image_impl (GnomePrintFilter *filter, const gdouble *a_src,
	    const guchar *b, gint w, gint h, gint r, gint c)
{
	GnomePrintFilterReverse *f = GNOME_PRINT_FILTER_REVERSE (filter);

	return gnome_print_image_transform_real (f->meta, a_src, b, w, h, r, c);
}

static gint
glyphlist_impl (GnomePrintFilter *filter,
		const gdouble *a_src, GnomeGlyphList *gl)
{
	GnomePrintFilterReverse *f = GNOME_PRINT_FILTER_REVERSE (filter);

	return gnome_print_glyphlist_transform_real (f->meta, a_src, gl);
}

static gint
setrgbcolor_impl (GnomePrintFilter *filter, gdouble r, gdouble g, gdouble b)
{
	GnomePrintFilterReverse *f = GNOME_PRINT_FILTER_REVERSE (filter);

	return gnome_print_setrgbcolor_real (f->meta, r, g, b);
}

static gint
setopacity_impl (GnomePrintFilter *filter, gdouble o)
{
	GnomePrintFilterReverse *f = GNOME_PRINT_FILTER_REVERSE (filter);

	return gnome_print_setopacity_real (f->meta, o);
}

static gint
setlinewidth_impl (GnomePrintFilter *filter, gdouble w)
{
	GnomePrintFilterReverse *f = GNOME_PRINT_FILTER_REVERSE (filter);

	return gnome_print_setlinewidth_real (f->meta, w);
}

static void
flush_impl (GnomePrintFilter *filter)
{
	GnomePrintFilterReverse *f = GNOME_PRINT_FILTER_REVERSE (filter);
	guint i, m = gnome_print_filter_count_successors (filter);
	guint j, n = gnome_print_meta_get_pages (GNOME_PRINT_META (f->meta));
	GnomePrintFilter *f_orig = NULL;

	g_object_get (G_OBJECT (f->pc), "filter", &f_orig, NULL);
	if (!m) {
		g_object_set (G_OBJECT (f->pc), "filter", NULL, NULL);
		for (j = n; j > 0; j--)
			gnome_print_meta_render_page (GNOME_PRINT_META (f->meta), f->pc,
					j - 1, TRUE);
	} else {
		for (i = 0; i < m; i++) {
			g_object_set (G_OBJECT (f->pc), "filter",
					gnome_print_filter_get_successor (filter, i), NULL);
			for (j = n; j > 0; j--)
				gnome_print_meta_render_page (GNOME_PRINT_META (f->meta), f->pc,
						j - 1, TRUE);
		}
	}
	g_object_set (G_OBJECT (f->pc), "filter", f_orig, NULL);
	gnome_print_meta_reset (GNOME_PRINT_META (f->meta));
}

static void
gnome_print_filter_reverse_finalize (GObject *object)
{
	GnomePrintFilterReverse *f = GNOME_PRINT_FILTER_REVERSE (object);

	if (f->meta) {
		g_object_unref (G_OBJECT (f->meta));
		f->meta = NULL;
	}
	if (f->pc) {
		g_object_unref (G_OBJECT (f->pc));
		f->pc = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gnome_print_filter_reverse_class_init (GnomePrintFilterReverseClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;
	GnomePrintFilterClass *f_class = (GnomePrintFilterClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize     = gnome_print_filter_reverse_finalize;
	object_class->get_property = gnome_print_filter_reverse_get_property;
	object_class->set_property = gnome_print_filter_reverse_set_property;

	g_object_class_override_property (object_class, PROP_NAME, "name");
	g_object_class_override_property (object_class, PROP_DESCRIPTION, "description");

	f_class->beginpage    = beginpage_impl;
	f_class->showpage     = showpage_impl;
	f_class->gsave        = gsave_impl;
	f_class->grestore     = grestore_impl;
	f_class->fill         = fill_impl;
	f_class->clip         = clip_impl;
	f_class->stroke       = stroke_impl;
	f_class->image        = image_impl;
	f_class->glyphlist    = glyphlist_impl;
	f_class->setrgbcolor  = setrgbcolor_impl;
	f_class->setopacity   = setopacity_impl;
	f_class->setlinewidth = setlinewidth_impl;
	f_class->flush        = flush_impl;
}

GType
gnome_print_filter_reverse_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GnomePrintFilterReverseClass), NULL, NULL,
			(GClassInitFunc) gnome_print_filter_reverse_class_init,
			NULL, NULL, sizeof (GnomePrintFilterReverse), 0, NULL
		};
		type = g_type_register_static (GNOME_TYPE_PRINT_FILTER,
				"GnomePrintFilterReverse", &info, 0);
	}
	return type;
}

G_MODULE_EXPORT GType gnome_print__filter_get_type (void);

G_MODULE_EXPORT GType
gnome_print__filter_get_type (void)
{
	return GNOME_TYPE_PRINT_FILTER_REVERSE;
}
