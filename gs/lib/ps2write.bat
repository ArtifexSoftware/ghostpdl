@rem $Id$
@rem Converting Postscript 3 or PDF to PostScript 2.

%GSC% -dBATCH -dNOPAUSE -sDEVICE=pdfwrite -sOutputFile=temp.pdf %more_param% -c mark /CompatibilityLevel 1.2 /OrderResources true /PSVersion 2017 /MaxInlineImage 2147483647 /CompressPages false /CompressFonts false .dicttomark setpagedevice -f %1
copy /b %GS_LIBPATH%opdfread.ps+%GS_LIBPATH%gs_agl.ps+%GS_LIBPATH%gs_mro_e.ps+%GS_LIBPATH%gs_mex_e.ps+%GS_LIBPATH%gs_mgl_e.ps+%GS_LIBPATH%gs_wan_e.ps+temp.pdf %2
