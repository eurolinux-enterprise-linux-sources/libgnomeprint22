#include <config.h>
#include "gnome-print-filter-rotate.h"

#include <gmodule.h>

#include <libgnomeprint/gnome-print-filter.h>
#include <libgnomeprint/gnome-print-i18n.h>

#include <libart_lgpl/art_affine.h>

#define FILTER_NAME        N_("rotate")
#define FILTER_DESCRIPTION N_("The rotate-filter rotates the page.")

struct _GnomePrintFilterRotate {
	GnomePrintFilter parent;

	gdouble theta;
};
                                                                                
struct _GnomePrintFilterRotateClass {
	GnomePrintFilterClass parent_class;
};

static GnomePrintFilterClass *parent_class = NULL;

enum {
	PROP_0,
	PROP_ROTATE,
	PROP_NAME,
	PROP_DESCRIPTION
};

static void
gnome_print_filter_rotate_get_property (GObject *object, guint n, GValue *v,
				GParamSpec *pspec)
{
	GnomePrintFilterRotate *filter = (GnomePrintFilterRotate *) object;

	switch (n) {
	case PROP_ROTATE:
		g_value_set_double (v, filter->theta);
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
gnome_print_filter_rotate_set_property (GObject *object, guint n,
		const GValue *v, GParamSpec *pspec)
{
	GnomePrintFilterRotate *filter = (GnomePrintFilterRotate *) object;

	switch (n) {
	case PROP_ROTATE:
		{
			gdouble d = g_value_get_double (v);
			GValueArray *va = g_value_array_new (6);
			GValue vd = {0,};
			guint i;
			gdouble a[6];

			filter->theta = d;
			art_affine_rotate (a, d);
			g_value_init (&vd, G_TYPE_DOUBLE);
			for (i = 0; i < 6; i++) {
				g_value_set_double (&vd, a[i]);
				g_value_array_append (va, &vd);
			}
			g_value_unset (&vd);
			g_object_set (object, "transform", va, NULL);
			g_value_array_free (va);
			gnome_print_filter_changed (GNOME_PRINT_FILTER (filter));
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, n, pspec);
	}
}

static void
gnome_print_filter_rotate_class_init (GnomePrintFilterRotateClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = gnome_print_filter_rotate_get_property;
	object_class->set_property = gnome_print_filter_rotate_set_property;

	g_object_class_override_property (object_class, PROP_NAME, "name");
	g_object_class_override_property (object_class, PROP_DESCRIPTION, "description");
	g_object_class_install_property (object_class, PROP_ROTATE,
		g_param_spec_double ("rotate",
		_("Rotate"), _("Rotate"),
		0., 360., 0., G_PARAM_READWRITE));
}

static void
gnome_print_filter_rotate_init (GnomePrintFilterRotate *f)
{
	g_object_set (G_OBJECT (f), "rotate", 0., NULL);
}

GType
gnome_print_filter_rotate_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GnomePrintFilterRotateClass), NULL, NULL,
			(GClassInitFunc) gnome_print_filter_rotate_class_init,
			NULL, NULL, sizeof (GnomePrintFilterRotate), 0,
			(GInstanceInitFunc) gnome_print_filter_rotate_init
		};
		type = g_type_register_static (GNOME_TYPE_PRINT_FILTER,
				"GnomePrintFilterRotate", &info, 0);
	}
	return type;
}

G_MODULE_EXPORT GType gnome_print__filter_get_type (void);

G_MODULE_EXPORT GType
gnome_print__filter_get_type (void)
{
	return GNOME_TYPE_PRINT_FILTER_ROTATE;
}
