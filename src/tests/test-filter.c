#include <libgnomeprint/gnome-print-filter.h>
#include <libgnomeprint/gnome-print-job.h>

#include "test-common.h"

int
main (int argc, char **argv)
{
	GnomePrintConfig *config;
	GnomePrintJob *job;
	GnomePrintContext *pc = NULL;
	guint i;

	g_type_init ();
	g_log_set_always_fatal (G_LOG_LEVEL_CRITICAL);

	config = gnome_print_config_default ();
	gnome_print_config_set (config, (const guchar *) "Printer",
			(const guchar *) "GENERIC");
	gnome_print_config_set (config, (const guchar *) GNOME_PRINT_KEY_PAPER_SIZE,
			(const guchar *) "A4");
	gnome_print_config_set (config,
			(const guchar *) "Settings.Output.Job.Filter",
			(const guchar *) "GnomePrintFilterReorder order=3,0,1,2");

	job = gnome_print_job_new (config);
	g_object_get (G_OBJECT (job), "context", &pc, NULL);
	for (i = 0; i < 4; i++)
		test_print_page (pc, i);
	gnome_print_job_close (job);
	g_object_unref (config);
	gnome_print_job_print_to_file (job, "o.ps");
	gnome_print_job_print (job);
	g_object_unref (G_OBJECT (job));

	return 0;
}
