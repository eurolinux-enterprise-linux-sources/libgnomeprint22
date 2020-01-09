#include <config.h>
#include "gnome-print-filter-select.h"

#include <gmodule.h>

#include <libgnomeprint/gnome-print-filter.h>
#include <libgnomeprint/gnome-print-i18n.h>

#define FILTER_NAME        N_("select")
#define FILTER_DESCRIPTION N_("The select-filter lets you select "	\
			      "pages to be printed.")

#define C(result) { int re = (result); if (re < 0) { return re; }}

enum {
	PROP_0,
	PROP_NAME,
	PROP_DESCRIPTION,
	PROP_FIRST,
	PROP_LAST,
	PROP_PAGES,
	PROP_SKIP,
	PROP_COLLECT
};

struct _GnomePrintFilterSelect {
	GnomePrintFilter parent;

	guint current;
	gboolean has_page;

	/* Properties */
	GArray *a;
	guint first, last, skip;
	gboolean collect;
};

struct _GnomePrintFilterSelectClass {
	GnomePrintFilterClass parent_class;
};

static GnomePrintFilterClass *parent_class = NULL;

static void
get_property_impl (GObject *object, guint n, GValue *v, GParamSpec *pspec)
{
	GnomePrintFilterSelect *f = (GnomePrintFilterSelect *) object;

	switch (n) {
	case PROP_PAGES:
		{
			guint i;
			GValue vd = {0,};
			GValueArray *va;

			if (!f->a) break;
			va = g_value_array_new (f->a->len);
			g_value_init (&vd, G_TYPE_BOOLEAN);
			for (i = 0; i < f->a->len; i++) {
				g_value_set_boolean (&vd, g_array_index (f->a, gboolean, i));
				g_value_array_append (va, &vd);
			}
			g_value_unset (&vd);
			g_value_set_boxed (v, va);
		}
		break;
	case PROP_COLLECT:     g_value_set_boolean (v, f->collect);     break;
	case PROP_FIRST:       g_value_set_uint    (v, f->first);       break;
	case PROP_LAST:        g_value_set_uint    (v, f->last);        break;
	case PROP_SKIP:        g_value_set_uint    (v, f->skip);        break;
	case PROP_NAME:        g_value_set_string  (v, _(FILTER_NAME)); break;
	case PROP_DESCRIPTION:
		g_value_set_string (v, _(FILTER_DESCRIPTION));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, n, pspec);
	}
}

static void
set_property_impl (GObject *object, guint n, const GValue *v, GParamSpec *pspec)
{
	GnomePrintFilterSelect *f = (GnomePrintFilterSelect *) object;
	guint i;

	switch (n) {
	case PROP_PAGES:
		{
			GValueArray *a = g_value_get_boxed (v);

			if (!a || !a->n_values) {
				if (f->a) {
					g_array_free (f->a, TRUE);
					f->a = NULL;
					gnome_print_filter_changed (GNOME_PRINT_FILTER (f));
				}
			} else {
				guint i;
				gboolean b = FALSE;

				if (!f->a && !a->n_values);
				else if (!f->a && a->n_values) {
					f->a = g_array_new (FALSE, TRUE, sizeof (gboolean));
					g_array_set_size (f->a, a->n_values);
					b = TRUE;
				} else if (f->a->len != a->n_values) {
					g_array_set_size (f->a, a->n_values);
					b = TRUE;
				}
				for (i = 0; i < a->n_values; i++) {
					gboolean be = g_value_get_boolean (g_value_array_get_nth (a, i));

					if (g_array_index (f->a, gboolean, i) != be) {
						g_array_index (f->a, gboolean, i) = be;
						b = TRUE;
					}
				}
				if (b)
					gnome_print_filter_changed (GNOME_PRINT_FILTER (f));
			}
		}
		break;
	case PROP_COLLECT:
		{
			gboolean b = g_value_get_boolean (v);

			if (b == f->collect)
				break;
			f->collect = b;
			gnome_print_filter_changed (GNOME_PRINT_FILTER (f));
		}
		break;
	case PROP_FIRST:
		i = g_value_get_uint (v);
		if (i != f->first) {
			f->first = i;
			gnome_print_filter_changed (GNOME_PRINT_FILTER (f));
		}
		break;
	case PROP_LAST:
		i = g_value_get_uint (v);
		if (i != f->last) {
			f->last = i;
			gnome_print_filter_changed (GNOME_PRINT_FILTER (f));
		}
		break;
	case PROP_SKIP:
		i = g_value_get_uint (v);
		if (i != f->skip) {
			f->skip = i;
			gnome_print_filter_changed (GNOME_PRINT_FILTER (f));
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, n, pspec);
	}
}

static gboolean
do_skip_page (GnomePrintFilterSelect *f, guint p)
{
	g_return_val_if_fail (GNOME_PRINT_IS_FILTER_SELECT (f), TRUE);

	return (
			(p < f->first) || /* All pages before first page */
			(p > f->last) ||  /* All pages after last page */
			((p != f->first) && ((p - f->first) % (f->skip + 1))) || /* Skipped */
			(f->a && (f->a->len > p) && !g_array_index (f->a, gboolean, p)));
}

static gboolean
do_skip (GnomePrintFilterSelect *f)
{
	g_return_val_if_fail (GNOME_PRINT_IS_FILTER_SELECT (f), TRUE);
	g_return_val_if_fail (f->current > 0, TRUE);

	return do_skip_page (f, f->current - 1);
}

static void
reset_impl (GnomePrintFilter *filter)
{
	GnomePrintFilterSelect *s = GNOME_PRINT_FILTER_SELECT (filter);

	s->current = 0;
	s->has_page = FALSE;
	parent_class->reset (filter);
}

static void
flush_impl (GnomePrintFilter *filter)
{
	GnomePrintFilterSelect *s = GNOME_PRINT_FILTER_SELECT (filter);

	if (s->collect && s->has_page) {
		parent_class->showpage (filter);
		s->has_page = FALSE;
	}
	parent_class->flush (filter);
}

static gint
beginpage_impl (GnomePrintFilter *filter,
		GnomePrintContext *pc, const guchar *n)
{
	GnomePrintFilterSelect *s = GNOME_PRINT_FILTER_SELECT (filter);

	s->current++;

	if (s->collect) {
		if (!(s->current - 1) || !((s->current - 1 - s->first) % (s->skip + 1))) {
			s->has_page = TRUE;
			return parent_class->beginpage (filter, pc, n);
		}
		g_object_set (G_OBJECT (filter), "context", pc, NULL);
		return GNOME_PRINT_OK;
	}

	if (!do_skip (s)) return parent_class->beginpage (filter, pc, n);
	g_object_set (G_OBJECT (filter), "context", pc, NULL);
	return GNOME_PRINT_OK;
}

static gint
showpage_impl (GnomePrintFilter *filter)
{
	GnomePrintFilterSelect *s = GNOME_PRINT_FILTER_SELECT (filter);

	if (s->collect) {
		if (!((s->current - s->first) % (s->skip + 1))) {
			s->has_page = FALSE;
			return parent_class->showpage (filter);
		}
		return GNOME_PRINT_OK;
	}

	if (!do_skip (s)) return parent_class->showpage (filter);

	return GNOME_PRINT_OK;
}

static gint
gsave_impl (GnomePrintFilter *f)
{
	GnomePrintFilterSelect *s = GNOME_PRINT_FILTER_SELECT (f);

	if (s->collect || !do_skip (s)) return parent_class->gsave (f);
	return GNOME_PRINT_OK;
}

static gint
grestore_impl (GnomePrintFilter *f)
{
	GnomePrintFilterSelect *s = GNOME_PRINT_FILTER_SELECT (f);

	if (s->collect || !do_skip (s)) return parent_class->grestore (f);
	return GNOME_PRINT_OK;
}

static gint
clip_impl (GnomePrintFilter *f, const ArtBpath *b, ArtWindRule r)
{
	GnomePrintFilterSelect *s = GNOME_PRINT_FILTER_SELECT (f);

	if (s->collect || !do_skip (s)) return parent_class->clip (f, b, r);
	return GNOME_PRINT_OK;
}

static gint
fill_impl (GnomePrintFilter *f,
		const ArtBpath *b, ArtWindRule r)
{
	GnomePrintFilterSelect *s = GNOME_PRINT_FILTER_SELECT (f);

	if (s->collect || !do_skip (s)) return parent_class->fill (f, b, r);
	return GNOME_PRINT_OK;
}

static gint
stroke_impl (GnomePrintFilter *f, const ArtBpath *b)
{
	GnomePrintFilterSelect *s = GNOME_PRINT_FILTER_SELECT (f);

	if (s->collect || !do_skip (s)) return parent_class->stroke (f, b);
	return GNOME_PRINT_OK;
}

static gint
image_impl (GnomePrintFilter *f,
	    const gdouble *a, const guchar *p, gint w, gint h, gint r, gint c)
{
	GnomePrintFilterSelect *s = GNOME_PRINT_FILTER_SELECT (f);

	if (s->collect || !do_skip (s))
		return parent_class->image (f, a, p, w, h, r, c);
	return GNOME_PRINT_OK;
}

static gint
glyphlist_impl (GnomePrintFilter *f,
		const gdouble *affine, GnomeGlyphList *gl)
{
	GnomePrintFilterSelect *s = GNOME_PRINT_FILTER_SELECT (f);

	if (s->collect || !do_skip (s))
		return parent_class->glyphlist (f, affine, gl);
	return GNOME_PRINT_OK;
}

static gint
setrgbcolor_impl (GnomePrintFilter *f, gdouble r, gdouble g, gdouble b)
{
	GnomePrintFilterSelect *s = GNOME_PRINT_FILTER_SELECT (f);

	if (s->collect || !do_skip (s))
		return parent_class->setrgbcolor (f, r, g, b);
	return GNOME_PRINT_OK;
}

static gint
setopacity_impl (GnomePrintFilter *f, gdouble opacity)
{
	GnomePrintFilterSelect *s = GNOME_PRINT_FILTER_SELECT (f);

	if (s->collect || !do_skip (s))
		return parent_class->setopacity (f, opacity);
	return GNOME_PRINT_OK;
}

static gint
setlinewidth_impl (GnomePrintFilter *f, gdouble width)
{
	GnomePrintFilterSelect *s = GNOME_PRINT_FILTER_SELECT (f);

	if (s->collect || !do_skip (s))
		return parent_class->setlinewidth (f, width);
	return GNOME_PRINT_OK;
}

static void
finalize_impl (GObject *object)
{
	GnomePrintFilterSelect *filter = (GnomePrintFilterSelect *) object;

	if (filter->a) {
		g_array_free (filter->a, TRUE);
		filter->a = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gnome_print_filter_select_class_init (GnomePrintFilterSelectClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;
	GnomePrintFilterClass *f_class = (GnomePrintFilterClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	f_class->flush        = flush_impl;
	f_class->reset        = reset_impl;

	f_class->beginpage    = beginpage_impl;
	f_class->showpage     = showpage_impl;
	f_class->gsave        = gsave_impl;
	f_class->grestore     = grestore_impl;
	f_class->clip         = clip_impl;
	f_class->fill         = fill_impl;
	f_class->stroke       = stroke_impl;
	f_class->image        = image_impl;
	f_class->glyphlist    = glyphlist_impl;
	f_class->setlinewidth = setlinewidth_impl;
	f_class->setopacity   = setopacity_impl;
	f_class->setrgbcolor  = setrgbcolor_impl;

	object_class->finalize     = finalize_impl;
	object_class->get_property = get_property_impl;
	object_class->set_property = set_property_impl;

	g_object_class_override_property (object_class, PROP_NAME, "name");
	g_object_class_override_property (object_class, PROP_DESCRIPTION, "description");
	g_object_class_install_property (object_class, PROP_SKIP,
		g_param_spec_uint ("skip", _("Skip"), _("Skip"), 0, G_MAXUINT, 0,
			G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_FIRST,
		g_param_spec_uint ("first", _("First page"), _("First page"),
			0, G_MAXUINT, 0, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_LAST,
		g_param_spec_uint ("last", _("Last page"), _("Last page"),
			0, G_MAXUINT, G_MAXUINT, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_PAGES,
		g_param_spec_value_array ("pages", _("Pages"), _("Pages"),
			g_param_spec_boolean ("page_item", _("Page"), _("Page"),
				FALSE, G_PARAM_READWRITE), G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_COLLECT,
		g_param_spec_boolean ("collect", _("Collect"), _("Collect"),
			FALSE, G_PARAM_READWRITE));
}

static void
gnome_print_filter_select_init (GnomePrintFilterSelect *s)
{
	s->last = G_MAXUINT;
}

GType
gnome_print_filter_select_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GnomePrintFilterSelectClass), NULL, NULL,
			(GClassInitFunc) gnome_print_filter_select_class_init,
			NULL, NULL, sizeof (GnomePrintFilterSelect), 0,
			(GInstanceInitFunc) gnome_print_filter_select_init
		};
		type = g_type_register_static (GNOME_TYPE_PRINT_FILTER,
				"GnomePrintFilterSelect", &info, 0);
	}
	return type;
}

G_MODULE_EXPORT GType gnome_print__filter_get_type (void);

G_MODULE_EXPORT GType
gnome_print__filter_get_type (void)
{
	return GNOME_TYPE_PRINT_FILTER_SELECT;
}
