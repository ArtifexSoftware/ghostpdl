# MS-Windows
CC=cl
CFLAGS=-W3 -Zi
LDLIBS=
OBJ=.obj
EXE=.exe
FE=/Fe
RM=-del

!include "common.mak"

clean: common_clean
	$(RM) *.ilk *.pdb *.opt
