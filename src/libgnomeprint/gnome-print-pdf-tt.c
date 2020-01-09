/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  gnome-print-pdf-tt.c: TrueType Font embeding for gnome-print-pdf
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
 *
 *  Copyright 2000-2001 Ximian, Inc.
 *
 *  References:
 *    [1] Portable Document Format Referece Manual, Version 1.3 (March 11, 1999)
 */

#include <config.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <libgnomeprint/gnome-print.h>
#include <libgnomeprint/gnome-print-private.h>
#include <libgnomeprint/gnome-font-private.h>
#include <libgnomeprint/gnome-print-pdf-private.h>
#include <libgnomeprint/ttsubset/gnome-print-tt-subset.h>

#define PSO_GLYPH_MARKED(pso,g) (pso->glyphs[(g) >> 5] & (1 << ((g) & 0x1f)))

static gint
gnome_print_pdf_ttsubset_low_glyph_bound (guchar *subsetfontname)
{
	gint len, lower;

	len = subsetfontname ? strlen (subsetfontname) : 0;

	if (len > 4)
		lower = *(subsetfontname + len - 4) == '_' ? atoi (subsetfontname + len - 3) : 0;
	else
		lower = 0;

	return (lower*255);
}

gint
gnome_print_pdf_tt_subset_embed (GnomePrintPdf *pdf,
			  GnomePrintPdfFont *font,
			  const guchar *file_name,
			  gint *object_number_ret) {
	GnomePrintBuffer b;
	GnomePrintReturnCode retval = GNOME_PRINT_ERROR_UNKNOWN;
	GnomeFontPsObject *pso = font->pso;
	gint i, j, k, lower, upper, nglyphs, code_glyph;
	guchar *subfont_file = NULL;
	gushort glyphArray[256];
        guchar encoding[256];

	nglyphs = pso->face->num_glyphs;

	lower = gnome_print_pdf_ttsubset_low_glyph_bound (pso->encodedname);
	upper = lower + 255;

	k = 1;
	font->code_to_glyph [0] = 0;
	glyphArray[0] = encoding[0] = 0;

	for ( j = lower; j < upper && j < nglyphs; j++) {
		if (PSO_GLYPH_MARKED(pso, j)) {
			code_glyph = j % 255 + 1;
			glyphArray [k] = j;
			font->code_to_glyph [code_glyph] = j;
			encoding [k] = code_glyph;
			k++;
		}
	}

	for (i = 1; i <= encoding [k-1]; i++)
		if (font->code_to_glyph [i] == -1)
			font->code_to_glyph [i] = 0;

	font->code_assigned = encoding [k-1];

	(void) gnome_print_pdf_tt_create_subfont  (file_name, &subfont_file, glyphArray, encoding, k);

	if (gnome_print_buffer_mmap (&b, subfont_file))
		goto pdf_truetype_error;

	if (b.buf_size < 8)
		goto pdf_truetype_error;

	*object_number_ret = gnome_print_pdf_object_new (pdf);
	
	/* Write the object */
	gnome_print_pdf_object_start (pdf, *object_number_ret, FALSE);
	gnome_print_pdf_fprintf    (pdf,
				    "/Length %d" EOL
				    "/Length1 %d" EOL
				    ">>" EOL
				    "stream" EOL,
				    b.buf_size + strlen (EOL),
				    b.buf_size);
	gnome_print_pdf_print_sized (pdf, b.buf, b.buf_size);
	gnome_print_pdf_fprintf (pdf, EOL);
	gnome_print_pdf_fprintf (pdf,
				 "endstream" EOL
				 "endobj" EOL);
	gnome_print_pdf_object_end  (pdf, *object_number_ret, TRUE);

	retval = GNOME_PRINT_OK;

pdf_truetype_error:
	if (b.buf)
		gnome_print_buffer_munmap (&b);
	if (retval != GNOME_PRINT_OK)
		g_warning ("Could not parse TrueType font from %s\n", subfont_file);
	if (subfont_file) {
		unlink (subfont_file);
		g_free (subfont_file);
	}

	return retval;
}	

void
gnome_print_pdf_font_tounicode (GnomePrintPdf *pdf, GnomePrintPdfFont *font,
				        gint *glyph2unicode,
					gint *object_number_descriptor_ret)
{
	GnomeFontPsObject *pso = font->pso;
	gint length_object_num, object_number_descriptor;
	gint nglyphs, len = 0, lower, upper, mapped_nos = 0;
	gint i, j, k, uni;
	gchar buf[48];

	nglyphs = pso->face->num_glyphs;
	memset(buf, 0, sizeof(buf)/sizeof(buf[0]));

	object_number_descriptor = gnome_print_pdf_object_new (pdf);
	*object_number_descriptor_ret = object_number_descriptor;

	gnome_print_pdf_object_start (pdf, object_number_descriptor, TRUE);
	gnome_print_pdf_fprintf (pdf,
				 "%d 0 obj" EOL,
				 object_number_descriptor);

        length_object_num = gnome_print_pdf_object_new (pdf);
        gnome_print_pdf_fprintf (pdf, "<<" EOL);
        gnome_print_pdf_fprintf (pdf, "/Length %d 0 R" EOL, length_object_num);
        gnome_print_pdf_fprintf (pdf, ">>" EOL);
        gnome_print_pdf_fprintf (pdf, "stream" EOL);

	len += gnome_print_pdf_fprintf (pdf,
			     "/CIDInit /ProcSet findresource begin" EOL
			     "12 dict begin" EOL
			     "begincmap" EOL
			     "/CIDSystemInfo" EOL
			     "<< /Registry (Adobe)" EOL
			     "/Ordering (UCS)" EOL
			     "/Supplement 0" EOL
			     ">> def" EOL
			     "/CMapName /Adobe-Identity-UCS def" EOL
			     "/CMapType 2 def" EOL
			     "1 begincodespacerange" EOL
			     "<00> <FFFF>" EOL
			     "endcodespacerange" EOL
			     );

	lower = gnome_print_pdf_ttsubset_low_glyph_bound (pso->encodedname);
	upper = lower + 255;

	for ( j = lower; j < upper && j < nglyphs; j++) {
		if (PSO_GLYPH_MARKED(pso, j)) {
			mapped_nos++;
		}
	}

	i = k = 0;

	for ( j = lower; j < upper && j < nglyphs; j++) {
		if (PSO_GLYPH_MARKED(pso, j)) {
			if ((i % 100) == 0) {
				if (i)
					len += gnome_print_pdf_fprintf (pdf,
						"endbfchar" EOL
						);
				len += gnome_print_pdf_fprintf (pdf,"%d", (mapped_nos-i > 100) ? 100 : mapped_nos-i);
				len += gnome_print_pdf_fprintf (pdf," beginbfchar" EOL);
			}

			uni = glyph2unicode[j];
			if (uni) {
				sprintf(buf,  "<%02x>  <%02x%02x>", k+1, uni/256, uni&255);
			} else {
				sprintf(buf,  "<%02x>  <fffd>", k+1);
			}
			len += gnome_print_pdf_fprintf (pdf, "%s" EOL, buf);
			i++;
		}
		k++;
	}
	len += gnome_print_pdf_fprintf (pdf,
    			"endbfchar" EOL
                      	"endcmap" EOL
                      	"CMapName currentdict /CMap defineresource pop" EOL
                      	"end" EOL
                      	"end" EOL 
			);


	gnome_print_pdf_fprintf (pdf,  "endstream" EOL);
	gnome_print_pdf_object_end   (pdf, object_number_descriptor, TRUE);
	gnome_print_pdf_fprintf (pdf,  "endobj" EOL);

	gnome_print_pdf_object_start (pdf, length_object_num, TRUE);
	gnome_print_pdf_fprintf (pdf,
				 "%d 0 obj" EOL
				 "%d" EOL
				 "endobj" EOL,
				 length_object_num, len);
	gnome_print_pdf_object_end (pdf, length_object_num, TRUE);
}
