%option prefix="yy"
%{
/*
  Scanner zum Einlesen von DSC Postscript Dateien
  von Carsten Hammer Hammer.Carsten@oce.de
 */
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#define YY_DECL int yylex(char *buf,int *len)
%}
%x PAGES
%%
<INITIAL>.*\n			strncpy(buf,yytext,*len);*len=yyleng;return(1001);
<INITIAL>.*\n/"%%Page: 1 1"	BEGIN(PAGES);strncpy(buf,yytext,*len);*len=yyleng;return(1001);
<PAGES>\n			;
<PAGES>.*\n			strncpy(buf,yytext,*len);return(yyleng>*len?*len:yyleng);
<PAGES>.*\n/"%%Page:"		strncpy(buf,yytext,*len);*len=yyleng;return(1000);
%%

int yywrap(){
return(1);
}
