%{
#include <glib.h>

#include <string.h>

#include <libgnomeprint/types.h>
#include <libgnomeprint/gnome-print-filter.h>

#define YYPARSE_PARAM graph

static void
set_value_from_string (GParamSpec *pspec, GValue *v, const gchar *s)
{
	g_return_if_fail (pspec);
	g_return_if_fail (v);
	g_return_if_fail (s);

	g_value_init (v, G_PARAM_SPEC_VALUE_TYPE (pspec));
	if (G_VALUE_HOLDS_INT (v))
		g_value_set_int (v, atoi (s));
	else if (G_VALUE_HOLDS_UINT (v))
		g_value_set_uint (v, atoi (s));
	else if (G_VALUE_HOLDS_DOUBLE (v))
		g_value_set_double (v, atof (s));
	else if (G_VALUE_HOLDS_BOOLEAN (v)) {
		if (!strcmp (s, "true"))
			g_value_set_boolean (v, TRUE);
		else if (!strcmp (s, "false"))
			g_value_set_boolean (v, FALSE);
		else
			g_value_set_boolean (v, atoi (s));
	} else if (G_VALUE_HOLDS_STRING (v))
		g_value_set_string (v, s);
	else if (G_VALUE_HOLDS (v, G_TYPE_VALUE_ARRAY)) {
		GValueArray *va = g_value_array_new (0);
		const gchar *begin = s, *end = s;

		while (*end != '\0') {
			GValue vd = {0,};
			gchar *pv;

			while ((*end != '\0') && (*end != ',')) end++;
			pv = g_strndup (begin, end - begin);
			set_value_from_string (((GParamSpecValueArray *) pspec)->element_spec, &vd, pv);
			g_free (pv);
			g_value_array_append (va, &vd);
			g_value_unset (&vd);
			if (*end != '\0') {
				end++;
				begin = end;
			}
		}
		g_value_set_boxed (v, va);
		g_value_array_free (va);
	} else {
		g_warning ("Not implemented (%s %s)!", s, g_type_name (
					G_PARAM_SPEC_VALUE_TYPE (pspec)));
	}
}

static void
gnome_print_filter_parse_prop (GnomePrintFilter *f, const gchar *prop, GError **e)
{
	GParamSpec *pspec;
	const gchar *p = prop;
	gchar *prop_name, *prop_value;
	GValue v = {0,};

	g_return_if_fail (GNOME_IS_PRINT_FILTER (f));
	g_return_if_fail (prop);

	while (*p != '=') p++;
	prop_name = g_strndup (prop, p - prop);
	pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (G_OBJECT (f)), prop_name);
	if (!pspec) {
		if (e && !*e) *e = g_error_new (GNOME_PRINT_FILTER_ERROR,
				GNOME_PRINT_FILTER_ERROR_SYNTAX,
				"filter '%s' does not have a property called '%s'",
				G_OBJECT_TYPE_NAME (G_OBJECT (f)), prop_name);
		g_free (prop_name);
		return;
	}
	g_free (prop_name);
	if (*p != '=') {
		if (e && !*e) *e = g_error_new (GNOME_PRINT_FILTER_ERROR,
				GNOME_PRINT_FILTER_ERROR_SYNTAX,
				"no value for property '%s' has been supplied", pspec->name);
		return;
	}
	p++;
	if (*p == '"' || *p == '\'')
		prop_value = g_strndup (p + 1, strlen (p + 1) - 1);
	else
		prop_value = g_strdup (p);
	set_value_from_string (pspec, &v, prop_value);
	g_free (prop_value);
	g_object_set_property (G_OBJECT (f), pspec->name, &v);
	g_value_unset (&v);
}

static int yylex (void *lvalp);
static int yyerror (const char *s);
%}

%union {
	gchar *s;
	GnomePrintFilter *f;
	pool_t *p;
	graph_t *g;
}

%token <s> IDENTIFIER
%token <s> ASSIGNMENT

%type <g> graph
%type <f> filter
%type <p> pool

%pure_parser

%start graph
%%

pool: filter {
			$$ = g_new0 (pool_t, 1);
			$$->a = g_ptr_array_sized_new (1);
			g_ptr_array_add ($$->a, $1);
		}
	| pool '{' filter '}' {
			$$ = $1;
			g_ptr_array_add ($$->a, $3);
		}
	| '{' filter '}' {
			$$ = g_new0 (pool_t, 1);
			$$->a = g_ptr_array_sized_new (1);
			g_ptr_array_add ($$->a, $2);
		};
filter: filter '!' pool {
			guint i;
			GnomePrintFilter *f;

			for (i = 0; i < $3->a->len; i++) {
				f = g_ptr_array_index ($3->a, i);
				gnome_print_filter_append_predecessor (f, $1);
				g_object_unref (G_OBJECT (f));
			}
			g_ptr_array_free ($3->a, TRUE);
			g_free ($3);
			$$ = $1;
		}
	| filter '!' filter {
			gnome_print_filter_append_predecessor ($3, $1);
			g_object_unref (G_OBJECT ($3));
			$$ = $1;
		}
  | filter '[' pool ']' {
			guint i;

			for (i = 0; i < $3->a->len; i++)
				gnome_print_filter_add_filter ($1,
					g_ptr_array_index ($3->a, i));
			g_ptr_array_free ($3->a, TRUE);
			g_free ($3);
			$$ = $1;
		}
	| IDENTIFIER {
			$$ = gnome_print_filter_new_from_module_name ($1, NULL);
			if (!$$) {
				((graph_t *) graph)->error = g_error_new (
					GNOME_PRINT_FILTER_ERROR, GNOME_PRINT_FILTER_ERROR_SYNTAX,
					"filter '%s' is unknown", $1);
				YYERROR;
			}
		}
	| filter ASSIGNMENT {
			gnome_print_filter_parse_prop ($1, $2, &((graph_t *) graph)->error);
			if (((graph_t *) graph)->error)
				YYERROR;
			$$ = $1;
		};
graph: filter {
			((graph_t *) graph)->filter = $1;
		};

%%

static int
yyerror (const char *s)
{
	return -1;
}

int                     _gnome_print_filter_parse_yylex            (YYSTYPE *lvalp);
struct yy_buffer_state *_gnome_print_filter_parse_yy_scan_string   (char *);
void                    _gnome_print_filter_parse_yy_delete_buffer (struct yy_buffer_state *);

static int
yylex (void *lvalp)
{
	return _gnome_print_filter_parse_yylex ((YYSTYPE *) lvalp);
}

GnomePrintFilter *
_gnome_print_filter_parse_launch (const gchar *description, GError **e)
{
	graph_t g;
	gchar *d;
	struct yy_buffer_state *buf;

	g_return_val_if_fail (description, NULL);

	memset (&g, 0, sizeof (g));

	d = g_strdup (description);
	buf = _gnome_print_filter_parse_yy_scan_string (d);
	g_free (d);
	yyparse (&g);
	_gnome_print_filter_parse_yy_delete_buffer (buf);

	if (e) *e = g.error;
	return g.filter;
}
