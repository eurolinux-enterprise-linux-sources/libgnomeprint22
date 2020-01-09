#include <config.h>
#include "gnome-print-filter-draft.h"

#include <gmodule.h>

#include <libgnomeprint/gnome-print-filter.h>
#include <libgnomeprint/gnome-print-i18n.h>

#include <math.h>
#include <string.h>

#include <libart_lgpl/art_affine.h>

#define FILTER_NAME        N_("draft")
#define FILTER_DESCRIPTION N_("The draft-filter marks each page as draft.")

/*
 * This could be the default image of a watermark
 * if we had access to the necessary libraries.
 */
#define DEFAULT_IMAGE "" \
"<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>" \
"<svg  width=\"95.991pt\" height=\"118.26pt\" viewBox=\"0 0 95.991 118.26\" xml:space=\"preserve\">" \
"  <g style=\"fill-rule:nonzero;clip-rule:nonzero;stroke:#000000;stroke-miterlimit:4;\">" \
"    <g style=\"stroke:none;\">" \
"      <g>" \
"        <path d=\"M86.068,0C61.466,0,56.851,35.041,70.691,35.041C84.529,35.041,110.671,0,86.068,0z\"/>" \
"        <path d=\"M45.217,30.699c7.369,0.45,15.454-28.122,1.604-26.325c-13.845,1.797-8.976,25.875-1.604,26.325z\"/>" \
"        <path d=\"M11.445,48.453c5.241-2.307,0.675-24.872-8.237-18.718c-8.908,6.155,2.996,21.024,8.237,18.718z\"/>" \
"        <path d=\"M26.212,36.642c6.239-1.272,6.581-26.864-4.545-22.273c-11.128,4.592-1.689,23.547,4.545,22.273z\"/>" \
"        <path d=\"M58.791,93.913c1.107,8.454-6.202,12.629-13.36,7.179C22.644,83.743,83.16,75.089,79.171,51.386c-3.311-19.674-63.676-13.617-70.55,17.166C3.968,89.374,27.774,118.26,52.614,118.26c12.22,0,26.315-11.034,28.952-25.012 c2.014-10.659-23.699-6.388-22.775,0.665z\"/>" \
"      </g>" \
"    </g>" \
"  </g>" \
"</svg>"

struct _GnomePrintFilterDraft {
	GnomePrintFilter parent;

	gdouble rotate, scale, opacity;
	struct {gdouble x, y;} translate;

	gchar *text;
	GnomeFont *font;
};
                                                                                
struct _GnomePrintFilterDraftClass {
	GnomePrintFilterClass parent_class;
};

static GnomePrintFilterClass *parent_class = NULL;

enum {
	PROP_0,
	PROP_NAME,
	PROP_DESCRIPTION,
	PROP_SCALE,
	PROP_ROTATE,
	PROP_TRANSLATE_X,
	PROP_TRANSLATE_Y,
	PROP_OPACITY,
	PROP_TEXT,
	PROP_FONT
};

static void
gnome_print_filter_draft_get_property (GObject *object, guint n, GValue *v,
				GParamSpec *pspec)
{
	GnomePrintFilterDraft *f = GNOME_PRINT_FILTER_DRAFT (object);

	switch (n) {
	case PROP_NAME:
		g_value_set_string (v, _(FILTER_NAME));
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (v, _(FILTER_DESCRIPTION));
		break;
	case PROP_SCALE:       g_value_set_double (v, f->scale      ); break;
	case PROP_ROTATE:      g_value_set_double (v, f->rotate     ); break;
	case PROP_OPACITY:     g_value_set_double (v, f->opacity    ); break;
	case PROP_TRANSLATE_X: g_value_set_double (v, f->translate.x); break;
	case PROP_TRANSLATE_Y: g_value_set_double (v, f->translate.y); break;
	case PROP_TEXT:        g_value_set_string (v, f->text       ); break;
	case PROP_FONT:        g_value_set_object (v, f->font       ); break;
	default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, n, pspec);
	}
}

static void
gnome_print_filter_draft_set_property (GObject *object, guint n,
		const GValue *v, GParamSpec *pspec)
{
	GnomePrintFilterDraft *f = (GnomePrintFilterDraft *) object;
	gdouble d;
	const gchar *s;
	GObject *o;

	switch (n) {
	case PROP_SCALE:
		d = g_value_get_double (v);
		if (d != f->scale) {
			f->scale = d;
			gnome_print_filter_changed (GNOME_PRINT_FILTER (f));
		}
		break;
	case PROP_ROTATE:
		d = g_value_get_double (v);
		if (d != f->rotate) {
			f->rotate = d;
			gnome_print_filter_changed (GNOME_PRINT_FILTER (f));
		}
		break;
	case PROP_OPACITY:
		d = g_value_get_double (v);
		if (d != f->opacity) {
			f->opacity = d;
			gnome_print_filter_changed (GNOME_PRINT_FILTER (f));
		}
		break;
	case PROP_TRANSLATE_X:
		d = g_value_get_double (v);
		if (d != f->translate.x) {
			f->translate.x = d;
			gnome_print_filter_changed (GNOME_PRINT_FILTER (f));
		}
		break;
	case PROP_TRANSLATE_Y:
		d = g_value_get_double (v);
		if (d != f->translate.y) {
			f->translate.y = d;
			gnome_print_filter_changed (GNOME_PRINT_FILTER (f));
		}
		break;
	case PROP_TEXT:
		s = g_value_get_string (v);
		if (!s) {
			if (f->text) { g_free (f->text); f->text = NULL; }
			gnome_print_filter_changed (GNOME_PRINT_FILTER (f));
			break;
		}
		if (f->text && !strcmp (s, f->text)) break;
		if (f->text) { g_free (f->text); f->text = NULL; }
		f->text = g_strdup (s);
		gnome_print_filter_changed (GNOME_PRINT_FILTER (f));
		break;
	case PROP_FONT:
		o = g_value_get_object (v);
		if (!o) {
			if (f->font) {
				g_object_unref (G_OBJECT (f->font));
				f->font = NULL;
			}
			gnome_print_filter_changed (GNOME_PRINT_FILTER (f));
			break;
		}
		if (f->font == GNOME_FONT (o)) break;
		if (f->font) {
			g_object_unref (G_OBJECT (f->font));
			f->font = NULL;
		}
		f->font = GNOME_FONT (o);
		g_object_ref (G_OBJECT (f->font));
		gnome_print_filter_changed (GNOME_PRINT_FILTER (f));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, n, pspec);
	}
}

#define CR(res) { gint result = (res); if (result < 0) return result; }

static gint
showpage_impl (GnomePrintFilter *filter)
{
	GnomePrintFilterDraft *f = GNOME_PRINT_FILTER_DRAFT (filter);
	gdouble a[6], ar[6];
	GnomeGlyphList *gl;
	gint r;

	art_affine_scale (a, f->scale, f->scale);
	a[4] = f->translate.x;
	a[5] = f->translate.y;
	art_affine_rotate (ar, f->rotate);
	art_affine_multiply (a, ar, a);

	parent_class->gsave (filter);
	parent_class->setopacity (filter, f->opacity);
	gl = gnome_glyphlist_from_text_sized_dumb (f->font,
			0xff & ((guint) (f->opacity * 256.)),
			0., 0., (guchar *) f->text, strlen (f->text));
	r = parent_class->glyphlist (filter, a, gl);
	gnome_glyphlist_unref (gl);
	parent_class->grestore (filter);

	return parent_class->showpage (filter);
}

static void
gnome_print_filter_draft_finalize (GObject *object)
{
	GnomePrintFilterDraft *f = GNOME_PRINT_FILTER_DRAFT (object);

	if (f->text) {
		g_free (f->text);
		f->text = NULL;
	}
	if (f->font) {
		g_object_unref (G_OBJECT (f->font));
		f->font = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gnome_print_filter_draft_class_init (GnomePrintFilterDraftClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;
	GnomePrintFilterClass *filter_class = (GnomePrintFilterClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = gnome_print_filter_draft_get_property;
	object_class->set_property = gnome_print_filter_draft_set_property;
	object_class->finalize     = gnome_print_filter_draft_finalize;

	g_object_class_override_property (object_class, PROP_NAME, "name");
	g_object_class_override_property (object_class, PROP_DESCRIPTION, "description");
	g_object_class_install_property (object_class, PROP_OPACITY,
		g_param_spec_double ("opacity", _("Opacity"), _("Opacity"),
			0., 1., 0.2, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_SCALE,
		g_param_spec_double ("scale", _("Scale"), _("Scale"),
			0., G_MAXDOUBLE, 1., G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_ROTATE,
		g_param_spec_double ("rotate", _("Rotate"), _("Rotate"),
			0., 360., 0., G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_TRANSLATE_X,
		g_param_spec_double ("translate_x", _("Translate horizontally"),
			_("Translate horizontally"), -G_MAXDOUBLE, G_MAXDOUBLE,
			0., G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_TRANSLATE_Y,
		g_param_spec_double ("translate_y", _("Translate vertically"),
			_("Translate vertically"), -G_MAXDOUBLE, G_MAXDOUBLE,
			0., G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_TEXT,
		g_param_spec_string ("text", _("Text"), _("Text"),
			_("Draft"), G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_FONT,
		g_param_spec_object ("font", _("Font"), _("Font"),
			GNOME_TYPE_FONT, G_PARAM_READWRITE));

	filter_class->showpage = showpage_impl;
}

static void
gnome_print_filter_draft_init (GnomePrintFilterDraft *filter)
{
	filter->rotate      = 0.;
	filter->scale       = 1.;
	filter->translate.x = 0.;
	filter->translate.y = 0.;
	filter->opacity     = 0.2;
	filter->text        = g_strdup (_("Draft"));
	filter->font        = gnome_font_find_closest ((guchar *) "Sans Regular", 12.0);
}

GType
gnome_print_filter_draft_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GnomePrintFilterDraftClass), NULL, NULL,
			(GClassInitFunc) gnome_print_filter_draft_class_init,
			NULL, NULL, sizeof (GnomePrintFilterDraft), 0,
			(GInstanceInitFunc) gnome_print_filter_draft_init
		};
		type = g_type_register_static (GNOME_TYPE_PRINT_FILTER,
				"GnomePrintFilterDraft", &info, 0);
	}
	return type;
}

G_MODULE_EXPORT GType gnome_print__filter_get_type (void);

G_MODULE_EXPORT GType
gnome_print__filter_get_type (void)
{
	return GNOME_TYPE_PRINT_FILTER_DRAFT;
}

