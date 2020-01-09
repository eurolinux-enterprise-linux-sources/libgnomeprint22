/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  gnome-print-gdi.c: the GDI backend
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
 *          Ivan, Wong Yat Cheung <email@ivanwong.info>
 *
 *  References:
 *          http://msdn.microsoft.com/library/default.asp?url=/library/en-us/gdi/prntspol_62ia.asp
 *
 *  Copyright (C) 2005 Novell Inc. and authors
 *
 */

#include <config.h>
#include <math.h>

#include <libgnomeprint/gnome-print-private.h>
#include <libgnomeprint/gnome-pgl-private.h>
#include <libgnomeprint/gnome-font-private.h>
#include <libgnomeprint/gnome-print-i18n.h>
#include <libgnomeprint/gp-gc-private.h>

#include <ft2build.h>
#include <freetype/freetype.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wingdi.h>
#include <winspool.h>
#include "gnome-print-gdi.h"

struct _GnomePrintGdiClass {
	GnomePrintContextClass parent_class;
};

struct _GnomePrintGdi {
	GnomePrintContext pc;
	HDC hDC;
	gint job_id;
	GnomeFont *selected_font;
	HFONT hOldFont;
	HPEN hOldPen;
	HBRUSH hOldBrush;
	HPEN hPen;
	HBRUSH hBrush;
	guint32 pen_color;
	guint32 brush_color;
	gboolean persistent_pen;
	gboolean persistent_brush;
	GSList *gstack;
};

typedef struct {
	gint fields;
	HPEN hPen;
	HBRUSH hBrush;
	HRGN hRgn;
	gboolean persistent_pen;
	gboolean persistent_brush;
} GState;

enum {
	GSTATE_PEN,
	GSTATE_BRUSH,
	GSTATE_CLIPRGN
};

static void gnome_print_gdi_class_init (GnomePrintGdiClass *klass);
static void gnome_print_gdi_init (GnomePrintGdi *gdi);
static void gnome_print_gdi_finalize (GObject *object);

static gint gnome_print_gdi_construct (GnomePrintContext *ctx);
static gint gnome_print_gdi_gsave (GnomePrintContext *pc);
static gint gnome_print_gdi_grestore (GnomePrintContext *pc);
static gint gnome_print_gdi_fill (GnomePrintContext *pc, const ArtBpath *bpath, ArtWindRule rule);
static gint gnome_print_gdi_clip (GnomePrintContext *pc, const ArtBpath *bpath, ArtWindRule rule);
static gint gnome_print_gdi_stroke (GnomePrintContext *pc, const ArtBpath *bpath);
static gint gnome_print_gdi_image (GnomePrintContext *pc, const gdouble *affine, const guchar *px, gint w, gint h, gint rowstride, gint ch);
static gint gnome_print_gdi_glyphlist (GnomePrintContext *pc, const gdouble *affine, GnomeGlyphList *gl);
static gint gnome_print_gdi_beginpage (GnomePrintContext *pc, const guchar *name);
static gint gnome_print_gdi_showpage (GnomePrintContext *pc);
static gint gnome_print_gdi_end_doc (GnomePrintContext *pc);
static gint gnome_print_gdi_close (GnomePrintContext *pc);

static GnomePrintContextClass *parent_class;

#define EPSILON 1e-9

/* We can use GetWindowExtEx() or SetWorldTransform() to do the scaling,
   but as GDI only accepts integer coordinates, we better let ourself
   have a chance to scale-up our values before passing them to GDI,
   to improve the accuracy. Note that a unit of MM_TWIPS == 1/1440 inch,
   and a point == 1/72 inch. Indeed, we can use MM_*TROPIC so that the
   scaling factor can be larger than 20, but who cares? */
#define POINT_PER_UNIT 20.0
#define PT(x) ((gint) ((x) * POINT_PER_UNIT + 0.5))

#define RGB_TO_COLORREF(x) RGB (((x) >> 24) & 0xff, \
				 ((x) >> 16) & 0xff, \
				 ((x) >> 8) & 0xff)

GType
gnome_print_gdi_get_type (void)
{
	static GType gdi_type = 0;
	if (!gdi_type) {
		static const GTypeInfo gdi_info = {
			sizeof (GnomePrintGdiClass),
			NULL, NULL,
			(GClassInitFunc) gnome_print_gdi_class_init,
			NULL, NULL,
			sizeof (GnomePrintGdi),
			0,
			(GInstanceInitFunc) gnome_print_gdi_init
		};
		gdi_type = g_type_register_static (GNOME_TYPE_PRINT_CONTEXT, "GnomePrintGdi", &gdi_info, 0);
	}
	return gdi_type;
}

static void
gnome_print_gdi_class_init (GnomePrintGdiClass *klass)
{
	GnomePrintContextClass *pc_class;
	GObjectClass *object_class;

	object_class = (GObjectClass *) klass;
	pc_class = (GnomePrintContextClass *)klass;

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gnome_print_gdi_finalize;

	pc_class->construct = gnome_print_gdi_construct;
	pc_class->beginpage = gnome_print_gdi_beginpage;
	pc_class->showpage = gnome_print_gdi_showpage;
	pc_class->end_doc = gnome_print_gdi_end_doc;
	pc_class->gsave = gnome_print_gdi_gsave;
	pc_class->grestore = gnome_print_gdi_grestore;
	pc_class->clip = gnome_print_gdi_clip;
	pc_class->fill = gnome_print_gdi_fill;
	pc_class->stroke = gnome_print_gdi_stroke;
	pc_class->image = gnome_print_gdi_image;
	pc_class->glyphlist = gnome_print_gdi_glyphlist;
	pc_class->close = gnome_print_gdi_close;
}

static void
gnome_print_gdi_init (GnomePrintGdi *gdi)
{
	gdi->hDC = NULL;
	gdi->job_id = 0;
	gdi->selected_font = NULL;
	gdi->hOldFont = NULL;
	gdi->hOldPen = NULL;
	gdi->hPen = NULL;
	gdi->pen_color = 0;
	gdi->hOldBrush = NULL;
	gdi->hBrush = NULL;
	gdi->brush_color = 0;
	gdi->persistent_pen = TRUE;
	gdi->persistent_brush = TRUE;
	gdi->gstack = NULL;
}

static void
gnome_print_gdi_finalize (GObject *object)
{
	GnomePrintGdi *gdi = GNOME_PRINT_GDI (object);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gint
gnome_print_gdi_construct (GnomePrintContext *ctx)
{
	GnomePrintGdi *gdi = GNOME_PRINT_GDI (ctx);
	gchar *printer, *ptr;
	const gchar *key;
	HANDLE hPrinter;
	LONG devmode_len;
	DEVMODE *pDevmode = NULL;
	gint id, numcopies = 1;
	gboolean duplex = FALSE, tumble = FALSE;
	DOCINFO doc_info;
	SIZE size;
	gdouble ctm[6];

	printer = gnome_print_config_get (ctx->config, "Printer");
	if (!printer) {
		g_warning ("Could not find \"Settings.Transport.Backend.Printer\"");
		return GNOME_PRINT_ERROR_UNKNOWN;
	}

	if (!OpenPrinter (printer, &hPrinter, NULL))
		return GNOME_PRINT_ERROR_UNKNOWN;

	devmode_len = DocumentProperties (NULL, hPrinter,
					  printer, NULL, NULL, 0);
	if (devmode_len > 0) {
		pDevmode = g_malloc (devmode_len);
		devmode_len = DocumentProperties (NULL, hPrinter, printer,
						  pDevmode, NULL, DM_OUT_BUFFER);
	}
	if (devmode_len <= 0) {
		ClosePrinter (hPrinter);
		if (pDevmode)
			g_free (pDevmode);
		return GNOME_PRINT_ERROR_UNKNOWN;
	}
	pDevmode->dmFields = 0;

	key = gnome_print_config_get (ctx->config, GNOME_PRINT_KEY_PAPER_SIZE);
	if (!strcmp(key, "Custom")) {
		gdouble width, height;
		const GnomePrintUnit *unit, *mm;

		mm = gnome_print_unit_get_by_abbreviation ("mm");
		gnome_print_config_get_length (ctx->config,
					       GNOME_PRINT_KEY_PAPER_WIDTH,
					       &width, &unit);
		gnome_print_convert_distance (&width, unit, mm);
		gnome_print_config_get_length (ctx->config,
					       GNOME_PRINT_KEY_PAPER_HEIGHT,
					       &height, &unit);		
		gnome_print_convert_distance (&height, unit, mm);
		pDevmode->dmPaperSize = 0;
		pDevmode->dmPaperWidth = (gshort) (width * 10.0f);
		pDevmode->dmPaperLength = (gshort) (height * 10.0f);
		pDevmode->dmFields |=
			DM_PAPERWIDTH | DM_PAPERLENGTH | DM_PAPERSIZE;
	}
	else {
		errno = 0;
		id = strtoul (key, &ptr, 10);
		if (!errno && id > 0 && id != pDevmode->dmPaperSize) {
			pDevmode->dmPaperSize = id;
			pDevmode->dmFields |= DM_PAPERSIZE;
		}
	}

	key = gnome_print_config_get (ctx->config,
				      GNOME_PRINT_KEY_PAPER_SOURCE);
	errno = 0;
	id = strtoul (key, &ptr, 10);
	if (!errno && id > 0 && id != pDevmode->dmDefaultSource) {
		pDevmode->dmDefaultSource = id;
		pDevmode->dmFields |= DM_DEFAULTSOURCE;
	}

	gnome_print_config_get_int (ctx->config,
				    GNOME_PRINT_KEY_NUM_COPIES,
				    &numcopies);
	if (numcopies != pDevmode->dmCopies) {
		pDevmode->dmCopies = numcopies;
		pDevmode->dmFields |= DM_COPIES;
	}

	gnome_print_config_get_boolean (ctx->config,
					GNOME_PRINT_KEY_DUPLEX,
					&duplex);
	gnome_print_config_get_boolean (ctx->config,
					GNOME_PRINT_KEY_TUMBLE,
					&tumble);
	if (duplex == (pDevmode->dmDuplex == DMDUP_SIMPLEX) ||
	    tumble != (pDevmode->dmDuplex == DMDUP_HORIZONTAL)) {
		pDevmode->dmDuplex = duplex ?
			(tumble ? DMDUP_HORIZONTAL : DMDUP_VERTICAL) :
			DMDUP_SIMPLEX;
		pDevmode->dmFields |= DM_DUPLEX;
	}

	if (pDevmode->dmFields) {
		DocumentProperties (NULL, hPrinter, printer,
				    pDevmode, pDevmode, DM_IN_BUFFER | DM_OUT_BUFFER);
	}
	ClosePrinter (hPrinter);
	gdi->hDC = CreateDC ("WINSPOOL", printer, NULL, pDevmode);
	g_free (pDevmode);
	if (!gdi->hDC)
		return GNOME_PRINT_ERROR_UNKNOWN;

	doc_info.cbSize = sizeof(doc_info);
	key = gnome_print_config_get (ctx->config, GNOME_PRINT_KEY_DOCUMENT_NAME);
	doc_info.lpszDocName = key && strlen(key) ? key : _("Untitled");
	doc_info.lpszOutput = gnome_print_config_get (ctx->config,
						      "Settings.Transport.Backend");
	doc_info.lpszOutput =
		!strcmp (gnome_print_config_get (ctx->config,
						 "Settings.Transport.Backend"), "file") ?
		gnome_print_config_get (ctx->config, "Settings.Output.Job.FileName") : NULL;
	doc_info.lpszDatatype  = NULL;
	doc_info.fwType  = 0;

	gdi->job_id = StartDoc (gdi->hDC, &doc_info);
	if (gdi->job_id <= 0) {
		DeleteDC (gdi->hDC);
		return GNOME_PRINT_ERROR_UNKNOWN;
	}

	SetGraphicsMode (gdi->hDC, GM_ADVANCED);
	SetMapMode (gdi->hDC, MM_TWIPS);

	GetWindowExtEx (gdi->hDC, &size);
	SetWindowOrgEx (gdi->hDC, 0, size.cy, NULL);

	SetTextAlign (gdi->hDC, TA_BASELINE);

	gdi->hOldPen = GetCurrentObject (gdi->hDC, OBJ_PEN);
	gdi->hOldBrush= GetCurrentObject (gdi->hDC, OBJ_BRUSH);
	
	if (gnome_print_config_get_transform (
		ctx->config, GNOME_PRINT_KEY_PAPER_ORIENTATION_MATRIX, ctm) &&
	    (fabs (ctm[0] - 1.0) > EPSILON || fabs (ctm[1]) > EPSILON ||
	     fabs (ctm[2]) > EPSILON || fabs (ctm[3] - 1.0) > EPSILON ||
	     fabs (ctm[4]) > EPSILON || fabs (ctm[5]) > EPSILON)) {
		XFORM xForm;

		xForm.eM11 = (FLOAT) ctm[0];
		xForm.eM12 = (FLOAT) ctm[1];
		xForm.eM21 = (FLOAT) ctm[2];
		xForm.eM22 = (FLOAT) ctm[3];
		xForm.eDx = (FLOAT) (ctm[4] * POINT_PER_UNIT);
		xForm.eDy = (FLOAT) (ctm[5] * POINT_PER_UNIT);
		SetWorldTransform(gdi->hDC, &xForm);
	}
   
	return GNOME_PRINT_OK;
}

static gint
gnome_print_gdi_gsave (GnomePrintContext *ctx)
{
	GnomePrintGdi *gdi = GNOME_PRINT_GDI (ctx);
	GPGC *gc = ctx->gc;
	GState *gstate;
	guint32 rgba;

	gstate = g_malloc (sizeof (GState));
	gstate->fields = 0;

	rgba = gp_gc_get_rgba (gc);

	if (rgba == gdi->pen_color &&
	    gp_gc_get_line_flag (gc) == GP_GC_FLAG_CLEAR &&
	    gp_gc_get_dash_flag (gc) == GP_GC_FLAG_CLEAR) {
		gstate->hPen = gdi->hPen;
		gstate->persistent_pen = gdi->persistent_pen;
		gdi->persistent_pen = TRUE;
		gstate->fields |= GSTATE_PEN;
	}

	if (gdi->hBrush && rgba == gdi->brush_color) {
		gstate->hBrush = gdi->hBrush;
		gstate->persistent_brush = gdi->persistent_brush;
		gdi->persistent_brush = TRUE;
		gstate->fields |= GSTATE_BRUSH;
	}

	gstate->hRgn = CreateRectRgn (0, 0, 10, 10);
	if (GetClipRgn (gdi->hDC, gstate->hRgn))
		gstate->fields |= GSTATE_CLIPRGN;
	else
		DeleteObject (gstate->hRgn);

	gdi->gstack = g_slist_prepend (gdi->gstack, gstate);

	return GNOME_PRINT_OK;
}

static gint
gnome_print_gdi_grestore (GnomePrintContext *ctx)
{
	GnomePrintGdi *gdi = GNOME_PRINT_GDI (ctx);
	GState *gstate, *prev_gstate;

	g_return_val_if_fail (gdi->gstack != NULL, GNOME_PRINT_ERROR_UNKNOWN);
	
	gstate = gdi->gstack->data;
	prev_gstate = gdi->gstack->next ?
		(GState *) gdi->gstack->next->data : NULL;

	if (gstate->fields & GSTATE_PEN && !gdi->persistent_pen) {
		DeleteObject (SelectObject (gdi->hDC, gstate->hPen));
		gdi->persistent_pen = gstate->persistent_pen;
	}

	if (gstate->fields & GSTATE_BRUSH && !gdi->persistent_brush) {
		DeleteObject (SelectObject (gdi->hDC, gstate->hBrush));
		gdi->persistent_brush = gstate->persistent_brush;
	}

	if (gstate->fields & GSTATE_CLIPRGN) {
		SelectClipRgn (gdi->hDC, gstate->hRgn);
		DeleteObject (gstate->hRgn);
	}

	g_free (gstate);
	gdi->gstack = g_slist_remove (gdi->gstack, gstate);

	return GNOME_PRINT_OK;
}

static gint
gnome_print_gdi_set_pen (GnomePrintContext *ctx)
{
	GnomePrintGdi *gdi = GNOME_PRINT_GDI (ctx);
	GPGC *gc = ctx->gc;
	DWORD pen_style;
	LOGBRUSH brush;
	HPEN hPen, hOldPen;
	gint lflag, dflag;
	guint32 rgba;

	rgba = gp_gc_get_rgba (gc);
	if (rgba != gdi->pen_color ||
	    (lflag = gp_gc_get_line_flag (gc)) != GP_GC_FLAG_CLEAR ||
	    (dflag = gp_gc_get_dash_flag (gc)) != GP_GC_FLAG_CLEAR) {
		pen_style = PS_GEOMETRIC;

		switch (gp_gc_get_linecap (gc)) {
		case ART_PATH_STROKE_CAP_BUTT:
			pen_style |= PS_ENDCAP_FLAT;
			break;
		case ART_PATH_STROKE_CAP_ROUND:
			pen_style |= PS_ENDCAP_ROUND;
			break;
		case ART_PATH_STROKE_CAP_SQUARE:
			pen_style |= PS_ENDCAP_SQUARE;
			break;
		}

		switch (gp_gc_get_linejoin (gc)) {
		case ART_PATH_STROKE_JOIN_MITER:
			pen_style |= PS_JOIN_MITER;
			break;
		case ART_PATH_STROKE_JOIN_ROUND:
			pen_style |= PS_JOIN_ROUND;
			break;
		case ART_PATH_STROKE_JOIN_BEVEL:
			pen_style |= PS_JOIN_BEVEL;
			break;
		}

		brush.lbStyle = BS_SOLID;
		brush.lbColor = RGB_TO_COLORREF (rgba);

		hPen = ExtCreatePen (pen_style | PS_SOLID,
				     PT (gp_gc_get_linewidth (gc)),
				     &brush,
				     0,
				     NULL);
		if (!hPen)
			return GNOME_PRINT_ERROR_UNKNOWN;

		hOldPen = SelectObject (gdi->hDC, hPen);
		if (gdi->persistent_pen)
			gdi->persistent_pen = FALSE;
		else
			DeleteObject (hOldPen);

		gdi->pen_color = rgba;
		gdi->hPen = hPen;

		if (lflag != GP_GC_FLAG_CLEAR)
			gp_gc_set_line_flag (gc, GP_GC_FLAG_CLEAR);
		if (dflag != GP_GC_FLAG_CLEAR)
			gp_gc_set_dash_flag (gc, GP_GC_FLAG_CLEAR);
	}

	return GNOME_PRINT_OK;
}

static gint
gnome_print_gdi_set_brush (GnomePrintContext *ctx)
{
	GnomePrintGdi *gdi = GNOME_PRINT_GDI (ctx);
	LOGBRUSH brush;
	HBRUSH hBrush, hOldBrush;
	guint32 rgba;

	rgba = gp_gc_get_rgba (ctx->gc);

	if (!gdi->hBrush || rgba != gdi->brush_color) {
		brush.lbStyle = BS_SOLID;
		brush.lbColor = RGB_TO_COLORREF (rgba);
		hBrush = CreateBrushIndirect (&brush);

		if (!hBrush)
			return GNOME_PRINT_ERROR_UNKNOWN;

		hOldBrush = SelectObject (gdi->hDC, hBrush);
		if (gdi->persistent_brush)
			gdi->persistent_brush = FALSE;
		else
			DeleteObject (hOldBrush);

		gdi->hBrush = hBrush;
		gdi->brush_color = rgba;
	}

	return GNOME_PRINT_OK;
}

static gint
gnome_print_gdi_realize_bpath (GnomePrintGdi *gdi, const ArtBpath *bpath)
{
	gboolean closed;
	POINT pts[3];

	closed = FALSE;

	BeginPath (gdi->hDC);

	while (bpath->code != ART_END) {
		switch (bpath->code) {
		case ART_MOVETO:
		case ART_MOVETO_OPEN:
			if (closed)
				CloseFigure (gdi->hDC);
			closed = (bpath->code == ART_MOVETO);
			MoveToEx (gdi->hDC, PT (bpath->x3), PT (bpath->y3), NULL);
			break;
		case ART_LINETO:
			LineTo (gdi->hDC, PT (bpath->x3), PT (bpath->y3));
			break;
		case ART_CURVETO:
			pts[0].x = PT (bpath->x1);
			pts[0].y = PT (bpath->y1);
			pts[1].x = PT (bpath->x2);
			pts[1].y = PT (bpath->y2);
			pts[2].x = PT (bpath->x3);
			pts[2].y = PT (bpath->y3);
			PolyBezierTo (gdi->hDC, pts, 3);
			break;
		default:
			g_warning ("Illegal pathcode %d in bpath", bpath->code);
			return GNOME_PRINT_ERROR_BADVALUE;
		}
		bpath += 1;
	}

	if (closed)
		CloseFigure (gdi->hDC);

	EndPath (gdi->hDC);
	
	return GNOME_PRINT_OK;
}

static gint
gnome_print_gdi_clip (GnomePrintContext *ctx, const ArtBpath *bpath, ArtWindRule rule)
{
	GnomePrintGdi *gdi = GNOME_PRINT_GDI (ctx);

	gnome_print_gdi_realize_bpath (gdi, bpath);
	SetPolyFillMode (gdi->hDC,
		rule == ART_WIND_RULE_NONZERO ? WINDING : ALTERNATE);
	SelectClipPath (gdi->hDC, RGN_COPY);

	return GNOME_PRINT_OK;
}

static gint
gnome_print_gdi_fill (GnomePrintContext *ctx, const ArtBpath *bpath, ArtWindRule rule)
{
	GnomePrintGdi *gdi = GNOME_PRINT_GDI (ctx);

	gnome_print_gdi_set_brush (ctx);
	gnome_print_gdi_realize_bpath (gdi, bpath);
	SetPolyFillMode (gdi->hDC,
		rule == ART_WIND_RULE_NONZERO ? WINDING : ALTERNATE);
	FillPath (gdi->hDC);

	return GNOME_PRINT_OK;
}

static gint
gnome_print_gdi_stroke (GnomePrintContext *ctx, const ArtBpath *bpath)
{
	GnomePrintGdi *gdi = GNOME_PRINT_GDI (ctx);

	gnome_print_gdi_set_pen (ctx);
	gnome_print_gdi_realize_bpath (gdi, bpath);
	StrokePath (gdi->hDC);

	return GNOME_PRINT_OK;
}

static int
gnome_print_gdi_image (GnomePrintContext *pc, const gdouble *ctm, const guchar *px, gint w, gint h, gint rowstride, gint ch)
{
	GnomePrintGdi *gdi = GNOME_PRINT_GDI (pc);
	gint bmi_size, dib_stride, i, x, y, channel, ret;
	PBITMAPINFO bmi;
	PBITMAPINFOHEADER bmh;
	guchar *buf, *dst_px, *dst;
	const guchar *src_px;
	XFORM xOldForm, xForm;

	bmi_size = sizeof (BITMAPINFOHEADER);
	if (ch == 1)
		bmi_size += sizeof (RGBQUAD) * 256;
	bmi = g_malloc (bmi_size);
	bmh = (PBITMAPINFOHEADER) bmi;

	channel = ch == 1 ? 1 : 3;
	bmh->biSize = sizeof (BITMAPINFOHEADER);
	bmh->biWidth = w;
	bmh->biHeight = h;
	bmh->biPlanes = 1;
	bmh->biBitCount = channel * 8;
	bmh->biCompression = BI_RGB;
	bmh->biSizeImage = 0;
	bmh->biXPelsPerMeter = 0;
	bmh->biYPelsPerMeter = 0;
	bmh->biClrUsed = 0;
	bmh->biClrImportant = 0;

	dib_stride = (w * channel + 3) & ~3;
	dst = buf = g_malloc (h * dib_stride);

	if (channel == 1) {
		RGBQUAD *colors = bmi->bmiColors;

		for (i = 0; i < 256; ++i, colors++) {
			colors->rgbBlue = i;
			colors->rgbGreen = i;
			colors->rgbRed = i;
			colors->rgbReserved = 0;
		}
	}

	px += (h - 1) * rowstride;
	for (y = 0; y < h; ++y, px -= rowstride, dst += dib_stride) {
		if (channel > 1) {
			src_px = px;
			dst_px = dst;
			for (x = 0; x < w; ++x, src_px += ch, dst_px += channel) {
				dst_px[2] = src_px[0];
				dst_px[1] = src_px[1];
				dst_px[0] = src_px[2];
			}
		}
		else
			memcpy (dst, px, w);
	}

	GetWorldTransform (gdi->hDC, &xOldForm);
	xForm.eM11 = (FLOAT) (ctm[0] * POINT_PER_UNIT);
	xForm.eM12 = (FLOAT) (ctm[1] * POINT_PER_UNIT);
	xForm.eM21 = (FLOAT) (ctm[2] * POINT_PER_UNIT);
	xForm.eM22 = (FLOAT) (ctm[3] * POINT_PER_UNIT);
	xForm.eDx = (FLOAT) (ctm[4] * POINT_PER_UNIT);
	xForm.eDy = (FLOAT) (ctm[5] * POINT_PER_UNIT);
	ModifyWorldTransform (gdi->hDC, &xForm, MWT_LEFTMULTIPLY);

	ret = StretchDIBits (gdi->hDC,
			     0, 1, 1, -1,
			     0, 0, w, h,
			     buf, bmi, DIB_RGB_COLORS, SRCCOPY);

	SetWorldTransform (gdi->hDC, &xOldForm);

	g_free (buf);
	g_free (bmi);

	return ret != GDI_ERROR ? GNOME_PRINT_OK : GNOME_PRINT_ERROR_UNKNOWN;
}

static gint
gnome_print_gdi_set_font (GnomePrintGdi *gdi, const GnomeFont *gnome_font)
{
	gdouble font_size;
	gboolean italic;
	gint weight;
	const gchar *name, *ps_name, *family;
	HFONT hFont;

	if (gdi->selected_font == gnome_font)
		return GNOME_PRINT_OK;

	font_size = gnome_font_get_size (gnome_font);
	italic = gnome_font_is_italic (gnome_font);
	weight = gnome_font_get_weight_code (gnome_font);
	name = gnome_font_get_name (gnome_font);
	ps_name = gnome_font_get_ps_name (gnome_font);
	family = gnome_font_get_family_name (gnome_font);
#if 0
	g_debug ("size: %f; italic: %d; weight: %d; name: %s; ps_name: %s; family: %s;",
		 font_size, italic, weight, name, ps_name, family);
#endif

	hFont = CreateFont ((gint) (PT (-font_size)),
			    0, /* nWidth */
			    0, /* nEscapement */
			    0, /* nOrientation */
			    weight, /* fnWeight */
			    italic, /* fdwItalic */
			    0, /* fdwUnderline */
			    0, /* fdwStrikeOut */
			    DEFAULT_CHARSET, /* fdwCharSet */
			    OUT_TT_ONLY_PRECIS, /* fdwOutputPrecision */
			    CLIP_DEFAULT_PRECIS, /* fdwClipPrecision */
			    PROOF_QUALITY, /* fdwQuality */
			    DEFAULT_PITCH, /* fdwPitchAndFamily */
			    family); /* lpszFace */
	if (!hFont) {
		g_warning ("CreateFont() Failed: "
			   "size: %f; italic: %d; weight: %d; name: %s;"
 			   "ps_name: %s; family: %s;",
			   font_size, italic, weight, name, ps_name, family);
		return GNOME_PRINT_ERROR_UNKNOWN;
	}

	hFont = SelectObject (gdi->hDC, hFont);
	if (!gdi->hOldFont)
		gdi->hOldFont = hFont;
	else
		DeleteObject (hFont);

	gdi->selected_font = (GnomeFont *) gnome_font;

	return GNOME_PRINT_OK;
}

#ifndef ETO_PDY
#define ETO_PDY 0x2000
#endif

static gint
gnome_print_gdi_glyphlist (GnomePrintContext *pc, const gdouble *a, GnomeGlyphList *gl)
{
	GnomePrintGdi *gdi = GNOME_PRINT_GDI (pc);
	static const gdouble id[] = {1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
	GnomePosGlyphList *pgl;
	gboolean rotate_or_scale, is_nt;
	gdouble x, y, delta;
	gint old_bkmode, s, i, j, ret = 0;
	gint16 *glyphs = NULL;
	gint *deltas = NULL, glyphs_size = 0, deltas_size = 0;
	XFORM xOldForm, xForm;

	rotate_or_scale = ((fabs (a[0] - 1.0) > EPSILON) ||
			   (fabs (a[1]) > EPSILON) ||
			   (fabs (a[2]) > EPSILON) ||
			   (fabs (a[3] - 1.0) > EPSILON));

	if (rotate_or_scale) {
		GetWorldTransform (gdi->hDC, &xOldForm);
		xForm.eM11 = (FLOAT) a[0];
		xForm.eM12 = (FLOAT) a[1];
		xForm.eM21 = (FLOAT) a[2];
		xForm.eM22 = (FLOAT) a[3];
	}

	pgl = gnome_pgl_from_gl (gl, id, GNOME_PGL_RENDER_DEFAULT);
	is_nt = G_WIN32_IS_NT_BASED ();

	old_bkmode = SetBkMode (gdi->hDC, TRANSPARENT);
	for (s = 0; s < pgl->num_strings; s++) {
		GnomePosString *ps;
		
		ps = pgl->strings + s;

		if (gnome_print_gdi_set_font (
			gdi, gnome_rfont_get_font (ps->rfont))) {
			continue;
		}

		SetTextColor (gdi->hDC,
			      RGB_TO_COLORREF (ps->color));

		if (glyphs_size < ps->length)
			glyphs = g_realloc (glyphs,
				(glyphs_size = ps->length) * sizeof (gint16));
		for (i = 0; i < ps->length; ++i)
			glyphs[i] = pgl->glyphs[i + ps->start].glyph;

		if (deltas_size < ps->length - 1)
			deltas = g_realloc (deltas,
				(deltas_size = ps->length - 1)  * sizeof (gint) *
				(is_nt ? 2 : 1));
		for (i = 0, j = ps->start; i < ps->length - 1; ++i, ++j) {
			delta = pgl->glyphs[j + 1].x - pgl->glyphs[j].x;
			if (is_nt) {
				deltas[i << 1] = PT (delta);
				delta = pgl->glyphs[j + 1].y - pgl->glyphs[j].y;
				deltas[(i << 1) + 1] = PT (delta);
			}
			else
				deltas[i] = PT (delta);
		}

		x = a[4] + pgl->glyphs[ps->start].x;
		y = a[5] + pgl->glyphs[ps->start].y;
		if (rotate_or_scale) {
			xForm.eDx = (gfloat) (x * POINT_PER_UNIT);
			xForm.eDy = (gfloat) (y * POINT_PER_UNIT);
			ModifyWorldTransform (gdi->hDC, &xForm, MWT_LEFTMULTIPLY);
			x = y = 0;
		}
		ret = ExtTextOutW (gdi->hDC, PT (x), PT (y), ETO_GLYPH_INDEX |
				   (is_nt ? ETO_PDY : 0), NULL, glyphs,
				   ps->length, deltas);
		if (rotate_or_scale)
			SetWorldTransform (gdi->hDC, &xOldForm);

#if 0
		g_debug ("x:%lf y:%lf", x, y);
#endif
	}
	SetBkMode (gdi->hDC, old_bkmode);

	if (glyphs)
		g_free (glyphs);
	if (deltas)
		g_free (deltas);
	gnome_pgl_destroy (pgl);

	return ret ? GNOME_PRINT_OK : GNOME_PRINT_ERROR_UNKNOWN;
}

static gint
gnome_print_gdi_end_doc (GnomePrintContext *pc)
{
	GnomePrintGdi *gdi = GNOME_PRINT_GDI (pc);

	return GNOME_PRINT_OK;
}

static gint
gnome_print_gdi_beginpage (GnomePrintContext *pc, const guchar *name)
{
	GnomePrintGdi *gdi = GNOME_PRINT_GDI (pc);

	g_return_val_if_fail (gdi->hDC != NULL, GNOME_PRINT_ERROR_UNKNOWN);

	return StartPage (gdi->hDC) ? GNOME_PRINT_OK : GNOME_PRINT_ERROR_UNKNOWN;
}

static gint
gnome_print_gdi_showpage (GnomePrintContext *pc)
{
	GnomePrintGdi *gdi = GNOME_PRINT_GDI (pc);

	g_return_val_if_fail (gdi->hDC != NULL, GNOME_PRINT_ERROR_UNKNOWN);

	return EndPage (gdi->hDC) ? GNOME_PRINT_OK : GNOME_PRINT_ERROR_UNKNOWN;
}

static gint
gnome_print_gdi_close (GnomePrintContext *pc)
{
	GnomePrintGdi *gdi = GNOME_PRINT_GDI (pc);

	g_return_val_if_fail (gdi->hDC != NULL, GNOME_PRINT_ERROR_UNKNOWN);

	EndDoc (gdi->hDC);

	if (gdi->hOldFont)
		DeleteObject (SelectObject (gdi->hDC, gdi->hOldFont));

	if (gdi->hPen)
		DeleteObject (SelectObject (gdi->hDC, gdi->hOldPen));

	if (gdi->hBrush)
		DeleteObject (SelectObject (gdi->hDC, gdi->hOldBrush));

	DeleteDC (gdi->hDC);

	return GNOME_PRINT_OK;
}

GnomePrintContext *
gnome_print_gdi_new (GnomePrintConfig *config)
{
	GnomePrintContext *ctx;
	gint ret;

	g_return_val_if_fail (config != NULL, NULL);

	ctx = g_object_new (GNOME_TYPE_PRINT_GDI, NULL);

	ret = gnome_print_context_construct (ctx, config);

	if (ret != GNOME_PRINT_OK) {
		g_object_unref (G_OBJECT (ctx));
		ctx = NULL;
	}

	return ctx;
}
