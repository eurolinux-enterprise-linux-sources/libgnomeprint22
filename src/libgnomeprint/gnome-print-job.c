/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  gnome-print-job.c: A print job interface
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
 *    Michael Zucchi <notzed@ximian.com>
 *    Lauris Kaplinski <lauris@ximian.com>
 *
 *  Copyright (C) 2000-2003 Ximian Inc.
 */

#define GNOME_PRINT_UNSTABLE_API

#include <config.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>

#include <libart_lgpl/art_affine.h>
#include <libgnomeprint/gpa/gpa-node.h>
#include <libgnomeprint/gpa/gpa-node-private.h>
#include <libgnomeprint/gpa/gpa-key.h>
#include <libgnomeprint/gnome-print-private.h>
#include <libgnomeprint/gnome-print-meta.h>
#include <libgnomeprint/gnome-print-transport.h>
#include <libgnomeprint/gnome-print-job.h>
#include <libgnomeprint/gnome-print-config-private.h>
#include <libgnomeprint/gnome-print-i18n.h>

#include <libgnomeprint/gnome-print-filter.h>

typedef struct _GnomePrintJobPrivate GnomePrintJobPrivate;

struct _GnomePrintJob {
	GObject object;

	GnomePrintConfig *config;
	GnomePrintContext *meta;
	gchar *input_file;

	GnomePrintJobPrivate *priv;
};

struct _GnomePrintJobClass {
	GObjectClass parent_class;
};

struct _GnomePrintJobPrivate {
	/* closed flag */
	guint closed : 1;

	/* Layout data */
	gdouble pw, ph;
	gdouble porient[6];
	gdouble lorient[6];
	gdouble lyw, lyh;
	gint num_affines;
	gdouble *affines;

	/* State data */
	gdouble PP2PA[6];
	gdouble PAW, PAH;
	gdouble LP2LY[6];
	gdouble LYW, LYH;
	gdouble LW, LH;
	gdouble *LY_AFFINES;
	GList *LY_LIST;
};

enum
{
	PROP_0,
	PROP_CONFIG,
	PROP_CONTEXT
};
 

#define GNOME_PRINT_JOB_CLOSED(m) ((m)->priv->closed)

static void gnome_print_job_class_init (GnomePrintJobClass *klass);
static void gnome_print_job_init (GnomePrintJob *job);
static void gnome_print_job_finalize (GObject *object);

static void gnome_print_job_update_layout_data (GnomePrintJob *job);
static void job_parse_config_data (GnomePrintJob *job);
static void job_clear_config_data (GnomePrintJob *job);

static void gnome_print_job_get_property (GObject      *object,
                                          guint         prop_id,
                                          GValue       *value,
                                          GParamSpec   *pspec);
static void gnome_print_job_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec);

static GObjectClass *parent_class;


GType
gnome_print_job_get_type (void)
{
	static GType type = 0;
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GnomePrintJobClass),
			NULL, NULL,
			(GClassInitFunc) gnome_print_job_class_init,
			NULL, NULL,
			sizeof (GnomePrintJob),
			0,
			(GInstanceInitFunc) gnome_print_job_init
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GnomePrintJob", &info, 0);
	}
	return type;
}

static void
gnome_print_job_class_init (GnomePrintJobClass *klass)
{
	GObjectClass *object_class;
	
	object_class = (GObjectClass *) klass;

	object_class->finalize     = gnome_print_job_finalize;
	object_class->set_property = gnome_print_job_set_property;
	object_class->get_property = gnome_print_job_get_property;

	g_object_class_install_property (object_class, PROP_CONFIG,
		g_param_spec_object ("config", _("Job Configuration"),
			_("The configuration for the print job"),
			GNOME_TYPE_PRINT_CONFIG,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_CONTEXT,
		g_param_spec_object ("context", _("Context"),
			_("The context for the print job"),
			GNOME_TYPE_PRINT_META, G_PARAM_READWRITE));

	parent_class = g_type_class_peek_parent (klass);
}

static void
gnome_print_job_init (GnomePrintJob *job)
{
	job->config = NULL;

	job->meta = g_object_new (GNOME_TYPE_PRINT_META, NULL);

	job->priv = g_new0 (GnomePrintJobPrivate, 1);
	job_clear_config_data (job);
}

static void
gnome_print_job_finalize (GObject *object)
{
	GnomePrintJob *job;

	job = GNOME_PRINT_JOB(object);

	if (job->config) {
		g_object_unref (G_OBJECT (job->config));
		job->config = NULL;
	}

	if (job->meta != NULL) {
		g_object_unref (G_OBJECT (job->meta));
		job->meta = NULL;
	}

	g_free (job->input_file);
	job->input_file = NULL;

	if (job->priv) {
		job_clear_config_data (job);
		g_free (job->priv);
		job->priv = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gnome_print_job_get_property (GObject *object, guint prop_id, GValue *value,
	                     GParamSpec *pspec)
{
	GnomePrintJob *job = GNOME_PRINT_JOB (object);

	switch (prop_id) {
	case PROP_CONFIG:
		g_value_set_object (value, job->config);
		break;
	case PROP_CONTEXT:
		g_value_set_object (value, job->meta);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gnome_print_job_set_property (GObject * object, guint prop_id,
	                     const GValue * value, GParamSpec * pspec)
{
	GnomePrintJob *job = GNOME_PRINT_JOB (object);

	switch (prop_id) {
	case PROP_CONFIG:
		if (job->config) gnome_print_config_unref (job->config);
		job->config = g_value_get_object (value);
		if (job->config) gnome_print_config_ref (job->config);
		else job->config = gnome_print_config_default ();
		break;
	case PROP_CONTEXT:
		if (job->meta) g_object_unref (job->meta);
		job->meta = g_value_get_object (value);
		if (job->meta) g_object_ref (G_OBJECT (job->meta));
		else job->meta = g_object_new (GNOME_TYPE_PRINT_META, NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}


/**
 * gnome_print_job_new:
 * @config: The job options, can be NULL in which case a
 *          default GnomePrintConfig is created
 * 
 * Creates a new GnomePrintJob.
 * 
 * Return value: A new GnomePrintJob, NULL on error
 **/
GnomePrintJob *
gnome_print_job_new (GnomePrintConfig *config)
{
	GnomePrintJob *job;
	
	job = g_object_new (GNOME_TYPE_PRINT_JOB, "config", config, NULL);

	return job;
}

/**
 * gnome_print_job_get_config:
 * @job: 
 * 
 * Gets a referenced pointer to the configuration of the job
 * 
 * Return Value: a referenced GnomePrintConfig for this job, NULL on error
 **/
GnomePrintConfig *
gnome_print_job_get_config (GnomePrintJob *job)
{
	g_return_val_if_fail (job != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_PRINT_JOB (job), NULL);

	if (job->config)
		gnome_print_config_ref (job->config);

	return job->config;
}

/**
 * gnome_print_job_get_context:
 * @job: An initialised GnomePrintJob.
 * 
 * Retrieve the GnomePrintContext which applications
 * print to.
 *
 * The caller is responsible to unref the context when s/he is done with it.
 * 
 * Return value: The printing context, NULL on error
 **/
GnomePrintContext *
gnome_print_job_get_context (GnomePrintJob *job)
{
	g_return_val_if_fail (job != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_PRINT_JOB (job), NULL);
	g_return_val_if_fail (job->input_file == NULL, NULL);

	g_object_ref (G_OBJECT (job->meta));

	return job->meta;
}

/**
 * gnome_print_job_set_file:
 * @job: An initialised GnomePrintJob.
 * @input_file: The file to input from.
 * 
 * Set the file to print from. This allows applications to use
 * gnome-print without using the drawing API by providing the
 * postscipt output.
 * 
 * Return value: None
 **/
void
gnome_print_job_set_file (GnomePrintJob *job, gchar *input_file)
{
	g_return_if_fail (GNOME_IS_PRINT_JOB (job));

	if (job->input_file) {
		g_free (job->input_file);
		job->input_file = NULL;
		gnome_print_config_set (job->config,
			(const guchar *) "Settings.Document.Source",
			(const guchar *) "");
		
	}
	if (input_file) {
		GPANode *node;

		job->input_file = g_strdup (input_file);
		node = gpa_node_lookup (GNOME_PRINT_CONFIG_NODE (job->config),
			(const guchar *) "Settings.Document.Source");
		if (!node) {
			node = gpa_node_lookup (GNOME_PRINT_CONFIG_NODE (job->config),
				(const guchar *) "Settings.Document");
			gpa_key_insert (node, (const guchar *) "Source",
				(const guchar *) input_file);
		}
		gnome_print_config_set (job->config,
			(const guchar *) "Settings.Document.Source",
			(const guchar *) input_file);
	}
}

static void
gnome_print_job_setup_context (GnomePrintJob *job, GnomePrintContext *pc)
{
	GnomePrintFilter *filter = NULL, *f = NULL;
	gchar *d;

	g_return_if_fail (GNOME_IS_PRINT_JOB (job));
	g_return_if_fail (GNOME_IS_PRINT_CONTEXT (pc));

	g_object_set (G_OBJECT (pc), "filter", NULL, NULL);

	/*
	 * There are three places for filters:
	 * (1) Filters specific to printers (like GnomePrintFilterFrgba) are
	 *     stored in Settings.Output.Job.Filter.
	 * (2) Filters independent from printers (like GnomePrintFilterSelect) are
	 *     stored in Settings.Document.Filter.
	 * (3) A multipage filter may be specified in the layout key in 
	 *     the print configuration. We continue to load it, but this 
	 *     key should be removed (it is not as flexible as a filter
	 *     pipeline).
	 * We load the second one first and append the printer specific filter.
	 */
	d = (gchar *) gnome_print_config_get (job->config,
			(const guchar *) "Settings.Output.Job.Filter");
	if (d) {
		filter = gnome_print_filter_new_from_description (d, NULL);
		g_free (d);
		if (filter) {
			f = g_object_new (GNOME_TYPE_PRINT_FILTER, NULL);
			gnome_print_filter_add_filter (f, filter);
			g_object_unref (G_OBJECT (filter));
			g_object_set (G_OBJECT (pc), "filter", f, NULL);
			g_object_unref (G_OBJECT (f));
		}
	}
	d = (gchar *) gnome_print_config_get (job->config,
			(const guchar *) "Settings.Document.Filter");
	if (d) {
		filter = gnome_print_filter_new_from_description (d, NULL);
		g_free (d);
		if (filter) {
			f = g_object_new (GNOME_TYPE_PRINT_FILTER, NULL);
			gnome_print_filter_add_filter (f, filter);
			g_object_unref (G_OBJECT (filter));
			g_object_get (G_OBJECT (pc), "filter", &filter, NULL);
			if (filter)
				gnome_print_filter_append_predecessor (filter, f);
			g_object_set (G_OBJECT (pc), "filter", f, NULL);
			g_object_unref (G_OBJECT (f));
		}
	}

	gnome_print_job_update_layout_data (job);
	if (job->priv->LY_LIST && (g_list_length (job->priv->LY_LIST) > 1) && 
			!((g_list_length (job->priv->LY_LIST) == 1) &&
				(((gdouble *) g_list_nth_data (job->priv->LY_LIST, 0)) [0] == 1.) &&
				(((gdouble *) g_list_nth_data (job->priv->LY_LIST, 0)) [1] == 0.) &&
				(((gdouble *) g_list_nth_data (job->priv->LY_LIST, 0)) [2] == 0.) &&
				(((gdouble *) g_list_nth_data (job->priv->LY_LIST, 0)) [3] == 1.) &&
				(((gdouble *) g_list_nth_data (job->priv->LY_LIST, 0)) [4] == 0.) &&
				(((gdouble *) g_list_nth_data (job->priv->LY_LIST, 0)) [5] == 0.))) {
		GValue vd = {0,};
		GValueArray *va = g_value_array_new (0);
		guint i, npages;
		
		npages = gnome_print_meta_get_pages (GNOME_PRINT_META (job->meta));
		npages = (npages + job->priv->num_affines - 1) / job->priv->num_affines;
		g_value_init (&vd, G_TYPE_DOUBLE);
		for (i = g_list_length (job->priv->LY_LIST); i > 0; i--) {
			gdouble *a = g_list_nth_data (job->priv->LY_LIST, i - 1);
			guint j;

			for (j = 6; j > 0; j--) {
				g_value_set_double (&vd, a[j - 1]);
				g_value_array_prepend (va, &vd);
			}
		}
		g_value_unset (&vd);
		f = gnome_print_filter_new_from_module_name ("multipage",
				"affines", va, NULL);
		g_value_array_free (va);
		if (f) {
			g_object_get (G_OBJECT (pc), "filter", &filter, NULL);
			if (filter)
				gnome_print_filter_append_predecessor (filter, f);
			g_object_set (G_OBJECT (pc), "filter", f, NULL);
			g_object_unref (G_OBJECT (f));
		}
	}

	g_object_get (G_OBJECT (pc), "filter", &filter, NULL);
	if (filter)
		gnome_print_filter_reset (filter);
}

/**
 * gnome_print_job_get_pages:
 * @job: An initialised and closed GnomePrintJob.
 * 
 * Find the number of pages stored in a completed printout.
 * This is the number of physical pages, so if the layout
 * can hold 4 pages per page, and 5 logical pages are printed
 * (5 beginpage/endpage convinations) 2 is returned 
 * 
 * Return value: the number of pages, 0 on error
 **/
int
gnome_print_job_get_pages (GnomePrintJob *job)
{
	gint n;
	GnomePrintContext *pc;

	g_return_val_if_fail (GNOME_IS_PRINT_JOB (job), 0);
	g_return_val_if_fail (GNOME_PRINT_JOB_CLOSED (job), 0);

	/*
	 * Filters can suppress or add pages. We therefore just pass everything
	 * through the filter pipeline and count the number of pages that come 
	 * out of it.
	 */
	pc = g_object_new (GNOME_TYPE_PRINT_META, NULL);
	gnome_print_job_setup_context (job, pc);
	gnome_print_meta_render (GNOME_PRINT_META (job->meta), pc);
	gnome_print_context_close (pc);
	n = gnome_print_meta_get_pages (GNOME_PRINT_META (pc));
	g_object_unref (G_OBJECT (pc));

	return n;
}

/**
 * gnome_print_job_get_page_size_from_config:
 * @config: 
 * @width: 
 * @height: 
 * 
 * Deprecated, use gnome_print_config_get_page_size
 * 
 * Return Value: TRUE on success, FALSE on error
 **/
gboolean
gnome_print_job_get_page_size_from_config (GnomePrintConfig *config, gdouble *width, gdouble *height)
{
	return gnome_print_config_get_page_size (config, width, height);
}

/**
 * gnome_print_job_get_page_size:
 * @job: 
 * @width: 
 * @height: 
 * 
 * Get the imaging area size that is available to the application
 * Sizes are given in PS points (GNOME_PRINT_PS_UNIT)
 * 
 * Return Value: TRUE on success, FALSE on error
 **/
gboolean
gnome_print_job_get_page_size (GnomePrintJob *job, gdouble *width, gdouble *height)
{
	g_return_val_if_fail (job != NULL, FALSE);
	g_return_val_if_fail (GNOME_IS_PRINT_JOB (job), FALSE);
	g_return_val_if_fail (width != NULL, FALSE);
	g_return_val_if_fail (height != NULL, FALSE);

	gnome_print_job_update_layout_data (job);

	if (job->priv->LY_LIST) {
		if (width)
			*width = job->priv->LW;
		if (height)
			*height = job->priv->LH;
	} else {
		if (width)
			*width = job->priv->pw;
		if (height)
			*height = job->priv->ph;
	}

	return TRUE;
}

/**
 * gnome_print_job_close:
 * @job: A GnomePrintJob which has had printing performed
 * 
 * Closes the GnomePrintJob @job, ready for printing or
 * previewing. To be called after the application has finished
 * sending the drawing commands
 * 
 * Return Value: 
 **/
gint
gnome_print_job_close (GnomePrintJob *job)
{
	g_return_val_if_fail (job != NULL, GNOME_PRINT_ERROR_UNKNOWN);
	g_return_val_if_fail (GNOME_IS_PRINT_JOB (job), GNOME_PRINT_ERROR_UNKNOWN);
	g_return_val_if_fail (job->input_file == NULL, 0);
	g_return_val_if_fail (!GNOME_PRINT_JOB_CLOSED (job), GNOME_PRINT_ERROR_UNKNOWN);

	job->priv->closed = TRUE;
	return gnome_print_context_close (job->meta);
}

/**
 * gnome_print_job_metadata_printer:
 * @job: 
 * @retval: 
 * 
 * We have to special case the Metadata printer where we want to create a file on
 * disk with the drawing commands to later play them. libgnomeprint needs to be
 * configured with --with-metadata-printer for it to show up in the printers list
 * this is used to create streams of drawing commands for example for the regression
 * test suite.
 * 
 * Return Value: TRUE if this job is using the Metadata (to file) printer
 **/
static gboolean
gnome_print_job_metadata_printer (GnomePrintJob *job, gint *retval)
{
	GnomePrintTransport *transport;
	gchar *drivername;
	gboolean print_to_file = FALSE;
	const guchar *buf;
	gint blen;

	drivername = (gchar *) gnome_print_config_get (job->config, (const guchar *) "Settings.Engine.Backend.Driver");

	if (drivername == NULL)
		return FALSE;

	if (strcmp (drivername, "gnome-print-meta") != 0) {
		g_free (drivername);
		return FALSE;
	}

	*retval = GNOME_PRINT_ERROR_UNKNOWN;
	
	gnome_print_config_get_boolean (job->config, (const guchar *) "Settings.Output.Job.PrintToFile", &print_to_file);
	if (!print_to_file) {
		g_warning ("Metadata printer should always be print to file");
		goto metadata_printer_done;
	}

	transport = gnome_print_transport_new (job->config);
	if (!transport) {
		g_warning ("Could not create transport for metadata printer");
		goto metadata_printer_done;
	}

	buf  = gnome_print_meta_get_buffer (GNOME_PRINT_META (job->meta));
	blen = gnome_print_meta_get_length (GNOME_PRINT_META (job->meta));

	gnome_print_transport_open  (transport);
	gnome_print_transport_write (transport, buf, blen);
	gnome_print_transport_write (transport, (const guchar *) "GNOME_METAFILE_END", strlen ("GNOME_METAFILE_END"));
	gnome_print_transport_close (transport);
	
	*retval = GNOME_PRINT_OK;

metadata_printer_done:

	g_free (drivername);

	return TRUE;
}

/**
 * gnome_print_job_print:
 * @job: A closed GnomePrintJob.
 * 
 * Print the pages stored in the GnomePrintJob to
 * the phyisical printing device.
 *
 * Return value: GNOME_PRINT_OK on success GNOME_PRINT_ERROR_UNKNOWN otherwise
 **/
gint
gnome_print_job_print (GnomePrintJob *job)
{
	GnomePrintContext *ctx;
	GnomePrintFilter *filter = NULL;
	gint lpages, copies, nstacks, npages, nsheets;
	gboolean collate, hwcopies;
	gint stack;
	gint ret;

	g_return_val_if_fail (job != NULL, GNOME_PRINT_ERROR_UNKNOWN);
	g_return_val_if_fail (GNOME_IS_PRINT_JOB (job), GNOME_PRINT_ERROR_UNKNOWN);
	g_return_val_if_fail (job->priv, GNOME_PRINT_ERROR_UNKNOWN);

	if (job->input_file) {
		GnomePrintTransport *transport = gnome_print_transport_new (job->config);
		return gnome_print_transport_print_file (transport, (const guchar *) job->input_file);
	}

	if (!job->priv->closed) {
		g_warning ("You should call gnome_print_job_close before calling\n"
			   "gnome_print_job_print\n");
		gnome_print_job_close (job);
	}

	/* Get number of pages in metafile */
	lpages = gnome_print_meta_get_pages (GNOME_PRINT_META (job->meta));
	if (lpages < 1)
		return GNOME_PRINT_OK;

	/* Special case where printer is a Metadata print to file */
	if (gnome_print_job_metadata_printer (job, &ret))
		return ret;

	/* Create the "real" context and set it up. */
	ctx = gnome_print_context_new (job->config);
	gnome_print_job_setup_context (job, ctx);
	g_object_get (G_OBJECT (ctx), "filter", &filter, NULL);

	collate = FALSE;
	gnome_print_config_get_boolean (job->config, (const guchar *) GNOME_PRINT_KEY_COLLATE, &collate);
	copies = 1;
	gnome_print_config_get_int (job->config, (const guchar *) GNOME_PRINT_KEY_NUM_COPIES, &copies);
	hwcopies = FALSE;
	gnome_print_config_get_boolean (job->config, 
					collate 
					? (const guchar *) GNOME_PRINT_KEY_COLLATED_COPIES_IN_HW
					: (const guchar *) GNOME_PRINT_KEY_NONCOLLATED_COPIES_IN_HW, 
					&hwcopies);

	if (hwcopies)
		copies = 1;

	if (collate) {
		nstacks = copies;
		nsheets = 1;
	} else {
		nstacks = 1;
		nsheets = copies;
	}

	if (job->priv->num_affines >= 1) {
		npages = (lpages + job->priv->num_affines - 1) / job->priv->num_affines;
	} else {
		npages = 0;
	}
	for (stack = 0; stack < nstacks; stack++) {
		gint page;
		for (page = 0; page < npages; page++) {
			gint sheet;
			for (sheet = 0; sheet < nsheets; sheet++) {
				gint start, i;
				/* Render physical page */
				start = page * job->priv->num_affines;
				for (i = start; (i < (start + job->priv->num_affines)) && (i < lpages); i++) {
					ret = gnome_print_meta_render_page (GNOME_PRINT_META (job->meta),
							ctx, i, TRUE);
					g_return_val_if_fail (ret == GNOME_PRINT_OK, ret);
				}
			}
		}
		if (stack + 1 < nstacks) {
			if (filter)
				gnome_print_filter_flush (filter);
			ret = gnome_print_end_doc (ctx);
			g_return_val_if_fail (ret == GNOME_PRINT_OK, ret);
		}
	}

	if (NULL != filter)
	    g_object_unref (G_OBJECT (filter));

	ret = gnome_print_context_close (ctx);
	g_object_unref (G_OBJECT (ctx));

	return ret;
}


/**
 * gnome_print_job_render:
 * @job: 
 * @ctx: 
 * 
 * Renders printout to specified context
 * (with layout, ignoring copies)
 * 
 * Return Value: 
 **/
gint
gnome_print_job_render (GnomePrintJob *job, GnomePrintContext *ctx)
{
	g_return_val_if_fail (GNOME_IS_PRINT_JOB (job), GNOME_PRINT_ERROR_UNKNOWN);

	gnome_print_job_setup_context (job, ctx);
	return gnome_print_meta_render (GNOME_PRINT_META (job->meta), ctx);
}

/**
 * gnome_print_job_render_page:
 * @job: 
 * @ctx: 
 * @page: 
 * @pageops: 
 * 
 * Renders the specified page @page
 * 
 * Return Value: 
 **/
gint
gnome_print_job_render_page (GnomePrintJob *job, GnomePrintContext *ctx, gint page, gboolean pageops)
{
	GnomePrintContext *pc;

	g_return_val_if_fail (GNOME_IS_PRINT_JOB (job), GNOME_PRINT_ERROR_UNKNOWN);

	pc = g_object_new (GNOME_TYPE_PRINT_META, NULL);
	gnome_print_job_setup_context (job, pc);
	gnome_print_meta_render (GNOME_PRINT_META (job->meta), pc);
	gnome_print_context_close (pc);
	gnome_print_meta_render_page (GNOME_PRINT_META (pc), ctx, page, pageops);
	g_object_unref (G_OBJECT (pc));
	return GNOME_PRINT_OK;
}

/* We need:
 *
 * Layout data
 *
 * - pw, ph
 * - porient
 * - lorient
 * - lyw, lyh
 * - num_affines
 * - affines
 *
 * State data
 *
 * - PP2PA
 * - PAW, PAH
 * - LP2LY
 * - LYW, LYH
 * - LW, LH
 * - LY_AFFINES
 * - LY_LIST
 */

#define EPSILON 1e-9
#ifdef JOB_VERBOSE
#define PRINT_2(s,a,b) g_print ("%s %g %g\n", s, (a), (b))
#define PRINT_DRECT(s,a) g_print ("%s %g %g %g %g\n", (s), (a)->x0, (a)->y0, (a)->x1, (a)->y1)
#define PRINT_AFFINE(s,a) g_print ("%s %g %g %g %g %g %g\n", (s), *(a), *((a) + 1), *((a) + 2), *((a) + 3), *((a) + 4), *((a) + 5))
#else
#define PRINT_2(s,a,b)
#define PRINT_DRECT(s,a)
#define PRINT_AFFINE(s,a)
#endif

static void
gnome_print_job_update_layout_data (GnomePrintJob *job)
{
	ArtDRect area, r;
	gdouble t;
	gdouble a[6];
	gint i;

	g_return_if_fail (job->priv);

	job_parse_config_data (job);

	/* Now comes the fun part */

	if (job->priv->num_affines < 1)
		return;
	if ((fabs (job->priv->pw) < EPSILON) || (fabs (job->priv->ph) < EPSILON))
		return;

	/* Initial setup */
	/* Calculate PP2PA */
	/* We allow only rectilinear setups, so we can cheat */
	job->priv->PP2PA[0] = job->priv->porient[0];
	job->priv->PP2PA[1] = job->priv->porient[1];
	job->priv->PP2PA[2] = job->priv->porient[2];
	job->priv->PP2PA[3] = job->priv->porient[3];
	t = job->priv->pw * job->priv->PP2PA[0] + job->priv->ph * job->priv->PP2PA[2];
	job->priv->PP2PA[4] = (t < 0) ? -t : 0.0;
	t = job->priv->pw * job->priv->PP2PA[1] + job->priv->ph * job->priv->PP2PA[3];
	job->priv->PP2PA[5] = (t < 0) ? -t : 0.0;
	PRINT_AFFINE ("PP2PA:", &job->priv->PP2PA[0]);

	/* PPDP - Physical Page Dimensions in Printer */
	/* A: PhysicalPage X PhysicalOrientation X TRANSLATE -> Physical Page in Printer */
	area.x0 = 0.0;
	area.y0 = 0.0;
	area.x1 = job->priv->pw;
	area.y1 = job->priv->ph;
	art_drect_affine_transform (&r, &area, job->priv->PP2PA);
	job->priv->PAW = r.x1 - r.x0;
	job->priv->PAH = r.y1 - r.y0;
	if ((job->priv->PAW < EPSILON) || (job->priv->PAH < EPSILON))
		return;

	/* Now we have to find the size of layout page */
	/* Again, knowing that layouts are rectilinear helps us */
	art_affine_invert (a, job->priv->affines);
	PRINT_AFFINE ("INV LY:", &a[0]);
	job->priv->LYW = job->priv->lyw * fabs (job->priv->pw * a[0] + job->priv->ph * a[2]);
	job->priv->LYH = job->priv->lyh * fabs (job->priv->pw * a[1] + job->priv->ph * a[3]);
	PRINT_2 ("LY Dimensions:", job->priv->LYW, job->priv->LYH);

	/* Calculate LP2LY */
	/* We allow only rectilinear setups, so we can cheat */
	job->priv->LP2LY[0] = job->priv->lorient[0];
	job->priv->LP2LY[1] = job->priv->lorient[1];
	job->priv->LP2LY[2] = job->priv->lorient[2];
	job->priv->LP2LY[3] = job->priv->lorient[3];
	/* Delay */
	job->priv->LP2LY[4] = 0.0;
	job->priv->LP2LY[5] = 0.0;
	/* Meanwhile find logical width and height */
	area.x0 = 0.0;
	area.y0 = 0.0;
	area.x1 = job->priv->LYW;
	area.y1 = job->priv->LYH;
	art_affine_invert (a, job->priv->LP2LY);
	art_drect_affine_transform (&r, &area, a);
	job->priv->LW = r.x1 - r.x0;
	job->priv->LH = r.y1 - r.y0;
	if ((job->priv->LW < EPSILON) || (job->priv->LH < EPSILON))
		return;
	PRINT_2 ("L Dimensions", job->priv->LW, job->priv->LH);
	/* Now complete matrix calculation */
	t = job->priv->LW * job->priv->LP2LY[0] + job->priv->LH * job->priv->LP2LY[2];
	job->priv->LP2LY[4] = (t < 0) ? -t : 0.0;
	t = job->priv->LW * job->priv->LP2LY[1] + job->priv->LH * job->priv->LP2LY[3];
	job->priv->LP2LY[5] = (t < 0) ? -t : 0.0;
	PRINT_AFFINE ("LP2LY:", &job->priv->LP2LY[0]);

	/* Good, now generate actual layout matrixes */

	job->priv->LY_AFFINES = g_new (gdouble, 6 * job->priv->num_affines);

	/* Extra fun */

	for (i = 0; i < job->priv->num_affines; i++) {
		gdouble ly2p[6];
		gdouble *ly2pa;
		/* Calculate Layout -> Physical Page affine */
		memcpy (ly2p, job->priv->affines + 6 * i, 6 * sizeof (gdouble));
		ly2p[4] *= job->priv->pw;
		ly2p[5] *= job->priv->ph;
		/* PRINT_AFFINE ("Layout -> Physical:", &l2p[0]); */
		art_affine_multiply (job->priv->LY_AFFINES + 6 * i, job->priv->LP2LY, ly2p);
		ly2pa = g_new (gdouble, 6);
		art_affine_multiply (ly2pa, job->priv->LY_AFFINES + 6 * i, job->priv->PP2PA);
		job->priv->LY_LIST = g_list_prepend (job->priv->LY_LIST, ly2pa);
	}

	job->priv->LY_LIST = g_list_reverse (job->priv->LY_LIST);
}

static void
job_parse_config_data (GnomePrintJob *job)
{
	const GnomePrintUnit *unit;
	GPANode *layout;

	g_return_if_fail (job->priv);

	job_clear_config_data (job);

	g_return_if_fail (job->config);

	/* Now the fun part */


	/* Physical size */
	if (gnome_print_config_get_length (job->config, (const guchar *) GNOME_PRINT_KEY_PAPER_WIDTH, &job->priv->pw, &unit)) {
		gnome_print_convert_distance (&job->priv->pw, unit, GNOME_PRINT_PS_UNIT);
	}
	if (gnome_print_config_get_length (job->config, (const guchar *) GNOME_PRINT_KEY_PAPER_HEIGHT, &job->priv->ph, &unit)) {
		gnome_print_convert_distance (&job->priv->ph, unit, GNOME_PRINT_PS_UNIT);
	}
	/* Physical orientation */
	gnome_print_config_get_transform (job->config, (const guchar *) GNOME_PRINT_KEY_PAPER_ORIENTATION_MATRIX, job->priv->porient);
	/* Logical orientation */
	gnome_print_config_get_transform (job->config, (const guchar *) GNOME_PRINT_KEY_PAGE_ORIENTATION_MATRIX, job->priv->lorient);
	/* Layout size */
	gnome_print_config_get_double (job->config, (const guchar *) GNOME_PRINT_KEY_LAYOUT_WIDTH, &job->priv->lyw);
	gnome_print_config_get_double (job->config, (const guchar *) GNOME_PRINT_KEY_LAYOUT_HEIGHT, &job->priv->lyh);

	/* Now come the affines */
	layout = gpa_node_get_child_from_path (GNOME_PRINT_CONFIG_NODE (job->config), (const guchar *) GNOME_PRINT_KEY_LAYOUT);
	if (layout) {
		gint numlp;
		numlp = 0;
		if (gpa_node_get_int_path_value (layout, (const guchar *) "LogicalPages", &numlp) && (numlp > 0)) {
			GPANode *pages;
			pages = gpa_node_get_child_from_path (layout, (const guchar *) "Pages");
			if (pages) {
				GPANode *page;
				gdouble *affines;
				gint pagenum;
				affines = g_new (gdouble, 6 * numlp);
				pagenum = 0;

				while (pagenum < numlp) {
					guchar *transform;
					guchar *child_id;
					child_id = (guchar *) g_strdup_printf ("LP%d", pagenum);
					page = gpa_node_get_child_from_path (pages, child_id);
					transform = gpa_node_get_value (page);
					gpa_node_unref (page);
					if (!transform) {
						g_warning ("Could not fetch transfrom from %s\n", child_id);
						break;
					}
					gnome_print_parse_transform (transform, affines + 6 * pagenum);
					g_free (transform);
					g_free (child_id);
					pagenum += 1;
				}
				gpa_node_unref (pages);
				if (pagenum == numlp) {
					job->priv->num_affines = numlp;
					job->priv->affines = affines;
				} else {
					g_free (affines);
				}
			}
		}
		gpa_node_unref (layout);
	} else {
		job->priv->affines = g_new (gdouble, 6);
		art_affine_identity (job->priv->affines);
		job->priv->num_affines = 1;
	}
}

#define A4_WIDTH (210 * 72 / 25.4)
#define A4_HEIGHT (297 * 72 / 25.4)

static void
job_clear_config_data (GnomePrintJob *job)
{
	g_return_if_fail (job->priv);

	job->priv->pw = A4_WIDTH;
	job->priv->ph = A4_HEIGHT;
	art_affine_identity (job->priv->porient);
	art_affine_identity (job->priv->lorient);
	job->priv->lyw = job->priv->pw;
	job->priv->lyh = job->priv->ph;
	job->priv->num_affines = 0;
	if (job->priv->affines) {
		g_free (job->priv->affines);
		job->priv->affines = NULL;
	}
	if (job->priv->LY_AFFINES) {
		g_free (job->priv->LY_AFFINES);
		job->priv->LY_AFFINES = NULL;
	}
	while (job->priv->LY_LIST) {
		g_free (job->priv->LY_LIST->data);
		job->priv->LY_LIST = g_list_remove (job->priv->LY_LIST, job->priv->LY_LIST->data);
	}
}


/* FIXME: This function should not be here, we should not be including gpa-private.h too */

void
gnome_print_layout_data_free (GnomePrintLayoutData *lyd)
{
	g_return_if_fail (lyd != NULL);

	if (lyd->pages)
		g_free (lyd->pages);
	g_free (lyd);
}

GnomePrintLayout *
gnome_print_layout_new_from_data (const GnomePrintLayoutData *lyd)
{
	GnomePrintLayout *ly;
	ArtDRect area, r;
	gdouble t;
	gdouble a[6];
	gint i;
	/* Layout data */
	gdouble PP2PA[6], LP2LY[6];
	gdouble PAW, PAH, LYW, LYH, LW, LH;

	g_return_val_if_fail (lyd != NULL, NULL);
	g_return_val_if_fail (lyd->num_pages > 0, NULL);
	g_return_val_if_fail (lyd->pages != NULL, NULL);

	/* Now comes the fun part */

	g_return_val_if_fail ((lyd->pw > EPSILON) && (lyd->ph > EPSILON), NULL);

	/* Initial setup */
	/* Calculate PP2PA */
	/* We allow only rectilinear setups, so we can cheat */
	PP2PA[0] = lyd->porient[0];
	PP2PA[1] = lyd->porient[1];
	PP2PA[2] = lyd->porient[2];
	PP2PA[3] = lyd->porient[3];
	t = lyd->pw * PP2PA[0] + lyd->ph * PP2PA[2];
	PP2PA[4] = (t < 0) ? -t : 0.0;
	t = lyd->pw * PP2PA[1] + lyd->ph * PP2PA[3];
	PP2PA[5] = (t < 0) ? -t : 0.0;
	PRINT_AFFINE ("PP2PA:", &PP2PA[0]);

	/* PPDP - Physical Page Dimensions in Printer */
	/* A: PhysicalPage X PhysicalOrientation X TRANSLATE -> Physical Page in Printer */
	area.x0 = 0.0;
	area.y0 = 0.0;
	area.x1 = lyd->pw;
	area.y1 = lyd->ph;
	art_drect_affine_transform (&r, &area, PP2PA);
	PAW = r.x1 - r.x0;
	PAH = r.y1 - r.y0;
	g_return_val_if_fail ((PAW > EPSILON) || (PAH > EPSILON), NULL);

	/* Now we have to find the size of layout page */
	/* Again, knowing that layouts are rectilinear helps us */
	art_affine_invert (a, lyd->pages[0].matrix);
	PRINT_AFFINE ("INV LY:", &a[0]);
	LYW = lyd->lyw * fabs (lyd->pw * a[0] + lyd->ph * a[2]);
	LYH = lyd->lyh * fabs (lyd->pw * a[1] + lyd->ph * a[3]);
	PRINT_2 ("LY Dimensions:", LYW, LYH);

	/* Calculate LP2LY */
	/* We allow only rectilinear setups, so we can cheat */
	LP2LY[0] = lyd->lorient[0];
	LP2LY[1] = lyd->lorient[1];
	LP2LY[2] = lyd->lorient[2];
	LP2LY[3] = lyd->lorient[3];
	/* Delay */
	LP2LY[4] = 0.0;
	LP2LY[5] = 0.0;
	/* Meanwhile find logical width and height */
	area.x0 = 0.0;
	area.y0 = 0.0;
	area.x1 = LYW;
	area.y1 = LYH;
	art_affine_invert (a, LP2LY);
	art_drect_affine_transform (&r, &area, a);
	LW = r.x1 - r.x0;
	LH = r.y1 - r.y0;
	g_return_val_if_fail ((LW > EPSILON) && (LH > EPSILON), NULL);
	PRINT_2 ("L Dimensions", LW, LH);
	/* Now complete matrix calculation */
	t = LW * LP2LY[0] + LH * LP2LY[2];
	LP2LY[4] = (t < 0) ? -t : 0.0;
	t = LW * LP2LY[1] + LH * LP2LY[3];
	LP2LY[5] = (t < 0) ? -t : 0.0;
	PRINT_AFFINE ("LP2LY:", &LP2LY[0]);

	/* So we are safely here and can allocate target */
	ly = g_new (GnomePrintLayout, 1);
	memcpy (ly->PP2PA, PP2PA, 6 * sizeof (gdouble));
	ly->PAW = PAW;
	ly->PAH = PAH;
	memcpy (ly->LP2LY, LP2LY, 6 * sizeof (gdouble));
	ly->LYW = LYW;
	ly->LYH = LYH;
	ly->LW = LW;
	ly->LH = LH;

	/* Good, now generate actual layout matrixes */

	ly->NLY = lyd->num_pages;
	ly->LYP = g_new (GnomePrintLayoutPage, 6);

	/* Extra fun */

	for (i = 0; i < lyd->num_pages; i++) {
		gdouble ly2p[6];
		/* Calculate Layout -> Physical Page affine */
		memcpy (ly2p, lyd->pages[i].matrix, 6 * sizeof (gdouble));
		ly2p[4] *= lyd->pw;
		ly2p[5] *= lyd->ph;
		/* PRINT_AFFINE ("Layout -> Physical:", &l2p[0]); */
		art_affine_multiply (ly->LYP[i].matrix, LP2LY, ly2p);
	}

	return ly;
}

void
gnome_print_layout_free (GnomePrintLayout *layout)
{
	g_return_if_fail (layout != NULL);

	if (layout->LYP)
		g_free (layout->LYP);
	g_free (layout);
}

gboolean
gnome_print_parse_transform (guchar *str, gdouble *transform)
{
	gdouble t[6];
	guchar *p;
	gchar *e;
	gint i;

	art_affine_identity (transform);

	p = str;
	p = (guchar *) strchr ((gchar *) str, '(');
	if (!p)
		return FALSE;
	p += 1;
	if (!*p)
		return FALSE;
	for (i = 0; i < 6; i++) {
		while (*p && g_ascii_isspace (*p)) p += 1;
		if (!strncmp ((gchar *) p, "SQRT2", 5)) {
			t[i] = M_SQRT2;
			e = (gchar *) p + 5;
		} else if (!strncmp ((gchar *) p, "-SQRT2", 6)) {
			t[i] = -M_SQRT2;
			e = (gchar *) p + 6;
		} else if (!strncmp ((gchar *) p, "SQRT1_2", 7)) {
			t[i] = M_SQRT1_2;
			e = (gchar *) p + 7;
		} else if (!strncmp ((gchar *) p, "-SQRT1_2", 8)) {
			t[i] = -M_SQRT1_2;
			e = (gchar *) p + 8;
		} else {
			t[i] = g_ascii_strtod ((gchar *) p, &e);
		}
		if (e == (gchar *) p)
			return FALSE;
		p = (guchar *) e;
	}

	memcpy (transform, t, 6 * sizeof (gdouble));

	return TRUE;
}


/**
 * gnome_print_job_set_print_to_file:
 * @gmp: job
 * @output: output file, if NULL sets print to file to FALSE
 * 
 * Sets/unsets the print to file option for the job 
 * 
 * Return Value: 
 **/
gint
gnome_print_job_print_to_file (GnomePrintJob *job, gchar *output)
{
	if (output) {
		gnome_print_config_set (job->config, (const guchar *) "Settings.Transport.Backend",    (const guchar *) "file");
		gnome_print_config_set (job->config, (const guchar *) GNOME_PRINT_KEY_OUTPUT_FILENAME, (const guchar *) output);
	} else {
		/* In the future we might want to use the default or even better,
		 * go back to the prev. selected printer (Chema)
		 */
		gnome_print_config_set (job->config, (const guchar *) "Settings.Transport.Backend", (const guchar *) "lpr");
	}
	
	return GNOME_PRINT_OK;
}

