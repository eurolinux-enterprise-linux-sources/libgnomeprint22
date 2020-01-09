#include <config.h>
#include "gnome-print-filter-zoom.h"

#include <gmodule.h>

#include <libgnomeprint/gnome-print-filter.h>
#include <libgnomeprint/gnome-print-i18n.h>

#include <libart_lgpl/art_affine.h>

#define FILTER_NAME        N_("zoom")
#define FILTER_DESCRIPTION N_("The zoom-filter zooms the input page.")

struct _GnomePrintFilterZoom {
	GnomePrintFilter parent;

	gdouble zoom;
};

struct _GnomePrintFilterZoomClass {
	GnomePrintFilterClass parent_class;
};

static GnomePrintFilterClass *parent_class = NULL;

enum {
	PROP_0,
	PROP_NAME,
	PROP_DESCRIPTION,
	PROP_ZOOM
};

static void
gnome_print_filter_zoom_get_property (GObject *object, guint n, GValue *v,
				GParamSpec *pspec)
{
	GnomePrintFilterZoom *filter = (GnomePrintFilterZoom *) object;

	switch (n) {
	case PROP_ZOOM:
		g_value_set_double (v, filter->zoom);
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
gnome_print_filter_zoom_set_property (GObject *object, guint n,
		const GValue *v, GParamSpec *pspec)
{
	GnomePrintFilterZoom *f = (GnomePrintFilterZoom *) object;

	switch (n) {
	case PROP_ZOOM:
		{
			gdouble d = g_value_get_double (v);
			GValueArray *va = g_value_array_new (6);
			GValue vd = {0,};
			guint i;
			gdouble a[6];

			f->zoom = d;
			art_affine_scale (a, f->zoom, f->zoom);
			g_value_init (&vd, G_TYPE_DOUBLE);
			for (i = 0; i < 6; i++) {
				g_value_set_double (&vd, a[i]);
				g_value_array_append (va, &vd);
			}
			g_value_unset (&vd);
			g_object_set (object, "transform", va, NULL);
			g_value_array_free (va);
			gnome_print_filter_changed (GNOME_PRINT_FILTER (f));
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, n, pspec);
	}
}

#define CR(res) { gint r = (res); if (r < 0) return r; }

static gint
beginpage_impl (GnomePrintFilter *filter,
		GnomePrintContext *pc, const guchar *name)
{
	GnomePrintFilterZoom *f = (GnomePrintFilterZoom *) filter;

	CR (parent_class->beginpage (filter, pc, name));
	CR (parent_class->setlinewidth (filter, f->zoom));

	return GNOME_PRINT_OK;
}

static void
gnome_print_filter_zoom_class_init (GnomePrintFilterZoomClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;
	GnomePrintFilterClass *filter_class = (GnomePrintFilterClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = gnome_print_filter_zoom_get_property;
	object_class->set_property = gnome_print_filter_zoom_set_property;

	g_object_class_override_property (object_class, PROP_NAME, "name");
	g_object_class_override_property (object_class, PROP_DESCRIPTION, "description");
	g_object_class_install_property (object_class, PROP_ZOOM,
		g_param_spec_double ("zoom",
		_("Zoom factor"), _("Zoom factor"),
		0., 10000., 1., G_PARAM_READWRITE));

	filter_class->beginpage = beginpage_impl;
}

static void
gnome_print_filter_zoom_init (GnomePrintFilterZoom *f)
{
	f->zoom = 1.;
}

GType
gnome_print_filter_zoom_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GnomePrintFilterZoomClass), NULL, NULL,
			(GClassInitFunc) gnome_print_filter_zoom_class_init,
			NULL, NULL, sizeof (GnomePrintFilterZoom), 0,
			(GInstanceInitFunc) gnome_print_filter_zoom_init
		};
		type = g_type_register_static (GNOME_TYPE_PRINT_FILTER,
				"GnomePrintFilterZoom", &info, 0);
	}
	return type;
}

G_MODULE_EXPORT GType gnome_print__filter_get_type (void);

G_MODULE_EXPORT GType
gnome_print__filter_get_type (void)
{
	return GNOME_TYPE_PRINT_FILTER_ZOOM;
}
