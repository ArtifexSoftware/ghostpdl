ANALYSE_COMMON_OBJ=ANALYSE$(OBJ)
LEX=c:\cygwin\bin\flex.exe

all:	pscolor$(EXE)


pscolor$(EXE): test.c lex.yy.c
	$(CC) $(CFLAGS) -D_Windows -DYY_NO_UNISTD_H -I../../base -I../../psi -I../../obj -Fepscolor.exe test.c lex.yy.c ../../bin/gsdll32.lib

test:	pscolor$(EXE)
	./pscolor$(EXE) <A.ps

lex.yy.c: instream.yy
	$(LEX) -l -8 instream.yy

#ijs_client_example$(EXE):	ijs_client_example$(OBJ) ijs_client$(OBJ) $(IJS_COMMON_OBJ) $(IJS_EXEC_SERVER)
#	$(CC) $(CFLAGS) $(FE)ijs_client_example$(EXE) ijs_client_example$(OBJ) ijs_client$(OBJ) $(IJS_COMMON_OBJ) $(IJS_EXEC_SERVER) $(LDLIBS)

#ijs_server_example$(EXE):	ijs_server_example$(OBJ) ijs_server$(OBJ) $(IJS_COMMON_OBJ)
#	$(CC) $(CFLAGS) $(FE)ijs_server_example$(EXE) ijs_server_example$(OBJ) ijs_server$(OBJ) $(IJS_COMMON_OBJ) $(LDLIBS)

common_clean:
	$(RM) *$(OBJ) pscolor$(EXE)

