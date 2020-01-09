#include <config.h>
#include "gnome-print-filter-position.h"

#include <gmodule.h>

#include <libgnomeprint/gnome-print-filter.h>
#include <libgnomeprint/gnome-print-i18n.h>

#include <libart_lgpl/art_affine.h>

#define FILTER_NAME        N_("position")
#define FILTER_DESCRIPTION N_("The position-filter repositions the " \
			      "page on the paper.")

struct _GnomePrintFilterPosition {
	GnomePrintFilter parent;

	gdouble x, y;
};

struct _GnomePrintFilterPositionClass {
	GnomePrintFilterClass parent_class;
};

static GnomePrintFilterClass *parent_class = NULL;

enum {
	PROP_0,
	PROP_X,
	PROP_Y,
	PROP_NAME,
	PROP_DESCRIPTION
};

static void
gnome_print_filter_position_get_property (GObject *object, guint n, GValue *v,
				GParamSpec *pspec)
{
	GnomePrintFilterPosition *filter = (GnomePrintFilterPosition *) object;

	switch (n) {
	case PROP_X:
		g_value_set_double (v, filter->x);
		break;
	case PROP_Y:
		g_value_set_double (v, filter->y);
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
gnome_print_filter_position_set_property (GObject *object, guint n,
		const GValue *v, GParamSpec *pspec)
{
	GnomePrintFilterPosition *f = (GnomePrintFilterPosition *) object;

	switch (n) {
	case PROP_X:
		{
			gdouble d = g_value_get_double (v);
			GValueArray *va = g_value_array_new (6);
			GValue vd = {0,};
			guint i;
			gdouble a[6];

			f->x = d;
			art_affine_translate (a, f->x, f->y);
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
	case PROP_Y:
		{
			gdouble d = g_value_get_double (v);
			GValueArray *va = g_value_array_new (6);
			GValue vd = {0,};
			guint i;
			gdouble a[6];

			f->y = d;
			art_affine_translate (a, f->x, f->y);
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

static void
gnome_print_filter_position_class_init (GnomePrintFilterPositionClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = gnome_print_filter_position_get_property;
	object_class->set_property = gnome_print_filter_position_set_property;

	g_object_class_override_property (object_class, PROP_NAME, "name");
	g_object_class_override_property (object_class, PROP_DESCRIPTION, "description");
	g_object_class_install_property (object_class, PROP_X,
		g_param_spec_double ("x",
		_("Move horizontally"), _("Move horizontally"),
		-G_MAXDOUBLE, G_MAXDOUBLE, 0., G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_Y,
		g_param_spec_double ("y",
		_("Move vertically"), _("Move vertically"),
		-G_MAXDOUBLE, G_MAXDOUBLE, 0., G_PARAM_READWRITE));
}

GType
gnome_print_filter_position_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GnomePrintFilterPositionClass), NULL, NULL,
			(GClassInitFunc) gnome_print_filter_position_class_init,
			NULL, NULL, sizeof (GnomePrintFilterPosition), 0, NULL
		};
		type = g_type_register_static (GNOME_TYPE_PRINT_FILTER,
				"GnomePrintFilterPosition", &info, 0);
	}
	return type;
}

G_MODULE_EXPORT GType gnome_print__filter_get_type (void);

G_MODULE_EXPORT GType
gnome_print__filter_get_type (void)
{
	return GNOME_TYPE_PRINT_FILTER_POSITION;
}
