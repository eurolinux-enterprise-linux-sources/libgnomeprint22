#include <config.h>
#include "gnome-print-filter-frgba.h"

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

#define FILTER_NAME        N_("frgba")
#define FILTER_DESCRIPTION N_("The frgba-filter renders semitransparent objects as bitmaps.")

struct _GnomePrintFilterFrgba {
	GnomePrintFilter parent;

	GnomePrintContext *pc;
	GnomePrintContext *meta;
};

struct _GnomePrintFilterFrgbaClass {
	GnomePrintFilterClass parent_class;
};

static GnomePrintFilterClass *parent_class = NULL;

enum {
	PROP_0,
	PROP_NAME,
	PROP_DESCRIPTION
};

static void
gnome_print_filter_frgba_get_property (GObject *object, guint n, GValue *v,
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
gnome_print_filter_frgba_set_property (GObject *object, guint n,
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
	GnomePrintFilterFrgba *f = (GnomePrintFilterFrgba *) filter;

	f->pc = pc;

	gnome_print_meta_reset (GNOME_PRINT_META (f->meta));
	C (gnome_print_beginpage_real (f->meta, name));
	return parent_class->beginpage (filter, pc, name);
}

static gint
showpage_impl (GnomePrintFilter *filter)
{
	GnomePrintFilterFrgba *f = (GnomePrintFilterFrgba *) filter;

	return parent_class->showpage (filter);
}

static gint
gsave_impl (GnomePrintFilter *filter)
{
	GnomePrintFilterFrgba *f = (GnomePrintFilterFrgba *) filter;

	C (gnome_print_gsave_real (f->meta));
	return parent_class->gsave (filter);
}

static gint
grestore_impl (GnomePrintFilter *filter)
{
	GnomePrintFilterFrgba *f = (GnomePrintFilterFrgba *) filter;

	C (gnome_print_grestore_real (f->meta));
	return parent_class->grestore (filter);
}

static gint
clip_impl (GnomePrintFilter *filter,
	   const ArtBpath *bpath, ArtWindRule rule)
{
	GnomePrintFilterFrgba *f = (GnomePrintFilterFrgba *) filter;

	C (gnome_print_clip_bpath_rule_real (f->meta, bpath, rule));
	return parent_class->clip (filter, bpath, rule);
}

static gint
stroke_impl (GnomePrintFilter *filter, const ArtBpath *bpath)
{
	GnomePrintFilterFrgba *f = (GnomePrintFilterFrgba *) filter;
	const ArtVpathDash *dash = gp_gc_get_dash (f->pc->gc);

	C (gnome_print_setlinewidth  (f->meta, gp_gc_get_linewidth  (f->pc->gc)));
	C (gnome_print_setmiterlimit (f->meta, gp_gc_get_miterlimit (f->pc->gc)));
	C (gnome_print_setlinejoin   (f->meta, gp_gc_get_linejoin   (f->pc->gc)));
	C (gnome_print_setlinecap    (f->meta, gp_gc_get_linecap    (f->pc->gc)));
	C (gnome_print_setdash       (f->meta, dash->n_dash, dash->dash, dash->offset));
	C (gnome_print_stroke_bpath_real  (f->meta, bpath));

	return parent_class->stroke (filter, bpath);
}

static void
gnome_print_filter_frgba_render_buf (GnomePrintFilterFrgba *f, ArtDRect *box)
{
	gdouble width, height;
	guint w, h;
	guchar *pixels;
	gdouble page2buf[6], a[6];
	GnomePrintContext *rbuf;

	width  = MAX (0., ceil ((box->x1 - box->x0) * 72. / 72.));
	height = MAX (0., ceil ((box->y1 - box->y0) * 72. / 72.));
	w = (guint) width;
	h = (guint) height;
	if (!w || !h) return;
	pixels = g_new (guchar, w * h * 3);
	memset (pixels, 0xff, w * h * 3);
	art_affine_translate (page2buf, -box->x0, -box->y1);
	art_affine_scale (a, width / (box->x1 - box->x0), -height / (box->y1 - box->y0));
	art_affine_multiply (page2buf, page2buf, a);
	rbuf = gnome_print_context_new_from_module_name ("rbuf");
	g_object_set (G_OBJECT (rbuf), "pixels", pixels, "width", w, "height", h,
			"rowstride", w * 3, "page2buf", page2buf, "alpha", FALSE, NULL);
	gnome_print_meta_render_data (rbuf,
			gnome_print_meta_get_buffer (GNOME_PRINT_META (f->meta)),
			gnome_print_meta_get_length (GNOME_PRINT_META (f->meta)));
	g_object_unref (G_OBJECT (rbuf));
	art_affine_translate (page2buf, box->x0, box->y0);
	art_affine_scale (a, box->x1 - box->x0, box->y1 - box->y0);
	art_affine_multiply (a, a, page2buf);
	parent_class->image (GNOME_PRINT_FILTER (f), a, pixels, w, h, w * 3, 3);
	g_free (pixels);
}

static gint
fill_impl (GnomePrintFilter *filter,
	   const ArtBpath *bpath, ArtWindRule rule)
{
	GnomePrintFilterFrgba *f = (GnomePrintFilterFrgba *) filter;
	ArtDRect box;
	ArtVpath *vpath;

	C (gnome_print_fill_bpath_rule_real (f->meta, bpath, rule));

	if (gp_gc_get_opacity (f->pc->gc) > 255. / 256.)
		return parent_class->fill (filter, bpath, rule);

	vpath = art_bez_path_to_vec (bpath, 0.25);
	art_vpath_bbox_drect (vpath, &box);
	art_free (vpath);

	parent_class->gsave (filter);
	parent_class->clip (filter, bpath, rule);
	gnome_print_filter_frgba_render_buf (f, &box);
	parent_class->grestore (filter);

	return GNOME_PRINT_OK;
}

static gint
image_impl (GnomePrintFilter *filter, const gdouble *a_src,
	    const guchar *b, gint w, gint h, gint r, gint c)
{
	static ArtDRect const ident = { 0., 0., 1., 1. };
	GnomePrintFilterFrgba *f = (GnomePrintFilterFrgba *) filter;
	ArtDRect bbox;
	ArtBpath bpath[5];

	C (gnome_print_image_transform_real (f->meta, a_src, b, w, h, r, c));
	if ((c == 1) || (c == 3))
		return parent_class->image (filter, a_src, b, w, h, r, c);

	art_drect_affine_transform (&bbox, &ident, a_src);
	bpath[0].code = ART_MOVETO; bpath[0].x3 = bbox.x0; bpath[0].y3 = bbox.y0;
	bpath[1].code = ART_LINETO; bpath[1].x3 = bbox.x0; bpath[1].y3 = bbox.y1;
	bpath[2].code = ART_LINETO, bpath[2].x3 = bbox.x1; bpath[2].y3 = bbox.y1;
	bpath[3].code = ART_LINETO, bpath[3].x3 = bbox.x1; bpath[3].y3 = bbox.y0;
	bpath[4].code = ART_END;

	parent_class->gsave (filter);
	parent_class->clip (filter, bpath, ART_WIND_RULE_NONZERO);
	gnome_print_filter_frgba_render_buf (f, &bbox);
	parent_class->grestore (filter);

	return GNOME_PRINT_OK;
}

static gint
glyphlist_impl (GnomePrintFilter *filter,
		const gdouble *a_src, GnomeGlyphList *gl)
{
	GnomePrintFilterFrgba *f = (GnomePrintFilterFrgba *) filter;
	ArtDRect box;

	C (gnome_print_glyphlist_transform_real (f->meta, a_src, gl));
	if (gp_gc_get_opacity (f->pc->gc) > 255. / 256.)
		return parent_class->glyphlist (filter, a_src, gl);

	gnome_glyphlist_bbox (gl, a_src, 0, &box);
	gnome_print_filter_frgba_render_buf (f, &box);

	return GNOME_PRINT_OK;
}

static gint
setrgbcolor_impl (GnomePrintFilter *filter, gdouble r, gdouble g, gdouble b)
{
	GnomePrintFilterFrgba *f = GNOME_PRINT_FILTER_FRGBA (filter);

	C (gnome_print_setrgbcolor_real (f->meta, r, g, b));
	return parent_class->setrgbcolor (filter, r, g, b);
}

static gint
setopacity_impl (GnomePrintFilter *filter, gdouble o)
{
	GnomePrintFilterFrgba *f = GNOME_PRINT_FILTER_FRGBA (filter);

	C (gnome_print_setopacity_real (f->meta, o));
	return parent_class->setopacity (filter, o);
}

static gint
setlinewidth_impl (GnomePrintFilter *filter, gdouble w)
{
	GnomePrintFilterFrgba *f = GNOME_PRINT_FILTER_FRGBA (filter);

	C (gnome_print_setlinewidth_real (f->meta, w));
	return parent_class->setlinewidth (filter, w);
}

static void
gnome_print_filter_frgba_finalize (GObject *object)
{
	GnomePrintFilterFrgba *f = GNOME_PRINT_FILTER_FRGBA (object);

	if (f->meta) {
		g_object_unref (G_OBJECT (f->meta));
		f->meta = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gnome_print_filter_frgba_class_init (GnomePrintFilterFrgbaClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;
	GnomePrintFilterClass *f_class = (GnomePrintFilterClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = gnome_print_filter_frgba_get_property;
	object_class->set_property = gnome_print_filter_frgba_set_property;
	object_class->finalize     = gnome_print_filter_frgba_finalize;

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
}

static void
gnome_print_filter_frgba_init (GnomePrintFilterFrgba *f)
{
	f->meta = g_object_new (GNOME_TYPE_PRINT_META, NULL);
}

GType
gnome_print_filter_frgba_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GnomePrintFilterFrgbaClass), NULL, NULL,
			(GClassInitFunc) gnome_print_filter_frgba_class_init,
			NULL, NULL, sizeof (GnomePrintFilterFrgba), 0,
			(GInstanceInitFunc) gnome_print_filter_frgba_init
		};
		type = g_type_register_static (GNOME_TYPE_PRINT_FILTER,
				"GnomePrintFilterFrgba", &info, 0);
	}
	return type;
}

G_MODULE_EXPORT GType gnome_print__filter_get_type (void);

G_MODULE_EXPORT GType
gnome_print__filter_get_type (void)
{
	return GNOME_TYPE_PRINT_FILTER_FRGBA;
}
