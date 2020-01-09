#include <config.h>
#include "gnome-print-filter-clip.h"

#include <gmodule.h>

#include <libgnomeprint/gnome-print-filter.h>
#include <libgnomeprint/gnome-print-i18n.h>

#define FILTER_NAME        N_("clip")
#define FILTER_DESCRIPTION N_("The clip-filter lets you select regions.")

struct _GnomePrintFilterClip {
	GnomePrintFilter parent;

	ArtBpath b[6];
};
                                                                                
struct _GnomePrintFilterClipClass {
	GnomePrintFilterClass parent_class;
};

static GnomePrintFilterClass *parent_class = NULL;

enum {
	PROP_0,
	PROP_NAME,
	PROP_DESCRIPTION,
	PROP_LEFT,
	PROP_RIGHT,
	PROP_TOP,
	PROP_BOTTOM
};

static void
gnome_print_filter_clip_get_property (GObject *object, guint n, GValue *v,
				GParamSpec *pspec)
{
	GnomePrintFilterClip *f = GNOME_PRINT_FILTER_CLIP (object);

	switch (n) {
	case PROP_NAME:
		g_value_set_string (v, _(FILTER_NAME));
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (v, _(FILTER_DESCRIPTION));
		break;
	case PROP_LEFT:   g_value_set_double (v, f->b[0].x3);    break;
	case PROP_RIGHT:  g_value_set_double (v, f->b[2].x3);    break;
	case PROP_TOP:    g_value_set_double (v, f->b[1].y3);    break;
	case PROP_BOTTOM: g_value_set_double (v, f->b[0].y3);    break;
	default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, n, pspec);
	}
}

static void
gnome_print_filter_clip_set_property (GObject *object, guint n,
		const GValue *v, GParamSpec *pspec)
{
	GnomePrintFilterClip *f = GNOME_PRINT_FILTER_CLIP (object);

	switch (n) {
	case PROP_LEFT: 
		f->b[0].x3 = f->b[1].x3 = f->b[4].x3 = g_value_get_double (v);
		break;
	case PROP_RIGHT:
		f->b[2].x3 = f->b[3].x3 = g_value_get_double (v);
		break;
	case PROP_BOTTOM:
		f->b[0].y3 = f->b[3].y3 = f->b[4].y3 = g_value_get_double (v);
		break;
	case PROP_TOP:
		f->b[1].y3 = f->b[2].y3 = g_value_get_double (v);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, n, pspec);
	}
}

static gint
stroke_impl (GnomePrintFilter *filter, const ArtBpath *bpath)
{
	GnomePrintFilterClip *f = GNOME_PRINT_FILTER_CLIP (filter);

	if ((f->b[0].x3 > -G_MAXDOUBLE) && (f->b[0].y3 > -G_MAXDOUBLE) &&
			(f->b[2].x3 <  G_MAXDOUBLE) && (f->b[1].y3 <  G_MAXDOUBLE)) {
		parent_class->gsave (filter);
		parent_class->clip (filter, f->b, ART_WIND_RULE_NONZERO);
		parent_class->stroke (filter, bpath);
		parent_class->grestore (filter);
	} else
		parent_class->stroke (filter, bpath);
	return GNOME_PRINT_OK;
}

static gint
fill_impl (GnomePrintFilter *filter, const ArtBpath *bpath, ArtWindRule rule)
{
	GnomePrintFilterClip *f = GNOME_PRINT_FILTER_CLIP (filter);

	if ((f->b[0].x3 > -G_MAXDOUBLE) && (f->b[0].y3 > -G_MAXDOUBLE) &&
			(f->b[2].x3 <  G_MAXDOUBLE) && (f->b[1].y3 <  G_MAXDOUBLE)) {
		parent_class->gsave (filter);
		parent_class->clip (filter, f->b, ART_WIND_RULE_NONZERO);
		parent_class->fill (filter, bpath, rule);
		parent_class->grestore (filter);
	} else
		parent_class->fill (filter, bpath, rule);
	return GNOME_PRINT_OK;
}

static gint
image_impl (GnomePrintFilter *filter, const gdouble *a,
		const guchar *b, gint w, gint h, gint r, gint c)
{
	GnomePrintFilterClip *f = GNOME_PRINT_FILTER_CLIP (filter);

	if ((f->b[0].x3 > -G_MAXDOUBLE) && (f->b[0].y3 > -G_MAXDOUBLE) &&
			(f->b[2].x3 <  G_MAXDOUBLE) && (f->b[1].y3 <  G_MAXDOUBLE)) {
		parent_class->gsave (filter);
		parent_class->clip (filter, f->b, ART_WIND_RULE_NONZERO);
		parent_class->image (filter, a, b, w, h, r, c);
		parent_class->grestore (filter);
	} else
		parent_class->image (filter, a, b, w, h, r, c);
	return GNOME_PRINT_OK;
}

static gint
glyphlist_impl (GnomePrintFilter *filter, const gdouble *a, GnomeGlyphList *g)
{
	GnomePrintFilterClip *f = GNOME_PRINT_FILTER_CLIP (filter);

	if ((f->b[0].x3 > -G_MAXDOUBLE) && (f->b[0].y3 > -G_MAXDOUBLE) &&
			(f->b[2].x3 <  G_MAXDOUBLE) && (f->b[1].y3 <  G_MAXDOUBLE)) {
		parent_class->gsave (filter);
		parent_class->clip (filter, f->b, ART_WIND_RULE_NONZERO);
		parent_class->glyphlist (filter, a, g);
		parent_class->grestore (filter);
	} else
		parent_class->glyphlist (filter, a, g);
	return GNOME_PRINT_OK;
}

static void
gnome_print_filter_clip_class_init (GnomePrintFilterClipClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;
	GnomePrintFilterClass *filter_class = (GnomePrintFilterClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = gnome_print_filter_clip_get_property;
	object_class->set_property = gnome_print_filter_clip_set_property;

	g_object_class_override_property (object_class, PROP_NAME, "name");
	g_object_class_override_property (object_class, PROP_DESCRIPTION, "description");
	g_object_class_install_property (object_class, PROP_LEFT,
		g_param_spec_double ("left", _("Left"), _("Left"),
			-G_MAXDOUBLE, G_MAXDOUBLE, -G_MAXDOUBLE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_RIGHT,
		g_param_spec_double ("right", _("Right"), _("Right"),
			-G_MAXDOUBLE, G_MAXDOUBLE, G_MAXDOUBLE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_TOP,
		g_param_spec_double ("top", _("Top"), _("Top"),
			-G_MAXDOUBLE, G_MAXDOUBLE, G_MAXDOUBLE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_BOTTOM,
		g_param_spec_double ("bottom", _("Bottom"), _("Bottom"),
			-G_MAXDOUBLE, G_MAXDOUBLE, -G_MAXDOUBLE, G_PARAM_READWRITE));

	object_class->get_property = gnome_print_filter_clip_get_property;
	object_class->set_property = gnome_print_filter_clip_set_property;

	filter_class->stroke    = stroke_impl;
	filter_class->fill      = fill_impl;
	filter_class->glyphlist = glyphlist_impl;
	filter_class->image     = image_impl;
}

static void
gnome_print_filter_clip_init (GnomePrintFilterClip *f)
{
	f->b[0].code = ART_MOVETO;
	f->b[0].x3 = -G_MAXDOUBLE;
	f->b[0].y3 = -G_MAXDOUBLE;
	f->b[1].code = ART_LINETO;
	f->b[1].x3 = -G_MAXDOUBLE;
	f->b[1].y3 =  G_MAXDOUBLE;
	f->b[2].code = ART_LINETO;
	f->b[2].x3 =  G_MAXDOUBLE;
	f->b[2].y3 =  G_MAXDOUBLE;
	f->b[3].code = ART_LINETO;
	f->b[3].x3 =  G_MAXDOUBLE;
	f->b[3].y3 = -G_MAXDOUBLE;
	f->b[4].code = ART_LINETO;
	f->b[4].x3 = -G_MAXDOUBLE;
	f->b[4].y3 = -G_MAXDOUBLE;
	f->b[5].code = ART_END;
}

GType
gnome_print_filter_clip_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GnomePrintFilterClipClass), NULL, NULL,
			(GClassInitFunc) gnome_print_filter_clip_class_init,
			NULL, NULL, sizeof (GnomePrintFilterClip), 0, 
			(GInstanceInitFunc) gnome_print_filter_clip_init
		};
		type = g_type_register_static (GNOME_TYPE_PRINT_FILTER,
				"GnomePrintFilterClip", &info, 0);
	}
	return type;
}

G_MODULE_EXPORT GType gnome_print__filter_get_type (void);

G_MODULE_EXPORT GType
gnome_print__filter_get_type (void)
{
	return GNOME_TYPE_PRINT_FILTER_CLIP;
}
