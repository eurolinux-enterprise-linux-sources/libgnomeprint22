/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  gnome-print-cups.c: A cups backend thingy
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
 *    Dave Camp <dave@ximian.com>
 *
 *  Copyright 2002  Ximian, Inc. and authors
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

#include <cups/ppd.h>

#include <libgnomecups/gnome-cups-init.h>
#include <libgnomecups/gnome-cups-printer.h>

#include "libgnomeprint/gnome-print-i18n.h"

static 	GModule *handle = NULL;
/* Argument order: id, name */

static const xmlChar *model_xml_template =
"<?xml version=\"1.0\"?>"
"<Model Id=\"%s\" Version=\"1.0\">"
"  <Name>%s</Name>"
"  <ModelVersion>0.0.1</ModelVersion>"
"  <Options>"
"    <Option Id=\"Transport\">"
"      <Option Id=\"Backend\" Type=\"List\" Default=\"CUPS\">"
"        <Item Id=\"CUPS\">"
"          <Name>CUPS</Name>"
"          <Key Id=\"Module\" Value=\"libgnomeprintcups.so\"/>"
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
"        <Option Id=\"PrintToFile\" Type=\"String\" Default=\"false\" Locked=\"true\"/>"
"        <Option Id=\"FileName\" Type=\"String\" Default=\"output.ps\"/>"
"      </Option>"
"    </Option>"
#if 0
"    <Option Id=\"Icon\">"
"      <Option Id=\"Filename\" Type=\"String\" Default=\"" DATADIR "/pixmaps/nautilus/default/i-printer.png\"/>"
"    </Option>"
#endif
"  </Options>"
"</Model>";

/* Argument order: id */

static const xmlChar *model_unknown_xml_template =
"<?xml version=\"1.0\"?>"
"<Model Id=\"%s\" Version=\"1.0\">"
"  <Name>Unavailable PPD File</Name>"
"  <ModelVersion>0.0.1</ModelVersion>"
"  <Options>"
"    <Option Id=\"Transport\">"
"      <Option Id=\"Backend\" Type=\"List\" Default=\"CUPS\">"
"        <Item Id=\"CUPS\">"
"          <Name>CUPS</Name>"
"          <Key Id=\"Module\" Value=\"libgnomeprintcups.so\"/>"
"        </Item>"
"      </Option>"
"    </Option>"
"    <Option Id=\"Output\">"
"      <Option Id=\"Media\">"
"        <Option Id=\"PhysicalSize\" Type=\"List\" Default=\"USLetter\">"
"          <Fill Ref=\"Globals.Media.PhysicalSize\"/>"
"        </Option>"
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
"        <Option Id=\"Duplex\" Type=\"String\" Default=\"true\"/>"
"        <Option Id=\"Tumble\" Type=\"String\" Default=\"false\"/>"
"        <Option Id=\"PrintToFile\" Type=\"String\" Default=\"false\" Locked=\"true\"/>"
"        <Option Id=\"FileName\" Type=\"String\" Default=\"output.ps\"/>"
"      </Option>"
"    </Option>"
#if 0
"    <Option Id=\"Icon\">"
"      <Option Id=\"Filename\" Type=\"String\" Default=\"" DATADIR "/pixmaps/nautilus/default/i-printer.png\"/>"
"    </Option>"
#endif
"  </Options>"
"</Model>";

typedef struct {
	GPAList *list;
	char *module_path;
} GnomePrintCupsNewPrinterCbData;

typedef struct {
	GnomeCupsPrinter *printer;
	GPAList *list;
	char *module_path;
} GnomePrintCupsLoadPrinterData;

G_MODULE_EXPORT void gpa_module_load_data (GPAPrinter *printer);

static void cb_get_ppd (guint id, ppd_file_t *ppd, GError **error, gpointer user_data);

static void
free_ppd_retrieval_data (gpointer p)
{
 	GnomePrintCupsLoadPrinterData *data = p;
 	g_object_unref (data->printer);
 	g_free (data->module_path);
 	g_free (data);
}

static void
start_printer_ppd_retrieval (GnomeCupsPrinter *printer,
 			     GPAList *printers_list,
 			     const char *module_path)
{
 	GnomePrintCupsLoadPrinterData *loaddata;
 	loaddata = g_new0 (GnomePrintCupsLoadPrinterData, 1);
 	loaddata->printer = g_object_ref (printer);
 	loaddata->list = printers_list;
 	loaddata->module_path = g_strdup (module_path);
 	gnome_cups_printer_get_ppd_async (printer,
		cb_get_ppd, loaddata, free_ppd_retrieval_data);
}

static void
cb_printer_attributes_changed (GnomeCupsPrinter *printer,
			       GnomePrintCupsNewPrinterCbData *data)
{
	start_printer_ppd_retrieval (printer, data->list, data->module_path);
	g_object_unref (printer);
}

static void
cb_printer_added (const char *name, GnomePrintCupsNewPrinterCbData *data)
{
	GPANode *node;
	GnomeCupsPrinter *printer;

	if ((node = gpa_printer_get_by_id (name)) != NULL) {
		gpa_node_unref (node);
		return;
	}

	printer = gnome_cups_printer_get (name);
	if (!gnome_cups_printer_get_attributes_initialized (printer))
		g_signal_connect (printer, "attributes-changed",
			G_CALLBACK (cb_printer_attributes_changed), data);
	else
		cb_printer_attributes_changed (printer, data);
}

static void
cb_printer_removed (const char *name, GPAList *list)
{
	GPANode *child = gpa_node_get_child (GPA_NODE (list), NULL);
	for (; NULL != child ; child = gpa_node_get_child (GPA_NODE (list), child))
		if (GPA_NODE_ID_COMPARE (child, name))
			break;
	if (NULL != child)
		gpa_node_detach (child);
}

static void
warn_of_unknown_encoding (char const *encoding)
{
	static GHashTable *unknown_encodings = NULL;

	if (encoding == NULL)
		return;

	if (unknown_encodings == NULL)
		unknown_encodings = g_hash_table_new_full (
			g_str_hash, g_str_equal, g_free, NULL);
	else if (NULL != g_hash_table_lookup (unknown_encodings, encoding))
		 return;
	g_hash_table_insert (unknown_encodings, g_strdup (encoding), GINT_TO_POINTER (1));
	g_warning ("iconv does not support ppd character encoding: %s, trying CSISOLatin1",
		   encoding);
}

static char *
get_paper_text (ppd_file_t const *ppd, ppd_size_t const *size)
{
	/* This is dumb and slow and ugly and crappy and I hate myself. (Dave)*/
	int i, j, k, len;
	char *result;
	ppd_group_t *group;

	for (i = 0; i < ppd->num_groups; i++) {
		group = &ppd->groups[i];
		for (j = 0; j < group->num_options; j++) {
			ppd_option_t *option = &group->options[j];
			if (strcmp (option->keyword, "PageSize"))
				continue;
			for (k = 0; k < option->num_choices; k++) {
				if (strcmp (option->choices[k].choice, size->name))
					continue;
				len = strlen (option->choices[k].text);
				if (ppd->lang_encoding) {
					result = g_convert (option->choices[k].text, len,
							    "UTF-8", ppd->lang_encoding,
							    NULL, NULL, NULL);
					if (result != NULL)
						return result;
					warn_of_unknown_encoding (ppd->lang_encoding);
				}
				return g_convert (option->choices[k].text,  len,
						  "UTF-8",	"CSISOLatin1",
						  NULL, NULL, NULL);
			}
		}
	}

	len = strlen (size->name);
	result = g_convert (size->name,  len, "UTF-8", ppd->lang_encoding,
			    NULL, NULL, NULL);
	if (result != NULL)
		return result;
	warn_of_unknown_encoding (ppd->lang_encoding);
	return g_convert (size->name,  len, "UTF-8", "CSISOLatin1",
			  NULL, NULL, NULL);
}

static GPANode *
option_list_new_with_default (GPANode      *parent,
			      const         guchar *id,
			      ppd_option_t *option)
{
	char *defchoice = g_strdup (option->defchoice);
	char *p = defchoice + strlen (defchoice);
	ppd_choice_t *choice;

	/* Strip trailing spaces and tabs since CUPS doesn't does this
	 */
	while (p > defchoice &&
	       (*(p - 1) == ' ' || *(p - 1) == '\t')) {
		*(p - 1) = '\0';
		p--;
	}

	choice = ppdFindChoice (option, defchoice);
	g_free (defchoice);

	if (!choice && option->num_choices > 0)
		choice = &option->choices[0];

	if (!choice)
		return NULL;

	return gpa_option_list_new (parent, id, choice->choice);
}

static GPANode *
load_paper_sizes (ppd_file_t *ppd, GPANode *parent)
{
	ppd_option_t *option;
	GPANode *node;
	int i;

	option = ppdFindOption (ppd, "PageSize");

	if (!option)
		/* Should we not add the PageSize if we can't find it
		 * or create a default one w/ A4 & USLetter? (Chema)
		 */
		return NULL;

	node = option_list_new_with_default (parent, "PhysicalSize",
					     option);
	if (!node)
		return NULL;

	for (i = 0; i < ppd->num_sizes; i++) {
		gchar *paper_name;

		paper_name = get_paper_text (ppd, &ppd->sizes[i]);

		if (paper_name != NULL) {
			GPANode *size;
			gchar *height;
			gchar *width;
			size = gpa_option_item_new (node,
						    ppd->sizes[i].name,
						    paper_name);
			g_free (paper_name);

			width  = g_strdup_printf ("%d",
						  (int)ppd->sizes[i].width);
			height = g_strdup_printf ("%d",
						  (int)ppd->sizes[i].length);

			gpa_option_key_new (size, "Width",  width);
			gpa_option_key_new (size, "Height", height);

			g_free (width);
			g_free (height);
		}
	}

	gpa_node_reverse_children (node);

	return node;
}

static void
load_paper_sources (ppd_file_t *ppd, GPANode *parent)
{
	ppd_option_t *option;
	GPANode *node;
	int i;

	option = ppdFindOption (ppd, "InputSlot");
	if (!option)
		return;

	node = option_list_new_with_default (parent, "PaperSource",
					     option);
	if (!node)
		return;

	for (i = 0; i < option->num_choices; i++)
		gpa_option_item_new (node,
				     option->choices[i].choice,
				     option->choices[i].text);
}

/* load_cups_hold_types */
/* we could either make an ipp request for job-hold-until-supported */
/* or list here the cups supported values */
/* Sicne these values are in fact not printer specific but cups specific, */
/* We'll just list them here. */
static void
load_cups_hold_types (GPANode *parent)
{
	GPANode *node;

	node = gpa_option_list_new (parent, "Hold", "no-hold");

	if (!node)
		return;

	gpa_option_item_new (node,"weekend",_("on the weekend"));
	gpa_option_item_new (node,"third-shift",
			     _("between midnight and 8 a.m."));
/* 	gpa_option_item_new (node,"night",_("between 6 p.m. and 6 a.m.")); */
	gpa_option_item_new (node,"evening",_("between 6 p.m. and 6 a.m."));
	gpa_option_item_new (node,"second-shift",
			     _("between 4 p.m. and midnight"));
	gpa_option_item_new (node,"day-time",_("between 6 a.m. and 6 p.m."));
	gpa_option_item_new (node,"indefinite",_("when manually released"));
	gpa_option_item_new (node,"no-hold",_("immediately"));
}

static void
load_cups_duplex (GnomeCupsPrinter *printer, GPANode *job)
{
	/* this set is pulled from cups/options.c */
	char *val = val = gnome_cups_printer_get_option_value (printer, "Duplex");	/* std */
	if (NULL == val)
		val = gnome_cups_printer_get_option_value (printer, "JCLDuplex");	/* Samsung */
	if (NULL == val)
		val = gnome_cups_printer_get_option_value (printer, "EFDuplex");	/* EFI */
	if (NULL == val)
		val = gnome_cups_printer_get_option_value (printer, "KD03Duplex");	/* Kodak */
	if (NULL != val) {
		gboolean duplex = FALSE, bind_short_edge = FALSE;

		/* we do not really need to ignore case, but it does not hurt */
		if (g_ascii_strcasecmp (val, "None") == 0) {
			/* this is the default */
		} else if (g_ascii_strcasecmp (val, "DuplexNoTumble") == 0) {
			duplex = TRUE;
			bind_short_edge = FALSE;
		} else if (g_ascii_strcasecmp (val, "DuplexTumble") == 0) {
			duplex = TRUE;
			bind_short_edge = TRUE;
		} else {
			g_warning ("Unknown Duplex setting == '%s'", val);
		}
		g_free (val);
		gpa_option_string_new (job, "Duplex",
			duplex ? "true" : "false");
		gpa_option_string_new (job, "Tumble",
			(duplex && bind_short_edge) ? "true" : "false"); /* be anal */
	}
}

static GPAModel *
get_model (GnomeCupsPrinter *cupsprinter, ppd_file_t *ppd)
{
	GPANode *media;
	GPANode *model;
	GPANode *output;
	GPANode *job;
	char *xml;
	char *id;

	id = g_strdup_printf ("Cups-%s-%s", ppd->manufacturer, ppd->nickname);

	model = gpa_model_get_by_id (id, TRUE);

	if (model) {
		g_free (id);
		return GPA_MODEL (model);
	}

	xml = g_strdup_printf (model_xml_template, id, ppd->nickname);
	model = gpa_model_new_from_xml_str (xml);
	g_free (xml);

	output = gpa_node_lookup (model, "Options.Output");
	media  = gpa_node_lookup (model, "Options.Output.Media");
	job  = gpa_node_lookup (model, "Options.Output.Job");

	load_paper_sizes   (ppd, media);
	load_paper_sources (ppd, output);
	load_cups_hold_types (job);
	load_cups_duplex (cupsprinter, job);

	gpa_node_unref (output);
	gpa_node_unref (media);
	gpa_node_unref (job);

	g_free (id);

	return (GPAModel *)model;
}

static GPAModel *
get_model_no_ppd (const gchar *printer)
{
	GPANode *media;
	GPANode *model;
	GPANode *job;
	char *xml;

	model = gpa_model_get_by_id ("Cups-unknown-unknown", TRUE);

	if (model)
		return GPA_MODEL (model);

	xml = g_strdup_printf (model_unknown_xml_template, "Cups-unknown-unknown");
	model = gpa_model_new_from_xml_str (xml);
	g_free (xml);

	media  = gpa_node_lookup (model, "Options.Output.Media");
	job  = gpa_node_lookup (model, "Options.Output.Job");

/* 	load_paper_sizes   (ppd, media); */
	load_cups_hold_types (job);

	gpa_node_unref (media);
	gpa_node_unref (job);

	return (GPAModel *)model;
}

static void
gnome_print_cups_adjust_settings (GPANode	   *settings,
				  GnomeCupsPrinter *printer)
{
	char *val = gnome_cups_printer_get_option_value (printer, "PageSize");
	if (val != NULL) {
		gpa_node_set_path_value (settings,
			"Output.Media.PhysicalSize", val);
		g_free (val);
	}

}

static void
add_printer_location (GnomeCupsPrinter *cupsprinter,
		      GPAPrinter       *printer)
{
	GPANode *state;
	GPANode *location;
	const char *str;

	state = gpa_printer_get_state (printer);
	location = gpa_node_get_child_from_path (state, "Location");
	if (location == NULL) {
		location = GPA_NODE (gpa_state_new ("Location"));
		gpa_node_attach (state, location);
	}
	str = gnome_cups_printer_get_location (cupsprinter);
	gpa_node_set_value (location, str);
}

static void
cb_attributes_changed (GnomeCupsPrinter *cupsprinter,
		       GPAPrinter     *printer)
{
	GPANode *state;
	GPANode *printerstate;
	GPANode *jobcount;
	const char *str;
	char *len_str;

	state = gpa_printer_get_state (printer);
	printerstate = gpa_node_get_child_from_path (state, "PrinterState");
	if (printerstate == NULL) {
		printerstate = GPA_NODE (gpa_state_new ("PrinterState"));
		gpa_node_attach (state, printerstate);
	}
	str = gnome_cups_printer_get_state_name (cupsprinter);
	gpa_node_set_value (printerstate, str);

	jobcount = gpa_node_get_child_from_path (state, "QueueLength");
	if (jobcount == NULL) {
		jobcount = GPA_NODE (gpa_state_new ("QueueLength"));
		gpa_node_attach (state, jobcount);
	}
	len_str = g_strdup_printf ("%d", gnome_cups_printer_get_job_count (cupsprinter));
	gpa_node_set_value (jobcount, len_str);
	g_free (len_str);
}

static void
cb_get_ppd (guint id, ppd_file_t *ppd, GError **error, gpointer user_data)
{
	GnomePrintCupsLoadPrinterData *data = user_data;
	const char *name;
	GnomeCupsPrinter *cupsprinter;
	GPANode *printer  = NULL;
	GPANode *settings = NULL;
	gboolean success = FALSE;
	GPAModel *model = NULL;
	GPAList *printers_list;

	cupsprinter = data->printer;
	printers_list = data->list;
	name = gnome_cups_printer_get_name (data->printer);

	if (NULL == ppd) {
		g_message ("The ppd file for the CUPS printer %s "
			   "could not be loaded.", name);
		model = get_model_no_ppd (name);
	} else
		model = get_model (cupsprinter, ppd);

	if (model == NULL) {
		g_warning ("Couldn't create model for %s!\n", name);
		goto out;
	}

	settings = gpa_settings_new (model, "Default", "SetIdFromCups");
	if (settings == NULL) {
		g_warning ("Couldn't create settings for %s!\n", name);
		goto out;
	}

	printer = gpa_printer_new_stub (name, name, data->module_path);

	if (printer != NULL && gpa_node_verify (printer)) {
		gnome_print_cups_adjust_settings (settings, cupsprinter);

		success = gpa_printer_complete_stub (GPA_PRINTER (printer),
						     model,
						     GPA_SETTINGS (settings));

		if (success) {
			if (gnome_cups_printer_get_is_default (cupsprinter))
				gpa_list_set_default (printers_list, printer);
			gpa_list_prepend (printers_list, printer);
		}
	} else {
		g_warning ("The CUPS printer %s could not be created\n", name);
		my_gpa_node_unref (printer);
		goto out;
	}

	if (success) {
		add_printer_location (cupsprinter, GPA_PRINTER (printer));
		cb_attributes_changed (cupsprinter, GPA_PRINTER (printer));
	}

	/* Do we have to add any further instances */
	/* FIXME - punt on instance bits for now */
	/* 	{ */
	/* 		char *id = g_strdup_printf ("SetIdFromCups-%s", printer->name); */
	/* 		settings = gpa_settings_new (model, printer->name, id); */
	/* 		g_free (id); */
	/* 		id = NULL; */
	/* 		if (settings != NULL) { */
	/* 			gnome_print_cups_adjust_settings (GPA_SETTINGS (settings), */
	/* 							  cupsprinter); */
	/* 			gpa_list_prepend (GPA_NODE (printer->settings), */
	/* 					  GPA_NODE (settings)); */
	/* 			GPA_SETTINGS (settings)->printer */
	/* 				= gpa_reference_new (GPA_NODE (printer), "Printer"); */
	/* 		} */
	/* 	} */

out:
	if (success == FALSE) {
		g_warning ("The data for the CUPS printer %s "
			   "could not be loaded.", name);

		if (model != NULL)
			my_gpa_node_unref (GPA_NODE (model));
		if (settings != NULL)
			my_gpa_node_unref (settings);
	}

	if (ppd) {
		ppdClose (ppd);
	}
}

static void
gnome_print_cups_printer_list_append (gpointer printers_list,
				      const gchar *module_path)
{
	GnomePrintCupsNewPrinterCbData *data;

	g_return_if_fail (printers_list != NULL);
	g_return_if_fail (GPA_IS_LIST (printers_list));

	/* make ourselves resident */
	if (!handle)
		handle = g_module_open (module_path, G_MODULE_BIND_LAZY);

	/* install cb_printer_added *before* calling gnome_cups_init(),
	 * since that gets the list of printers */
	data = g_new0 (GnomePrintCupsNewPrinterCbData, 1);
	data->list = printers_list;
	data->module_path = g_strdup (module_path);
	gnome_cups_printer_new_printer_notify_add (
		(GnomeCupsPrinterAddedCallback) cb_printer_added, data);
	gnome_cups_printer_printer_removed_notify_add (
		(GnomeCupsPrinterRemovedCallback) cb_printer_removed, printers_list);
	gnome_cups_init (NULL);
}

static void
start_polling (GPAPrinter *printer)
{
	GnomeCupsPrinter *cupsprinter;

	cupsprinter = gnome_cups_printer_get (printer->name);
	cb_attributes_changed (cupsprinter, printer);
	g_signal_connect_object (cupsprinter, "attributes-changed",
				 G_CALLBACK (cb_attributes_changed), printer, 0);
}

static void
stop_polling (GPAPrinter *printer)
{
	GnomeCupsPrinter *cupsprinter;

	cupsprinter = gnome_cups_printer_get (printer->name);
	g_signal_handlers_disconnect_by_func (cupsprinter,
					      G_CALLBACK (cb_attributes_changed),
					      printer);
	/* Unref twice since _get refs itself */
	g_object_unref (G_OBJECT (cupsprinter));
	g_object_unref (G_OBJECT (cupsprinter));
}

G_MODULE_EXPORT void gpa_module_polling (GPAPrinter *printer,
					 gboolean polling)
{
	if (polling)
		start_polling (printer);
	else
		stop_polling (printer);
}


/*  ------------- GPA load_data ------------- */

G_MODULE_EXPORT void gpa_module_load_data (GPAPrinter *printer)
{
}


/*  ------------- GPA init ------------- */
G_MODULE_EXPORT gboolean gpa_module_init (GpaModuleInfo *info);

G_MODULE_EXPORT gboolean
gpa_module_init (GpaModuleInfo *info)
{
	info->printer_list_append = gnome_print_cups_printer_list_append;
	return TRUE;
}
