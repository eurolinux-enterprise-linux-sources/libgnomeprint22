%{
#include <glib.h>

#include <libgnomeprint/types.h>
#include <libgnomeprint/grammar.tab.h>

#define YY_DECL int _gnome_print_filter_parse_yylex (YYSTYPE *lvalp)
%}

_operator "["|"]"|[!{}]
_identifier [[:alpha:]][[:alnum:]\-_%:]*
_char ("\\".)|([^[:space:]])
_string {_char}+|("\""([^\"]|"\\\"")*"\"")|("'"([^']|"\\\"")*"'")
_assign [[:space:]]*"="[[:space:]]*
_assignment {_identifier}{_assign}{_string}

%x value
%option noyywrap
%option nounput
%%

{_assignment} {
	lvalp->s = yytext;
	BEGIN (INITIAL);
	return ASSIGNMENT;
}

{_identifier} {
	lvalp->s = yytext;
	BEGIN (INITIAL);
	return IDENTIFIER;
}

{_operator} { return *yytext; }

[[:space:]]+ { }

%%
