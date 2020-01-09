#include <test-common.h>

#include <libgnomeprint/gnome-print-job.h>

int
main (int argc, char **argv)
{
	GnomePrintJob *job;
	GnomePrintConfig *config;
	GnomePrintContext *context;
	guint i;

	g_type_init ();
	g_log_set_always_fatal (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL);

	job = gnome_print_job_new (NULL);
	g_object_get (G_OBJECT (job), "config", &config, NULL);
	g_object_get (G_OBJECT (job), "context", &context, NULL);

	gnome_print_config_set (config, (const guchar *) "Printer",
			(const guchar *) "GENERIC");
	gnome_print_config_set (config, (const guchar *) GNOME_PRINT_KEY_PAPER_SIZE,
			(const guchar *) "A4");

	for (i = 0; i < 5; i++)
		test_print_page (context, i);
	gnome_print_job_close (job);

	gnome_print_job_print_to_file (job, "o.ps");
	gnome_print_job_print (job);
	g_message ("Please check output in file 'o.ps'. "
			"It should contain 5 pages.");

	gnome_print_config_set (config,
			(const guchar *) GNOME_PRINT_KEY_LAYOUT,
			(const guchar *) "4_1");
	gnome_print_job_print_to_file (job, "o4.ps");
	gnome_print_job_print (job);
	g_message ("Please check output in file 'o4.ps'. "
			"It should contain 2 pages.");

	g_object_unref (G_OBJECT (job));

	return 0;
}
