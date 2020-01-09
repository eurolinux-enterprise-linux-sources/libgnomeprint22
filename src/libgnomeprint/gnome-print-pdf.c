/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  gnome-print-pdf.c: the PDF backend
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useoful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors:
 *    Chema Celorio <chema@celorio.com>
 *    Lauris Kaplinski <lauris@helixcode.com>
 *
 *  Copyright 2000-2001 Ximian, Inc. and authors
 *
 *  References:
 *    [1] Portable Document Format Referece Manual, Version 1.3 (March 11, 1999)
 */

#include <config.h>
#include <math.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <locale.h>

#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_misc.h>
#include <libgnomeprint/gnome-print-private.h>
#include <libgnomeprint/gp-gc-private.h>
#include <libgnomeprint/gnome-print-transport.h>
#include <libgnomeprint/gnome-font-private.h>
#include <libgnomeprint/gnome-print-encode.h>
#include <libgnomeprint/gnome-pgl-private.h>
#include <libgnomeprint/gnome-print-pdf-private.h>
#include <libgnomeprint/gnome-print-pdf.h>

#define GNOME_PRINT_PDF_INITIAL_BUFFER_SIZE 1024

typedef struct _GnomePrintPdfClass GnomePrintPdfClass;
typedef struct _GnomePrintPdfPage   GnomePrintPdfPage;
typedef struct _GnomePrintPdfObject GnomePrintPdfObject;

typedef enum {
	PDF_COMPRESSION_NONE,
	PDF_COMPRESSION_FLATE,
	PDF_COMPRESSION_HEX
}PdfCompressionType;

typedef enum {
	PDF_GRAPHIC_MODE_GRAPHICS,
	PDF_GRAPHIC_MODE_TEXT
} PdfGraphicMode;

typedef enum {
	PDF_COLOR_MODE_DEVICEGRAY,
	PDF_COLOR_MODE_DEVICERGB,
	PDF_COLOR_MODE_DEVICECMYK
}PdfColorModes;

typedef enum {
	PDF_COLOR_GROUP_FILL,
	PDF_COLOR_GROUP_STROKE,
	PDF_COLOR_GROUP_BOTH
}PdfColorGroup;

struct _GnomePrintPdfClass {
	GnomePrintContextClass parent_class;
};

struct _GnomePrintPdf {
	GnomePrintContext pc;

	/* Bounding box */
	ArtDRect bbox;

	/* lists */
	GList *fonts; /* list of GnomePrintPdfFont objects */
	GList *pages; /* list of GnomePrintPdfPage objects */

	/* State */
	GnomePrintPdfFont *selected_font;
	gdouble r, g, b;    /* Fill colors */
	gdouble r_, g_, b_; /* Stroke colors */
	gint color_set_fill;
	gint color_set_stroke;
	PdfGraphicMode mode;

	/* Transport */
	gint offset;

	/* Objects */
	GList *objects;
	gint current_object;

        /* We have a default GS which we use in all the pages */
	gint object_number_gs;

	/* Contents */
	gchar *stream;
	gint   stream_used;
	gint   stream_allocated;
};

struct _GnomePrintPdfObject {
	gint number;
	gint offset;
};

struct _GnomePrintPdfPage {
	gchar *name;
	gint number;

	guint shown : 1;
	guint used_grayscale_images : 1;
	guint used_color_images : 1;
	guint used_text : 1;
	guint gs_set : 1;

	/* Resources */
	GList *images;
	GList *fonts;

	/* Object number for pages */
	gint object_number_page;
	gint object_number_contents;
	gint object_number_resources;

};

static void gnome_print_pdf_class_init (GnomePrintPdfClass *klass);
static void gnome_print_pdf_init (GnomePrintPdf *pdf);
static void gnome_print_pdf_finalize (GObject *object);

static gint gnome_print_pdf_construct (GnomePrintContext *ctx);
static gint gnome_print_pdf_gsave (GnomePrintContext *pc);
static gint gnome_print_pdf_grestore (GnomePrintContext *pc);
static gint gnome_print_pdf_fill (GnomePrintContext *pc, const ArtBpath *bpath, ArtWindRule rule);
static gint gnome_print_pdf_clip (GnomePrintContext *pc, const ArtBpath *bpath, ArtWindRule rule);
static gint gnome_print_pdf_stroke (GnomePrintContext *pc, const ArtBpath *bpath);
static gint gnome_print_pdf_image (GnomePrintContext *pc, const gdouble *affine, const guchar *px, gint w, gint h, gint rowstride, gint ch);
static gint gnome_print_pdf_glyphlist (GnomePrintContext *pc, const gdouble *affine, GnomeGlyphList *gl);
static gint gnome_print_pdf_beginpage (GnomePrintContext *pc, const guchar *name);
static gint gnome_print_pdf_showpage (GnomePrintContext *pc);
static gint gnome_print_pdf_close (GnomePrintContext *pc);

static gint gnome_print_pdf_set_color (GnomePrintPdf *pdf, PdfColorGroup color_group);
static gint gnome_print_pdf_set_line (GnomePrintPdf *pdf);
static gint gnome_print_pdf_set_dash (GnomePrintPdf *pdf);

static gint gnome_print_pdf_set_color_real (GnomePrintPdf *pdf, PdfColorGroup color_group,
					    gdouble r, gdouble g, gdouble b);

static gint gnome_print_pdf_set_font_real (GnomePrintPdf *pdf, const GnomeFont *font, gboolean subfont_flag, gint page);
static void gnome_print_pdf_font_print_encoding (GnomePrintPdf *pdf, GnomePrintPdfFont *font);
static void gnome_print_pdf_font_print_widths (GnomePrintPdf *pdf, GnomePrintPdfFont *font);
static void gnome_print_pdf_font_print_lastchar (GnomePrintPdf *pdf, GnomePrintPdfFont *font);

static gint gnome_print_pdf_print_bpath (GnomePrintPdf *pdf, const ArtBpath *bpath);

static gchar *gnome_print_pdf_get_date (void);

static gint  gnome_print_pdf_graphic_mode_set (GnomePrintPdf *pdf, PdfGraphicMode mode);
static gint  gnome_print_pdf_page_fprintf (GnomePrintPdf *pdf, const char *format, ...) G_GNUC_PRINTF (2, 3);

static GnomePrintContextClass *parent_class;

GType
gnome_print_pdf_get_type (void)
{
	static GType pdf_type = 0;
	if (!pdf_type) {
		static const GTypeInfo pdf_info = {
			sizeof (GnomePrintPdfClass),
			NULL, NULL,
			(GClassInitFunc) gnome_print_pdf_class_init,
			NULL, NULL,
			sizeof (GnomePrintPdf),
			0,
			(GInstanceInitFunc) gnome_print_pdf_init
		};
		pdf_type = g_type_register_static (GNOME_TYPE_PRINT_CONTEXT, "GnomePrintPdf", &pdf_info, 0);
	}
	return pdf_type;
}

static void
gnome_print_pdf_class_init (GnomePrintPdfClass *klass)
{
	GnomePrintContextClass *pc_class;
	GnomePrintPdfClass *pdf_class;
	GObjectClass *object_class;

	object_class = (GObjectClass *) klass;
	pc_class = (GnomePrintContextClass *)klass;
	pdf_class = (GnomePrintPdfClass *)klass;

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gnome_print_pdf_finalize;

	pc_class->construct = gnome_print_pdf_construct;
	pc_class->beginpage = gnome_print_pdf_beginpage;
	pc_class->showpage = gnome_print_pdf_showpage;
	pc_class->gsave = gnome_print_pdf_gsave;
	pc_class->grestore = gnome_print_pdf_grestore;
	pc_class->clip = gnome_print_pdf_clip;
	pc_class->fill = gnome_print_pdf_fill;
	pc_class->stroke = gnome_print_pdf_stroke;
	pc_class->image = gnome_print_pdf_image;
	pc_class->glyphlist = gnome_print_pdf_glyphlist;
	pc_class->close = gnome_print_pdf_close;

}

static void
gnome_print_pdf_init (GnomePrintPdf *pdf)
{
	pdf->selected_font    = NULL;
	pdf->color_set_fill   = FALSE;
	pdf->color_set_stroke = FALSE;
	pdf->mode = PDF_GRAPHIC_MODE_GRAPHICS;

	pdf->offset = 0;

	pdf->pages = NULL;
	pdf->fonts = NULL;
	pdf->objects = NULL;

	pdf->current_object = 0;
	pdf->object_number_gs = gnome_print_pdf_object_new (pdf);

	pdf->stream_allocated = GNOME_PRINT_PDF_INITIAL_BUFFER_SIZE;
	pdf->stream = g_malloc (pdf->stream_allocated);
	pdf->stream_used = 0;
}

static void
gnome_print_pdf_finalize (GObject *object)
{
	GnomePrintPdf *pdf;
	GList *list;

	pdf = GNOME_PRINT_PDF (object);

	/* Free pages */
	list = pdf->pages;
	while (list) {
		GnomePrintPdfPage *page;
		page = (GnomePrintPdfPage *) list->data;
		if (!page->shown)
			g_warning ("Page %d %s was not shown", page->number, page->name);
		if (page->name)
			g_free (page->name);
		g_list_free (page->fonts);
		g_free (page);
		list = list->next;
	}
	g_list_free (pdf->pages);

	/* Free fonts */
	list = pdf->fonts;
	while (list) {
		GnomePrintPdfFont *font;
		font = list->data;
		if (font->face)
			g_object_unref (G_OBJECT (font->face));
		font->face = NULL;
		if (font->pso)
			gnome_font_face_pso_free (font->pso);
		font->pso = NULL;
		g_free (font->code_to_glyph);
		g_hash_table_destroy (font->glyph_to_code);
		g_free (font);
		list = list->next;
	}
	g_list_free (pdf->fonts);

	/* Page stream */
	g_free (pdf->stream);
	pdf->stream_allocated = 0;
	pdf->stream_used = 0;

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gint
gnome_print_pdf_construct (GnomePrintContext *ctx)
{
	GnomePrintPdf *pdf;
	gint ret = GNOME_PRINT_OK;

	pdf = GNOME_PRINT_PDF (ctx);

	ret += gnome_print_context_create_transport (ctx);
	ret += gnome_print_transport_open (ctx->transport);
	g_return_val_if_fail (ret >= 0, ret);

        /* FIXME: if this is the logical area, we know it from the paper size and margins, fix. (Chema) */
	/* Set bbox to empty practical infinity */
	pdf->bbox.x0 = 0.0;
	pdf->bbox.y0 = 0.0;
	pdf->bbox.x1 = 210 * 72.0 / 25.4;
	pdf->bbox.y1 = 297 * 72.0 / 25.4;

	gnome_print_config_get_length (ctx->config, GNOME_PRINT_KEY_PAPER_WIDTH,  &pdf->bbox.x1, NULL);
	gnome_print_config_get_length (ctx->config, GNOME_PRINT_KEY_PAPER_HEIGHT, &pdf->bbox.y1, NULL);

	if (ctx->config) {
		gdouble pp2pa[6];
		art_affine_identity (pp2pa);
		if (gnome_print_config_get_transform (ctx->config, GNOME_PRINT_KEY_PAPER_ORIENTATION_MATRIX, pp2pa)) {
			art_drect_affine_transform (&pdf->bbox, &pdf->bbox, pp2pa);
			pdf->bbox.x1 -= pdf->bbox.x0;
			pdf->bbox.y1 -= pdf->bbox.y0;
			pdf->bbox.x0 = 0.0;
			pdf->bbox.y0 = 0.0;
		}
	}

	gnome_print_pdf_fprintf (pdf, "%cPDF-1.4" EOL, '%');
	gnome_print_pdf_fprintf (pdf, "%c%c%c%c%c" EOL, '%',
				 181, 237, 174, 251); /* [1] page 22-23 */

	return ret;
}

static gint
gnome_print_pdf_page_write (GnomePrintPdf *pdf, gchar *text)
{
	gint len;
	GnomePrintPdfPage *page;
	gint grow_size = GNOME_PRINT_PDF_INITIAL_BUFFER_SIZE;

	len = strlen (text);
	page = pdf->pages->data;
	if ((pdf->stream_used + len + 1) > pdf->stream_allocated) {
		while ((pdf->stream_used + len + 1) > pdf->stream_allocated) {
			pdf->stream_allocated += grow_size;
			grow_size <<= 1;
		}
		pdf->stream = g_realloc (pdf->stream, pdf->stream_allocated);
	}

	memcpy (pdf->stream + pdf->stream_used, text, len * sizeof (gchar));
	pdf->stream_used += len;
	pdf->stream [pdf->stream_used] = 0;

	return len;
}


/* Note "format" should be locale independent, so it should not use %.2f,
 * and pdf does not support scientific notation so it should not use %g
 */
static gint
gnome_print_pdf_page_fprintf (GnomePrintPdf *pdf, const char *format, ...)
{
 	va_list arguments;
	gint len;
 	gchar *text;

	/* Compose the text */
 	va_start (arguments, format);
 	text = g_strdup_vprintf (format, arguments);
 	va_end (arguments);

	/* Write it */
	len = gnome_print_pdf_page_write (pdf, text);

	g_free (text);

	return len;
}

/* Allowed conversion specifiers are 'f', 'F'
 *
 * NOTE: From pdf ref 1.5 v 6 : section 3.2.2
 * 	PDF does not support the PostScript syntax for numbers with nondecimal
 * 	radices (such as 16#FFFE) or in exponential format (such as 6.02E23).
 * 	so %g and %e will not work
 */
static gint
gnome_print_pdf_page_print_double (GnomePrintPdf *pdf,
				   const gchar *format, gdouble x)
{
 	gchar text [G_ASCII_DTOSTR_BUF_SIZE];
	g_ascii_formatd (text, G_ASCII_DTOSTR_BUF_SIZE, format, x);
	return gnome_print_pdf_page_write (pdf, text);
}

static gint
gnome_print_pdf_gsave (GnomePrintContext *ctx)
{
	GnomePrintPdf *pdf;

	pdf = GNOME_PRINT_PDF (ctx);

	gnome_print_pdf_graphic_mode_set (pdf, PDF_GRAPHIC_MODE_GRAPHICS);

	gnome_print_pdf_page_write (pdf, "q" EOL);

	return GNOME_PRINT_OK;
}

static gint
gnome_print_pdf_grestore (GnomePrintContext *ctx)
{
	GnomePrintPdf *pdf;

	pdf = GNOME_PRINT_PDF (ctx);

	gnome_print_pdf_graphic_mode_set (pdf, PDF_GRAPHIC_MODE_GRAPHICS);

	pdf->selected_font    = NULL;
	pdf->color_set_fill   = FALSE;
	pdf->color_set_stroke = FALSE;

	gnome_print_pdf_page_write (pdf, "Q" EOL);

	return GNOME_PRINT_OK;
}

static gint
gnome_print_pdf_clip (GnomePrintContext *ctx, const ArtBpath *bpath, ArtWindRule rule)
{
	GnomePrintPdf *pdf;

	pdf = GNOME_PRINT_PDF (ctx);

	gnome_print_pdf_graphic_mode_set (pdf, PDF_GRAPHIC_MODE_GRAPHICS);
	gnome_print_pdf_print_bpath (pdf, bpath);

	if (rule == ART_WIND_RULE_NONZERO) {
		gnome_print_pdf_page_write (pdf, "W n" EOL);
	} else {
		gnome_print_pdf_page_write (pdf, "W* n" EOL);
	}

	return GNOME_PRINT_OK;
}

static gint
gnome_print_pdf_fill (GnomePrintContext *ctx, const ArtBpath *bpath, ArtWindRule rule)
{
	GnomePrintPdf *pdf;

	pdf = GNOME_PRINT_PDF (ctx);

	gnome_print_pdf_graphic_mode_set (pdf, PDF_GRAPHIC_MODE_GRAPHICS);

	gnome_print_pdf_set_color (pdf, PDF_COLOR_GROUP_FILL);
	gnome_print_pdf_print_bpath (pdf, bpath);

	if (rule == ART_WIND_RULE_NONZERO) {
		gnome_print_pdf_page_write (pdf, "f" EOL);
	} else {
		gnome_print_pdf_page_write (pdf, "f*" EOL);
	}

	/* Chema Foo */
	/* Update bbox */

	return GNOME_PRINT_OK;
}

static gint
gnome_print_pdf_stroke (GnomePrintContext *ctx, const ArtBpath *bpath)
{
	GnomePrintPdf *pdf;

	pdf = GNOME_PRINT_PDF (ctx);

	gnome_print_pdf_graphic_mode_set (pdf, PDF_GRAPHIC_MODE_GRAPHICS);

	gnome_print_pdf_set_color (pdf, PDF_COLOR_GROUP_STROKE);
	gnome_print_pdf_set_line (pdf);
	gnome_print_pdf_set_dash (pdf);
	gnome_print_pdf_print_bpath (pdf, bpath);

	gnome_print_pdf_page_write (pdf, "S" EOL);

	return GNOME_PRINT_OK;
}

/* todo: this and pdf_image() can probably be merged into a single function */
static void
gnome_print_pdf_rgba_image_mask (GnomePrintContext *pc, gint object_num, const guchar *data,
				 gint width, gint height, gint rowstride)
{
	GnomePrintPdfPage *page;
	GnomePrintPdf *pdf;
	gint hex_size, row, ret = 0;
	guchar *hex;
	gint length_object_num, len = 0;

	pdf = GNOME_PRINT_PDF (pc);

	/* The image object */
	gnome_print_pdf_object_start (pdf, object_num, FALSE);
	gnome_print_pdf_fprintf (pdf,
				 "/Type /XObject" EOL
				 "/Subtype /Image" EOL
				 "/Name /Im%d" EOL
				 "/Width %d" EOL
				 "/Height %d" EOL
				 "/ColorSpace /DeviceGray" EOL
				 "/BitsPerComponent 8" EOL,
				 object_num,
				 width,
				 height);
	length_object_num = gnome_print_pdf_object_new (pdf);
	gnome_print_pdf_fprintf (pdf, "/Length %d 0 R" EOL, length_object_num);

	/* FIXME: We are not doing stream compresion/encoding yet. */
	gnome_print_pdf_fprintf (pdf, "/Filter [/ASCIIHexDecode ]" EOL);
	gnome_print_pdf_fprintf (pdf, ">>" EOL);
	gnome_print_pdf_fprintf (pdf, "stream" EOL);

	/* The image data */
	hex = g_new (guchar, gnome_print_encode_hex_wcs (width * 3));
	for (row = 0; row < height; row++) {
		gint x;
		hex_size = 0;

		for (x = 0; x < rowstride; x += 4) {
			hex_size += gnome_print_encode_hex (data + (row * rowstride) + x + 3,
							    hex + hex_size, 1);
			hex_size--; /* encode_hex always (annoyingly) appends a \n at the end */
		}

		len += gnome_print_pdf_print_sized (pdf, hex, hex_size * sizeof (gchar));
		len += gnome_print_pdf_fprintf (pdf, EOL);
	}
	g_free (hex);
	/* END: The image data */

	gnome_print_pdf_fprintf (pdf,  "endstream" EOL "endobj" EOL);
	ret += gnome_print_pdf_object_end (pdf, object_num, TRUE);

	/* The length object,
	 * which we write at the end so that we don't
	 * have to pre-calculate the size of the stream. This allows us to write the
	 * pdf in one pass and not have to keep the image in memory.
	 */
	gnome_print_pdf_object_start (pdf, length_object_num, TRUE);
	gnome_print_pdf_fprintf (pdf,
				 "%d 0 obj" EOL
				 "%d" EOL
				 "endobj" EOL,
				 length_object_num, len);
	gnome_print_pdf_object_end (pdf, length_object_num, TRUE);

	/* Reference on the page stream to the image that has just been created */
	page = pdf->pages->data;

	/* And now add it to the images used for this page */
	page->images = g_list_prepend (page->images, GINT_TO_POINTER (object_num));
}

/* FIXME: Change the other gnome-print image functions to match this prototype */
static int
gnome_print_pdf_image (GnomePrintContext *pc, const gdouble *ctm, const guchar *data,
		       gint width, gint height, gint rowstride, gint bytes_per_pixel)
{
	GnomePrintPdfPage *page;
	GnomePrintPdf *pdf;
	gint hex_size, row, object_num, ret = 0, mask_object_num = 0;
	guchar *hex;
	gint length_object_num, len = 0;

	g_return_val_if_fail (1 == bytes_per_pixel || 3 == bytes_per_pixel || 4 == bytes_per_pixel, GNOME_PRINT_ERROR_UNKNOWN);

	pdf = GNOME_PRINT_PDF (pc);

	gnome_print_pdf_graphic_mode_set (pdf, PDF_GRAPHIC_MODE_GRAPHICS);

	/* The image object */
	object_num = gnome_print_pdf_object_new (pdf);

	gnome_print_pdf_object_start (pdf, object_num, FALSE);
	gnome_print_pdf_fprintf (pdf,
				 "/Type /XObject" EOL
				 "/Subtype /Image" EOL
				 "/Name /Im%d" EOL
				 "/Width %d" EOL
				 "/Height %d" EOL
				 "/BitsPerComponent 8" EOL
				 "/ColorSpace /%s" EOL,
				 object_num,
				 width,
				 height,
				 bytes_per_pixel == 1 ? "DeviceGray" : "DeviceRGB");
	length_object_num = gnome_print_pdf_object_new (pdf);
	gnome_print_pdf_fprintf (pdf, "/Length %d 0 R" EOL, length_object_num);

	if (bytes_per_pixel == 4) {
		mask_object_num = gnome_print_pdf_object_new (pdf);
		gnome_print_pdf_fprintf (pdf, "/SMask %d 0 R" EOL, mask_object_num);
	}

	/* FIXME: We are not doing stream compresion/encoding yet. */
	gnome_print_pdf_fprintf (pdf, "/Filter [/ASCIIHexDecode ]" EOL);
	gnome_print_pdf_fprintf (pdf, ">>" EOL);
	gnome_print_pdf_fprintf (pdf, "stream" EOL);

	/* The image data */
	hex = g_new (guchar, gnome_print_encode_hex_wcs (width * bytes_per_pixel));
	for (row = 0; row < height; row++) {
		hex_size = 0;

		if (bytes_per_pixel != 4) {
			hex_size = gnome_print_encode_hex (data + row * rowstride,
							   hex, width * bytes_per_pixel);
		} else {
			gint x;

			for (x = 0; x < rowstride; x += 4) {
				hex_size += gnome_print_encode_hex (data + (row * rowstride) + x,
								    hex + hex_size, 3);
				hex_size--; /* encode_hex always (annoyingly) appends a \n at the end */
			}
		}

		len += gnome_print_pdf_print_sized (pdf, hex, hex_size * sizeof (gchar));
		len += gnome_print_pdf_fprintf (pdf, EOL);
	}
	g_free (hex);
	/* END: The image data */

	gnome_print_pdf_fprintf (pdf,  "endstream" EOL "endobj" EOL);
	ret += gnome_print_pdf_object_end (pdf, object_num, TRUE);

	/* The length object,
	 * which we write at the end so that we don't
	 * have to pre-calculate the size of the stream. This allows us to write the
	 * pdf in one pass and not have to keep the image in memory.
	 */
	gnome_print_pdf_object_start (pdf, length_object_num, TRUE);
	gnome_print_pdf_fprintf (pdf,
				 "%d 0 obj" EOL
				 "%d" EOL
				 "endobj" EOL,
				 length_object_num, len);
	gnome_print_pdf_object_end (pdf, length_object_num, TRUE);

	/* Reference on the page stream to the image that has just been created */
	page = pdf->pages->data;
	if (bytes_per_pixel == 1)
		page->used_grayscale_images = TRUE;
	else
		page->used_color_images = TRUE;

	gnome_print_pdf_page_write (pdf, "q" EOL);
	gnome_print_pdf_page_print_double (pdf, "%.2f",
				      ctm[0]);
	gnome_print_pdf_page_write (pdf, " ");
	gnome_print_pdf_page_print_double (pdf, "%.2f",
				      ctm[1]);
	gnome_print_pdf_page_write (pdf, " ");
	gnome_print_pdf_page_print_double (pdf, "%.2f",
				      ctm[2]);
	gnome_print_pdf_page_write (pdf, " ");
	gnome_print_pdf_page_print_double (pdf, "%.2f",
				      ctm[3]);
	gnome_print_pdf_page_write (pdf, " ");
	gnome_print_pdf_page_print_double (pdf, "%.2f",
				      ctm[4]);
	gnome_print_pdf_page_write (pdf, " ");
	gnome_print_pdf_page_print_double (pdf, "%.2f",
				      ctm[5]);
	gnome_print_pdf_page_write (pdf, " cm" EOL);
	gnome_print_pdf_page_fprintf (pdf, "/Im%d Do" EOL, object_num);
	gnome_print_pdf_page_write (pdf, "Q" EOL);

	/* And now add it to the images used for this page */
	page->images = g_list_prepend (page->images, GINT_TO_POINTER (object_num));

	if (bytes_per_pixel == 4)
		gnome_print_pdf_rgba_image_mask (pc, mask_object_num, data, width, height, rowstride);

	return ret;
}


static gint
gnome_print_pdf_assign_code_to_glyph (GnomePrintPdf *pdf, gint glyph)
{
	GnomePrintPdfFont *font = pdf->selected_font;
	gint code;

	code = GPOINTER_TO_INT (g_hash_table_lookup (font->glyph_to_code,
						     GINT_TO_POINTER (glyph)));
	if (code < 1) {
		gnome_font_face_pso_mark_glyph (font->pso, glyph);
		code = (++font->code_assigned);
		font->code_to_glyph [code] = glyph;
		g_hash_table_insert (font->glyph_to_code,
				     GINT_TO_POINTER (glyph),
				     GINT_TO_POINTER (code));
	}

	return code;
}

static gint
gnome_print_pdf_glyphlist (GnomePrintContext *pc, const gdouble *ctm, GnomeGlyphList *gl)
{
	static gdouble id[] = {1.0, 0.0, 0.0, 1.0, 0.0, 0.0};

	GnomePrintPdf	  *pdf = GNOME_PRINT_PDF (pc);
	GnomePrintPdfPage *page;
	GnomePosGlyphList *pgl;
	GnomePosGlyph const *glyph;
	ArtPoint advance;
	char	 octal [ 1 /* header char  \\ */ +
			11 /* 32 bit int in octal */ +
			 1 /* trailing null */];
	gint	 ret, s, master_glyph, glyph_page, code;
	gint	 x_adjust, y_adjust, cur_y_adjust = 0;
	gdouble	 Tm [6], scale [6], base_y, prev_x = 0;
	enum { OUTSIDE, IN_ARRAY, IN_GLYPHS } state;

	gnome_print_pdf_graphic_mode_set (pdf, PDF_GRAPHIC_MODE_TEXT);

	page = pdf->pages->data;
	page->used_text = TRUE;

	pgl = gnome_pgl_from_gl (gl, id, GNOME_PGL_RENDER_DEFAULT);

	for (s = 0; s < pgl->num_strings; s++) {
		GnomePosString *ps;
		GnomeFont *font;
		gint i, current_page = -1;

		ps = pgl->strings + s;
		font = gnome_rfont_get_font (ps->rfont);

		ret = gnome_print_pdf_set_color_real (pdf,
						      PDF_COLOR_GROUP_FILL,
						      ((ps->color >> 24) & 0xff) / 255.0,
						      ((ps->color >> 16) & 0xff) / 255.0,
						      ((ps->color >>  8) & 0xff) / 255.0);

		art_affine_scale (scale, font->size, font->size);
		scale[4]	  = pgl->glyphs[ps->start].x;
		scale[5] = base_y = pgl->glyphs[ps->start].y;
		art_affine_multiply (Tm, scale, ctm);

		gnome_print_pdf_page_print_double (pdf, "%.2f", Tm [0]);
		gnome_print_pdf_page_write (pdf, " ");
		gnome_print_pdf_page_print_double (pdf, "%.2f", Tm [1]);
		gnome_print_pdf_page_write (pdf, " ");
		gnome_print_pdf_page_print_double (pdf, "%.2f", Tm [2]);
		gnome_print_pdf_page_write (pdf, " ");
		gnome_print_pdf_page_print_double (pdf, "%.2f", Tm [3]);
		gnome_print_pdf_page_write (pdf, " ");
		gnome_print_pdf_page_print_double (pdf, "%.2f", Tm [4]);
		gnome_print_pdf_page_write (pdf, " ");
		gnome_print_pdf_page_print_double (pdf, "%.2f", Tm [5]);
		gnome_print_pdf_page_write (pdf, " Tm" EOL);

		/* Build string */
		state = OUTSIDE;
		for (i = ps->start; i < ps->start + ps->length; i++) {
			glyph	     = pgl->glyphs + i;
			master_glyph = glyph->glyph;

			if (i != ps->start) {
				state = IN_GLYPHS;
				x_adjust = (int)  (advance.x + .5) -
					   (int) ((1000. *(glyph->x - prev_x) / font->size) + .5);
				y_adjust = (int)  (advance.y + .5) -
					   (int) ((1000. *(glyph->y - base_y) / font->size) + .5);
#if 0
				fprintf (stderr, "advance [%d].x = %4.0f\tvs a delta of\t%g\n", i,
					 advance.x, 1000. *(glyph->x - prev_x) / font->scale);
				fprintf (stderr, "advance [%d].y = %4.0f\tvs a delta of\t%g\n", i,
					 advance.y, 1000. *(glyph->y - base_y) / font->scale);
#endif
				if (x_adjust != 0) {
					gnome_print_pdf_page_fprintf (pdf, ") %d" EOL, x_adjust);
					state = IN_ARRAY;
				}

				if (cur_y_adjust != y_adjust) {
					if (state == IN_GLYPHS)
						gnome_print_pdf_page_write (pdf, ")");
					gnome_print_pdf_page_fprintf (pdf, " %d] TJ " EOL,
						(cur_y_adjust = y_adjust));
					state = OUTSIDE;
				}
				if (NEEDED_SUBSETTING (font)) {
					glyph_page = master_glyph / 255;
					if (glyph_page != current_page) {
						if (state == IN_GLYPHS)
							gnome_print_pdf_page_write (pdf, ")] TJ" EOL);
						else if (state == IN_ARRAY)
							gnome_print_pdf_page_write (pdf, "] TJ" EOL);
						state = OUTSIDE;
						gnome_print_pdf_set_font_real (pdf, font, TRUE,
							(current_page = glyph_page));
					}
				}
			} else if (NEEDED_SUBSETTING (font)) {
				current_page   = master_glyph / 255;
				gnome_print_pdf_set_font_real (pdf, font, TRUE, current_page);
			} else
				gnome_print_pdf_set_font_real (pdf, font, FALSE, 0);

			if (NEEDED_SUBSETTING (font)) {
				gnome_font_face_pso_mark_glyph (pdf->selected_font->pso, master_glyph);
				/* For each sub ttf font the first glyph is
				 * null and 255th one is a valid glyph, also 1
				 * needed to be added to each subglyph  for
				 * taking into account first null glyph added
				 * to every subfont */
				code = master_glyph ? master_glyph % 255 + 1 : master_glyph;
			} else
				code = gnome_print_pdf_assign_code_to_glyph (pdf, master_glyph);

			if (state < IN_GLYPHS) {
				ret = gnome_print_pdf_page_write (pdf, (state == OUTSIDE) ? "[(" : "(");
				state = IN_GLYPHS;
			}

			g_snprintf (octal, sizeof(octal), "\\%o", code);
			ret = gnome_print_pdf_page_write (pdf, octal);
			g_return_val_if_fail (ret >= 0, ret);

			gnome_font_face_get_glyph_stdadvance (font->face, master_glyph, &advance);
			prev_x = glyph->x;
		}
		ret = gnome_print_pdf_page_write (pdf, ")] TJ" EOL);
	}

	gnome_pgl_destroy (pgl);

	return GNOME_PRINT_OK;
}

static gint
gnome_print_pdf_beginpage (GnomePrintContext *pc, const guchar *name)
{
	GnomePrintPdfPage *page;
	GnomePrintPdf *pdf;
	gint number;

	pdf = GNOME_PRINT_PDF (pc);

	number = g_list_length (pdf->pages) + 1;
        /* Create the page */
	page = g_new (GnomePrintPdfPage, 1);
	page->name = g_strdup (name);
	page->number = number;
	page->shown  = FALSE;
	page->gs_set = FALSE;
	page->used_text             = FALSE;
	page->used_color_images     = FALSE;
	page->used_grayscale_images = FALSE;
	page->images = NULL;
	page->fonts  = NULL;
	page->object_number_page      = gnome_print_pdf_object_new (pdf);
	page->object_number_contents  = gnome_print_pdf_object_new (pdf);
	page->object_number_resources = gnome_print_pdf_object_new (pdf);

	pdf->pages = g_list_prepend (pdf->pages, page);

	pdf->stream_used = 0;
	pdf->stream[0] = '\0';
	pdf->selected_font    = NULL;
	pdf->color_set_fill   = FALSE;
	pdf->color_set_stroke = FALSE;

	/* We don't need to clip the page as we do in the PS2 driver because the PDF pages
	 * have a crop box. See section 6.4. (Chema)
	 */

	return GNOME_PRINT_OK;
}

static gint
gnome_print_pdf_page_write_contents (GnomePrintPdf *pdf, GnomePrintPdfPage *page)
{
	PdfCompressionType compr_type;
	/* FIXME: Not doing compresion yet */
#if 0
	const gchar *compr_string;
	gchar *compressed_stream = NULL;
	gint compressed_stream_length;
	gint stream_length;
#endif
	gint ret = 0;
	gint real_length = 0;
#if 0
	compr_string = gpa_settings_query_options (pdf->gpa_settings, "TextCompression");
	compr_type = gnome_print_pdf_compr_from_string (compr_string);
#endif
	compr_type = PDF_COMPRESSION_NONE;

	switch (compr_type) {
#if 0
	case PDF_COMPRESSION_FLATE:
		stream_length =  page->stream_used;
		compressed_stream_length = gnome_print_encode_deflate_wcs (stream_length);
		compressed_stream	= g_malloc (compressed_stream_length);
		real_length = gnome_print_encode_deflate (page->stream, compressed_stream,
		break;
#endif
	case PDF_COMPRESSION_NONE:
	default:
		real_length = pdf->stream_used;
	}

	ret += gnome_print_pdf_object_start (pdf, page->object_number_contents, FALSE);

#if 0
	if (compr_type != PDF_COMPRESSION_NONE) {
		pdf->offset += gnome_print_pdf_write_stream (pdf,
							     compressed_stream,
							     real_length,
							     compr_type);
		ret += gnome_print_pdf_write  (pc, EOL);
	}
#endif
	gnome_print_pdf_fprintf (pdf,"/Length %d" EOL, pdf->stream_used);
	gnome_print_pdf_fprintf (pdf,">>" EOL);
	gnome_print_pdf_fprintf (pdf,"stream" EOL);
	gnome_print_pdf_print_sized (pdf, pdf->stream, pdf->stream_used);
	gnome_print_pdf_fprintf (pdf, "endstream" EOL);
	gnome_print_pdf_fprintf (pdf, "endobj" EOL);
	ret += gnome_print_pdf_object_end (pdf, page->object_number_contents, TRUE);

#if 0
	if (compr_type != PDF_COMPRESSION_NONE)
		g_free (compressed_stream);
#endif

	return ret;
}

static gint
gnome_print_pdf_page_write_resources (GnomePrintPdf *pdf, GnomePrintPdfPage *page)
{
	GList *list;
	gint ret = 0;

	ret += gnome_print_pdf_object_start (pdf, page->object_number_resources, FALSE);

        /* Write the procset */
	/* FIXME: The PDF standard says that we should add all the procsets by default
	 * I read it as very little gain from specifying them and a risk of not adding
	 * a ProcSet when needed.
	 */
	gnome_print_pdf_fprintf  (pdf, "/ProcSet [/PDF ");
	if (page->used_text)
		gnome_print_pdf_fprintf (pdf, "/Text ");
	if (page->used_grayscale_images)
		gnome_print_pdf_fprintf (pdf, "/ImageB ");
	if (page->used_color_images)
		gnome_print_pdf_fprintf (pdf, "/ImageC ");
	gnome_print_pdf_fprintf (pdf, "]" EOL);

	/* Print Fonts stuff */
	if (page->fonts) {
		gnome_print_pdf_fprintf (pdf, "/Font <<" EOL);
		list = page->fonts;
		while (list) {
			GnomePrintPdfFont *font = list->data;
			gnome_print_pdf_fprintf (pdf, "/F%i %i 0 R" EOL,
						 font->object_number,
						 font->object_number);
			list = list->next;
		}
		gnome_print_pdf_fprintf (pdf, ">>" EOL);
	}

	/* Print Images stuff */
	if (page->images) {
		gnome_print_pdf_fprintf (pdf, "/XObject <<" EOL);
		list = page->images;
		while (list) {
			gint object_num;
			object_num = GPOINTER_TO_INT (list->data);
			gnome_print_pdf_fprintf (pdf,
						 "/Im%d %d 0 R" EOL,
						 object_num,
						 object_num);
			list = list->next;
		}
		gnome_print_pdf_fprintf (pdf, ">>" EOL);
	}

	/* Print the default graphic state */
	gnome_print_pdf_fprintf (pdf, "/ExtGState <<" EOL);
	gnome_print_pdf_fprintf (pdf, "/GS1 %d 0 R" EOL, pdf->object_number_gs);
	gnome_print_pdf_fprintf (pdf, ">>" EOL);
	ret += gnome_print_pdf_object_end   (pdf, page->object_number_resources, FALSE);

	return ret;
}

static gint
gnome_print_pdf_showpage (GnomePrintContext *pc)
{
	GnomePrintPdfPage *page;
	GnomePrintPdf *pdf;
	gint ret = 0;

	pdf = GNOME_PRINT_PDF (pc);

	g_return_val_if_fail (pdf != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (pdf->pages != NULL, GNOME_PRINT_ERROR_NOPAGE);
	g_return_val_if_fail (pdf->pages->data != NULL, GNOME_PRINT_ERROR_UNKNOWN);

	page = pdf->pages->data;

	g_return_val_if_fail (page->shown == FALSE, GNOME_PRINT_ERROR_UNKNOWN);

	gnome_print_pdf_page_write_contents  (pdf, page);
	gnome_print_pdf_page_write_resources (pdf, page);

	page->shown = TRUE;

	g_list_free (page->images);

	gnome_print_pdf_graphic_mode_set (pdf, PDF_GRAPHIC_MODE_GRAPHICS);

	pdf->stream_used = 0;
	pdf->selected_font    = NULL;
	pdf->color_set_fill   = FALSE;
	pdf->color_set_stroke = FALSE;


	g_return_val_if_fail (ret >= 0, ret);

	return GNOME_PRINT_OK;
}

/**
 * gnome_print_pdf_object_new:
 * @pdf:
 *
 * Dispatches object numbers
 *
 * Return Value:
 **/
gint
gnome_print_pdf_object_new (GnomePrintPdf *pdf)
{
	GnomePrintPdfObject *object;

	object = g_new (GnomePrintPdfObject, 1);
	object->offset = 0;
	object->number = g_list_length (pdf->objects) + 1;

	pdf->objects = g_list_prepend (pdf->objects, object);

	return object->number;
}

/**
 * gnome_print_pdf_object_start:
 * @pdf:
 * @object_number:
 *
 *
 *
 * Return Value:
 **/
gint
gnome_print_pdf_object_start (GnomePrintPdf *pdf, gint object_number, gboolean dont_print)
{
	GnomePrintPdfObject *object;
	gint ret = 0;

	g_return_val_if_fail (pdf->current_object == 0, -1);
	g_return_val_if_fail (object_number > 0, -1);

	object = g_list_nth_data (pdf->objects, g_list_length (pdf->objects) - object_number);
	object->offset = pdf->offset;
	if (!dont_print)
		gnome_print_pdf_fprintf (pdf,
					 "%d 0 obj" EOL
					 "<<" EOL,
					 object_number);
	pdf->current_object = object_number;

	return ret;
}

/**
 * gnome_print_pdf_object_end:
 * @pc:
 * @object_number:
 * @dont_print:
 *
 *
 *
 * Return Value:
 **/
gint
gnome_print_pdf_object_end (GnomePrintPdf *pdf, gint object_number, gboolean dont_print)
{
	gint ret = 0;

	g_return_val_if_fail (pdf->current_object == object_number, -1);
	pdf->current_object = 0;

	if (!dont_print)
		gnome_print_pdf_fprintf (pdf,
					 ">>" EOL
					 "endobj" EOL);
	return ret;
}


/**
 * gnome_print_pdf_write_pages:
 * @pdf:
 *
 *
 *
 * Return Value:
 **/
static gint
gnome_print_pdf_write_pages (GnomePrintPdf *pdf, gint object_number_catalog)
{
	ArtPoint p;
	GnomePrintPdfPage *nth_page;
	GList *list;
	gint col;
	gint ret = 0;
	gint object_number_pages;

	object_number_pages = gnome_print_pdf_object_new (pdf);
	/* Write each page "page object" */
	pdf->pages = g_list_reverse (pdf->pages);
	list = pdf->pages;
	while (list) {
		nth_page = (GnomePrintPdfPage *) list->data;
		ret += gnome_print_pdf_object_start (pdf, nth_page->object_number_page, FALSE);
		gnome_print_pdf_fprintf (pdf,
					 "/Type /Page" EOL
					 "/Parent %d 0 R" EOL
					 "/Resources %d 0 R" EOL
					 "/Contents %d 0 R" EOL,
					 object_number_pages,
					 nth_page->object_number_resources,
					 nth_page->object_number_contents);
		ret += gnome_print_pdf_object_end   (pdf, nth_page->object_number_page, FALSE);
		list = list->next;
	}

	/* Now write the "Pages" object */
	ret += gnome_print_pdf_object_start (pdf, object_number_pages, FALSE);
	gnome_print_pdf_fprintf (pdf,
				 "/Type /Pages" EOL
				 "/Kids [");
	col = 0;
	list = pdf->pages;
	while (list) {
		nth_page = (GnomePrintPdfPage *) list->data;
		gnome_print_pdf_fprintf (pdf, "%d 0 R ", nth_page->object_number_page);
		col++;
		/* We need to split this list*/
		if (col == 10) {
			gnome_print_pdf_fprintf (pdf, EOL);
			col = 0;
		}
		list = list->next;
	}

	gnome_print_config_get_length (GNOME_PRINT_CONTEXT (pdf)->config,
				       GNOME_PRINT_KEY_PAPER_WIDTH,  &p.x, NULL);
	gnome_print_config_get_length (GNOME_PRINT_CONTEXT (pdf)->config,
				       GNOME_PRINT_KEY_PAPER_HEIGHT, &p.y, NULL);
	gnome_print_pdf_fprintf (pdf,
					"]" EOL
					"/Count %d" EOL
					"/MediaBox [0 0 %d %d]" EOL,
					g_list_length (pdf->pages),
					(gint) p.x, (gint) p.y);

	ret += gnome_print_pdf_object_end (pdf, object_number_pages, FALSE);

	/* Now write the catalog */
	ret += gnome_print_pdf_object_start (pdf, object_number_catalog, FALSE);
	gnome_print_pdf_fprintf (pdf,
					"/Type /Catalog" EOL
					"/Pages %d 0 R" EOL,
					object_number_pages);
	ret += gnome_print_pdf_object_end (pdf, object_number_catalog, FALSE);

	return ret;
}

/**
 * gnome_print_pdf_write_fonts:
 * @pdf:
 *
 *
 *
 * Return Value:
 **/
static gint
gnome_print_pdf_write_fonts (GnomePrintPdf *pdf)
{
	GnomePrintPdfFont *font;
	GList *list;

	list = pdf->fonts;
	while (list) {
		font = list->data;
		gnome_print_pdf_font_print_widths (pdf, font);
		/* '/Encoding' not needed when the font is symbolic */
		if (!NEEDED_SUBSETTING(font))
			gnome_print_pdf_font_print_encoding (pdf, font);
		gnome_print_pdf_font_print_lastchar (pdf, font);
		list = list->next;
	}

	return GNOME_PRINT_OK;
}

/**
 * gnome_print_pdf_close_write_objects:
 * @pdf:
 *
 * Write all the objects that go at the end of the document
 *
 * Return Value:
 **/
static gint
gnome_print_pdf_close_write_last_objects (GnomePrintPdf *pdf)
{
	GList *list;
	gint object_num;
	gint object_num_info;
	gint object_num_catalog;
	gint xref_offset;
	gchar *date, *producer;
	gint ret = 0, objects;

	/* Write the default Halftone object */
	object_num = gnome_print_pdf_object_new (pdf);
	ret += gnome_print_pdf_object_start (pdf, object_num, FALSE);
	gnome_print_pdf_fprintf  (pdf,
				  "/Type /Halftone" EOL
				  "/HalftoneType 1" EOL
				  "/HalftoneName (Default)" EOL
				  "/Frequency 60" EOL
				  "/Angle 45" EOL
				  "/SpotFunction /Round" EOL);
	ret += gnome_print_pdf_object_end   (pdf, object_num, FALSE );

	/* Write default GS (Graphic State) object */
	ret += gnome_print_pdf_object_start (pdf, pdf->object_number_gs, FALSE);
	gnome_print_pdf_fprintf (pdf,
				 "/Type /ExtGState" EOL
				 "/SA false" EOL
				 "/OP false" EOL
				 "/HT /Default" EOL);
	ret += gnome_print_pdf_object_end   (pdf, pdf->object_number_gs, FALSE);

	/* Write fonts */
	gnome_print_pdf_write_fonts (pdf);

	/* Write pages */
	object_num_catalog = gnome_print_pdf_object_new (pdf);
	gnome_print_pdf_write_pages (pdf, object_num_catalog);

	/* Write info block */
	date     = gnome_print_pdf_get_date ();
	producer = g_strdup_printf ("libgnomeprint Ver: %s", VERSION);
	object_num_info = gnome_print_pdf_object_new (pdf);
	ret += gnome_print_pdf_object_start (pdf, object_num_info, FALSE);
	gnome_print_pdf_fprintf (pdf,
				 "/CreationDate (%s)" EOL
				 "/Producer (%s)" EOL,
				 date,
				 producer);
	ret += gnome_print_pdf_object_end   (pdf, object_num_info, FALSE);
	g_free (date);
	g_free (producer);

	/* Write xref */
	xref_offset = pdf->offset;
	objects = g_list_length (pdf->objects) + 1;
	gnome_print_pdf_fprintf (pdf,
				 "xref" EOL
				 "0 %d" EOL
				 "%010d %05d f",
				 objects,
				 0,
				 65535);

	/* Work around a bug in ghostscript */
	gnome_print_pdf_fprintf (pdf, (strlen (EOL) == 1) ? " " EOL : EOL);

	pdf->objects = g_list_reverse (pdf->objects);
	list = pdf->objects;
	for (; list != NULL; list = list->next) {
		GnomePrintPdfObject *nth_object;
		nth_object = (GnomePrintPdfObject *) list->data;
		if (nth_object->offset < 1)
			g_warning ("Object with offset Zero while creating pdf file");
		gnome_print_pdf_fprintf (pdf,
					 "%010i %05i n",
					 nth_object->offset,
					 0);

		/* Work arround a bug in ghostscript */
		gnome_print_pdf_fprintf (pdf, (strlen (EOL) == 1) ? " " EOL : EOL);
	}

	/* Write trailer */
	gnome_print_pdf_fprintf (pdf,
				 "trailer" EOL
				 "<<" EOL
				 "/Size %d" EOL
				 "/Root %d 0 R" EOL
				 "/Info %d 0 R" EOL
				 ">>" EOL
				 "startxref" EOL
				 "%d" EOL
				 "%c%cEOF" EOL,
				 objects,
				 object_num_catalog,
				 object_num_info,
				 xref_offset,
				 '%', '%');

	return ret;
}

static int 
subsetfontname_cmp (gconstpointer a, gconstpointer b)
{
	GnomeFontFace *face_a = ((GnomePrintPdfFont *) a)->face;
	GnomeFontFace *face_b = ((GnomePrintPdfFont *) b)->face;

	return strcmp((const char *)face_a->psname, (const char *)face_b->psname);
}

#define UCS2_UPPER_BOUND 0xFFFF
 
static void
gnome_print_embed_all_pdf_fonts (GnomePrintPdf *pdf)
{
	GList *list, *prev_list = NULL, *copy_list;
	gint *glyph2unicode;

	g_return_if_fail (pdf != NULL);
	
	glyph2unicode = (gint *) calloc (UCS2_UPPER_BOUND, sizeof(gint));

	copy_list = list = g_list_copy ((GList *)pdf->fonts);

	list = g_list_sort (list, subsetfontname_cmp);

	while (list) {
		gint font_change = 1;
		GnomePrintPdfFont *font;

		font = list->data;

		if (prev_list)
			font_change = subsetfontname_cmp (prev_list->data, list->data);
		if (font_change && font && font->face && font->face->ft_face) {
			int i;
			for(i = 0; i < UCS2_UPPER_BOUND; i++) {
				int glyph = FT_Get_Char_Index (font->face->ft_face, i);
				glyph2unicode[glyph] = i;
			}
		}
		gnome_print_embed_pdf_font (pdf, font, glyph2unicode);
		prev_list = list;
		list = list->next;
	}

	g_list_free (copy_list);

	free (glyph2unicode);
}

#undef UCS2_UPPER_BOUND

static gint
gnome_print_pdf_close (GnomePrintContext *pc)
{
	GnomePrintPdfPage *page;
	GnomePrintPdf *pdf;
	gint ret = 0;

	pdf = GNOME_PRINT_PDF (pc);

	g_return_val_if_fail (pc->transport != NULL, GNOME_PRINT_ERROR_UNKNOWN);

	gnome_print_embed_all_pdf_fonts (pdf);

	page = pdf->pages ? pdf->pages->data : NULL;
	if (!pdf->pages || !page || !page->shown) {
		g_warning ("file %s: line %d: Closing PDF context without final showpage", __FILE__, __LINE__);
		ret = gnome_print_showpage (pc);
		g_return_val_if_fail (ret >= 0, ret);
	}

	ret += gnome_print_pdf_close_write_last_objects (pdf);

 	gnome_print_transport_close (pc->transport);
	g_object_unref (G_OBJECT (pc->transport));
	pc->transport = NULL;

	return GNOME_PRINT_OK;
}

static gint
gnome_print_pdf_set_color (GnomePrintPdf *pdf, PdfColorGroup color_group)
{
	GnomePrintContext *ctx;
	gint ret = 0;

	ctx = GNOME_PRINT_CONTEXT (pdf);

	gnome_print_pdf_set_color_real (pdf,
					color_group,
					gp_gc_get_red   (ctx->gc),
					gp_gc_get_green (ctx->gc),
					gp_gc_get_blue  (ctx->gc));

	return ret;
}

static gint
gnome_print_pdf_set_line (GnomePrintPdf *pdf)
{
	GnomePrintContext *ctx;

	ctx = GNOME_PRINT_CONTEXT (pdf);

	if (gp_gc_get_line_flag (ctx->gc) == GP_GC_FLAG_CLEAR)
		return GNOME_PRINT_OK;
	gnome_print_pdf_page_print_double (pdf, "%.2f",
					   gp_gc_get_linewidth (ctx->gc));
	gnome_print_pdf_page_fprintf (pdf, " w %d J %d j ",
				      gp_gc_get_linecap (ctx->gc),
				      gp_gc_get_linejoin (ctx->gc));
	gnome_print_pdf_page_print_double (pdf, "%.2f",
					   gp_gc_get_miterlimit (ctx->gc));
	gnome_print_pdf_page_write (pdf, " M" EOL);
	gp_gc_set_line_flag (ctx->gc, GP_GC_FLAG_CLEAR);

	return GNOME_PRINT_OK;
}

static gint
gnome_print_pdf_set_dash (GnomePrintPdf *pdf)
{
	GnomePrintContext *ctx;
	const ArtVpathDash *dash;
	gint i;

	ctx = GNOME_PRINT_CONTEXT (pdf);

	if (gp_gc_get_dash_flag (ctx->gc) == GP_GC_FLAG_CLEAR)
		return GNOME_PRINT_OK;

	dash = gp_gc_get_dash (ctx->gc);
	gnome_print_pdf_page_write (pdf, "[");
	for (i = 0; i < dash->n_dash; i++) {
		gnome_print_pdf_page_write (pdf, " ");
		gnome_print_pdf_page_print_double (pdf, "%.2f", dash->dash[i]);
	}
	gnome_print_pdf_page_write (pdf, "]");
	gnome_print_pdf_page_print_double (pdf, "%.2f",
				 dash->n_dash > 0 ? dash->offset : 0.0);
	gnome_print_pdf_page_write (pdf, " d" EOL);
	gp_gc_set_dash_flag (ctx->gc, GP_GC_FLAG_CLEAR);

	return GNOME_PRINT_OK;
}

static gint
gnome_print_pdf_set_color_real (GnomePrintPdf *pdf,
				PdfColorGroup color_group,
				gdouble r, gdouble g, gdouble b)
{
	GnomePrintContext *ctx;
	gboolean fill, stroke;

	ctx = GNOME_PRINT_CONTEXT (pdf);

	fill   = (color_group == PDF_COLOR_GROUP_FILL   || color_group == PDF_COLOR_GROUP_BOTH);
	stroke = (color_group == PDF_COLOR_GROUP_STROKE || color_group == PDF_COLOR_GROUP_BOTH);

	if (fill) {
		if (pdf->color_set_fill && (r == pdf->r) && (g == pdf->g) && (b == pdf->b))
			goto gnome_print_pdf_set_color_stroke;
		gnome_print_pdf_page_print_double (pdf, "%.3f", r);
		gnome_print_pdf_page_write (pdf, " ");
		gnome_print_pdf_page_print_double (pdf, "%.3f", g);
		gnome_print_pdf_page_write (pdf, " ");
		gnome_print_pdf_page_print_double (pdf, "%.3f", b);
		gnome_print_pdf_page_write (pdf, " rg" EOL);
		pdf->r = r;
		pdf->g = g;
		pdf->b = b;
		pdf->color_set_fill = TRUE;
	}

gnome_print_pdf_set_color_stroke:
	if (stroke) {
		if (pdf->color_set_stroke && (r == pdf->r_) && (g == pdf->g_) && (b == pdf->b_))
			return GNOME_PRINT_OK;
		gnome_print_pdf_page_print_double (pdf, "%.3f", r);
		gnome_print_pdf_page_write (pdf, " ");
		gnome_print_pdf_page_print_double (pdf, "%.3f", g);
		gnome_print_pdf_page_write (pdf, " ");
		gnome_print_pdf_page_print_double (pdf, "%.3f", b);
		gnome_print_pdf_page_write (pdf, " RG" EOL);
		pdf->r_ = r;
		pdf->g_ = g;
		pdf->b_ = b;
		pdf->color_set_stroke = TRUE;
	}

	return GNOME_PRINT_OK;
}

static void
gnome_print_pdf_font_print_lastchar (GnomePrintPdf *pdf, GnomePrintPdfFont *font)
{
	gint num = font->object_number_lastchar;

	gnome_print_pdf_object_start (pdf, num, TRUE);
	gnome_print_pdf_fprintf (pdf,
				 "%d 0 obj" EOL
				 "%d" EOL
				 "endobj" EOL,
				 num, font->code_assigned);
	gnome_print_pdf_object_end (pdf, num, TRUE);

}

#define MAX_COL 80

static void
gnome_print_pdf_font_print_widths (GnomePrintPdf *pdf, GnomePrintPdfFont *font)
{
	GnomeFontFace *face = font->face;
	gint object_number_widths = font->object_number_widths;
	gint col, i;

	/* Print widths */
	gnome_print_pdf_object_start (pdf, object_number_widths, TRUE);
	gnome_print_pdf_fprintf (pdf, "%d 0 obj" EOL, object_number_widths);
	gnome_print_pdf_fprintf (pdf, "[ 0 ");
	col = 0;
	i = 1;
	while (font->code_to_glyph [i] >= 0) {
		ArtPoint point;
		gnome_font_face_get_glyph_stdadvance (face, font->code_to_glyph [i], &point);
		if (col > MAX_COL) {
			gnome_print_pdf_fprintf (pdf, EOL);
			col = 0;
		}
		col += gnome_print_pdf_print_double (pdf, "%.1f", point.x);
		gnome_print_pdf_print_sized (pdf, " ", 1);
		i++;
	}

	gnome_print_pdf_fprintf (pdf, "]" EOL "endobj" EOL);
	gnome_print_pdf_object_end   (pdf, object_number_widths, TRUE);
}

static void
gnome_print_pdf_font_print_encoding (GnomePrintPdf *pdf, GnomePrintPdfFont *font)
{
	GnomeFontFace *face = font->face;
	gint object_number_encoding = font->object_number_encoding;
	gint col, i;

	/* Print encoding */
	gnome_print_pdf_object_start (pdf, object_number_encoding, FALSE);
	gnome_print_pdf_fprintf (pdf,"/Type /Encoding" EOL);
	/* Add a dummy encoding so that ghotscript can parse it fine
	 * this is really not a problem because we completely define the
	 * encoding with the /Differences. (Chema)
	 */
	gnome_print_pdf_fprintf (pdf,"/BaseEncoding /MacRomanEncoding" EOL);
	gnome_print_pdf_fprintf (pdf,"/Differences [1" EOL);
	col = 0;
	i = 1;
	while (font->code_to_glyph [i] >= 0) {
		const gchar *name;
		name = gnome_font_face_get_glyph_ps_name (face, font->code_to_glyph [i]);
		if (col > MAX_COL) {
			gnome_print_pdf_fprintf (pdf, EOL);
			col = 0;
		}
		col += gnome_print_pdf_fprintf (pdf, "/%s ", name);
		i++;
	}
	gnome_print_pdf_fprintf (pdf, "]" EOL);
	gnome_print_pdf_object_end   (pdf, object_number_encoding, FALSE);
}

static gint32
gnome_font_face_get_pdf_flags (GnomeFontFace *face)
{
	/* See the PDF 1.3 standard, section 7.11.2, page 225 */
	gboolean fixed_width;
	gboolean serif;
	gboolean symbolic;
	gboolean script;
	gboolean italic;
	gboolean all_cap;
	gboolean small_cap;
	gboolean force_bold;
	gboolean adobe_roman_set;
	gint32 flags;

	fixed_width = gnome_font_face_is_fixed_width (face);
	serif = TRUE;
	symbolic = FALSE; /* FIXME */
	script = FALSE;  /* FIXME */
	italic = gnome_font_face_is_italic (face);
	all_cap = FALSE;
	small_cap = FALSE;
	force_bold = FALSE;
	adobe_roman_set = TRUE;

	flags = 0;
	flags =
		fixed_width     |
		(serif      << 1) |
		(symbolic   << 2) |
		(script     << 3) |
		/* 5th bit is reserved */
		(adobe_roman_set << 5) |
		(italic     << 6) |
		/* 8th - 16 are reserved */
		(all_cap    << 16) |
		(small_cap  << 17) |
		(force_bold << 18);
	/* 20-32 reserved */

	return flags;
}

/**
 * my_fix_drect:
 * @rect:
 *
 * Freetype is returning some invalid numbers for the font's BBox
 * clean the numbers, we need this work around because even if
 * freetype fixes it, there are still versions deployed that have
 * it.
 **/
static void
my_fix_drect (ArtDRect *rect)
{
#define TOO_BIG(x)    (x>100000)
#define TOO_SMALL(x) (x<-100000)
	if (TOO_BIG (rect->x0) ||
	    TOO_SMALL (rect->x0))
		rect->x0 = 0.0;
	if (TOO_BIG (rect->x1) ||
	    TOO_SMALL (rect->x1))
		rect->x1 = 0.0;
	if (TOO_BIG (rect->y0) ||
	    TOO_SMALL (rect->y0))
		rect->y0 = 0.0;
	if (TOO_BIG (rect->y1) ||
	    TOO_SMALL (rect->y1))
		rect->y1 = 0.0;
#undef TOO_BIG
#undef TOO_SMALL
}

static GnomePrintReturnCode
gnome_print_pdf_font_print_descriptor (GnomePrintPdf *pdf, GnomePrintPdfFont *font,
				       gint *object_number_descriptor_ret)
{
	ArtDRect *rect, new_rect;
	GnomeFontFace *face;
	gint ascent, descent, flags, stemv, stemh;
	gint italic_angle, capheight, xheight;
	gdouble val;
	gint fontfile_object_number;
	gint object_number_descriptor;
	gint embed_result;
	const gchar *file_name;

	face = font->face;

	file_name = font->face->entry->file;
	if (font->is_type_1) {
		embed_result = gnome_print_pdf_t1_embed (pdf, file_name, &fontfile_object_number);
	} else {
		/* We always subset tt fonts */
		embed_result = gnome_print_pdf_tt_subset_embed (pdf, font, file_name, &fontfile_object_number);
	}

	if (embed_result != GNOME_PRINT_OK) {
		g_print ("Could not embed font %s\n", gnome_font_face_get_ps_name (font->face));
		return GNOME_PRINT_ERROR_UNKNOWN;
	}

	ascent  = (gint) gnome_font_face_get_ascender (face);
	descent = (gint) gnome_font_face_get_descender (face);
	if (NEEDED_SUBSETTING(font)) {
		/* Symbol encoding for subsetted ttf fonts */
		flags = 4;
	} else {
		flags = gnome_font_face_get_pdf_flags (face);
	}
#ifdef GET_STEMS
	gnome_print_pdf_type1_get_stems (face, &stemv, &stemh);
#else
	stemv = 0; stemh = 0;
#endif
	g_object_get (G_OBJECT (face), "ItalicAngle", &val, NULL);
	italic_angle = (gint) val;
	g_object_get (G_OBJECT (face), "CapHeight", &val, NULL);
	capheight = (gint) val;
	g_object_get (G_OBJECT (face), "XHeight", &val, NULL);
	xheight = (gint) val;
	g_object_get (G_OBJECT (face), "FontBBox", &rect, NULL);
	art_drect_copy (&new_rect, rect);
	my_fix_drect (&new_rect);
	g_free (rect);

	object_number_descriptor = gnome_print_pdf_object_new (pdf);
	*object_number_descriptor_ret = object_number_descriptor;
	/* Print Descriptor */
	gnome_print_pdf_object_start (pdf, object_number_descriptor, FALSE);
	gnome_print_pdf_fprintf (pdf,
				 "/Type /FontDescriptor" EOL
				 "/Ascent %d" EOL
				 "/CapHeight %d" EOL
				 "/Descent %d" EOL
				 "/Flags %d" EOL
				 "/FontBBox [",
				 ascent, capheight,
				 descent * -1, flags);
	gnome_print_pdf_print_double (pdf, "%.2f", new_rect.x0);
	gnome_print_pdf_fprintf (pdf, " ");
	gnome_print_pdf_print_double (pdf, "%.2f", new_rect.y0);
	gnome_print_pdf_fprintf (pdf, " ");
	gnome_print_pdf_print_double (pdf, "%.2f", new_rect.x1);
	gnome_print_pdf_fprintf (pdf, " ");
	gnome_print_pdf_print_double (pdf, "%.2f", new_rect.y1);
	gnome_print_pdf_fprintf (pdf,
				 "]" EOL
				 "/FontName /%s" EOL
				 "/ItalicAngle %d" EOL
				 "/StemV %d" EOL
				 "/XHeight %d" EOL,
				 font->pso->encodedname,
				 italic_angle, stemv, xheight);


	if (embed_result == GNOME_PRINT_OK)
		gnome_print_pdf_fprintf (pdf,
					 "/%s %d 0 R" EOL,
					 font->is_type_1 ? "FontFile" : "FontFile2",
					 fontfile_object_number	);

	gnome_print_pdf_object_end   (pdf, object_number_descriptor, FALSE);

	return GNOME_PRINT_OK;
}

/*
 * See
 * http://partners.adobe.com/public/developer/en/pdf/PDFReference15_v5.pdf
 * Section Section 5.5.3 Font Subsets.
 * BaseFont name for subfont needs to be 'XXXXXX+Original Font's BaseFont name'
 */

static guchar *
gnome_print_pdf_get_subfont_name (guchar *basefont, guint unique_subfont_id)
{
	int i;
	guchar *full_basefont;

	if (!basefont)
		return NULL;
	/* 6 letters, a '+' and ending NUL */
	full_basefont = (guchar *)g_malloc (strlen(basefont) + 6 + 1 + 1);

	for (i = 0; i < 6; i++) {
		int offset = unique_subfont_id%26;
		unique_subfont_id /= 26;
		full_basefont[i] = 'A'+offset;
	}
	full_basefont[6] = '+';
	strcpy(full_basefont + 7, basefont);
	return (full_basefont);
}

void
gnome_print_embed_pdf_font (GnomePrintPdf *pdf, GnomePrintPdfFont *font, gint *glyph2unicode)
{
	gint object_number_descriptor = 0;
	gint object_number_tounicode = 0;
	guchar *basefont_name;
	gboolean plan_b = FALSE; /* TRUE if we could not embed the font */

	if (!font->is_basic_14) {
		GnomePrintReturnCode retval;
		retval =  gnome_print_pdf_font_print_descriptor (pdf, font,
								 &object_number_descriptor);
		if (retval != GNOME_PRINT_OK) {
			g_warning ("Could not embed font %s, using Times-Roman instead.",
				   gnome_font_face_get_ps_name (font->face));
			plan_b = TRUE;
			font->is_type_1 = TRUE; /* It isn't, but the substitute is */
		} 

		if (NEEDED_SUBSETTING(font))
			gnome_print_pdf_font_tounicode (pdf, font, glyph2unicode, &object_number_tounicode);
		
	}

	basefont_name = gnome_print_pdf_get_subfont_name ((guchar *)gnome_font_face_get_ps_name (font->face), font->object_number);
	/* Write the object data */
	gnome_print_pdf_object_start (pdf, font->object_number, FALSE);
	gnome_print_pdf_fprintf  (pdf,
				  "/Type /Font" EOL
				  "/Subtype /%s" EOL
				  "/BaseFont /%s" EOL
				  "/Name /F%i" EOL,
				  font->is_type_1 ? "Type1" : "TrueType",
				  plan_b ? (const guchar *) "Times-Roman" : basefont_name,
				  font->object_number);
	g_free (basefont_name);

	if (!font->is_basic_14) {
		gnome_print_pdf_fprintf  (pdf,
					  "/FirstChar %d" EOL
					  "/LastChar %d 0 R" EOL
					  "/Widths %d 0 R" EOL,
					  0,
					  font->object_number_lastchar,
					  font->object_number_widths);
		if (!NEEDED_SUBSETTING(font))
			gnome_print_pdf_fprintf  (pdf,
					  "/Encoding %i 0 R" EOL,
					  font->object_number_encoding);
	}
	if (!plan_b) {
		gnome_print_pdf_fprintf (pdf,
					 "/FontDescriptor %d 0 R" EOL,
					  object_number_descriptor);
	}

	if (NEEDED_SUBSETTING(font)) {
		gnome_print_pdf_fprintf (pdf, 
					"/ToUnicode %d 0 R" EOL, 
					object_number_tounicode);
	}

	gnome_print_pdf_object_end   (pdf, font->object_number, FALSE);
}

static gint
gnome_print_pdf_set_font_real (GnomePrintPdf *pdf, const GnomeFont *gnome_font, gboolean subfont_flag, gint subfont_number)
{
	GnomePrintPdfPage *page;
	GnomePrintPdfFont *font = NULL;
	GnomeFontFace *face;
	GList *needle;
	guchar *encodedname = NULL;
	gint i;

	face = gnome_font->face;

	if (subfont_flag) {
		if (subfont_number == 0)
			encodedname = g_strdup_printf ("GnomeUni-%s", face->psname);
		else
			encodedname = g_strdup_printf ("GnomeUni-%s_%03d", face->psname, subfont_number);
	}

	/* Check if the font is aready in Memory  */
	needle = pdf->fonts;
	while (needle) {
		font = needle->data;
		if (subfont_flag) {
			if  (!strcmp(font->pso->encodedname, encodedname))
				break;
		} else if (font->face == face)
			break;

		needle = needle->next;
	}
	/* If we already created this font, just set the font */
	g_free (encodedname);
	if (needle != NULL)
		goto gnome_print_pdf_set_font_real_page_stuff;

	/* Create a new GnomePrintPdfFont */
	font = g_new (GnomePrintPdfFont, 1);
	font->face = gnome_font_get_face (gnome_font);
	g_object_ref (font->face);
	font->pso  = gnome_font_face_pso_new (font->face, NULL, subfont_number);
	font->is_basic_14 = FALSE;
	font->nglyphs = gnome_font_face_get_num_glyphs (face);
	font->glyph_to_code = g_hash_table_new (NULL, NULL);

	/* All 256 charaters can have glyphs
	 * ie, 0-255, we need 256th postion to be -1
	 */

	font->code_to_glyph = g_new (gint, 257);
	for (i = 0; i < 257; i++) {
		font->code_to_glyph [i] = -1;
	}
	font->code_assigned = 0;

	/* Get object numbers for the font */
	if (font->is_basic_14) {
		font->object_number_encoding = -1;
		font->object_number_widths   = -1;
		font->object_number_lastchar = -1;
	} else {
		if (!subfont_flag)
                	font->object_number_encoding = gnome_print_pdf_object_new (pdf);
		font->object_number_widths   = gnome_print_pdf_object_new (pdf);
		font->object_number_lastchar = gnome_print_pdf_object_new (pdf);
	}
	font->object_number = gnome_print_pdf_object_new (pdf);

	/* Make sure this backend can handle this font type */
	if (face->entry->type == GP_FONT_ENTRY_TYPE1)
		font->is_type_1 = TRUE;
	else if (face->entry->type == GP_FONT_ENTRY_TRUETYPE)
		font->is_type_1 = FALSE;
	else {
		g_warning ("We only support True Type and Type 1 fonts for now");
		return GNOME_PRINT_ERROR_UNKNOWN;
	}


	/* Add it to the list of fonts of this job */
	pdf->fonts = g_list_prepend (pdf->fonts, font);
gnome_print_pdf_set_font_real_page_stuff:
	/* Is the font alraedy set? If it is just return */
	if (pdf->selected_font == font)
		return GNOME_PRINT_OK;
	pdf->selected_font = font;

	/* Verify that it is listed as a resource of this page, if not
	 * add it as such.
	 */
	page = pdf->pages->data;
	if (!g_list_find (page->fonts, font))
		page->fonts = g_list_prepend (page->fonts, font);

	gnome_print_pdf_page_fprintf (pdf, "/F%d 1 Tf" EOL,
				      font->object_number);


	return GNOME_PRINT_OK;
}

GnomePrintContext *
gnome_print_pdf_new (GnomePrintConfig *config)
{
	GnomePrintContext *ctx;
	gint ret;

	g_return_val_if_fail (config != NULL, NULL);

	ctx = g_object_new (GNOME_TYPE_PRINT_PDF, NULL);

	ret = gnome_print_context_construct (ctx, config);

	if (ret != GNOME_PRINT_OK) {
		g_object_unref (G_OBJECT (ctx));
		ctx = NULL;
	}

	return ctx;
}

static gint
gnome_print_pdf_print_bpath (GnomePrintPdf *pdf, const ArtBpath *bpath)
{
	gboolean started, closed;

	started = FALSE;
	closed = FALSE;
	while (bpath->code != ART_END) {
		switch (bpath->code) {
		case ART_MOVETO_OPEN:
			if (started && closed)
				gnome_print_pdf_page_write (pdf, "h" EOL);
			closed = FALSE;
			started = FALSE;
			gnome_print_pdf_page_print_double (pdf, "%.2f", bpath->x3);
			gnome_print_pdf_page_write (pdf, " ");
			gnome_print_pdf_page_print_double (pdf, "%.2f", bpath->y3);
			gnome_print_pdf_page_write (pdf, " m" EOL);
			break;
		case ART_MOVETO:
			if (started && closed)
				gnome_print_pdf_page_write (pdf, "h" EOL);
			closed = TRUE;
			started = TRUE;
			gnome_print_pdf_page_print_double (pdf, "%.2f", bpath->x3);
			gnome_print_pdf_page_write (pdf, " ");
			gnome_print_pdf_page_print_double (pdf, "%.2f", bpath->y3);
			gnome_print_pdf_page_write (pdf, " m" EOL);
			break;
		case ART_LINETO:
			gnome_print_pdf_page_print_double (pdf, "%.2f", bpath->x3);
			gnome_print_pdf_page_write (pdf, " ");
			gnome_print_pdf_page_print_double (pdf, "%.2f", bpath->y3);
			gnome_print_pdf_page_write (pdf, " l" EOL);
			break;
		case ART_CURVETO:
			gnome_print_pdf_page_print_double (pdf, "%.2f", bpath->x1);
			gnome_print_pdf_page_write (pdf, " ");
			gnome_print_pdf_page_print_double (pdf, "%.2f", bpath->y1);
			gnome_print_pdf_page_write (pdf, " ");
			gnome_print_pdf_page_print_double (pdf, "%.2f", bpath->x2);
			gnome_print_pdf_page_write (pdf, " ");
			gnome_print_pdf_page_print_double (pdf, "%.2f", bpath->y2);
			gnome_print_pdf_page_write (pdf, " ");
			gnome_print_pdf_page_print_double (pdf, "%.2f", bpath->x3);
			gnome_print_pdf_page_write (pdf, " ");
			gnome_print_pdf_page_print_double (pdf, "%.2f", bpath->y3);
			gnome_print_pdf_page_write (pdf, " c" EOL);
			break;
		default:
			g_warning ("Path structure is corrupted");
			return -1;
		}
		bpath += 1;
	}

	if (started && closed)
		gnome_print_pdf_page_write (pdf, "h" EOL);

	return 0;
}

/* Other stuff */
static gchar*
gnome_print_pdf_get_date (void)
{
	time_t clock;
	struct tm *now;
	gchar *date;

#ifdef ADD_TIMEZONE_STAMP
  extern char * tzname[];
	/* TODO : Add :
		 "[+-]"
		 "HH'" Offset from gmt in hours
		 "OO'" Offset from gmt in minutes
	   we need to use tz_time. but I don't
	   know how protable this is. Chema */
	gprint ("Timezone %s\n", tzname[0]);
	gprint ("Timezone *%s*%s*%li*\n", tzname[1], timezone);
#endif

	clock = time (NULL);
	now = localtime (&clock);

	date = g_strdup_printf ("D:%04d%02d%02d%02d%02d%02d",
				now->tm_year + 1900,
				now->tm_mon + 1,
				now->tm_mday,
				now->tm_hour,
				now->tm_min,
				now->tm_sec);

	return date;
}


gint
gnome_print_pdf_fprintf (GnomePrintPdf *pdf, const char *format, ...)
{
 	va_list arguments;
 	gchar *text;
	gint len;

 	va_start (arguments, format);
 	text = g_strdup_vprintf (format, arguments);
 	va_end (arguments);

	len = strlen (text);
	gnome_print_transport_write (GNOME_PRINT_CONTEXT (pdf)->transport, text, len);
	pdf->offset += len;
	g_free (text);

	return len;
}

/* Allowed conversion specifiers are 'f', 'F'
 *
 * NOTE: From pdf ref 1.5 v 6 : section 3.2.2
 * 	PDF does not support the PostScript syntax for numbers with nondecimal
 * 	radices (such as 16#FFFE) or in exponential format (such as 6.02E23).
 * 	so %g and %e will not work
 */
gint
gnome_print_pdf_print_double (GnomePrintPdf *pdf, const gchar *format, gdouble x)
{
 	gchar text [G_ASCII_DTOSTR_BUF_SIZE];
	gint len;

	g_ascii_formatd (text, G_ASCII_DTOSTR_BUF_SIZE, format, x);
	len = strlen (text);
	gnome_print_transport_write (GNOME_PRINT_CONTEXT (pdf)->transport, text, len);
	pdf->offset += len;
	return len;
}


gint
gnome_print_pdf_print_sized (GnomePrintPdf *pdf, const char *content, gint len)
{
	gnome_print_transport_write (GNOME_PRINT_CONTEXT (pdf)->transport, content, len);
	pdf->offset += len;
	return len;
}

static gint
gnome_print_pdf_graphic_mode_set (GnomePrintPdf *pdf, PdfGraphicMode mode)
{
	GnomePrintPdfPage *page = pdf->pages->data;

	if (!page->gs_set) {
		gnome_print_pdf_page_write (pdf, "/GS1 gs" EOL);
		page->gs_set = TRUE;
	}

	if (pdf->mode == mode)
		return GNOME_PRINT_OK;

	if (mode == PDF_GRAPHIC_MODE_GRAPHICS)
		gnome_print_pdf_page_write (pdf, "ET" EOL);
	else
		gnome_print_pdf_page_write (pdf, "BT" EOL);

	pdf->mode = mode;

	return GNOME_PRINT_OK;
}
