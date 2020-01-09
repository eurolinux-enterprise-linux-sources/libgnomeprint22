/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  gnome-print-filter-multipage.c: Filter for printing several pages onto single output page
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors:
 *    Chris Lahey <clahey@helixcode.com>
 *
 *  Copyright (C) 1999-2003 Ximian Inc.
 */

#include <config.h>
#include "gnome-print-filter-multipage.h"

#include <string.h>
#include <math.h>

#include <gmodule.h>

#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_bpath.h>

#include <libgnomeprint/gnome-print-private.h>
#include <libgnomeprint/gp-gc-private.h>
#include <libgnomeprint/gnome-font.h>
#include <libgnomeprint/gnome-print-i18n.h>
#include <libgnomeprint/gnome-print-filter.h>

struct _GnomePrintFilterMultipage {
	GnomePrintFilter parent;

	GList *affines; /* Of type double[6] */
	GList *subpage;
};

struct _GnomePrintFilterMultipageClass {
	GnomePrintFilterClass parent_class;
};

#define FILTER_NAME        N_("multipage")
#define FILTER_DESCRIPTION N_("Filter for printing several pages onto single output page")

static GnomePrintFilterClass *parent_class = NULL;

enum {
	PROP_0,
	PROP_NAME,
	PROP_DESCRIPTION,
	PROP_AFFINES
};

static void
gnome_print_filter_multipage_get_property (GObject *object, guint n, GValue *v,
		GParamSpec *pspec)
{
	GnomePrintFilterMultipage *mp = GNOME_PRINT_FILTER_MULTIPAGE (object);

	switch (n) {
	case PROP_AFFINES:
		{
			guint i, n = g_list_length (mp->affines);
			GValueArray *va;
			GValue vd = {0,};

			if (!n) break;
			va = g_value_array_new (n);
			g_value_init (&vd, G_TYPE_DOUBLE);
			for (i = 0; i < n; i++) {
				gdouble *a = g_list_nth_data (mp->affines, i);
				guint j;

				for (j = 0; j < 6; j++) {
					g_value_set_double (&vd, a[j]);
					g_value_array_append (va, &vd);
				}
			}
			g_value_unset (&vd);
			g_value_set_boxed (v, va);
			g_value_array_free (va);
		}
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
gnome_print_filter_multipage_set_property (GObject *object, guint n,
		const GValue *v, GParamSpec *pspec)
{
	GnomePrintFilterMultipage *mp = GNOME_PRINT_FILTER_MULTIPAGE (object);

	switch (n) {
	case PROP_AFFINES:
		{
			GValueArray *va = g_value_get_boxed (v);
			guint i, n = va ? ((va->n_values < 6) ? 1 : va->n_values / 6) : 1;

			while (g_list_length (mp->affines) > n) {
				g_free (g_list_nth_data (mp->affines, 0));
				mp->affines = g_list_remove_link (mp->affines, mp->affines);
			}
			while (g_list_length (mp->affines) < n) {
				gdouble *a = g_new0 (gdouble, 6);

				art_affine_identity (a);
				mp->affines = g_list_append (mp->affines, a);
			}
			if (!va || (va->n_values < 6))
				art_affine_identity (mp->affines->data);
			else
				for (i = 0; i < n; i++) {
					gdouble *a = g_list_nth_data (mp->affines, i);
					guint j;

					for (j = 0; (j < 6) && (6 * i + j < va->n_values); j++)
						a[j] = g_value_get_double (g_value_array_get_nth (va, 6 * i + j));
				}
			mp->subpage = mp->affines;
		}
		gnome_print_filter_changed (GNOME_PRINT_FILTER (mp));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, n, pspec);
	}
}

static void
gnome_print_filter_multipage_finalize (GObject *object)
{
	GnomePrintFilterMultipage *mp = GNOME_PRINT_FILTER_MULTIPAGE (object);

	while (mp->affines) {
		g_free (mp->affines->data);
		mp->affines = g_list_remove_link (mp->affines, g_list_nth (mp->affines, 0));
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gnome_print_filter_multipage_flush (GnomePrintFilter *filter)
{
	GnomePrintFilterMultipage *mp = GNOME_PRINT_FILTER_MULTIPAGE (filter);

	if (mp->subpage != mp->affines) {
		mp->subpage = mp->affines;
		parent_class->showpage (filter);
	}

	parent_class->flush (filter);
}

static gint
gnome_print_filter_multipage_beginpage (GnomePrintFilter *filter,
		GnomePrintContext *pc, const guchar *name)
{
	GnomePrintFilterMultipage *mp = GNOME_PRINT_FILTER_MULTIPAGE (filter);
	GValueArray *va;
	GValue vd = {0,};
	guint i;

	g_value_init (&vd, G_TYPE_DOUBLE);
	va = g_value_array_new (6);
	for (i = 0; i < 6; i++) {
		g_value_set_double (&vd, ((gdouble *) mp->subpage->data)[i]);
		g_value_array_append (va, &vd);
	}
	g_value_unset (&vd);
	g_object_set (G_OBJECT (filter), "transform", va, NULL);
	g_value_array_free (va);
	if (mp->subpage == mp->affines)
		parent_class->beginpage (filter, pc, name);

	return GNOME_PRINT_OK;
}

static gint
gnome_print_filter_multipage_showpage (GnomePrintFilter *filter)
{
	GnomePrintFilterMultipage *mp = GNOME_PRINT_FILTER_MULTIPAGE (filter);

	mp->subpage = mp->subpage->next;
	if (mp->subpage == NULL) {
		mp->subpage = mp->affines;
		parent_class->showpage (filter);
	}
	g_object_set (G_OBJECT (filter), "transform", NULL, NULL);

	return GNOME_PRINT_OK;
}

static void
gnome_print_filter_multipage_reset (GnomePrintFilter *filter)
{
	GnomePrintFilterMultipage *mp = GNOME_PRINT_FILTER_MULTIPAGE (filter);

	mp->subpage = mp->affines;

	parent_class->reset (filter);
}

static void
param_affines_set_default (GParamSpec *pspec, GValue *v)
{
	GValueArray *va = g_value_array_new (6);
	GValue ve = {0,};
	guint i;
	gdouble a[] = {1., 0., 0., 1., 0., 0.};

	g_value_init (&ve, G_TYPE_DOUBLE);
	for (i = 0; i < G_N_ELEMENTS (a); i++) {
		g_value_set_double (&ve, a[i]);
		g_value_array_append (va, &ve);
	}
	g_value_unset (&ve);
	g_value_set_boxed (v, va);
	g_value_array_free (va);
}

static gint
param_affines_cmp (GParamSpec *pspec, const GValue *value1,
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

struct {
	GParamSpecValueArray parent_instance;
} GnomePrintFilterMultipageParamAffines;

static void
param_affines_init (GParamSpec *pspec)
{
	GParamSpecValueArray *pspec_va = (GParamSpecValueArray *) pspec;

	pspec_va->element_spec = g_param_spec_double ("affine", _("Affine"),
			_("Affine"), -G_MAXDOUBLE, G_MAXDOUBLE, 0., G_PARAM_READWRITE);
	g_param_spec_ref (pspec_va->element_spec);
	g_param_spec_sink (pspec_va->element_spec);
}

static GType
gnome_print_filter_multipage_param_affines_get_type (void)
{
	static GType type;
	if (G_UNLIKELY (type) == 0) {
		static GParamSpecTypeInfo pspec_info = {
			sizeof (GnomePrintFilterMultipageParamAffines), 0,
			param_affines_init,
			0xdeadbeef, NULL, param_affines_set_default, NULL, param_affines_cmp
		};
		pspec_info.value_type = G_TYPE_VALUE_ARRAY;
		type = g_param_type_register_static ("GnomePrintLayoutSelectorParamAffines", &pspec_info);
	}
	return type;
}

static void
gnome_print_filter_multipage_class_init (GnomePrintFilterMultipageClass *klass)
{
	GObjectClass *object_class;
	GnomePrintFilterClass *f_class;
	GParamSpec *pspec;

	f_class = GNOME_PRINT_FILTER_CLASS (klass);
	f_class->beginpage = gnome_print_filter_multipage_beginpage;
	f_class->showpage  = gnome_print_filter_multipage_showpage;
	f_class->flush     = gnome_print_filter_multipage_flush;
	f_class->reset     = gnome_print_filter_multipage_reset;

	object_class = (GObjectClass *) klass;
	object_class->finalize     = gnome_print_filter_multipage_finalize;
	object_class->get_property = gnome_print_filter_multipage_get_property;
	object_class->set_property = gnome_print_filter_multipage_set_property;

	g_object_class_override_property (object_class, PROP_NAME, "name");
	g_object_class_override_property (object_class, PROP_DESCRIPTION, "description");
	pspec = g_param_spec_internal (
			gnome_print_filter_multipage_param_affines_get_type (),
			"affines", _("Affines"), _("Affines"), G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_AFFINES, pspec);

	parent_class = g_type_class_peek_parent (klass);
}

static void
gnome_print_filter_multipage_init (GnomePrintFilterMultipage *mp)
{
	gdouble *a = g_new0 (gdouble, 6);

	art_affine_identity (a);
	mp->affines = g_list_append (NULL, a);
	mp->subpage = mp->affines;
}

GType
gnome_print_filter_multipage_get_type (void)
{
	static GType type = 0;
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GnomePrintFilterMultipageClass),
			NULL, NULL,
			(GClassInitFunc) gnome_print_filter_multipage_class_init,
			NULL, NULL,
			sizeof (GnomePrintFilterMultipage),
			0, (GInstanceInitFunc) gnome_print_filter_multipage_init
		};
		type = g_type_register_static (GNOME_TYPE_PRINT_FILTER, "GnomePrintFilterMultipage", &info, 0);
	}
	return type;
}

G_MODULE_EXPORT GType gnome_print__filter_get_type (void);

G_MODULE_EXPORT GType
gnome_print__filter_get_type (void)
{
	return GNOME_TYPE_PRINT_FILTER_MULTIPAGE;
}
