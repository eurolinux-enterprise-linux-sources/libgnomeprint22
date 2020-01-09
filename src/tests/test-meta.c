#include <libgnomeprint/gnome-print-meta.h>

#include <math.h>
#include <stdio.h>

int
main (int argc, char **argv)
{
	GnomePrintContext *pc, *npc;
	guint i;
	FILE *f;
	gint length;
	const guchar *data;

	g_type_init ();

	pc = g_object_new (GNOME_TYPE_PRINT_META, NULL);
	for (i = 0; i < 5; i++) {
		gchar *txt;
		gdouble d, e, a;

		gnome_print_beginpage (pc, (const guchar *) "test");
		gnome_print_setrgbcolor (pc, 0., 0., 0.);
		gnome_print_rect_stroked (pc, 0., 0., 100., 100.);
		gnome_print_gsave (pc);
		gnome_print_translate (pc, 50., 50.);
		gnome_print_scale (pc, 50., 50.);
		for (a = 0; a < M_PI; a += M_PI_4 / 4.) {
			d = cos (a);
			e = sqrt (1 - d * d);
			gnome_print_line_stroked (pc, d, e, -d, -e);
		}
		gnome_print_grestore (pc);
		gnome_print_gsave (pc);
		gnome_print_moveto (pc, 10., 100.);
		gnome_print_scale (pc, 5., 5.);
		gnome_print_show (pc, (const guchar *) "Test page");
		gnome_print_grestore (pc);
		gnome_print_moveto (pc, 10., 10.);
		gnome_print_scale (pc, 30., 30.);
		txt = g_strdup_printf ("%i", i + 1);
		gnome_print_show (pc, (const guchar *) txt);
		g_free (txt);

		gnome_print_showpage (pc);
	}

	length = gnome_print_meta_get_length (GNOME_PRINT_META (pc));
	data = gnome_print_meta_get_buffer (GNOME_PRINT_META (pc));
	g_print ("Writing %i bytes to file 'o.meta'... ", length);
	f = fopen ("o.meta", "w");
	fwrite (data, 1, length, f);
	fclose (f);
	g_print ("Done.\n");

	npc = g_object_new (GNOME_TYPE_PRINT_META, NULL);
	gnome_print_meta_render (GNOME_PRINT_META (pc), npc);
	g_object_unref (G_OBJECT (pc));
	g_object_unref (G_OBJECT (npc));

	return 0;
}
