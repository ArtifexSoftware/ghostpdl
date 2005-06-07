@rem $Id$
@rem Converting Postscript 3 or PDF into PostScript 2.

set invoke0=%GSC% -dBATCH -dNOPAUSE -dNOOUTERSAVE -sDEVICE=ps2write -sOutputFile=%2 %more_param% 
set invoke1=-c mark /PreserveHalftoneInfo true /TransferFunctionInfo /Preserve 
set invoke2=/MaxViewerMemorySize 8000000 /CompressPages false /CompressFonts false /ASCII85EncodePages true 
set invoke3=%more_deviceparam% .dicttomark setpagedevice -f %more_param1%
set invoke=%invoke0% %invoke1% %invoke2% %invoke3%

if %jobserver%. == yes. goto s
%invoke%  %1
goto j
:s
%invoke% -c false 0 startjob pop -f - < %1
:j
