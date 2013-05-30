cl -c test_evenbetter.c
cl /O2 /DUSE_SSE2 -c evenbetter-rll.c
link /SUBSYSTEM:CONSOLE /OUT:test_evenbetter.exe test_evenbetter.obj evenbetter-rll.obj _eb_sse2.obj /NODEFAULTLIB:LIBC.lib /NODEFAULTLIB:LIBCMTD.lib LIBCMT.lib shell32.lib comdlg32.lib gdi32.lib user32.lib winspool.lib advapi32.lib
