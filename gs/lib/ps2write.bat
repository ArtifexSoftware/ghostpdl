@rem $Id$
@rem Converting Postscript 3 or PDF to PostScript 2.

%GSC% -dBATCH -dNOPAUSE -sDEVICE=pdfwrite -sOutputFile=temp.pdf %more_param% -c mark /ForOPDFRead true /PreserveHalftoneInfo true /TransferFunctionInfo /Preserve /MaxViewerMemorySize 8000000 /CompressPages false /CompressFonts false /ASCII85EncodePages true .dicttomark setpagedevice -f %1
copy /b %GS_LIBPATH%opdfread.ps+%GS_LIBPATH%gs_agl.ps+%GS_LIBPATH%gs_mro_e.ps+%GS_LIBPATH%gs_mgl_e.ps+temp.pdf+%GS_LIBPATH%EndOfTask.ps %2
