
all:	bin/pscolor.exe


bin/pscolor.exe: test.c lex.yy.c
	cl -D_Windows -DYY_NO_UNISTD_H -I..\..\obj -I..\..\base -I..\..\psi -Febin/pscolor.exe test.c lex.yy.c ../../bin/gsdll32.lib

test:	bin/pscolor.exe
	./bin/pscolor.exe <input.ps

lex.yy.c: instream.yy
	flex -l -8 instream.yy
