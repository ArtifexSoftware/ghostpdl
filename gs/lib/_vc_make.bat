cd ..\gs 
c:\progra~1\devstudio\VC\bin\nmake /F _vc_temp.mak CONFIG=5 gsargs.obj gsnogc.obj echogs.exe 
c:\progra~1\devstudio\VC\bin\nmake /F _vc_temp.mak CONFIG=5 ld5.tr gconfig5.obj gscdefs5.obj 
..\gs\echogs.exe -w _wm_cdir.bat cd -s -r _vc_dir.bat 
_wm_cdir.bat 
