/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  gnome-print-win32.c: Win32 backend
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
 *    Ivan, Wong Yat Cheung <email@ivanwong.info>
 *
 *  Copyright (C) 2005 Novell Inc. and authors
 *
 */

#include <config.h>
#include <glib.h>
#include <gmodule.h>
#include <locale.h>

#include <libgnomeprint/gnome-print-module.h>
#include <libgnomeprint/gpa/gpa-model.h>
#include <libgnomeprint/gpa/gpa-printer.h>
#include <libgnomeprint/gpa/gpa-option.h>
#include <libgnomeprint/gpa/gpa-settings.h>
#include <libgnomeprint/gpa/gpa-state.h>
#include <libgnomeprint/gpa/gpa-utils.h>
#include <windows.h>

#include "libgnomeprint/gnome-print-i18n.h"

static 	GModule *handle = NULL;
/* Argument order: id, name */

static const xmlChar *model_xml_template =
"<?xml version=\"1.0\"?>"
"<Model Id=\"%s\" Version=\"1.0\">"
"  <Name>%s</Name>"
"  <ModelVersion>0.0.1</ModelVersion>"
"  <Options>"
"    <Option Id=\"Engine\">"
"      <Option Id=\"Backend\" Type=\"List\" Default=\"gnome\">"
"        <Item Id=\"gnome\">"
"          <Name>GNOME</Name>"
"          <Key Id=\"Driver\" Value=\"gnome-print-gdi\"/>"
"        </Item>"
"      </Option>"
"    </Option>"
"    <Option Id=\"Transport\">"
"      <Option Id=\"Backend\" Type=\"List\" Default=\"win32\">"
"        <Item Id=\"win32\">"
"          <Name>Win32</Name>"
"          <Key Id=\"Module\" Value=\"libgnomeprintwin32.dll\"/>"
"        </Item>"
"        <Item Id=\"file\">"
"          <Name>File</Name>"
"          <Key Id=\"Module\" Value=\"libgnomeprintwin32.dll\"/>"
"          <Option Id=\"FileName\" Type=\"String\" Default=\"output.raw\"/>"
"        </Item>"
"      </Option>"
"    </Option>"
"    <Option Id=\"Output\">"
"      <Option Id=\"Media\">"
"        <Option Id=\"PhysicalOrientation\" Type=\"List\" Default=\"R0\">"
"          <Fill Ref=\"Globals.Media.PhysicalOrientation\"/>"
"        </Option>"
"        <Key Id=\"Margins\">"
"          <Key Id=\"Left\" Value=\"0\"/>"
"          <Key Id=\"Right\" Value=\"0\"/>"
"          <Key Id=\"Top\" Value=\"0\"/>"
"          <Key Id=\"Bottom\" Value=\"0\"/>"
"        </Key>"
"      </Option>"
"      <Option Id=\"Job\">"
"        <Option Id=\"NumCopies\" Type=\"String\" Default=\"1\"/>"
"        <Option Id=\"NonCollatedCopiesHW\" Type=\"String\" Default=\"true\"/>"
"        <Option Id=\"CollatedCopiesHW\" Type=\"String\" Default=\"false\"/>"
"        <Option Id=\"Collate\" Type=\"String\" Default=\"false\"/>"
"        <Option Id=\"PrintToFile\" Type=\"String\" Default=\"false\"/>"
"        <Option Id=\"FileName\" Type=\"String\" Default=\"output.raw\"/>"
"      </Option>"
"    </Option>"
#if 0
"    <Option Id=\"Icon\">"
"      <Option Id=\"Filename\" Type=\"String\" Default=\"" DATADIR "/pixmaps/nautilus/default/i-printer.png\"/>"
"    </Option>"
#endif
"  </Options>"
"</Model>";

G_MODULE_EXPORT gboolean gpa_module_init (GpaModuleInfo *info);
G_MODULE_EXPORT void gpa_module_polling (GPAPrinter *printer, gboolean polling);

#define WIN32_MAX_PAPER_NAME_LEN 64
#define WIN32_MAX_BIN_NAME_LEN 24

static const gchar *paper_nicks[] = {
	"000_INVALID",
	"001_LETTER",  /* Letter 8 1/2 x 11 in               */
	"002_LETTERSMALL",  /* Letter Small 8 1/2 x 11 in         */
	"003_TABLOID",  /* Tabloid 11 x 17 in                 */
	"004_LEDGER",  /* Ledger 17 x 11 in                  */
	"005_LEGAL",  /* Legal 8 1/2 x 14 in                */
	"006_STATEMENT",  /* Statement 5 1/2 x 8 1/2 in         */
	"007_EXECUTIVE",  /* Executive 7 1/4 x 10 1/2 in        */
	"008_A3",  /* A3 297 x 420 mm                    */
	"009_A4",  /* A4 210 x 297 mm                    */
	"010_A4SMALL",  /* A4 Small 210 x 297 mm              */
	"011_A5",  /* A5 148 x 210 mm                    */
	"012_B4",  /* B4 (JIS) 250 x 354                 */
	"013_B5",  /* B5 (JIS) 182 x 257 mm              */
	"014_FOLIO",  /* Folio 8 1/2 x 13 in                */
	"015_QUARTO",  /* Quarto 215 x 275 mm                */
	"016_10X14",  /* 10x14 in                           */
	"017_11X17",  /* 11x17 in                           */
	"018_NOTE",  /* Note 8 1/2 x 11 in                 */
	"019_ENV_9",  /* Envelope #9 3 7/8 x 8 7/8          */
	"020_ENV_10",  /* Envelope #10 4 1/8 x 9 1/2         */
	"021_ENV_11",  /* Envelope #11 4 1/2 x 10 3/8        */
	"022_ENV_12",  /* Envelope #12 4 \276 x 11           */
	"023_ENV_14",  /* Envelope #14 5 x 11 1/2            */
	"024_CSHEET",  /* C size sheet                       */
	"025_DSHEET",  /* D size sheet                       */
	"026_ESHEET",  /* E size sheet                       */
	"027_ENV_DL",  /* Envelope DL 110 x 220mm            */
	"028_ENV_C5",  /* Envelope C5 162 x 229 mm           */
	"029_ENV_C3",  /* Envelope C3  324 x 458 mm          */
	"030_ENV_C4",  /* Envelope C4  229 x 324 mm          */
	"031_ENV_C6",  /* Envelope C6  114 x 162 mm          */
	"032_ENV_C65",  /* Envelope C65 114 x 229 mm          */
	"033_ENV_B4",  /* Envelope B4  250 x 353 mm          */
	"034_ENV_B5",  /* Envelope B5  176 x 250 mm          */
	"035_ENV_B6",  /* Envelope B6  176 x 125 mm          */
	"036_ENV_ITALY",  /* Envelope 110 x 230 mm              */
	"037_ENV_MONARCH",  /* Envelope Monarch 3.875 x 7.5 in    */
	"038_ENV_PERSONAL",  /* 6 3/4 Envelope 3 5/8 x 6 1/2 in    */
	"039_FANFOLD_US",  /* US Std Fanfold 14 7/8 x 11 in      */
	"040_FANFOLD_STD_GERMAN",  /* German Std Fanfold 8 1/2 x 12 in   */
	"041_FANFOLD_LGL_GERMAN",  /* German Legal Fanfold 8 1/2 x 13 in */
	"042_ISO_B4",  /* B4 (ISO) 250 x 353 mm              */
	"043_JAPANESE_POSTCARD",  /* Japanese Postcard 100 x 148 mm     */
	"044_9X11",  /* 9 x 11 in                          */
	"045_10X11",  /* 10 x 11 in                         */
	"046_15X11",  /* 15 x 11 in                         */
	"047_ENV_INVITE",  /* Envelope Invite 220 x 220 mm       */
	"048_RESERVED_48",  /* RESERVED--DO NOT USE               */
	"049_RESERVED_49",  /* RESERVED--DO NOT USE               */
	"050_LETTER_EXTRA",  /* Letter Extra 9 \275 x 12 in        */
	"051_LEGAL_EXTRA",  /* Legal Extra 9 \275 x 15 in         */
	"052_TABLOID_EXTRA",  /* Tabloid Extra 11.69 x 18 in        */
	"053_A4_EXTRA",  /* A4 Extra 9.27 x 12.69 in           */
	"054_LETTER_TRANSVERSE",  /* Letter Transverse 8 \275 x 11 in   */
	"055_A4_TRANSVERSE",  /* A4 Transverse 210 x 297 mm         */
	"056_LETTER_EXTRA_TRANSVERSE", /* Letter Extra Transverse 9\275 x 12 in */
	"057_A_PLUS",  /* SuperA/SuperA/A4 227 x 356 mm      */
	"058_B_PLUS",  /* SuperB/SuperB/A3 305 x 487 mm      */
	"059_LETTER_PLUS",  /* Letter Plus 8.5 x 12.69 in         */
	"060_A4_PLUS",  /* A4 Plus 210 x 330 mm               */
	"061_A5_TRANSVERSE",  /* A5 Transverse 148 x 210 mm         */
	"062_B5_TRANSVERSE",  /* B5 (JIS) Transverse 182 x 257 mm   */
	"063_A3_EXTRA",  /* A3 Extra 322 x 445 mm              */
	"064_A5_EXTRA",  /* A5 Extra 174 x 235 mm              */
	"065_B5_EXTRA",  /* B5 (ISO) Extra 201 x 276 mm        */
	"066_A2",  /* A2 420 x 594 mm                    */
	"067_A3_TRANSVERSE",  /* A3 Transverse 297 x 420 mm         */
	"068_A3_EXTRA_TRANSVERSE",  /* A3 Extra Transverse 322 x 445 mm   */
	"069_DBL_JAPANESE_POSTCARD", /* Japanese Double Postcard 200 x 148 mm */
	"070_A6",  /* A6 105 x 148 mm                 */
	"071_JENV_KAKU2",  /* Japanese Envelope Kaku #2       */
	"072_JENV_KAKU3",  /* Japanese Envelope Kaku #3       */
	"073_JENV_CHOU3",  /* Japanese Envelope Chou #3       */
	"074_JENV_CHOU4",  /* Japanese Envelope Chou #4       */
	"075_LETTER_ROTATED",  /* Letter Rotated 11 x 8 1/2 11 in */
	"076_A3_ROTATED",  /* A3 Rotated 420 x 297 mm         */
	"077_A4_ROTATED",  /* A4 Rotated 297 x 210 mm         */
	"078_A5_ROTATED",  /* A5 Rotated 210 x 148 mm         */
	"079_B4_JIS_ROTATED",  /* B4 (JIS) Rotated 364 x 257 mm   */
	"080_B5_JIS_ROTATED",  /* B5 (JIS) Rotated 257 x 182 mm   */
	"081_JAPANESE_POSTCARD_ROTATED", /* Japanese Postcard Rotated 148 x 100 mm */
	"082_DBL_JAPANESE_POSTCARD_ROTATED", /* Double Japanese Postcard Rotated 148 x 200 mm */
	"083_A6_ROTATED",  /* A6 Rotated 148 x 105 mm         */
	"084_JENV_KAKU2_ROTATED",  /* Japanese Envelope Kaku #2 Rotated */
	"085_JENV_KAKU3_ROTATED",  /* Japanese Envelope Kaku #3 Rotated */
	"086_JENV_CHOU3_ROTATED",  /* Japanese Envelope Chou #3 Rotated */
	"087_JENV_CHOU4_ROTATED",  /* Japanese Envelope Chou #4 Rotated */
	"088_B6_JIS",  /* B6 (JIS) 128 x 182 mm           */
	"089_B6_JIS_ROTATED",  /* B6 (JIS) Rotated 182 x 128 mm   */
	"090_12X11",  /* 12 x 11 in                      */
	"091_JENV_YOU4",  /* Japanese Envelope You #4        */
	"092_JENV_YOU4_ROTATED",  /* Japanese Envelope You #4 Rotated*/
	"093_P16K",  /* PRC 16K 146 x 215 mm            */
	"094_P32K",  /* PRC 32K 97 x 151 mm             */
	"095_P32KBIG",  /* PRC 32K(Big) 97 x 151 mm        */
	"096_PENV_1",  /* PRC Envelope #1 102 x 165 mm    */
	"097_PENV_2",  /* PRC Envelope #2 102 x 176 mm    */
	"098_PENV_3",  /* PRC Envelope #3 125 x 176 mm    */
	"099_PENV_4",  /* PRC Envelope #4 110 x 208 mm    */
	"100_PENV_5", /* PRC Envelope #5 110 x 220 mm    */
	"101_PENV_6", /* PRC Envelope #6 120 x 230 mm    */
	"102_PENV_7", /* PRC Envelope #7 160 x 230 mm    */
	"103_PENV_8", /* PRC Envelope #8 120 x 309 mm    */
	"104_PENV_9", /* PRC Envelope #9 229 x 324 mm    */
	"105_PENV_10", /* PRC Envelope #10 324 x 458 mm   */
	"106_P16K_ROTATED", /* PRC 16K Rotated                 */
	"107_P32K_ROTATED", /* PRC 32K Rotated                 */
	"108_P32KBIG_ROTATED", /* PRC 32K(Big) Rotated            */
	"109_PENV_1_ROTATED", /* PRC Envelope #1 Rotated 165 x 102 mm */
	"110_PENV_2_ROTATED", /* PRC Envelope #2 Rotated 176 x 102 mm */
	"111_PENV_3_ROTATED", /* PRC Envelope #3 Rotated 176 x 125 mm */
	"112_PENV_4_ROTATED", /* PRC Envelope #4 Rotated 208 x 110 mm */
	"113_PENV_5_ROTATED", /* PRC Envelope #5 Rotated 220 x 110 mm */
	"114_PENV_6_ROTATED", /* PRC Envelope #6 Rotated 230 x 120 mm */
	"115_PENV_7_ROTATED", /* PRC Envelope #7 Rotated 230 x 160 mm */
	"116_PENV_8_ROTATED", /* PRC Envelope #8 Rotated 309 x 120 mm */
	"117_PENV_9_ROTATED", /* PRC Envelope #9 Rotated 324 x 229 mm */
	"118_PENV_10_ROTATED", /* PRC Envelope #10 Rotated 458 x 324 mm */
};

static const paper_nicks_count = sizeof (paper_nicks) / sizeof (paper_nicks[0]);

static const gchar *bin_nicks[] = {
	"000_INVALID",
	"001_UPPER",
	"002_LOWER",
	"003_MIDDLE",
	"004_MANUAL",
	"005_ENVELOPE",
	"006_ENVMANUAL",
	"007_AUTO",
	"008_TRACTOR",
	"009_SMALLFMT",
	"010_LARGEFMT",
	"011_LARGECAPACITY",
	"014_CASSETTE",
	"015_FORMSOURCE"
};

static const bin_nicks_count = sizeof (bin_nicks) / sizeof (bin_nicks[0]);

static gchar *
get_paper_nick (WORD paper)
{
	return	paper > 0 && paper < paper_nicks_count ?
		g_strdup (paper_nicks[paper]) :
		g_strdup_printf ("%03d_USER", paper);
}

static gchar *
get_bin_nick (WORD bin)
{
	return	bin > 0 && bin < bin_nicks_count ?
		g_strdup (bin_nicks[bin]) :
		g_strdup_printf ("%03d_USER", bin);
}

static void
load_paper_sizes (PRINTER_INFO_5 *info, DEVMODE *pDevmode, GPANode *parent)
{
	GPANode *node, *size;
	DWORD paper_cnt, i;
	LPTSTR paper_names;
	gchar *paper_name, *paper_nick;
	gchar paper_buf[WIN32_MAX_PAPER_NAME_LEN + 1];
	gchar fp_buf[G_ASCII_DTOSTR_BUF_SIZE];
	POINT *paper_sizes;
	WORD *papers;

	paper_nick = get_paper_nick(pDevmode->dmPaperSize);
	node = gpa_option_list_new (parent, "PhysicalSize", paper_nick);
	g_free (paper_nick);
	if (!node)
		return;

	paper_cnt = DeviceCapabilities (info->pPrinterName, info->pPortName,
					DC_PAPERNAMES, NULL, NULL);
	if (paper_cnt <= 0)
		return;

	paper_names = g_malloc (paper_cnt * sizeof(gchar) * WIN32_MAX_PAPER_NAME_LEN);
	paper_sizes = g_malloc (paper_cnt * sizeof(POINT));
	papers = g_malloc (paper_cnt * sizeof(WORD));
	if (DeviceCapabilities (info->pPrinterName, info->pPortName,
				DC_PAPERNAMES, paper_names, NULL) != paper_cnt)
		g_warning ("Expected %d DC_PAPERNAMES", paper_cnt);
	if (DeviceCapabilities (info->pPrinterName, info->pPortName,
				DC_PAPERS, (LPTSTR) papers, NULL) != paper_cnt)
		g_warning ("Expected %d DC_PAPERS", paper_cnt);
	if (DeviceCapabilities (info->pPrinterName, info->pPortName,
				DC_PAPERSIZE, (LPTSTR) paper_sizes, NULL)
	    != paper_cnt)
		g_warning ("Expected %d DC_PAPERS", paper_cnt);

	paper_buf[WIN32_MAX_PAPER_NAME_LEN] = '\0';
	for (i = 0; i < paper_cnt; ++i) {
		gchar *height;
		gchar *width;

		strncpy (paper_buf, paper_names + i * WIN32_MAX_PAPER_NAME_LEN,
			 WIN32_MAX_PAPER_NAME_LEN);
		paper_name = g_locale_to_utf8 (paper_buf, -1, NULL, NULL, NULL);
		paper_nick = get_paper_nick (papers[i]);
		size = gpa_option_item_new (node,
					    paper_nick,
					    paper_name);
		g_free (paper_nick);
		g_free (paper_name);

		width  = g_strdup_printf ("%smm",
					  g_ascii_dtostr (fp_buf,
							  sizeof (fp_buf),
							  (gfloat) paper_sizes[i].x * 0.1f));
		height  = g_strdup_printf ("%smm",
					   g_ascii_dtostr (fp_buf,
							   sizeof (fp_buf),
							   (gfloat) paper_sizes[i].y * 0.1f));

		gpa_option_key_new (size, "Width",  width);
		gpa_option_key_new (size, "Height", height);

		g_free (width);
		g_free (height);
	}
	size = gpa_option_item_new (node, "Custom", _("Custom"));
	gpa_option_string_new (size, "Width", "210mm");
	gpa_option_string_new (size, "Height", "297mm");

	gpa_node_reverse_children (node);

	g_free (paper_names);
	g_free (paper_sizes);
	g_free (papers);
}

static void
load_paper_sources (PRINTER_INFO_5 *info, DEVMODE *pDevmode, GPANode *parent)
{
	GPANode *node;
	DWORD bin_cnt, i;
	LPTSTR bin_names;
	gchar *bin_name, *bin_nick;
	gchar  bin_buf[WIN32_MAX_PAPER_NAME_LEN + 1];
	WORD *bins;

	bin_nick = get_bin_nick(pDevmode->dmDefaultSource);
	node = gpa_option_list_new (parent, "PaperSource", bin_nick);
	g_free (bin_nick);
	if (!node)
		return;

	bin_cnt = DeviceCapabilities (info->pPrinterName, info->pPortName,
				      DC_BINNAMES, NULL, NULL);
	if (bin_cnt <= 0)
		return;

	bin_names = g_malloc (bin_cnt * sizeof(gchar) * WIN32_MAX_BIN_NAME_LEN);
	bins = g_malloc (bin_cnt * sizeof(WORD));
	if (DeviceCapabilities (info->pPrinterName, info->pPortName,
				DC_BINNAMES, bin_names, NULL) != bin_cnt)
		g_warning ("Expected %d DC_BINNAMES", bin_cnt);
	if (DeviceCapabilities (info->pPrinterName, info->pPortName,
				DC_BINS, (LPTSTR) bins, NULL) != bin_cnt)
		g_warning ("Expected %d DC_BINS", bin_cnt);

	bin_buf[WIN32_MAX_BIN_NAME_LEN] = '\0';
	for (i = 0; i < bin_cnt; ++i) {
		GPANode *bin;

		strncpy (bin_buf, bin_names + i * WIN32_MAX_BIN_NAME_LEN,
			 WIN32_MAX_BIN_NAME_LEN);
		bin_name = g_locale_to_utf8 (bin_buf, -1, NULL, NULL, NULL);
		bin_nick = get_bin_nick (bins[i]);
		bin = gpa_option_item_new (node,
					   bin_nick,
					   bin_name);
		g_free (bin_nick);
		g_free (bin_name);
	}

	gpa_node_reverse_children (node);

	g_free (bin_names);
	g_free (bins);
}

static void
load_other_settings (PRINTER_INFO_5 *info, DEVMODE *pDevmode, GPANode *model)
{
	GPANode *job;

	job = gpa_node_lookup (model, "Options.Output.Job");

	if (DeviceCapabilities (info->pPrinterName, info->pPortName,
				DC_DUPLEX, NULL, NULL) > 0) {
		gpa_option_string_new (job, "Duplex",
				       pDevmode->dmDuplex == DMDUP_SIMPLEX ? "false" : "true");
		gpa_option_string_new (job, "Tumble",
				       pDevmode->dmDuplex == DMDUP_HORIZONTAL ? "true" : "false");
	}

	gpa_node_unref (job);
}

static void
adjust_settings (DEVMODE *pDevmode, GPANode *settings)
{
	GPANode *node;

	if ((node = gpa_node_lookup (settings, "Output.Job.Duplex")) != NULL) {
		gpa_node_set_value (node,
				    pDevmode->dmDuplex == DMDUP_SIMPLEX ? "false" : "true");
		gpa_node_set_path_value (settings, "Output.Job.Tumble",
					 pDevmode->dmDuplex == DMDUP_HORIZONTAL ? "true" : "false");
		gpa_node_unref (node);
	}
}

static GPAModel *
get_model (PRINTER_INFO_5 *info, DEVMODE *pDevmode)
{
	GPANode *media;
	GPANode *model;
	GPANode *output;
	gchar *xml;
	gchar *id;
	gchar *pname;

	pname = g_locale_to_utf8 (info->pPrinterName, -1, NULL, NULL, NULL);
	id = g_strdup_printf ("Win32-%s", pname);
	xml = g_strdup_printf ((char *)model_xml_template, id, pname);
	model = gpa_model_new_from_xml_str (xml);
	g_free (xml);
	g_free (id);
	g_free (pname);

	output = gpa_node_lookup (model, "Options.Output");
	media  = gpa_node_lookup (model, "Options.Output.Media");

	load_paper_sizes (info, pDevmode, media);
	load_paper_sources (info, pDevmode, output);
	load_other_settings (info, pDevmode, model);

	gpa_node_unref (output);
	gpa_node_unref (media);

	return GPA_MODEL (model);
}

static gboolean
append_printer (GPAList *printers_list, PRINTER_INFO_5 *info, const gchar *def_printer, const gchar *module_path)
{
	DEVMODE *pDevmode;
	LONG devmode_len;
	HANDLE hPrinter;
	GPANode *settings = NULL;
	GPANode *printer = NULL;
	GPAModel *model = NULL;
	gboolean retval = FALSE;
	gchar *pname;

	if (!OpenPrinter (info->pPrinterName, &hPrinter, NULL))
		return FALSE;

	devmode_len = DocumentProperties (NULL, hPrinter,
					  info->pPrinterName, NULL, NULL, 0);
	if (devmode_len > 0) {
		pDevmode = g_malloc (devmode_len);
		devmode_len = DocumentProperties (NULL, hPrinter, info->pPrinterName,
						  pDevmode, NULL, DM_OUT_BUFFER);
	}
	ClosePrinter (hPrinter);

	if (devmode_len < 0)
		return FALSE;

	model = get_model (info, pDevmode);
	if (model == NULL) {
		g_warning ("model creation failed\n");
		goto append_printer_exit;
	}

	settings = gpa_settings_new (model, (guchar *)"Default",
		(guchar *)"SetIdFromWin32");

	if (settings == NULL) {
		g_warning ("settings creation failed\n");
		goto append_printer_exit;
	}

	pname = g_locale_to_utf8 (info->pPrinterName, -1, NULL, NULL, NULL);
	printer = gpa_printer_new_stub (info->pPrinterName, pname, module_path);
	if (printer == NULL) {
		g_warning ("gpa_printer_new failed\n");
		goto append_printer_exit;
	}

	if (gpa_node_verify (printer)) {
		adjust_settings (pDevmode, settings);
		gpa_printer_complete_stub (GPA_PRINTER (printer),
					   model,
					   GPA_SETTINGS (settings));
		gpa_list_prepend (printers_list, printer);
		if (!strcmp (def_printer, info->pPrinterName))
			gpa_list_set_default (printers_list, printer);
		retval = TRUE;
	} else {
		g_warning ("gpa_node_verify failed\n");
	}

append_printer_exit:
	if (retval == FALSE) {
		g_warning ("The printer %s could not be created\n", pname);

		my_gpa_node_unref (printer);
		my_gpa_node_unref (GPA_NODE (model));
		my_gpa_node_unref (settings);
	}
	g_free (pname);

	return retval;
}

static void
gnome_print_win32_printer_list_append (gpointer printers_list,
				       const gchar *module_path)
{
	PRINTER_INFO_5 *info5_arr = NULL;
	DWORD buf_size = 0, printer_returned;
	guint i;
	gchar def_printer[256], *comma;
#if 0
	PRINTER_INFO_2 *info2_arr = NULL;
	DATATYPES_INFO_1 *datatype_arr = NULL;
#endif

	g_return_if_fail (printers_list != NULL);
	g_return_if_fail (GPA_IS_LIST (printers_list));

	if (GetProfileString ("Windows", "Device", ",,,", def_printer, 256) > 0) {
		if ((comma = strchr (def_printer, ',')) != NULL)
			*comma = '\0';
	}
	else
		def_printer[0] = '\0';

	while (!EnumPrinters (PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS, NULL, 5, (LPBYTE) info5_arr,
			      buf_size, &buf_size, &printer_returned))
		info5_arr = g_realloc (info5_arr, buf_size);

	for (i = 0; i < printer_returned; ++i)
		append_printer (GPA_LIST (printers_list), info5_arr + i, def_printer, module_path);

#if 0
	while (!EnumPrinters (PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS, NULL, 2, (LPBYTE) info2_arr,
			      buf_size, &buf_size, &printer_returned))
		info2_arr = g_realloc (info2_arr, buf_size);

	while (!EnumPrintProcessorDatatypes (NULL, "WinPrint", 1, (LPBYTE) datatype_arr, buf_size, &buf_size, &printer_returned))
		datatype_arr = g_realloc (datatype_arr, buf_size);

	g_free (info2_arr);
	g_free (datatype_arr);
#endif

	g_free (info5_arr);
}

static gchar *
get_status_str (gint status)
{
	gchar *real_str;
	GString *status_str;
	static const gchar sep[] = ", %s";

	if (!status)
		return g_strdup (_("Ready"));

#define append_status(x) { g_string_append_len (status_str, ", ", 2); g_string_append (status_str, x); }

	status_str = g_string_new ("");
	if (status & PRINTER_STATUS_PAUSED) append_status (_("Paused"));
	if (status & PRINTER_STATUS_ERROR) append_status (_("Error"));
	if (status & PRINTER_STATUS_PENDING_DELETION) append_status (_("Pending Deletion"));
	if (status & PRINTER_STATUS_PAPER_JAM) append_status (_("Paper Jam"));
	if (status & PRINTER_STATUS_PAPER_OUT) append_status (_("Paper Out"));
	if (status & PRINTER_STATUS_MANUAL_FEED) append_status (_("Manual Feed"));
	if (status & PRINTER_STATUS_PAPER_PROBLEM) append_status (_("Paper Problem"));
	if (status & PRINTER_STATUS_OFFLINE) append_status (_("Offline"));
	if (status & PRINTER_STATUS_IO_ACTIVE) append_status (_("IO Active"));
	if (status & PRINTER_STATUS_BUSY) append_status (_("Busy"));
	if (status & PRINTER_STATUS_PRINTING) append_status (_("Printing"));
	if (status & PRINTER_STATUS_OUTPUT_BIN_FULL) g_string_append_printf (status_str, "Output Bin Full");
	if (status & PRINTER_STATUS_NOT_AVAILABLE) append_status (_("Not Available"));
	if (status & PRINTER_STATUS_WAITING) append_status (_("Waiting"));
	if (status & PRINTER_STATUS_PROCESSING) append_status (_("Processing"));
	if (status & PRINTER_STATUS_INITIALIZING) append_status (_("Initializing"));
	if (status & PRINTER_STATUS_WARMING_UP) append_status (_("Warming Up"));
	if (status & PRINTER_STATUS_TONER_LOW) append_status (_("Toner Low"));
	if (status & PRINTER_STATUS_NO_TONER) append_status (_("No Toner"));
	if (status & PRINTER_STATUS_PAGE_PUNT) append_status (_("Page Punt"));
	if (status & PRINTER_STATUS_USER_INTERVENTION) append_status (_("User Intervention"));
	if (status & PRINTER_STATUS_OUT_OF_MEMORY) g_string_append_printf (status_str, "Out Of Memory");
	if (status & PRINTER_STATUS_DOOR_OPEN) append_status (_("Door Open"));
	if (status & PRINTER_STATUS_SERVER_UNKNOWN) append_status (_("Server Unknown"));
	if (status & PRINTER_STATUS_POWER_SAVE) append_status (_("Power Save"));

	if (status_str->len)
		g_string_erase (status_str, 0, 2);

	real_str = status_str->str;
	g_string_free (status_str, FALSE);

	return real_str;
}
static gboolean
cb_update_status (gpointer data)
{
	GPAPrinter *printer = GPA_PRINTER (data);
	GPANode *state, *node;
	gchar *str;
	HANDLE hPrinter;
	PRINTER_INFO_2 *info2_arr = NULL;
	DWORD buf_size = 0;
	gboolean res;

	if (!printer->polling)
		return FALSE;

	str = g_locale_from_utf8(printer->name, -1, NULL, NULL, NULL);
	res = OpenPrinter (str, &hPrinter, NULL);
	g_free (str);
	if (!res)
		return FALSE;

	while (!GetPrinter (hPrinter, 2, (LPBYTE) info2_arr,
			    buf_size, &buf_size))
		info2_arr = g_realloc (info2_arr, buf_size);

	ClosePrinter (hPrinter);

	state = gpa_printer_get_state (printer);
	node = gpa_node_get_child_from_path (state, "PrinterState");
	if (!node) {
		node = GPA_NODE (gpa_state_new ("PrinterState"));
		gpa_node_attach (state, node);
	}
	str = get_status_str (info2_arr->Status);
	gpa_node_set_value (node, str);
	g_free (str);

	node = gpa_node_get_child_from_path (state, "QueueLength");
	if (!node) {
		node = GPA_NODE (gpa_state_new ("QueueLength"));
		gpa_node_attach (state, node);
	}
	str = g_strdup_printf ("%d", info2_arr->cJobs);
	gpa_node_set_value (node, str);
	g_free (str);

	node = gpa_node_get_child_from_path (state, "Location");
	if (!node) {
		node = GPA_NODE (gpa_state_new ("Location"));
		gpa_node_attach (state, node);
	}
	str = g_locale_to_utf8 (info2_arr->pLocation, -1, NULL, NULL, NULL);
	gpa_node_set_value (node, str);
	
	g_free (str);
	g_free (info2_arr);

	return TRUE;
}

void
gpa_module_polling (GPAPrinter *printer, gboolean polling)
{
	if (polling) {
		printer->polling = TRUE;
		cb_update_status (printer);
		g_timeout_add (5000, cb_update_status, printer);
	}
	else
		printer->polling = FALSE;
}

gboolean
gpa_module_init (GpaModuleInfo *info)
{
	info->printer_list_append = gnome_print_win32_printer_list_append;
	return TRUE;
}
