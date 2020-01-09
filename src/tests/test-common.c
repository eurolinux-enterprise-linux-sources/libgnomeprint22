#include "test-common.h"

void
test_print_page (GnomePrintContext *pc, guint page)
{
	gint i, j;
        gint max_i = 10;
        gint max_j = 10;
        gint size, spacing, x, y;
        GnomeFont *font;
        gchar *txt;
 
				txt = g_strdup_printf ("Test page %i", page);
        gnome_print_beginpage (pc, (const guchar *) txt);
				g_free (txt);
         
        font = gnome_font_find_closest ((const guchar *) "Times New Roman", 44);
        gnome_print_setfont (pc, font);
 
        gnome_print_moveto (pc, 50, 50);
        gnome_print_show (pc, (const guchar *) "Print Preview test");
 
        txt = g_strdup_printf ("Page %i", page);
        gnome_print_moveto (pc, 50, 800);
        gnome_print_show (pc, (const guchar *) txt);
        g_free (txt);
 
        gnome_print_moveto (pc, 50, 100);
        gnome_print_lineto (pc, 450, 100);
        gnome_print_lineto (pc, 450, 150);
        gnome_print_lineto (pc, 50, 150);
        gnome_print_closepath (pc);
        gnome_print_fill (pc);

        gnome_print_setrgbcolor (pc, 1, 1, 1);
        gnome_print_moveto (pc, 55, 105);
        gnome_print_show (pc, (const guchar *) "Inverted text");
 
        gnome_print_setrgbcolor (pc, 1, 0, 0);
        gnome_print_moveto (pc, 55, 160);
        gnome_print_show (pc, (const guchar *) "Red");
 
        gnome_print_setrgbcolor (pc, 0, 1, 0);
        gnome_print_moveto (pc, 155, 160);
        gnome_print_show (pc, (const guchar *) "Green");
 
        gnome_print_setrgbcolor (pc, 0, 0, 1);
        gnome_print_moveto (pc, 305, 160);
        gnome_print_show (pc, (const guchar *) "Blue");
         
        for (i = 0; i < max_i; i++) {
                for (j = 0; j < max_j; j++) {
                        gdouble r;
                        gdouble g;
                        gdouble b;
 
                        r = ((gdouble) i) / ((gdouble) max_i);
                        g = ((gdouble) j) / ((gdouble) max_j);
                        b = 1 - r;
 
                        size = 15;
                        spacing = size + 2;
                        x = 100;
                        y = 250;
 
                        gnome_print_setrgbcolor (pc, r, g, b);
                        gnome_print_moveto (pc, x + (i * spacing), y + (j * spacing));
                        gnome_print_lineto (pc, x + (i * spacing), y + (j * spacing) + size);
                        gnome_print_lineto (pc, x + (i * spacing) + size, y + (j * spacing) + size);
                        gnome_print_lineto (pc, x + (i * spacing) + size, y + (j * spacing));
                        gnome_print_closepath (pc);
                        gnome_print_fill (pc);
                }
        }
 
        for (i = 0; i < max_i; i++) {
                for (j = 0; j < max_j; j++) {
                        gdouble r;
                        gdouble g;
                        gdouble b;
 
                        r = ((gdouble) i) / ((gdouble) max_i);
                        g = ((gdouble) j) / ((gdouble) max_j);
                        b = 1 - r;
 
                        size = 15;
                        spacing = size + 2;
                        x = 300;
                        y = y;
 
                        gnome_print_setrgbcolor (pc, r, g, b);
                        gnome_print_moveto (pc, x + (i * spacing), y + (j * spacing));
                        gnome_print_lineto (pc, x + (i * spacing), y + (j * spacing) + size);
                        gnome_print_lineto (pc, x + (i * spacing) + size, y + (j * spacing) + size);
                        gnome_print_lineto (pc, x + (i * spacing) + size, y + (j * spacing));
                        gnome_print_closepath (pc);
                        gnome_print_stroke (pc);
                }
        }
 
 
        for (i = 0; i < max_i; i++) {
                for (j = 0; j < max_j; j++) {
                        gdouble r;
                        gdouble g;
                        gdouble b;
 
                        r = ((gdouble) i) / ((gdouble) max_i);
                        g = r;
                        b = r;
 
                        size = 15;
                        spacing = size + 2;
                        x = 100;
                        y = 450;
 
                        gnome_print_setrgbcolor (pc, r, g, b);
                        gnome_print_moveto (pc, x + (i * spacing), y + (j * spacing));
                        gnome_print_lineto (pc, x + (i * spacing), y + (j * spacing) + size);
                        gnome_print_lineto (pc, x + (i * spacing) + size, y + (j * spacing) + size);
                        gnome_print_lineto (pc, x + (i * spacing) + size, y + (j * spacing));
                        gnome_print_closepath (pc);
                        gnome_print_stroke (pc);
                }
        }
 
        for (i = 0; i < max_i; i++) {
                for (j = 0; j < max_j; j++) {
                        gdouble r;
                        gdouble g;
                        gdouble b;
 
                        r = ((gdouble) i) / ((gdouble) max_i);
                        g = r;
                        b = r;
 
                        size = 15;
                        spacing = size + 2;
                        x = 300;
                        y = 450;
 
                        gnome_print_setrgbcolor (pc, r, g, b);
                        gnome_print_moveto (pc, x + (i * spacing), y + (j * spacing));
                        gnome_print_lineto (pc, x + (i * spacing), y + (j * spacing) + size);
                        gnome_print_lineto (pc, x + (i * spacing) + size, y + (j * spacing) + size);
                        gnome_print_lineto (pc, x + (i * spacing) + size, y + (j * spacing));
                        gnome_print_closepath (pc);
                        gnome_print_fill (pc);
                }
        }

				gnome_print_setopacity (pc, 0.3);
				gnome_print_setrgbcolor (pc, 255., 0., 0.);
				gnome_print_rect_filled (pc, 240., 50., 100., 450.);
        gnome_print_showpage (pc);
}
