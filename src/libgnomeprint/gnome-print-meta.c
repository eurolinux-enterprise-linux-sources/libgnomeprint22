/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  gnome-print-meta.c: Metafile implementation for gnome-print
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
 *    Miguel de Icaza <miguel@gnu.org>
 *    Michael Zucchi <notzed@helixcode.com>
 *    Morten Welinder <terra@diku.dk>
 *    Lauris Kaplinski <lauris@ximian.com>
 *
 *  Copyright (C) 1999-2003 Ximian Inc. and authors
 */

#include <config.h>
#include <math.h>
#include <string.h>

#include <libgnomeprint/gnome-glyphlist-private.h>
#include <libgnomeprint/gnome-print-private.h>
#include <libgnomeprint/gnome-print-meta.h>
#include <libgnomeprint/gp-gc-private.h>

struct _GnomePrintMeta {
	GnomePrintContext pc;

	guchar *buf;
	gint b_length;
	gint b_size;

	gint page;    /* Start of current page */
	gint pagenum; /* Number of current page, or -1 */
};

typedef struct _GnomePrintMetaClass GnomePrintMetaClass;
struct _GnomePrintMetaClass {
	GnomePrintContextClass parent_class;
};

static GnomePrintContextClass *parent_class = NULL;

/* Note: this must continue to be the same length  as "GNOME_METAFILE-0.0" */

#define METAFILE_SIGNATURE "GNOME_METAFILE-3.0"
#define METAFILE_SIGNATURE_SIZE 18
#define METAFILE_HEADER_SIZE (METAFILE_SIGNATURE_SIZE + 4)
#define PAGE_SIGNATURE "PAGE"
#define PAGE_SIGNATURE_SIZE 4
#define PAGE_HEADER_SIZE (PAGE_SIGNATURE_SIZE + 4)
#define BLOCKSIZE 4096

typedef enum {
	GNOME_META_BEGINPAGE,
	GNOME_META_SHOWPAGE,
	GNOME_META_GSAVE,
	GNOME_META_GRESTORE,
	GNOME_META_CLIP,
	GNOME_META_FILL,
	GNOME_META_STROKE,
	GNOME_META_IMAGE,
	GNOME_META_GLYPHLIST,
	GNOME_META_COLOR,
	GNOME_META_LINE,
	GNOME_META_DASH
} GnomeMetaType;

typedef enum {
	GNOME_META_DOUBLE_INT,        /* An integer.  */
	GNOME_META_DOUBLE_INT1000,    /* An integer to be divided by 1000.  */
	GNOME_META_DOUBLE_I386        /* IEEE-xxxx little endian.  */
} GnomeMetaDoubleType;

#define GPM_ENSURE_SPACE(m,s) (((m)->b_length + (s) <= (m)->b_size) || gpm_ensure_space (m, s))

static void gnome_print_meta_class_init (GnomePrintMetaClass *klass);
static void gnome_print_meta_init (GnomePrintMeta *meta);

static void meta_finalize (GObject *object);

static int meta_beginpage (GnomePrintContext *pc, const guchar *name);
static int meta_showpage (GnomePrintContext *pc);
static int meta_gsave (GnomePrintContext *pc);
static int meta_grestore (GnomePrintContext *pc);
static int meta_clip (GnomePrintContext *pc, const ArtBpath *bpath, ArtWindRule rule);
static int meta_fill (GnomePrintContext *pc, const ArtBpath *bpath, ArtWindRule rule);
static int meta_stroke (GnomePrintContext *pc, const ArtBpath *bpath);
static int meta_image (GnomePrintContext *pc, const gdouble *affine, const guchar *px, gint w, gint h, gint rowstride, gint ch);
static int meta_glyphlist (GnomePrintContext *pc, const gdouble *affine, GnomeGlyphList *gl);

static void meta_color (GnomePrintContext *pc);
static void meta_line (GnomePrintContext *pc);
static void meta_dash (GnomePrintContext *pc);

static void gpm_encode_string (GnomePrintContext *pc, const gchar *str);
static void gpm_encode_int (GnomePrintContext *pc, gint32 value);
static void gpm_encode_double (GnomePrintContext *pc, double d);
static void gpm_encode_block (GnomePrintContext *pc, const gchar *data, gint size);

static gboolean gpm_ensure_space (GnomePrintMeta *meta, int size);
static void gpm_encode_bpath (GnomePrintContext *pc, const ArtBpath *bpath);
static const guchar *gpm_decode_bpath (const guchar *data, ArtBpath **bpath);

static const guchar *gpm_decode_string (const guchar *data, guchar **dest);

/* Encoding and decoding of header data */
static void gpm_encode_int_header (GnomePrintContext *pc, gint32 value);
static const guchar *gpm_decode_int_header (const guchar *data, gint32 *dest);

GType
gnome_print_meta_get_type (void)
{
	static GType meta_type = 0;
	if (!meta_type) {
		static const GTypeInfo meta_info = {
			sizeof (GnomePrintMetaClass),
			NULL, NULL,
			(GClassInitFunc) gnome_print_meta_class_init,
			NULL, NULL,
			sizeof (GnomePrintMeta),
			0,
			(GInstanceInitFunc) gnome_print_meta_init
		};
		meta_type = g_type_register_static (GNOME_TYPE_PRINT_CONTEXT, "GnomePrintMeta", &meta_info, 0);
	}

	return meta_type;
}

static void
gnome_print_meta_class_init (GnomePrintMetaClass *klass)
{
	GObjectClass *object_class;
	GnomePrintContextClass *pc_class;

	object_class = (GObjectClass *) klass;
	pc_class = (GnomePrintContextClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = meta_finalize;

	pc_class->beginpage = meta_beginpage;
	pc_class->showpage = meta_showpage;

	pc_class->gsave = meta_gsave;
	pc_class->grestore = meta_grestore;

	pc_class->clip = meta_clip;
	pc_class->fill = meta_fill;
	pc_class->stroke = meta_stroke;
	pc_class->image = meta_image;
	pc_class->glyphlist = meta_glyphlist;
}

static void
gnome_print_meta_init (GnomePrintMeta *meta)
{
	gnome_print_meta_reset (meta);
}

static void
meta_finalize (GObject *object)
{
	GnomePrintMeta *meta;

	meta = GNOME_PRINT_META (object);

	g_free (meta->buf);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

gint
gnome_print_meta_get_length (const GnomePrintMeta *meta)
{
	g_return_val_if_fail (GNOME_IS_PRINT_META (meta), 0);

	return meta->b_length;
}

static void
gnome_print_meta_set_length (GnomePrintMeta *meta, guint len)
{
	gint32 l;

	g_return_if_fail (GNOME_IS_PRINT_META (meta));

	meta->b_length = len;
	l = g_htonl (meta->b_length);
	memcpy (meta->buf + METAFILE_SIGNATURE_SIZE, &l, 4);
}

static gint
meta_beginpage (GnomePrintContext *ctx, const guchar *name)
{
	GnomePrintMeta *meta;

	meta = GNOME_PRINT_META (ctx);

	/* Save page header position */
	meta->page = gnome_print_meta_get_length (meta);

	/* Encode page header */
	gpm_encode_block (ctx, PAGE_SIGNATURE, PAGE_SIGNATURE_SIZE);
	gpm_encode_int_header (ctx, 0);

	/* Increase page count */
	meta->pagenum += 1;

	/* Encode beginpage */
	gpm_encode_int (ctx, GNOME_META_BEGINPAGE);
	gpm_encode_string (ctx, name ? (const gchar *) name : "");

	return GNOME_PRINT_OK;
}

static gint
meta_showpage (GnomePrintContext *ctx)
{
	GnomePrintMeta *meta;
	gint32 len;

	meta = GNOME_PRINT_META (ctx);

	/* Encode showpage */
	gpm_encode_int (ctx, GNOME_META_SHOWPAGE);

	/* Save page length */
	len = g_htonl (meta->b_length - meta->page - PAGE_HEADER_SIZE);
	memcpy (meta->buf + meta->page + PAGE_SIGNATURE_SIZE, &len, 4);

	/* Clear graphic state */
	gp_gc_set_ctm_flag (ctx->gc, GP_GC_FLAG_UNSET);
	gp_gc_set_color_flag (ctx->gc, GP_GC_FLAG_UNSET);
	gp_gc_set_line_flag (ctx->gc, GP_GC_FLAG_UNSET);
	gp_gc_set_dash_flag (ctx->gc, GP_GC_FLAG_UNSET);

	return GNOME_PRINT_OK;
}

static int
meta_gsave (GnomePrintContext *pc)
{
	gpm_encode_int (pc, GNOME_META_GSAVE);

	return GNOME_PRINT_OK;
}

static int
meta_grestore (GnomePrintContext *ctx)
{
	gpm_encode_int (ctx, GNOME_META_GRESTORE);

	/* Clear graphic state */
	gp_gc_set_ctm_flag (ctx->gc, GP_GC_FLAG_UNSET);
	gp_gc_set_color_flag (ctx->gc, GP_GC_FLAG_UNSET);
	gp_gc_set_line_flag (ctx->gc, GP_GC_FLAG_UNSET);
	gp_gc_set_dash_flag (ctx->gc, GP_GC_FLAG_UNSET);

	return GNOME_PRINT_OK;
}

static int
meta_clip (GnomePrintContext *pc, const ArtBpath *bpath, ArtWindRule rule)
{
	gpm_encode_int (pc, GNOME_META_CLIP);
	gpm_encode_bpath (pc, bpath);
	gpm_encode_int (pc, rule);

	return GNOME_PRINT_OK;
}

static int
meta_fill (GnomePrintContext *pc, const ArtBpath *bpath, ArtWindRule rule)
{
	meta_color (pc);

	gpm_encode_int (pc, GNOME_META_FILL);
	gpm_encode_bpath (pc, bpath);
	gpm_encode_int (pc, rule);

	return GNOME_PRINT_OK;
}

static int
meta_stroke (GnomePrintContext *pc, const ArtBpath *bpath)
{
	meta_color (pc);
	meta_line (pc);
	meta_dash (pc);

	gpm_encode_int (pc, GNOME_META_STROKE);
	gpm_encode_bpath (pc, bpath);

	return GNOME_PRINT_OK;
}

static int
meta_image (GnomePrintContext *pc, const gdouble *affine, const guchar *px, gint w, gint h, gint rowstride, gint ch)
{
	int i, y;
	GnomePrintMeta *meta;

	gpm_encode_int (pc, GNOME_META_IMAGE);
	for (i = 0; i < 6; i++) gpm_encode_double (pc, affine[i]);
	gpm_encode_int (pc, h);
	gpm_encode_int (pc, w);
	gpm_encode_int (pc, ch);

	meta = (GnomePrintMeta *)pc;
	if (!GPM_ENSURE_SPACE (meta, w * ch * h)) {
		g_warning ("file %s: line %d: Cannot grow metafile buffer (%d bytes)", __FILE__, __LINE__, w * ch * h);
		return GNOME_PRINT_ERROR_UNKNOWN;
	}

	for (y = 0; y < h; y++){
		gpm_encode_block (pc, (const gchar *) px, w * ch);
		px += rowstride;
	}

	return GNOME_PRINT_OK;
}

static int
meta_glyphlist (GnomePrintContext *pc, const gdouble *affine, GnomeGlyphList *gl)
{
	gint i;

	gpm_encode_int (pc, GNOME_META_GLYPHLIST);
	for (i = 0; i < 6; i++) {
		gpm_encode_double (pc, affine[i]);
	}
	gpm_encode_int (pc, gl->g_length);
	for (i = 0; i < gl->g_length; i++) {
		gpm_encode_int (pc, gl->glyphs[i]);
	}
	gpm_encode_int (pc, gl->r_length);
	for (i = 0; i < gl->r_length; i++) {
		gpm_encode_int (pc, gl->rules[i].code);
		switch (gl->rules[i].code) {
		case GGL_POSITION:
		case GGL_ADVANCE:
		case GGL_COLOR:
			gpm_encode_int (pc, gl->rules[i].value.ival);
			break;
		case GGL_MOVETOX:
		case GGL_MOVETOY:
		case GGL_RMOVETOX:
		case GGL_RMOVETOY:
		case GGL_LETTERSPACE:
		case GGL_KERNING:
			gpm_encode_double (pc, gl->rules[i].value.dval);
			break;
		case GGL_FONT:
			gpm_encode_double (pc, gnome_font_get_size (gl->rules[i].value.font));
			gpm_encode_string (pc, (const gchar *) gnome_font_get_name (gl->rules[i].value.font));
			break;
		default:
			break;
		}
	}

	return GNOME_PRINT_OK;
}

/* Private GPGC methods */

static void
meta_color (GnomePrintContext *pc)
{
	if (gp_gc_get_color_flag (pc->gc) != GP_GC_FLAG_CLEAR) {
		gpm_encode_int (pc, GNOME_META_COLOR);
		gpm_encode_double (pc, gp_gc_get_red (pc->gc));
		gpm_encode_double (pc, gp_gc_get_green (pc->gc));
		gpm_encode_double (pc, gp_gc_get_blue (pc->gc));
		gpm_encode_double (pc, gp_gc_get_opacity (pc->gc));
		gp_gc_set_color_flag (pc->gc, GP_GC_FLAG_CLEAR);
	}
}

static void
meta_line (GnomePrintContext *pc)
{
	if (gp_gc_get_line_flag (pc->gc) != GP_GC_FLAG_CLEAR) {
		gpm_encode_int (pc, GNOME_META_LINE);
		gpm_encode_double (pc, gp_gc_get_linewidth (pc->gc));
		gpm_encode_double (pc, gp_gc_get_miterlimit (pc->gc));
		gpm_encode_int (pc, gp_gc_get_linejoin (pc->gc));
		gpm_encode_int (pc, gp_gc_get_linecap (pc->gc));
		gp_gc_set_line_flag (pc->gc, GP_GC_FLAG_CLEAR);
	}
}

static void
meta_dash (GnomePrintContext *pc)
{
	if (gp_gc_get_dash_flag (pc->gc) != GP_GC_FLAG_CLEAR) {
		const ArtVpathDash *dash;
		gint i;
		dash = gp_gc_get_dash (pc->gc);
		gpm_encode_int (pc, GNOME_META_DASH);
		gpm_encode_int (pc, dash->n_dash);
		for (i = 0; i < dash->n_dash; i++) {
			gpm_encode_double (pc, dash->dash[i]);
		}
		gpm_encode_double (pc, dash->offset);
		gp_gc_set_dash_flag (pc->gc, GP_GC_FLAG_CLEAR);
	}
}

/**
 * gnome_print_meta_new:
 *
 * Creates a new Metafile context GnomePrint object.
 *
 * Returns: An empty %GnomePrint context that represents a
 * metafile priting context.
 */

/**
 * gnome_print_meta_new:
 * @void: 
 * 
 * Create a new metafile context
 * 
 * Return Value: 
 **/
GnomePrintContext *
gnome_print_meta_new (void)
{
	GnomePrintMeta *meta;

	meta = g_object_new (GNOME_TYPE_PRINT_META, NULL);

	return (GnomePrintContext *) meta;
}

void
gnome_print_meta_reset (GnomePrintMeta *meta)
{
	g_return_if_fail (GNOME_IS_PRINT_META (meta));

	if (meta->buf) g_free (meta->buf);
	meta->buf = g_new (guchar, BLOCKSIZE);
	meta->b_length = 0;
	meta->b_size = BLOCKSIZE;

	gpm_encode_block (GNOME_PRINT_CONTEXT (meta), METAFILE_SIGNATURE,
			  METAFILE_SIGNATURE_SIZE);
	gpm_encode_int_header (GNOME_PRINT_CONTEXT (meta), 0);

	meta->page = 0;
	meta->pagenum = -1;
}

const guchar *
gnome_print_meta_get_buffer (const GnomePrintMeta *meta)
{
	g_return_val_if_fail (meta != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_PRINT_META (meta), NULL);

	return meta->buf;
}

int
gnome_print_meta_get_pages (const GnomePrintMeta *meta)
{
	g_return_val_if_fail (meta != NULL, 0);
	g_return_val_if_fail (GNOME_IS_PRINT_META (meta), 0);

	return meta->pagenum + 1;
}

static const guchar *
decode_int (const guchar *data, gint32 *dest)
{
	guint32 vabs = 0, mask = 0x3f;
	int bits = 6, shift = 0;
	int neg;
	char c;

	g_return_val_if_fail (data, NULL);
	g_return_val_if_fail (dest, NULL);

	neg = (*data & 0x40);
	do {
		vabs |= ((c = *data++) & mask) << shift;
		shift += bits;
		bits = 7;
		mask = 0x7f;
	} while ((c & 0x80) == 0);

	*dest = neg ? -vabs : vabs;
	return data;
}

static const guchar *
decode_double (const guchar *data, double *dest)
{
	int i;
	data = decode_int (data, &i);

	switch ((GnomeMetaDoubleType)i) {
	case GNOME_META_DOUBLE_INT:
		data = decode_int (data, &i);
		*dest = i;
		break;
	case GNOME_META_DOUBLE_INT1000:
		data = decode_int (data, &i);
		*dest = i / 1000.0;
		break;
	case GNOME_META_DOUBLE_I386:
#if G_BYTE_ORDER == G_BIG_ENDIAN
		g_assert (sizeof (double) == 8);
		for (i = 0; i < sizeof (double); i++)
			((guint8 *)dest)[sizeof (double) - 1 - i] = data[i];
#elif G_BYTE_ORDER == G_LITTLE_ENDIAN
		g_assert (sizeof (double) == 8);
		memcpy (dest, data, sizeof (double));
#else
#error decode_double_needs_attention
#endif
		data += 8; /* An i386 double is eight bytes.  */
		break;
	default:
		*dest = 0;   /* ??? */
	}

	return data;
}

static const guchar *
gpm_decode_string (const guchar *data, guchar **dest)
{
	gint32 len;
	data = decode_int (data, &len);
	*dest = g_malloc (len + 1);
	memcpy (*dest, data, len);
	(*dest)[len] = 0;
	return data + len;
}

static gint
gpm_render (GnomePrintContext *dest, const guchar *data, gint pos, gint len, gboolean pageops)
{
	const guchar *end;

	data = data + pos;
	end = data + len;
	while (data < end){
		gint32 opcode, i;
		guchar *cval;
		gint32 ival;
		gdouble dval;
		ArtBpath *bpath;

		data = decode_int (data, &opcode);
		switch ((GnomeMetaType) opcode) {
		case GNOME_META_BEGINPAGE:
			data = gpm_decode_string (data, &cval);
			if (pageops)
				gnome_print_beginpage (dest, cval);
			g_free (cval);
			break;
		case GNOME_META_SHOWPAGE:
			if (pageops) gnome_print_showpage (dest);
			break;
		case GNOME_META_GSAVE:
			gnome_print_gsave (dest);
			break;
		case GNOME_META_GRESTORE:
			gnome_print_grestore (dest);
			break;
		case GNOME_META_CLIP:
			data = gpm_decode_bpath (data, &bpath);
			data = decode_int (data, &ival);
			gnome_print_clip_bpath_rule (dest, bpath, ival);
			g_free (bpath);
			break;
		case GNOME_META_FILL:
			data = gpm_decode_bpath (data, &bpath);
			data = decode_int (data, &ival);
			gnome_print_fill_bpath_rule (dest, bpath, ival);
			g_free (bpath);
			break;
		case GNOME_META_STROKE:
			data = gpm_decode_bpath (data, &bpath);
			gnome_print_stroke_bpath (dest, bpath);
			g_free (bpath);
			break;
		case GNOME_META_IMAGE: {
			gdouble affine[6];
			gint32 width, height, channels;
			guchar *buf;

			data = decode_double (data, &affine[0]);
			data = decode_double (data, &affine[1]);
			data = decode_double (data, &affine[2]);
			data = decode_double (data, &affine[3]);
			data = decode_double (data, &affine[4]);
			data = decode_double (data, &affine[5]);
			data = decode_int (data, &height);
			data = decode_int (data, &width);
			data = decode_int (data, &channels);
			buf = g_new (guchar, height * width * channels);
			memcpy (buf, data, height * width * channels);
			data += height * width * channels;
			gnome_print_image_transform (dest, affine, buf, width, height, channels * width, channels);
			g_free (buf);
			break;
		}
		case GNOME_META_GLYPHLIST: {
			GnomeGlyphList *gl;
			gdouble affine[6];
			gint32 len, code, ival, i;
			gdouble dval;

			data = decode_double (data, &affine[0]);
			data = decode_double (data, &affine[1]);
			data = decode_double (data, &affine[2]);
			data = decode_double (data, &affine[3]);
			data = decode_double (data, &affine[4]);
			data = decode_double (data, &affine[5]);
			gl = gnome_glyphlist_new ();
			data = decode_int (data, &len);
			if (len > 0) {
				gl->glyphs = g_new (int, len);
				gl->g_length = len;
				gl->g_size = len;
				for (i = 0; i < len; i++) {
					data = decode_int (data, &ival);
					gl->glyphs[i] = ival;
				}
			}
			data = decode_int (data, &len);
			if (len > 0) {
				gl->rules = g_new (GGLRule, len);
				gl->r_length = len;
				gl->r_size = len;
				for (i = 0; i < len; i++) {
					data = decode_int (data, &code);
					gl->rules[i].code = code;
					switch (code) {
					case GGL_POSITION:
					case GGL_ADVANCE:
					case GGL_COLOR:
						data = decode_int (data, &ival);
						gl->rules[i].value.ival = ival;
						break;
					case GGL_MOVETOX:
					case GGL_MOVETOY:
					case GGL_RMOVETOX:
					case GGL_RMOVETOY:
					case GGL_LETTERSPACE:
					case GGL_KERNING:
						data = decode_double (data, &dval);
						gl->rules[i].value.dval = dval;
						break;
					case GGL_FONT: {
						GnomeFont *font;
						guchar *name;
						data = decode_double (data, &dval);
						data = gpm_decode_string (data, &name);
						font = gnome_font_find (name, dval);
						if (font == NULL)
							g_warning ("Cannot find font: %s\n", name);
						g_free (name);
						gl->rules[i].value.font = font;
						break;
					}
					default:
						break;
					}
				}
			}
			gnome_print_glyphlist_transform (dest, affine, gl);
			gnome_glyphlist_unref (gl);
			break;
		}
		break;
		case GNOME_META_COLOR: {
			gdouble r, g, b, a;
			data = decode_double (data, &r);
			data = decode_double (data, &g);
			data = decode_double (data, &b);
			gnome_print_setrgbcolor (dest, r, g, b);
			data = decode_double (data, &a);
			gnome_print_setopacity (dest, a);
			break;
		}
		case GNOME_META_LINE:
			data = decode_double (data, &dval);
			gnome_print_setlinewidth (dest, dval);
			data = decode_double (data, &dval);
			gnome_print_setmiterlimit (dest, dval);
			data = decode_int (data, &ival);
			gnome_print_setlinejoin (dest, ival);
			data = decode_int (data, &ival);
			gnome_print_setlinecap (dest, ival);
			break;
		case GNOME_META_DASH: {
			int n;
			double *values, offset;

			data = decode_int (data, &n);
			values = g_new (double, n);
			for (i = 0; i < n; i++) {
				data = decode_double (data, &values [i]);
			}
			data = decode_double (data, &offset);
			gnome_print_setdash (dest, n, values, offset);
			g_free (values);
			break;
		}
		default:
			g_warning ("Serious print meta data corruption %d", opcode);
			break;
		}
	}

	return GNOME_PRINT_OK;
}

static void
search_page (const guchar *b, guint b_len, guint page, guint *pos, guint *len)
{
	gint pagenum;

	g_return_if_fail (b != NULL);
	g_return_if_fail (pos != NULL);

	*pos = METAFILE_HEADER_SIZE;

        /* Search the page */
	for (pagenum = 0; *pos < b_len; pagenum++) {
		gint32 l;

		if (strncmp ((const gchar *) (b + *pos), PAGE_SIGNATURE, PAGE_SIGNATURE_SIZE))
			break;
		gpm_decode_int_header (b + *pos + PAGE_SIGNATURE_SIZE, &l);
		*pos += PAGE_HEADER_SIZE;
		if (pagenum == page) {
			if (len) *len = MIN (b_len - *pos, l);
			return;
		}
		*pos += l;
	}

	/* We have not found the page. */
	*pos = b_len;
	if (len) *len = 0;
}

/**
 * gnome_print_meta_render:
 * @meta:
 * @dest:
 *
 * Render to a specified output context
 *
 * Return Value:
 **/
gint
gnome_print_meta_render (const GnomePrintMeta *meta, GnomePrintContext *dst)
{
	g_return_val_if_fail (GNOME_IS_PRINT_META (meta),
			      GNOME_PRINT_ERROR_BADCONTEXT);

	return gnome_print_meta_render_data (dst, meta->buf, meta->b_length);
}

/**
 * gnome_print_meta_get_page_name:
 * @meta:
 * @page:
 * @page_name:
 *
 * Get the name of a specific page.
 *
 * Return Value:
 **/
gint
gnome_print_meta_get_page_name (const GnomePrintMeta *meta, guint page, guchar **page_name)
{
	guint pos;
	gint32 opcode;
	const guchar *data;

	g_return_val_if_fail (GNOME_IS_PRINT_META (meta), GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (page_name != NULL, GNOME_PRINT_ERROR_BADCONTEXT);

	*page_name = NULL;

	/* Search the page */
	search_page (meta->buf, meta->b_length, page, &pos, NULL);
	if (pos >= meta->b_length) return GNOME_PRINT_ERROR_BADVALUE;

	data = decode_int (meta->buf + pos, &opcode);
	if (opcode != GNOME_META_BEGINPAGE) return GNOME_PRINT_ERROR_BADCONTEXT;
	gpm_decode_string (data, page_name);

	return GNOME_PRINT_OK;
}

/**
 * gnome_print_meta_render_page:
 * @meta:
 * @dest:
 * @page:
 *
 * Render page to specified output context
 * 
 * Return Value:
 **/
gint
gnome_print_meta_render_page (const GnomePrintMeta *meta, GnomePrintContext *dst, gint page, gboolean pageops)
{
	g_return_val_if_fail (GNOME_IS_PRINT_META (meta), GNOME_PRINT_ERROR_BADCONTEXT);

	return gnome_print_meta_render_data_page (dst, meta->buf,
					meta->b_length, page, pageops);
}

/**
 * gnome_print_meta_render_data:
 * @ctx: 
 * @data: 
 * @length: 
 * 
 * Render stream to specified output context
 * 
 * Return Value: 
 **/
gint
gnome_print_meta_render_data (GnomePrintContext *ctx, const guchar *data, gint length)
{
	gint pos;

	g_return_val_if_fail (ctx != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (ctx), GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (data != NULL, GNOME_PRINT_ERROR_UNKNOWN);
	g_return_val_if_fail (length >= METAFILE_HEADER_SIZE, GNOME_PRINT_ERROR_UNKNOWN);
	g_return_val_if_fail (!strncmp ((const gchar *) data, METAFILE_SIGNATURE, METAFILE_SIGNATURE_SIZE), GNOME_PRINT_ERROR_UNKNOWN);

	pos = METAFILE_HEADER_SIZE;

	while (pos < length) {
		gint32 len;
		g_return_val_if_fail (!strncmp ((const gchar *) (data + pos), PAGE_SIGNATURE, PAGE_SIGNATURE_SIZE), GNOME_PRINT_ERROR_UNKNOWN);
		gpm_decode_int_header (data + pos + PAGE_SIGNATURE_SIZE, &len);
		pos += PAGE_HEADER_SIZE;
		if (len == 0)
			len = length - pos;
		gpm_render (ctx, data, pos, len, TRUE);
		pos += len;
	}

	return GNOME_PRINT_OK;
}

/**
 * gnome_print_meta_render_data_page:
 * @ctx: 
 * @data: 
 * @length: 
 * @page: 
 * @pageops: wether you want to send begingpage/showpage to output
 * 
 * Render page to specified output context
 * 
 * Return Value: 
 **/
gint
gnome_print_meta_render_data_page (GnomePrintContext *ctx, const guchar *data, gint length, gint page, gboolean pageops)
{
	guint len, pos;

	g_return_val_if_fail (ctx != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (ctx), GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (data != NULL, GNOME_PRINT_ERROR_UNKNOWN);
	g_return_val_if_fail (length >= METAFILE_HEADER_SIZE, GNOME_PRINT_ERROR_UNKNOWN);
	g_return_val_if_fail (!strncmp ((const gchar *) data, METAFILE_SIGNATURE, METAFILE_SIGNATURE_SIZE), GNOME_PRINT_ERROR_UNKNOWN);

	search_page (data, length, page, &pos, &len);
	if (!len) return GNOME_PRINT_ERROR_BADVALUE;
	return gpm_render (ctx, data, pos, len, pageops);
}

/**
 * gnome_print_meta_render_file:
 * @ctx: 
 * @filename: 
 * 
 * See gnome_print_meta_render_data
 * 
 * Return Value: 
 **/
gint
gnome_print_meta_render_file (GnomePrintContext *ctx, const guchar *filename)
{
	GnomePrintReturnCode retval;
	GnomePrintBuffer b;

	g_return_val_if_fail (ctx != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (ctx), GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (filename != NULL, GNOME_PRINT_ERROR_UNKNOWN);

	retval = gnome_print_buffer_mmap (&b, filename);
	if (retval != GNOME_PRINT_OK)
		return retval;
	
	retval = gnome_print_meta_render_data (ctx, b.buf, b.buf_size);

	gnome_print_buffer_munmap (&b);

	return retval;
}

/**
 * gnome_print_meta_render_file_page:
 * @ctx: 
 * @filename: 
 * @page: 
 * @pageops: 
 * 
 * See gnome_print_meta_render_data_page
 * 
 * Return Value: 
 **/
gint
gnome_print_meta_render_file_page (GnomePrintContext *ctx, const guchar *filename, gint page, gboolean pageops)
{
	GnomePrintReturnCode retval;
	GnomePrintBuffer b;

	g_return_val_if_fail (ctx != NULL, GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (GNOME_IS_PRINT_CONTEXT (ctx), GNOME_PRINT_ERROR_BADCONTEXT);
	g_return_val_if_fail (filename != NULL, GNOME_PRINT_ERROR_UNKNOWN);

	retval = gnome_print_buffer_mmap (&b, filename);
	if (retval != GNOME_PRINT_OK)
		return retval;
	
	retval = gnome_print_meta_render_data_page (ctx, b.buf, b.buf_size, page, pageops);

	gnome_print_buffer_munmap (&b);

	return retval;
}

static void
gpm_encode_string (GnomePrintContext *pc, const gchar *str)
{
	gint bytes;

	bytes = strlen (str);

	gpm_encode_int (pc, bytes);
	gpm_encode_block (pc, str, bytes);
}

static void
gpm_encode_int (GnomePrintContext *pc, gint32 value)
{
	GnomePrintMeta *meta;
	guchar *out, *out0;
	guint32 vabs, mask;
	int bits;

	meta = (GnomePrintMeta *) pc;

	if (!GPM_ENSURE_SPACE (meta, sizeof (value) * 8 / 7 + 1)) {
		g_warning ("file %s: line %d: Cannot grow metafile buffer (%d bytes)", __FILE__, __LINE__, sizeof (value) * 8 / 7 + 1);
		return;
	}

	/*
	 * We encode an integer as a sequence of bytes where all but the
	 * last have the high bit set to zero.  The final byte does have
	 * that bit set.
	 *
	 * Bit 6 of the first byte contains the sign bit.
	 *
	 * The remaining 6, 7, ..., 7 bits contain the absolute value,
	 * starting with the six least significant bits in the first
	 * byte.
	 */

	out0 = out = meta->buf + meta->b_length;
	vabs = (value >= 0) ? value : -value;
	bits = 6;
	mask = 0x3f;

	do {
		*out++ = (vabs & mask);
		vabs >>= bits;
		bits = 7;
		mask = 0x7f;
	} while (vabs);

	out[-1] |= 0x80;
	if (value < 0) out0[0] |= 0x40;
	gnome_print_meta_set_length (meta, out - meta->buf);
}

static void
gpm_encode_double (GnomePrintContext *pc, double d)
{
	if (d == (gint32)d) {
		gpm_encode_int (pc, GNOME_META_DOUBLE_INT);
		gpm_encode_int (pc, (gint32)d);
	} else {
		double d1000 = d * 1000;
		if (d1000 == (gint32)d1000) {
			gpm_encode_int (pc, GNOME_META_DOUBLE_INT1000);
			gpm_encode_int (pc, (gint32)d1000);
		} else {
			gpm_encode_int (pc, GNOME_META_DOUBLE_I386);
#if G_BYTE_ORDER == G_BIG_ENDIAN
			g_assert (sizeof (double) == 8);
			{
				int i;
				const guchar *t  = (guchar *)&d;
				guchar block[sizeof (d)];

				for (i = 0; i < sizeof (d); i++)
					block[sizeof (d) - 1 - i] = t[i];
				gpm_encode_block (pc, block, sizeof (d));
			}
#elif G_BYTE_ORDER == G_LITTLE_ENDIAN
			g_assert (sizeof (double) == 8);
			gpm_encode_block (pc, (const gchar *) &d, sizeof (d));
#else
#error encode_double_needs_attention
#endif
		}
	}
}

static void
gpm_encode_block (GnomePrintContext *pc, const gchar *data, gint size)
{
	GnomePrintMeta *meta = GNOME_PRINT_META (pc);

	if (!GPM_ENSURE_SPACE (meta, size)) {
		g_warning ("file %s: line %d: Cannot grow metafile buffer (%d bytes)", __FILE__, __LINE__, size);
		return;
	}

	memcpy (meta->buf + meta->b_length, data, size);
	gnome_print_meta_set_length (meta, meta->b_length + size);
}

static gboolean
gpm_ensure_space (GnomePrintMeta *meta, int size)
{
	int req;
	guchar *new;

	req = MAX (BLOCKSIZE, meta->b_length + size - meta->b_size);

	new = g_realloc (meta->buf, meta->b_size + req);
	g_return_val_if_fail (new != NULL, FALSE);

	meta->buf = new;
	meta->b_size = meta->b_size + req;

	return TRUE;
}

static void
gpm_encode_bpath (GnomePrintContext *pc, const ArtBpath *bpath)
{
	gint len;

	len = 0;
	while (bpath[len].code != ART_END) len++;
	gpm_encode_int (pc, len + 1);

	while (bpath->code != ART_END) {
		gpm_encode_int (pc, bpath->code);
		switch (bpath->code) {
		case ART_CURVETO:
			gpm_encode_double (pc, bpath->x1);
			gpm_encode_double (pc, bpath->y1);
			gpm_encode_double (pc, bpath->x2);
			gpm_encode_double (pc, bpath->y2);
		case ART_MOVETO:
		case ART_MOVETO_OPEN:
		case ART_LINETO:
			gpm_encode_double (pc, bpath->x3);
			gpm_encode_double (pc, bpath->y3);
			break;
		default:
			g_warning ("Illegal pathcode in Bpath");
			break;
		}
		bpath += 1;
	}
	gpm_encode_int (pc, ART_END);
}

static const guchar *
gpm_decode_bpath (const guchar *data, ArtBpath **bpath)
{
	ArtBpath *p;
	gint32 len, code;

	data = decode_int (data, &len);
	if (!len) {
		g_warning ("Could not decode bpath: Corrupt data!");
		return NULL;
	}
	*bpath = g_new (ArtBpath, len);

	p = *bpath;
	data = decode_int (data, &code);
	while (code != ART_END) {
		p->code = code;
		switch (code) {
		case ART_CURVETO:
			data = decode_double (data, &p->x1);
			data = decode_double (data, &p->y1);
			data = decode_double (data, &p->x2);
			data = decode_double (data, &p->y2);
		case ART_MOVETO:
		case ART_MOVETO_OPEN:
		case ART_LINETO:
			data = decode_double (data, &p->x3);
			data = decode_double (data, &p->y3);
			break;
		default:
			g_warning ("Illegal pathcode %d", code);
			break;
		}
		p += 1;
		data = decode_int (data, &code);
	}
	p->code = ART_END;

	return data;
}

/*
 * Less compact, but gauranteed size (just makes things easier to encode
 * fixed size data like headers)
 */

static void
gpm_encode_int_header (GnomePrintContext *pc, gint32 value)
{
	gint32 new;

	new = g_htonl (value);

	gpm_encode_block (pc, (const gchar *) &new, sizeof (gint32));
}

static const guchar *
gpm_decode_int_header (const guchar *data, gint32 *dest)
{
	gint32 nint;

	memcpy (&nint, data, sizeof (gint32));
	*dest = g_ntohl (nint);

	return data + sizeof (gint32);
}
