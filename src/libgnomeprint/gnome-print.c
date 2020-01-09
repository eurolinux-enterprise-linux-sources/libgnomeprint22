/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  gnome-print.c: Abstract base class of gnome-print drivers
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
 *    Raph Levien <raph@acm.org>
 *    Miguel de Icaza <miguel@kernel.org>
 *    Lauris Kaplinski <lauris@ximian.com>
 *    Chema Celorio <chema@celorio.com>
 *
 *  Copyright (C) 1999-2001 Ximian Inc. and authors
 */

#include <config.h>
#include <string.h>
#include <gmodule.h>
#include <errno.h>
#include <fcntl.h>

#include <libgnomeprint/gnome-print-filter.h>

#include <libgnomeprint/gnome-print-i18n.h>
#include <libgnomeprint/gnome-print-private.h>
#include <libgnomeprint/gp-gc-private.h>
#include <libgnomeprint/gnome-print-transport.h>
#include <libgnomeprint/gnome-print-ps2.h>
#include <libgnomeprint/gnome-print-pdf.h>
#include <libgnomeprint/gnome-print-meta.h>
#include <libgnomeprint/gnome-print-rbuf.h>

#ifdef ENABLE_SVG
#include <libgnomeprint/gnome-print-svg.h>
#endif

#ifdef HAVE_GDI
#include <libgnomeprint/gnome-print-gdi.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

gchar *gnome_print_locale_dir = GNOMELOCALEDIR;
gchar *gnome_print_modules_dir = GNOME_PRINT_MODULES_DIR;
gchar *gnome_print_data_dir = GNOME_PRINT_DATA_DIR;

#ifdef HAVE_MMAP
/* For the buffer stuff, remove when the buffer stuff is moved out here */
#include <sys/mman.h>
#include <unistd.h>

#ifndef PROT_READ
#define PROT_READ 0x1
#endif

#if defined(FREEBSD) || defined(__FreeBSD__)
/* We must keep the file open while pages are mapped.  */
/* http://www.freebsd.org/cgi/query-pr.cgi?pr=48291 */
#define HAVE_BROKEN_MMAP
#endif

#elif defined G_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#endif /* HAVE_MMAP */

#ifndef O_BINARY
#define O_BINARY 0
#endif

enum {
	PROP_0,
	PROP_CONFIG,
	PROP_TRANSPORT,
	PROP_FILTER
};

static void gnome_print_context_class_init (GnomePrintContextClass *klass);

static GObjectClass *parent_class = NULL;

#define C(result) { gint re = (result); if (re < 0) { return re; }}

struct _GnomePrintContextPrivate {
	GnomePrintFilter *filter;
};

static void
gnome_print_context_init (GnomePrintContext *pc)
{
	pc->gc = gp_gc_new ();
	pc->priv = g_new0 (GnomePrintContextPrivate, 1);
}

GType
gnome_print_context_get_type (void)
{
	static GType pc_type = 0;
	if (!pc_type) {
		static const GTypeInfo pc_info = {
			sizeof (GnomePrintContextClass),
			NULL, NULL,
			(GClassInitFunc) gnome_print_context_class_init,
			NULL, NULL,
			sizeof (GnomePrintContext),
			0,
			(GInstanceInitFunc) gnome_print_context_init
		};
		pc_type = g_type_register_static (G_TYPE_OBJECT, "GnomePrintContext", &pc_info, 0);
	}
	return pc_type;
}

static void
gnome_print_context_get_property (GObject *object, guint n, GValue *v,
		GParamSpec *pspec)
{
	GnomePrintContext *context = GNOME_PRINT_CONTEXT (object);

	switch (n) {
	case PROP_CONFIG:
		g_value_set_object (v, context->config);
		break;
	case PROP_TRANSPORT:
		g_value_set_object (v, context->transport);
		break;
	case PROP_FILTER:
		g_value_set_object (v, context->priv->filter);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, n, pspec);
	}
}

static void
gnome_print_context_set_property (GObject *object, guint n, const GValue *v,
		GParamSpec *pspec)
{
	GnomePrintContext *context = GNOME_PRINT_CONTEXT (object);
	GnomePrintFilter *filter;

	switch (n) {
	case PROP_CONFIG:
		if (context->config)
			g_object_unref (context->config);
		context->config = g_value_get_object (v);
		if (context->config)
			g_object_ref (context->config);
		break;
	case PROP_TRANSPORT:
		if (context->transport)
			g_object_unref (context->transport);
		context->transport = g_value_get_object (v);
		if (context->transport)
			g_object_ref (context->transport);
		break;
	case PROP_FILTER:
		if (context->priv->filter) {
			g_object_unref (G_OBJECT (context->priv->filter));
			context->priv->filter = NULL;
		}
		filter = g_value_get_object (v);
		if (filter) {
			g_object_ref (G_OBJECT (filter));
			context->priv->filter = filter;
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, n, pspec);
	}
}

static void
gnome_print_context_finalize (GObject *object)
{
	GnomePrintContext *pc = GNOME_PRINT_CONTEXT (object);

	if (pc->priv) {
		if (pc->priv->filter) {
			g_object_unref (G_OBJECT (pc->priv->filter));
			pc->priv->filter = NULL;
		}
		g_free (pc->priv);
		pc->priv = NULL;
	}

	if (pc->transport) {
		g_warning ("file %s: line %d: Destroying Context with open transport", __FILE__, __LINE__);
		g_object_unref (G_OBJECT (pc->transport));
		pc->transport = NULL;
	}

	if (pc->config) {
		gnome_print_config_unref (pc->config);
		pc->config = NULL;
	}

	if (pc->gc) {
		gp_gc_unref (pc->gc);
		pc->gc = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gnome_print_context_class_init (GnomePrintContextClass *klass)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (klass);

	object_class = (GObjectClass*) klass;
	object_class->finalize = gnome_print_context_finalize;
	object_class->get_property = gnome_print_context_get_property;
	object_class->set_property = gnome_print_context_set_property;
	g_object_class_install_property (object_class, PROP_CONFIG,
			g_param_spec_object ("config", "Config", "Config",
				GNOME_TYPE_PRINT_CONFIG, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_TRANSPORT,
			g_param_spec_object ("transport", "Transport", "Transport",
				GNOME_TYPE_PRINT_TRANSPORT, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_FILTER,
			g_param_spec_object ("filter", _("Filter"), _("Filter"),
				GNOME_TYPE_PRINT_FILTER, G_PARAM_READWRITE));
}

gint
gnome_print_context_construct (GnomePrintContext *pc, GnomePrintConfig *config)
{
	GnomePrintReturnCode retval = GNOME_PRINT_OK;

	g_return_val_if_fail (pc != NULL, GNOME_PRINT_ERROR_UNKNOWN);
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_UNKNOWN);
	g_return_val_if_fail (config != NULL, GNOME_PRINT_ERROR_UNKNOWN);

	g_return_val_if_fail (pc->config == NULL, GNOME_PRINT_ERROR_UNKNOWN);

	pc->config = gnome_print_config_ref (config);

	if (GNOME_PRINT_CONTEXT_GET_CLASS (pc)->construct)
		retval = GNOME_PRINT_CONTEXT_GET_CLASS (pc)->construct (pc);

	return retval;
}

gint
gnome_print_context_create_transport (GnomePrintContext *pc)
{
	g_return_val_if_fail (pc != NULL, GNOME_PRINT_ERROR_UNKNOWN);
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_UNKNOWN);
	g_return_val_if_fail (pc->config != NULL, GNOME_PRINT_ERROR_UNKNOWN);
	g_return_val_if_fail (pc->transport == NULL, GNOME_PRINT_ERROR_UNKNOWN);

	pc->transport = gnome_print_transport_new (pc->config);

	if (pc->transport == NULL) {
		g_warning ("Could not create transport inside gnome_print_context_create_transport");
		return GNOME_PRINT_ERROR_UNKNOWN;
	}

	return GNOME_PRINT_OK;
}

gint
gnome_print_beginpage_real (GnomePrintContext *pc, const guchar *name)
{
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADVALUE);
	g_return_val_if_fail (name != NULL, GNOME_PRINT_ERROR_BADVALUE);

	pc->pages++;
	if (GNOME_PRINT_CONTEXT_GET_CLASS (pc)->beginpage)
		C (GNOME_PRINT_CONTEXT_GET_CLASS (pc)->beginpage (pc, name));

	return GNOME_PRINT_OK;
}

gint
gnome_print_showpage_real (GnomePrintContext *pc)
{
	gint result = GNOME_PRINT_OK;

	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADVALUE);

	if (GNOME_PRINT_CONTEXT_GET_CLASS (pc)->showpage)
		result = GNOME_PRINT_CONTEXT_GET_CLASS (pc)->showpage (pc);
	C (result);

	return GNOME_PRINT_OK;
}

gint
gnome_print_grestore_real (GnomePrintContext *pc)
{
	gint result = GNOME_PRINT_OK;

	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADVALUE);

	if (GNOME_PRINT_CONTEXT_GET_CLASS (pc)->grestore)
		result = GNOME_PRINT_CONTEXT_GET_CLASS (pc)->grestore (pc);
	gp_gc_grestore (pc->gc);
	C (result);

	return GNOME_PRINT_OK;
}

gint
gnome_print_gsave_real (GnomePrintContext *pc)
{
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADVALUE);

	gp_gc_gsave (pc->gc);
	if (GNOME_PRINT_CONTEXT_GET_CLASS (pc)->gsave)
		C (GNOME_PRINT_CONTEXT_GET_CLASS (pc)->gsave (pc));

	return GNOME_PRINT_OK;
}

gint
gnome_print_fill_bpath_rule_real (GnomePrintContext *pc,
				  const ArtBpath *bpath, ArtWindRule rule)
{
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADVALUE);

	if (GNOME_PRINT_CONTEXT_GET_CLASS (pc)->fill)
		C (GNOME_PRINT_CONTEXT_GET_CLASS (pc)->fill (pc, bpath, rule)); 

	return GNOME_PRINT_OK;
}

gint
gnome_print_clip_bpath_rule_real (GnomePrintContext *pc,
				  const ArtBpath *bpath, ArtWindRule rule)
{
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADVALUE);

	if (GNOME_PRINT_CONTEXT_GET_CLASS (pc)->clip)
		C (GNOME_PRINT_CONTEXT_GET_CLASS (pc)->clip (pc, bpath, rule));

	return GNOME_PRINT_OK;
}

gint
gnome_print_stroke_bpath_real (GnomePrintContext *pc, const ArtBpath *bpath)
{
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADVALUE);

	if (GNOME_PRINT_CONTEXT_GET_CLASS (pc)->stroke)
		C (GNOME_PRINT_CONTEXT_GET_CLASS (pc)->stroke (pc, bpath));

	return GNOME_PRINT_OK;
}

gint
gnome_print_image_transform_real (GnomePrintContext *pc, const gdouble *a,
		const guchar *px, gint w, gint h, gint r, gint ch)
{
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADVALUE);

	if (GNOME_PRINT_CONTEXT_GET_CLASS (pc)->image)
		C (GNOME_PRINT_CONTEXT_GET_CLASS (pc)->image (pc, a, px, w, h, r, ch));

	return GNOME_PRINT_OK;
}

gint
gnome_print_glyphlist_transform_real (GnomePrintContext *pc,
				      const gdouble *a, GnomeGlyphList *l)
{
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADVALUE);

	if (GNOME_PRINT_CONTEXT_GET_CLASS (pc)->glyphlist)
		C (GNOME_PRINT_CONTEXT_GET_CLASS (pc)->glyphlist (pc, a, l));

	return GNOME_PRINT_OK;
}

gint
gnome_print_setrgbcolor_real (GnomePrintContext *pc, gdouble r, gdouble g, gdouble b)
{
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADVALUE);

	C (gp_gc_set_rgbcolor (pc->gc, r, g, b));

	return GNOME_PRINT_OK;
}

gint
gnome_print_setopacity_real (GnomePrintContext *pc, gdouble opacity)
{
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADVALUE);

	C (gp_gc_set_opacity (pc->gc, opacity));

	return GNOME_PRINT_OK;
}

gint
gnome_print_setlinewidth_real (GnomePrintContext *pc, double width)
{
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADVALUE);

	C (gp_gc_set_linewidth (pc->gc, width));

	return GNOME_PRINT_OK;
}

/* Direct class-method frontends */

/**
 * gnome_print_beginpage:
 * @pc: A #GnomePrintContext
 * @name: Name of the page, NULL if you just want to use the page number of the page
 *
 * Starts new output page with @name. Naming is used for interactive
 * contexts like #GnomePrintPreview and Document Structuring Convention
 * conformant PostScript output.
 * This function has to be called before any drawing methods and immediately
 * after each #gnome_print_showpage albeit the last one. It also resets
 * graphic state values (transformation, color, line properties, font),
 * so one has to define these again at the beginning of each page.
 *
 * Returns: #GNOME_PRINT_OK or positive value on success, negative error
 * code on failure.
 */
int
gnome_print_beginpage (GnomePrintContext *pc, const guchar *name)
{
	guchar *real_name = (guchar *) name;
	gint ret;

	g_return_val_if_fail (pc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (pc->gc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);

	gp_gc_reset (pc->gc);
	pc->haspage = TRUE;

	if (!name)
		real_name = (guchar *) g_strdup_printf ("%d", pc->pages + 1);
	if (pc->priv->filter)
		ret = gnome_print_filter_beginpage (pc->priv->filter, pc, real_name);
	else
		ret = gnome_print_beginpage_real (pc, real_name);
	if (!name)
		g_free (real_name);

	return ret;
}

/**
 * gnome_print_showpage:
 * @pc: A #GnomePrintContext
 *
 * Finishes rendering of current page, and marks it as shown. All subsequent
 * drawing methods will fail, until new page is started with #gnome_print_newpage.
 * Printing contexts may process drawing methods differently - some do
 * rendering immediately (like #GnomePrintPreview), some accumulate all
 * operators to internal stack, and only after #gnome_print_showpage is
 * any output produced.
 *
 * Returns: #GNOME_PRINT_OK or positive value on success, negative error
 * code on failure.
 */
int
gnome_print_showpage (GnomePrintContext *pc)
{
	gint result = GNOME_PRINT_OK;

	g_return_val_if_fail (pc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (pc->gc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (pc->haspage, GNOME_PRINT_ERROR_NOPAGE);

	if (pc->priv->filter)
		result = gnome_print_filter_showpage (pc->priv->filter);
	else
		result = gnome_print_showpage_real (pc);
	pc->haspage = FALSE;
	C (result);

	return GNOME_PRINT_OK;
}

/**
 * gnome_print_end_doc:
 * @pc: A #GnomePrintContext
 *
 * to be called at the end of any copy of the document before the next
 * copy starts. It will do such things as ejecting a page in duplex printing.
 *
 *
 *
 *
 *
 * Returns: #GNOME_PRINT_OK or positive value on success, negative error
 * code on failure.
 */
int
gnome_print_end_doc (GnomePrintContext *pc)
{
	gint ret;

	g_return_val_if_fail (pc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc),
			      GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (pc->gc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (!pc->haspage, GNOME_PRINT_ERROR_NOMATCH);

	ret = GNOME_PRINT_OK;

	if (GNOME_PRINT_CONTEXT_GET_CLASS (pc)->end_doc)
		ret = GNOME_PRINT_CONTEXT_GET_CLASS (pc)->end_doc (pc);

	return ret;
}

/**
 * gnome_print_gsave:
 * @pc: A #GnomePrintContext
 *
 * Saves current graphic state (transformation, color, line properties, font)
 * into stack (push). Values itself remain unchanged.
 * You can later restore saved values, using #gnome_print_grestore, but not
 * over page boundaries. Graphic state stack has to be cleared for each
 * #gnome_print_showpage, i.e. the number of #gnome_print_gsave has to
 * match the number of #gnome_print_grestore for each page.
 *
 * Returns: #GNOME_PRINT_OK or positive value on success, negative error
 * code on failure.
 */
int
gnome_print_gsave (GnomePrintContext *pc)
{
	g_return_val_if_fail (pc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (pc->gc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (pc->haspage, GNOME_PRINT_ERROR_NOPAGE);

	if (pc->priv->filter)
		return gnome_print_filter_gsave (pc->priv->filter);
	else
		return gnome_print_gsave_real (pc);
}

/**
 * gnome_print_grestore:
 * @pc: A #GnomePrintContext
 *
 * Retrieves last saved graphic state from stack (pop). Stack has to be
 * at least the size of one.
 *
 * Returns: #GNOME_PRINT_OK or positive value on success, negative error
 * code on failure.
 */
int
gnome_print_grestore (GnomePrintContext *pc)
{
	g_return_val_if_fail (pc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (pc->gc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (pc->haspage, GNOME_PRINT_ERROR_NOPAGE);

	if (pc->priv->filter)
		return gnome_print_filter_grestore (pc->priv->filter);
	else
		return gnome_print_grestore_real (pc);
}

int
gnome_print_clip_bpath_rule (GnomePrintContext *pc, const ArtBpath *bpath, ArtWindRule rule)
{
	g_return_val_if_fail (pc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (pc->gc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (pc->haspage, GNOME_PRINT_ERROR_NOPAGE);
	g_return_val_if_fail ((rule == ART_WIND_RULE_NONZERO) || (rule == ART_WIND_RULE_ODDEVEN), GNOME_PRINT_ERROR_BADVALUE);

	if (pc->priv->filter)
		return gnome_print_filter_clip (pc->priv->filter, bpath, rule);
	else
		return gnome_print_clip_bpath_rule_real (pc, bpath, rule);
}

int
gnome_print_fill_bpath_rule (GnomePrintContext *pc, const ArtBpath *bpath, ArtWindRule rule)
{
	g_return_val_if_fail (pc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (pc->gc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (pc->haspage, GNOME_PRINT_ERROR_NOPAGE);
	g_return_val_if_fail ((rule == ART_WIND_RULE_NONZERO) || (rule == ART_WIND_RULE_ODDEVEN), GNOME_PRINT_ERROR_BADVALUE);

	if (pc->priv->filter)
		return gnome_print_filter_fill (pc->priv->filter, bpath, rule);
	else
		return gnome_print_fill_bpath_rule_real (pc, bpath, rule);
}

int
gnome_print_stroke_bpath (GnomePrintContext *pc, const ArtBpath *bpath)
{
	g_return_val_if_fail (pc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (pc->gc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (pc->haspage, GNOME_PRINT_ERROR_NOPAGE);
	g_return_val_if_fail (bpath != NULL, GNOME_PRINT_ERROR_BADVALUE);

	if (pc->priv->filter)
		return gnome_print_filter_stroke (pc->priv->filter, bpath);
	else
		return gnome_print_stroke_bpath_real (pc, bpath);
}

int
gnome_print_image_transform (GnomePrintContext *pc, const gdouble *affine, const guchar *px, gint w, gint h, gint rowstride, gint ch)
{
	g_return_val_if_fail (pc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (pc->gc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (pc->haspage, GNOME_PRINT_ERROR_NOPAGE);
	g_return_val_if_fail (affine != NULL, GNOME_PRINT_ERROR_BADVALUE);
	g_return_val_if_fail (px != NULL, GNOME_PRINT_ERROR_BADVALUE);
	g_return_val_if_fail (w > 0, GNOME_PRINT_ERROR_BADVALUE);
	g_return_val_if_fail (h > 0, GNOME_PRINT_ERROR_BADVALUE);
	g_return_val_if_fail (rowstride >= ch * w, GNOME_PRINT_ERROR_BADVALUE);
	g_return_val_if_fail ((ch == 1) || (ch == 3) || (ch == 4), GNOME_PRINT_ERROR_BADVALUE);

	if (pc->priv->filter)
		return gnome_print_filter_image (pc->priv->filter, affine, px, w, h, rowstride, ch);
	else
		return gnome_print_image_transform_real (pc, affine, px, w, h, rowstride, ch);
}

int
gnome_print_glyphlist_transform (GnomePrintContext *pc, const gdouble *affine, GnomeGlyphList *gl)
{
	g_return_val_if_fail (pc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (pc->gc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (pc->haspage, GNOME_PRINT_ERROR_NOPAGE);
	g_return_val_if_fail (affine != NULL, GNOME_PRINT_ERROR_BADVALUE);
	g_return_val_if_fail (gl != NULL, GNOME_PRINT_ERROR_BADVALUE);
	g_return_val_if_fail (GNOME_IS_GLYPHLIST (gl), GNOME_PRINT_ERROR_BADVALUE);

	if (pc->priv->filter)
		return gnome_print_filter_glyphlist (pc->priv->filter, affine, gl);
	else
		return gnome_print_glyphlist_transform_real (pc, affine, gl);
}

/**
 * gnome_print_setrgbcolor:
 * @pc: A #GnomePrintContext
 * @r: Red channel value
 * @g: Green channel value
 * @b: Blue channel value
 *
 * Sets color in graphic state to RGB triplet. This does not imply anything
 * about which colorspace is used internally.
 * Channel values are clamped to 0.0 - 1.0 region, 0.0 meaning minimum.
 *
 * Returns: #GNOME_PRINT_OK or positive value on success, negative error
 * code on failure.
 */
int
gnome_print_setrgbcolor (GnomePrintContext *pc, double r, double g, double b)
{
	g_return_val_if_fail (pc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (pc->gc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);

	if (pc->priv->filter)
		return gnome_print_filter_setrgbcolor (pc->priv->filter, r, g, b);
	else
		return gnome_print_setrgbcolor_real (pc, r, g, b);
}

/**
 * gnome_print_setopacity:
 * @pc: A #GnomePrintContext
 * @opacity: Opacity value
 *
 * Sets painting opacity in graphic state to given value.
 * Value is clamped to 0.0 - 1.0 region, 0.0 meaning full transparency and
 * 1.0 completely opaque paint.
 *
 * Returns: #GNOME_PRINT_OK or positive value on success, negative error
 * code on failure.
 */
int
gnome_print_setopacity (GnomePrintContext *pc, double opacity)
{
	g_return_val_if_fail (pc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (pc->gc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);

	if (pc->priv->filter)
		return gnome_print_filter_setopacity (pc->priv->filter, opacity);
	else
		return gnome_print_setopacity_real (pc, opacity);
}

/**
 * gnome_print_setlinewidth:
 * @pc: A #GnomePrintContext
 * @width: Line width in user coordinates
 *
 * Sets line width in graphic state to given value.
 * Value is given in user coordinates, so effective line width depends on
 * CTM at the moment of #gnome_print_stroke or #gnome_print_strokepath.
 * Line width is always uniform in all directions, regardless of stretch
 * factor of CTM.
 * Default line width is 1.0 in user coordinates.
 *
 * Returns: #GNOME_PRINT_OK or positive value on success, negative error
 * code on failure.
 */
int
gnome_print_setlinewidth (GnomePrintContext *pc, double width)
{
	g_return_val_if_fail (pc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (pc->gc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);

	if (pc->priv->filter)
		return gnome_print_filter_setlinewidth (pc->priv->filter, width);
	else
		return gnome_print_setlinewidth_real (pc, width);
}

/**
 * gnome_print_context_close:
 * @pc: A #GnomePrintContext
 *
 * Informs given #GnomePrintContext that application has finished print
 * job. From that point on, @pc has to be considered illegal pointer,
 * and any further printing operation with it may kill application.
 * Some printing contexts may not start printing before context is
 * closed.
 *
 * Returns: #GNOME_PRINT_OK or positive value on success, negative error
 * code on failure.
 */
int
gnome_print_context_close (GnomePrintContext *pc)
{
	gint ret = GNOME_PRINT_OK;

	g_return_val_if_fail (pc != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (pc), GNOME_PRINT_ERROR_BADCONTEXT);

	if (pc->priv->filter)
		gnome_print_filter_flush (pc->priv->filter);

	if (GNOME_PRINT_CONTEXT_GET_CLASS (pc)->close)
		ret = GNOME_PRINT_CONTEXT_GET_CLASS (pc)->close (pc);

	if (ret != GNOME_PRINT_OK) {
		g_warning ("Could not close transport inside gnome_print_context_close");
		return ret;
	}

	if (pc->transport) {
		g_warning ("file %s: line %d: Closing Context should clear transport", __FILE__, __LINE__);
		return ret;
	}

	return ret;
}

GnomePrintContext *
gnome_print_context_new_from_module_name (const gchar *module_name)
{
	if (!module_name) return NULL;

	if (!strcmp (module_name, "rbuf"))
		return g_object_new (GNOME_TYPE_PRINT_RBUF, NULL);
	if (!strcmp (module_name, "meta"))
		return g_object_new (GNOME_TYPE_PRINT_META, NULL);

	return NULL;
}

/**
 * gnome_print_context_new:
 * @config: GnomePrintConfig object to query print settings from
 *
 * Create new printing context from config. You have to have set
 * all the options/settings beforehand, as changing the config of
 * an existing context has undefined results.
 *
 * Also, if creating the context by hand, it completely ignores layout and
 * orientation value. If you need those, use GnomePrintJob. The
 * later also can create output context for you, so in most cases
 * you may want to ignore gnome_print_context_new at all.
 *
 * Returns: The new GnomePrintContext or NULL on error
 */
GnomePrintContext *
gnome_print_context_new (GnomePrintConfig *config)
{
	GnomePrintContext *pc = NULL;
	gchar *drivername;

	g_return_val_if_fail (config != NULL, NULL);

	drivername = (gchar *) gnome_print_config_get (config, 
			(const guchar *) "Settings.Engine.Backend.Driver");
	if (drivername == NULL) {
		drivername = g_strdup ("gnome-print-ps");
	}

	if (strcmp (drivername, "gnome-print-ps") == 0) {
		g_free (drivername);
		return gnome_print_ps2_new (config);
	}

	if (strcmp (drivername, "gnome-print-pdf") == 0) {
		pc = gnome_print_pdf_new (config);
		if (pc == NULL)
			return NULL;
		g_free (drivername);
		return pc;
	}

#ifdef ENABLE_SVG
	if (strcmp (drivername, "gnome-print-svg") == 0) {
		pc = gnome_print_svg_new (config);
		if (pc == NULL)
			return NULL;
		g_free (drivername);
		return pc;
	}
#endif

#ifdef HAVE_GDI
	if (strcmp (drivername, "gnome-print-gdi") == 0) {
		pc = gnome_print_gdi_new (config);
		if (pc == NULL)
			return NULL;
		g_free (drivername);
		return pc;
	}
#endif

	if (pc == NULL)
		g_warning ("Could not create context for driver: %s", drivername);

	g_free (drivername);

	return pc;
}


void
gnome_print_buffer_munmap (GnomePrintBuffer *b)
{
	if (b->buf) {
#ifdef HAVE_MMAP
		if (b->was_mmaped)
			munmap (b->buf, b->buf_size);
		else
#elif defined G_OS_WIN32
		if (b->was_mmaped)
			UnmapViewOfFile (b->buf);
		else
#endif
			g_free (b->buf);
	}

	b->buf = NULL;
	b->buf_size = 0;

#ifdef HAVE_BROKEN_MMAP
	if (b->fd != -1)
		close (b->fd);
#endif
}

#define GP_MMAP_BUFSIZ 4096

gint
gnome_print_buffer_mmap (GnomePrintBuffer *b,
			 const guchar *file_name)
{
	struct stat s;
	gint fh;

	b->buf = NULL;
	b->buf_size = 0;
	b->was_mmaped = FALSE;
	b->fd = -1;

	fh = open ((const gchar *) file_name, O_RDONLY | O_BINARY);
	if (fh < 0) {
		g_warning ("Can't open \"%s\"", file_name);
		return GNOME_PRINT_ERROR_UNKNOWN;
	}
	if (fstat (fh, &s) != 0) {
		g_warning ("Can't stat \"%s\"", file_name);
		close (fh);
		return GNOME_PRINT_ERROR_UNKNOWN;
	}

#ifdef HAVE_MMAP
	b->buf = mmap (NULL, s.st_size, PROT_READ, MAP_SHARED, fh, 0);
#elif defined G_OS_WIN32
	{
		HANDLE handle;

		handle = CreateFileMapping ((HANDLE) _get_osfhandle (fh), NULL, PAGE_READONLY, 0,
		s.st_size, NULL);
		if (!handle) {
			g_warning ("Can't CreateFileMapping file %s", file_name);
			return GNOME_PRINT_ERROR_UNKNOWN;
		}
		b->buf = MapViewOfFile (handle, FILE_MAP_READ, 0, 0, 0);
		CloseHandle (handle);
		if (!b->buf) {
			g_warning ("Can't MapViewOfFile file %s", file_name);
			return GNOME_PRINT_ERROR_UNKNOWN;
		}
		b->buf_size = s.st_size;
	}
#endif

	if ((b->buf == NULL) || (b->buf == (void *) -1)) {
		g_warning ("Can't mmap file %s - attempting a fallback...", file_name);

		b->buf = g_try_malloc (s.st_size);
		b->buf_size = s.st_size;
		if (b->buf != NULL) {
			gssize nread, total_read;

			nread = total_read = 0;

			while (total_read < s.st_size) {
				nread = read (fh, b->buf + total_read,
					      MIN (GP_MMAP_BUFSIZ, s.st_size - total_read));
				if (nread == 0) {
					b->buf_size = total_read;
					break; /* success */
				} else if (nread == -1) {
					if (errno != EINTR) {
						g_free (b->buf);
						b->buf = NULL;
						b->buf_size = 0;
						break; /* failure */
				}
				} else
					total_read += nread;
			}
		}
	} else {
		b->was_mmaped = TRUE;
		b->buf_size = s.st_size;
	}

#ifdef HAVE_BROKEN_MMAP
	if (b->buf != NULL)
		b->fd = fh;
	else
#endif
		close (fh);

	if ((b->buf == NULL) || (b->buf == (void *) -1)) {
		g_warning ("Can't mmap file %s", file_name);
		return GNOME_PRINT_ERROR_UNKNOWN;
	}

	return GNOME_PRINT_OK;
}

#ifdef G_OS_WIN32
BOOL APIENTRY DllMain (HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved)
{
	char *gnome_print_prefix;

	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			gnome_print_prefix = g_win32_get_package_installation_directory (NULL, 
				"libgnomeprint-2-2-0.dll");
			if (gnome_print_prefix) {
				gnome_print_locale_dir = g_build_filename (
					gnome_print_prefix, "share", "locale", NULL);
				gnome_print_modules_dir = g_build_filename (
					gnome_print_prefix, "lib", "libgnomeprint", VERSION, "modules", NULL);
				gnome_print_data_dir = g_build_filename (
					gnome_print_prefix, "share", "libgnomeprint", VERSION, NULL);
			}

			break;
		case DLL_PROCESS_DETACH:
			g_free (gnome_print_locale_dir);
			g_free (gnome_print_modules_dir);
			g_free (gnome_print_data_dir);

			break;
	}

	return TRUE;
}
#endif
