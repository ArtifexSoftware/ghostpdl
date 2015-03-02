
%{

/*************************************************************************
*
* This software module was originally contributed by Microsoft
* Corporation in the course of development of the
* ITU-T T.832 | ISO/IEC 29199-2 ("JPEG XR") format standard for
* reference purposes and its performance may not have been optimized.
*
* This software module is an implementation of one or more
* tools as specified by the JPEG XR standard.
*
* ITU/ISO/IEC give You a royalty-free, worldwide, non-exclusive
* copyright license to copy, distribute, and make derivative works
* of this software module or modifications thereof for use in
* products claiming conformance to the JPEG XR standard as
* specified by ITU-T T.832 | ISO/IEC 29199-2.
*
* ITU/ISO/IEC give users the same free license to this software
* module or modifications thereof for research purposes and further
* ITU/ISO/IEC standardization.
*
* Those intending to use this software module in products are advised
* that its use may infringe existing patents. ITU/ISO/IEC have no
* liability for use of this software module or modifications thereof.
*
* Copyright is not released for products that do not conform to
* to the JPEG XR standard as specified by ITU-T T.832 |
* ISO/IEC 29199-2.
*
* Microsoft Corporation retains full right to modify and use the code
* for its own purpose, to assign or donate the code to a third party,
* and to inhibit third parties from using the code for products that
* do not conform to the JPEG XR standard as specified by ITU-T T.832 |
* ISO/IEC 29199-2.
* 
* This copyright notice must be included in all copies or derivative
* works.
* 
* Copyright (c) ITU-T/ISO/IEC 2008.
**********************************************************************/

#ifdef _MSC_VER
#pragma comment (user,"$Id: qp_lexor.lex,v 1.6 2008/03/14 16:08:04 steve Exp $")
#else
#ident "$Id: qp_lexor.lex,v 1.6 2008/03/14 16:08:04 steve Exp $"
#endif

# include  "qp.tab.h"
# include  "jxr_priv.h"

%}

%option noyywrap
%option batch
%option never-interactive
%option nounput

  /* Windows VidualStudio does not have a <unistd.h> header file, */
  /* so we want the nounistd option to prevent it. However, this  */
  /* option doesn't seem to work properly, so we instead rely on  */
  /* "sed" to remove the include from the */
  /* generated code. */
  /* %option nounistd */

%%


[ \t\r\n] { /* Skip white space */; }
"#".* { /* Skip comments */; }

"DC" { return K_DC; }
"LP" { return K_LP; }
"HP" { return K_HP; }

"channel" { return K_CHANNEL; }
"independent" { return K_INDEPENDENT; }
"separate" { return K_SEPARATE; }
"tile" { return K_TILE; }
"uniform" { return K_UNIFORM; }

[0-9]+ { qp_lval.number = strtoul(yytext, 0, 0); return NUMBER; }

. { return yytext[0]; }

%%
