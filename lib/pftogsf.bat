@echo off

rem ******************************
rem * Convert .pf? files to .gsf *
rem ******************************

call "%~dp0gssetgs.bat"
echo (wrfont.ps) run (unprot.ps) run unprot >"%TEMP%\_temp_.ps"
echo systemdict /definefont. /definefont load put >>"%TEMP%\_temp_.ps"
echo systemdict /definefont { userdict /LFN 3 index put definefont. } bind put >>"%TEMP%\_temp_.ps"
echo ARGUMENTS 0 get (r) file .loadfont LFN findfont setfont prunefont reprot >>"%TEMP%\_temp_.ps"
echo ARGUMENTS 1 get (w) file dup writefont closefile quit >>"%TEMP%\_temp_.ps"
rem for %%f in (cyr cyri) do %GSC% -P- -dSAFER -q -dNODISPLAY -dWRITESYSTEMDICT -- _temp_.ps fonts\pfa\%%f.pfa fonts\%%f.gsf
rem for %%f in (ncrr ncrb ncrri ncrbi) do %GSC% -P- -dSAFER -q -dNODISPLAY -dWRITESYSTEMDICT -- _temp_.ps fonts\pfa\%%f.pfa fonts\%%f.gsf
rem for %%f in (bchr bchb bchri bchbi) do %GSC% -P- -dSAFER -q -dNODISPLAY -dWRITESYSTEMDICT -- _temp_.ps fonts\pfa\%%f.pfa fonts\%%f.gsf
rem for %%f in (putr putb putri putbi) do %GSC% -P- -dSAFER -q -dNODISPLAY -dWRITESYSTEMDICT -- _temp_.ps fonts\pfa\%%f.pfa fonts\%%f.gsf
rem for %%f in (n019003l n021003l u003043t u004006t) do %GSC% -P- -dSAFER -q -dNODISPLAY -dWRITESYSTEMDICT -- _temp_.ps fonts\%%f.gsf %%f.gsf
for %%f in (hig_____ kak_____) do %GSC% -P- -dSAFER -q -dNODISPLAY -dWRITESYSTEMDICT -- "%TEMP%\_temp_.ps" fonts\pfb\%%f.pfb %%f.gsf
rem %GSC% -P- -dSAFER -q -dNODISPLAY -dWRITESYSTEMDICT -- _temp_.ps allfonts\baxter.pfb baxter.gsf
if exist "%TEMP%\_temp_.ps" erase "%TEMP%\_temp_.ps"
