$ ! Copyright (C) 1997, 1998 Aladdin Enterprises.
$ ! All rights reserved.
$ !
$ ! This file is part of Aladdin Ghostscript.
$ !
$ ! Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
$ ! or distributor accepts any responsibility for the consequences of using it,
$ ! or for whether it serves any particular purpose or works at all, unless he
$ ! or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
$ ! License (the "License") for full details.
$ !
$ ! Every copy of Aladdin Ghostscript must include a copy of the License,
$ ! normally in a plain ASCII text file named PUBLIC.  The License grants you
$ ! the right to copy, modify and redistribute Aladdin Ghostscript, but only
$ ! under certain conditions described in the License.  Among other things, the
$ ! License requires that the copyright notice and this notice be preserved on
$ ! all copies.
$ !
$ !
$ ! VMS "makefile" for Ghostscript.
$ !
$ WSO = "WRITE SYS$OUTPUT"
$ ON ERROR THEN GOTO DONE
$ ON CONTROL_Y THEN GOTO DONE
$ !
$ ! Version numbers.
$ ! MINOR0 is different from MINOR only if MINOR is a single digit.
$ !
$ GS_VERSION_MAJOR = "5"
$ GS_VERSION_MINOR = "14"
$ GS_VERSION_MINOR0 = "14"
$ ! Revision date: year x 10000 + month x 100 + day.
$ GS_REVISIONDATE  = "19980616"
$ ! Derived values
$ GS_VERSION       = GS_VERSION_MAJOR + GS_VERSION_MINOR0
$ GS_DOT_VERSION   = GS_VERSION_MAJOR + "." + GS_VERSION_MINOR
$ GS_REVISION      = GS_VERSION
$ JSRCDIR = "[.jpeg-6a]"
$ JVERSION = 6
$!!! PSRCDIR = "[.libpng-0_89c]"
$!!! PVERSION = 89
$ PSRCDIR = "[.libpng-0_96]"
$ PVERSION = 96
$ ZSRCDIR = "[.zlib-1_0_4]"
$
$ !
$ ! Check input parameters
$ !
$ IF P1 .NES. "" .AND. P1 .NES. "DEBUG" .AND. P1 .NES. "LINK" .AND. -
   P1 .NES. "BUILD" .AND. F$EXTRACT(0,5,P1) .NES. "FROM=" THEN -
   GS_LIB_DEFAULT = P1
$ IF P2 .NES. "" .AND. P2 .NES. "DEBUG" .AND. P2 .NES. "LINK" .AND. -
   P2 .NES. "BUILD" .AND. F$EXTRACT(0,5,P2) .NES. "FROM=" THEN -
   GS_LIB_DEFAULT = P2
$ !
$ IF P1 .NES. "DEBUG" .AND. P2 .NES. "DEBUG" THEN GOTO NODEBUG
$ CC_QUAL = CC_QUAL + "/NOOPT/DEBUG"
$ IF CC_DEF .NES. "" THEN CC_DEF= CC_DEF + ","
$ CC_DEF= CC_DEF + "DEBUG"
$ L_QUAL  = (L_QUAL-"/NOTRACEBACK") + "/DEBUG"
$ !
$ NODEBUG:
$ IF CC_DEF .NES. "" THEN CC_QUAL= CC_QUAL + "/DEFINE=(" + CC_DEF + ")"
$ If P1 .EQS. "LINK" .OR. P2 .EQS. "LINK" THEN GOTO LINK_ONLY
$ If P1 .EQS. "BUILD" .OR. P2 .EQS. "BUILD" THEN GOTO BUILD_EXES
$ FROM = ""
$ IF F$EXTRACT(0,5,P1) .EQS. "FROM=" THEN FROM = F$EXTRACT(5,F$LENGTH(P1)-5,P1)
$ IF F$EXTRACT(0,5,P2) .EQS. "FROM=" THEN FROM = F$EXTRACT(5,F$LENGTH(P2)-5,P2)
$ INSREP = "INSERT"
$ IF FROM .NES. "" THEN INSREP = "REPLACE"
$ !
$ !
$ ! Compile and link genarch.c and then run it to create the arch.h header file
$ !
$ WSO "''CC_COMMAND'''CC_QUAL'/NOLIST/OBJECT=GENARCH.OBJ GENARCH.C"
$ 'CC_COMMAND''CC_QUAL'/NOLIST/OBJECT=GENARCH.OBJ GENARCH.C
$ LINK/NOMAP/EXE=GENARCH.EXE GENARCH.OBJ,RTL.OPT/OPT
$ GENARCH = "$" + F$ENVIRONMENT("DEFAULT") + "GENARCH.EXE"
$ GENARCH ARCH.H
$ DELETE GENARCH.EXE.*,GENARCH.OBJ.*
$ PURGE ARCH.H
$ !
$ !
$ ! Compile and link echogs.c; define ECHOGS as a command
$ !
$ WSO "''CC_COMMAND'''CC_QUAL'/NOLIST/OBJECT=ECHOGS.OBJ ECHOGS.C"
$ 'CC_COMMAND''CC_QUAL'/NOLIST/OBJECT=ECHOGS.OBJ ECHOGS.C
$ LINK/NOMAP/EXE=ECHOGS.EXE ECHOGS.OBJ,RTL.OPT/OPT
$ ECHOGS = "$" + F$ENVIRONMENT("DEFAULT") + "ECHOGS.EXE"
$ DELETE ECHOGS.OBJ;*
$ PURGE ECHOGS.EXE
$ !
$ !
$ ! Compile and link genconf.c; define GENCONF as a command
$ !
$ WSO "''CC_COMMAND'''CC_QUAL/NOLIST/OBJECT=GENCONF.OBJ GENCONF.C"
$ 'CC_COMMAND''CC_QUAL'/NOLIST/OBJECT=GENCONF.OBJ GENCONF.C
$ LINK/NOMAP/EXE=GENCONF.EXE GENCONF.OBJ,RTL.OPT/OPT
$ GENCONF = "$" + F$ENVIRONMENT("DEFAULT") + "GENCONF.EXE"
$ DELETE GENCONF.OBJ;*
$ PURGE GENCONF.EXE
$ !
$ !
$ ! Create GSSETDEV.COM, GSSETMOD.COM, and GSADDMOD.COM.
$ ! (These aren't distributed with the fileset because the .COM suffix
$ ! causes certain development tools to treat them incorrectly.)
$ !
$ !
$ OPEN/WRITE SETDEV GSSETDEV.COM
$   WRITE SETDEV "$ echogs -w 'p1'.dev - -dev 'p1' -obj 'p2 'p3 'p4 'p5 'p6 'p7 'p8"
$ CLOSE SETDEV
$ !
$ OPEN/WRITE SETMOD GSSETMOD.COM
$   WRITE SETMOD "$ if p2 .nes. """""
$   WRITE SETMOD "$ then"
$   WRITE SETMOD "$   echogs -w 'p1'.dev - 'p2 'p3 'p4 'p5 'p6 'p7 'p8"
$   WRITE SETMOD "$ else"
$   WRITE SETMOD "$   echogs -w 'p1'.dev"
$   WRITE SETMOD "$ endif"
$ CLOSE SETMOD
$ !
$ OPEN/WRITE ADDMOD GSADDMOD.COM
$   WRITE ADDMOD "$ if (p2 .nes. """") then echogs -a 'p1'.dev - 'p2 'p3 'p4 'p5 'p6 'p7 'p8"
$ CLOSE ADDMOD
$ !
$ !
$ ! Define SETDEV, SETMOD, ADDMOD as commands to execute GSSETDEV.COM,
$ ! GSSETMOD.COM and GSADDMOD.COM respectively.  Those three command
$ ! procedures make use of ECHOGS.EXE and the ECHOGS verb.
$ !
$ SETDEV = "@GSSETDEV.COM"
$ SETMOD = "@GSSETMOD.COM"
$ ADDMOD = "@GSADDMOD.COM"
$ !
$ !
$ ! Build GCONFIG_.H
$ !
$ ECHOGS -w gconfig_.h #define HAVE_SYS_TIME_H
$ !
$ ! Build GCONFIGV.H
$ !
$ ECHOGS -w gconfigv.h #define USE_ASM 0
$ ECHOGS -a gconfigv.h #define USE_FPU 1
$ ECHOGS -a gconfigv.h #define EXTEND_NAMES 0
$ !
$ ! Now generate *.dev files
$ !
$ DEV_LIST_NAMES = "FEATURE_DEVS DEVICE_DEVS DEVICE_DEVS1 DEVICE_DEVS2 DEVICE_DEVS3 DEVICE_DEVS4 DEVICE_DEVS5 DEVICE_DEVS6 DEVICE_DEVS7 DEVICE_DEVS8 DEVICE_DEVS9 DEVICE_DEVS10 DEVICE_DEVS11 DEVICE_DEVS12 DEVICE_DEVS13 DEVICE_DEVS14 DEVICE_DEVS15"
$ NDEV_MOD = 0
$ DEV_MODULES = " "
$ I = 0
$ ON WARNING THEN GOTO NO_ACTION
$ DEVS_OUTER:
$   DEV_LIST = F$ELEMENT(I," ",DEV_LIST_NAMES)
$   IF DEV_LIST .EQS. " " THEN GOTO DEVS_DONE
$   I = I+1
$   IF F$TYPE('DEV_LIST') .EQS. "" THEN GOTO DEVS_OUTER
$   IF F$EDIT('DEV_LIST',"COLLAPSE") .EQS. "" THEN GOTO DEVS_OUTER
$   J = 0
$   DEVS_INNER:
$     ACTION = F$ELEMENT(J," ",'DEV_LIST')
$     IF ACTION .EQS. " " THEN GOTO DEVS_OUTER
$     J = J+1
$     ! Replace "." with "_"
$     IF F$LOCATE(".",ACTION) .NE. F$LENGTH(ACTION) THEN -
$       ACTION = F$ELEMENT(0,".",ACTION) + "_" + F$ELEMENT(1,".",ACTION)
$     GOSUB 'ACTION'
$   GOTO DEVS_INNER
$ !
$ DEVS_DONE:
$ ON WARNING THEN CONTINUE
$ !
$ !
$ ! And now build gconfig.h and gconfigf.h
$ !
$ GOSUB GCONFIG_H
$ GOSUB GCONFIGF_H
$ !
$ DEV_MODULES = F$EDIT(DEV_MODULES,"TRIM")
$ DEV_MODULES'NDEV_MOD' = F$EDIT(DEV_MODULES,"COMPRESS")
$ IF F$LENGTH(DEV_MODULES'NDEV_MOD') .EQ. 0 THEN NDEV_MOD = NDEV_MOD - 1
$ !
$ ! Create an empty object library if compilation is started from the beginning.
$ !
$ IF FROM .EQS. "" THEN LIBRARY/CREATE GS.OLB
$ !
$ ! Now compile device modules found in the DEV_MODULES symbols
$ !
$ DEFDIR = F$PARSE(F$ENVIRONMENT("DEFAULT"),,,"DIRECTORY","SYNTAX_ONLY")
$ OPEN/WRITE OPT_FILE GS.OPT
$ OPT_LINE = "GS.OLB/LIBRARY/INCLUDE=("
$ COMMA = ""
$ !
$ I = 0
$ IDEV = 0
$ OUTER_DEV_LOOP:
$ DEV_MODULES = F$EDIT(DEV_MODULES'IDEV',"TRIM")
$ COMPILE_LOOP:
$   MODULE = F$ELEMENT(I," ",DEV_MODULES)
$   IF MODULE .EQS. " " THEN GOTO END_COMPILE
$   I = I + 1
$   NAME = F$PARSE(MODULE,,,"NAME","SYNTAX_ONLY")
$   IF NAME .EQS. FROM THEN FROM= ""
$   IF FROM .NES. "" THEN GOTO MOD_COMPILED
$   DIR  = F$PARSE(MODULE,,,"DIRECTORY","SYNTAX_ONLY") - DEFDIR
$   IF DIR .NES. ""
$   THEN
$     COPY 'MODULE'.C []
$     INC = "/INCLUDE=(''DEFDIR',''DIR')"
$   ELSE
$     INC = ""
$   ENDIF
$   WSO "''CC_COMMAND'''CC_QUAL'''INC'/NOLIST/OBJECT=''NAME'.OBJ ''MODULE'.C"
$   'CC_COMMAND''CC_QUAL''INC'/NOLIST/OBJECT='NAME'.OBJ 'NAME'.C
$   LIBRARY/'INSREP' GS.OLB 'NAME'.OBJ
$   DELETE 'NAME'.OBJ.*
$   IF DIR .NES. "" THEN DELETE 'NAME'.C;
$ MOD_COMPILED:
$   IF F$LENGTH(OPT_LINE) .GE. 70
$   THEN
$     OPT_LINE = OPT_LINE + COMMA + "-"
$     WRITE OPT_FILE OPT_LINE
$     OPT_LINE = NAME
$   ELSE
$     OPT_LINE = OPT_LINE + COMMA + NAME
$   ENDIF
$   COMMA = ","
$ GOTO COMPILE_LOOP
$ !
$ END_COMPILE:
$ I = 0
$ IDEV = IDEV + 1
$ IF IDEV .LE. NDEV_MOD THEN GOTO OUTER_DEV_LOOP
$ !
$ OPT_LINE = OPT_LINE + ")"
$ WRITE OPT_FILE OPT_LINE
$ IF F$SEARCH("SYS$SHARE:DECW$XMLIBSHR12.EXE") .NES. ""
$ THEN
$   WRITE OPT_FILE "SYS$SHARE:DECW$XMLIBSHR12.EXE/SHARE"
$   WRITE OPT_FILE "SYS$SHARE:DECW$XTLIBSHRR5.EXE/SHARE"
$   WRITE OPT_FILE "SYS$SHARE:DECW$XLIBSHR.EXE/SHARE"
$ ELSE
$   WRITE OPT_FILE "SYS$SHARE:DECW$XMLIBSHR.EXE/SHARE"
$   WRITE OPT_FILE "SYS$SHARE:DECW$XTSHR.EXE/SHARE"
$   WRITE OPT_FILE "SYS$SHARE:DECW$XLIBSHR.EXE/SHARE"
$ ENDIF
$ WRITE OPT_FILE "Ident=""gs ''GS_DOT_VERSION'"""
$ CLOSE OPT_FILE
$ !
$ !
$ ! Is the DECwindows environment about?  Must be installed in order to
$ ! build the executable program gs.exe.
$ !
$ IF F$SEARCH("SYS$SHARE:DECW$XLIBSHR.EXE") .NES. "" THEN GOTO CHECK2
$ WSO "DECwindows user environment not installed;"
$ WSO "unable to build executable programs."
$ GOTO DONE
$ !
$ CHECK2:
$ IF F$TRNLNM("DECW$INCLUDE") .NES. "" THEN GOTO BUILD_EXES
$ WSO "You must invoke @DECW$STARTUP before using this"
$ WSO "command procedure to build the executable programs."
$ GOTO DONE
$ !
$ ! Build the executables
$ !
$ BUILD_EXES:
$ !
$ DEFINE X11 DECW$INCLUDE
$ !
$ LINK_ONLY:
$ WSO "''CC_COMMAND'''CC_QUAL'/NOLIST/OBJECT=GCONFIG.OBJ GCONF.C"
$ 'CC_COMMAND''CC_QUAL/NOLIST/OBJECT=GCONFIG.OBJ GCONF.C
$ !
$ WSO "''CC_COMMAND'''CC_QUAL'/NOLIST/OBJECT=ICONFIG.OBJ ICONF.C"
$ 'CC_COMMAND''CC_QUAL/NOLIST/OBJECT=ICONFIG.OBJ ICONF.C
$ !
$ WSO "''CC_COMMAND'''CC_QUAL'/NOLIST/OBJECT=ICCINIT0.OBJ ICCINIT0.C"
$ 'CC_COMMAND''CC_QUAL/NOLIST/OBJECT=ICCINIT0.OBJ ICCINIT0.C
$ !
$ WSO "''CC_COMMAND'''CC_QUAL'/NOLIST/OBJECT=GSCDEFS.OBJ GSCDEF.C"
$ 'CC_COMMAND''CC_QUAL/NOLIST/OBJECT=GSCDEFS.OBJ GSCDEF.C
$ !
$ WSO "''CC_COMMAND'''CC_QUAL'/NOLIST/OBJECT=GS.OBJ GS.C"
$ 'CC_COMMAND''CC_QUAL/NOLIST/OBJECT=GS.OBJ GS.C
$ !
$ WSO "Linking ... "
$ WSO "LINK''L_QUAL'/NOMAP/EXE=GS.EXE GS,GCONFIG,ICONFIG,ICCINIT0,GSCDEFS,GS.OPT/OPT"
$ LINK'L_QUAL/NOMAP/EXE=GS.EXE GS,GCONFIG,ICONFIG,ICCINIT0,GSCDEFS,GS.OPT/OPT
$ !
$ IF P1 .EQS. "LINK" THEN GOTO AFTER_DEL
$ DELETE GS.OBJ.*,GCONFIG.OBJ.*,ICONFIG.OBJ.*,ICCINIT0.OBJ.*,GSCDEFS.OBJ.*
$ !
$ GOTO DONE
$ !
$ !
$ DEVS_TR:
$ ! quote the dashes so that they are not interpreted as continuation
$ ! marks when the following DCL symbol is not defined!
$   ECHOGS -w devs.tr "-" -include vms_.dev
$   ECHOGS -a devs.tr "-" 'FEATURE_DEVS'
$   ECHOGS -a devs.tr "-" 'DEVICE_DEVS'
$   ECHOGS -a devs.tr "-" 'DEVICE_DEVS1'
$   ECHOGS -a devs.tr "-" 'DEVICE_DEVS2'
$   ECHOGS -a devs.tr "-" 'DEVICE_DEVS3'
$   ECHOGS -a devs.tr "-" 'DEVICE_DEVS4'
$   ECHOGS -a devs.tr "-" 'DEVICE_DEVS5'
$   ECHOGS -a devs.tr "-" 'DEVICE_DEVS6'
$   ECHOGS -a devs.tr "-" 'DEVICE_DEVS7'
$   ECHOGS -a devs.tr "-" 'DEVICE_DEVS8'
$   ECHOGS -a devs.tr "-" 'DEVICE_DEVS9'
$   ECHOGS -a devs.tr "-" 'DEVICE_DEVS10'
$   ECHOGS -a devs.tr "-" 'DEVICE_DEVS11'
$   ECHOGS -a devs.tr "-" 'DEVICE_DEVS12'
$   ECHOGS -a devs.tr "-" 'DEVICE_DEVS13'
$   ECHOGS -a devs.tr "-" 'DEVICE_DEVS14'
$   ECHOGS -a devs.tr "-" 'DEVICE_DEVS15'
$   RETURN
$ !
$ GCONFIG_H:
$   GOSUB DEVS_TR
$   GOSUB VMS_DEV
$   GOSUB LIBCORE_DEV
$   GENCONF "devs.tr" libcore.dev -h gconfig.h
$   ECHOGS -a gconfig.h "#define GS_LIB_DEFAULT ""''GS_LIB_DEFAULT'"""
$   ECHOGS -a gconfig.h "#define SEARCH_HERE_FIRST ''SEARCH_HERE_FIRST'"
$   ECHOGS -a gconfig.h "#define GS_DOCDIR ""''GS_DOCDIR'"""
$   ECHOGS -a gconfig.h "#define GS_INIT ""''GS_INIT'"""
$   ECHOGS -a gconfig.h "#define GS_REVISION ''GS_REVISION'"
$   ECHOGS -a gconfig.h "#define GS_REVISIONDATE ''GS_REVISIONDATE'"
$   RETURN
$ !
$ BBOX_DEV:
$   SETMOD bbox "gdevbbox.obj"
$   ADD_DEV_MODULES = "gdevbbox.obj"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! The render/RGB device is only here as an example, but we can configure
$! it as a real device for testing.
$ RRGB_DEV:
$   SETDEV rrgb "gdevrrgb.obj"
$   ADD_DEV_MODULES = "gdevrrgb.obj gdevprn.obj"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ LIBS_DEV:
$   LIB1s = "gsalloc.obj gsbitops.obj gsbittab.obj"
$   LIB2s = "gschar.obj gscolor.obj gscoord.obj gsdevice.obj gsdevmem.obj"
$   LIB3s = "gsdparam.obj gsfont.obj gsht.obj gshtscr.obj"
$   LIB4s = "gsimage.obj gsimpath.obj gsinit.obj gsiodev.obj"
$   LIB5s = "gsline.obj gsmatrix.obj gsmemory.obj gsmisc.obj"
$   LIB6s = "gspaint.obj gsparam.obj gspath.obj gsstate.obj gsutil.obj"
$   LIBs  = "''LIB1s' ''LIB2s' ''LIB3s' ''LIB4s' ''LIB5s' ''LIB6s'"
$   echogs -w libs.dev 'LIB1s'
$   echogs -a libs.dev 'LIB2s'
$   echogs -a libs.dev 'LIB3s'
$   echogs -a libs.dev 'LIB4s'
$   echogs -a libs.dev 'LIB5s'
$   echogs -a libs.dev 'LIB6s'
$   ADDMOD libs -init gscolor
$   ADD_DEV_MODULES = "''LIBs'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ LIBX_DEV:
$   LIB1x = "gxacpath.obj gxbcache.obj"
$   LIB2x = "gxccache.obj gxccman.obj gxcht.obj gxcmap.obj gxcpath.obj"
$   LIB3x = "gxdcconv.obj gxdcolor.obj gxdither.obj gxfill.obj gxht.obj"
$   LIB4x = "gximage.obj gximage0.obj gximage1.obj gximage2.obj"
$   LIB5x = "gxpaint.obj gxpath.obj gxpath2.obj gxpcopy.obj"
$   LIB6x = "gxpdash.obj gxpflat.obj gxsample.obj gxstroke.obj"
$   LIBx  = "''LIB1x' ''LIB2x' ''LIB3x' ''LIB4x' ''LIB5x' ''LIB6x'"
$   echogs -w libx.dev 'LIB1x'
$   echogs -a libx.dev 'LIB2x'
$   echogs -a libx.dev 'LIB3x'
$   echogs -a libx.dev 'LIB4x'
$   echogs -a libx.dev 'LIB5x'
$   echogs -a libx.dev 'LIB6x'
$   addmod libx -init gximage1 gximage2
$   ADD_DEV_MODULES = "''LIBx'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ LIBD_DEV:
$   LIB1d = "gdevabuf.obj gdevddrw.obj gdevdflt.obj gdevnfwd.obj"
$   LIB2d = "gdevmem.obj gdevm1.obj gdevm2.obj gdevm4.obj gdevm8.obj"
$   LIB3d = "gdevm16.obj gdevm24.obj gdevm32.obj gdevmpla.obj"
$   LIBd  = "''LIB1d' ''LIB2d' ''LIB3d'"
$   echogs -w libd.dev 'LIB1d'
$   echogs -a libd.dev 'LIB2d'
$   echogs -a libd.dev 'LIB3d'
$   ADD_DEV_MODULES = "''LIBd'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ LIBCORE_DEV:
$   GOSUB LIBS_DEV
$   GOSUB LIBX_DEV
$   GOSUB LIBD_DEV
$   GOSUB ISCALE_DEV
$   GOSUB ROPLIB_DEV
$   SETMOD libcore
$   ADDMOD libcore -dev nullpage
$   ADDMOD libcore -include libs libx libd iscale roplib
$   RETURN
$ !
$! ---------------- File streams ---------------- #
$ SFILE_DEV:
$   sfile_="sfx''FILE_IMPLEMENTATION'.obj stream.obj"
$   SETMOD sfile "''sfile_'"
$   ADD_DEV_MODULES = "''sfile_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ CFE_DEV:
$   IF F$TYPE(cfe_) .NES. "" THEN RETURN
$   cfe_ = "scfe.obj scfetab.obj shc.obj"
$   SETMOD cfe 'cfe_'
$   ADD_DEV_MODULES = "''cfe_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ CFD_DEV:
$   IF F$TYPE(cfd_) .NES. "" THEN RETURN
$   cfd_ = "scfd.obj scfdtab.obj"
$   SETMOD cfd 'cfd_'
$   ADD_DEV_MODULES = "''cfd_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! ---------------- DCT (JPEG) filters ---------------- #
$! These are used by Level 2, and by the JPEG-writing driver.
$! Encoding (compression)
$ SDCTE_DEV: 
$   IF F$TYPE(sdcte_) .NES. "" THEN RETURN
$   sdcte_ = "sdctc.obj sjpegc.obj sdcte.obj sjpege.obj"
$   GOSUB JPEGE_DEV
$   SETMOD sdcte 'sdcte_'
$   ADDMOD sdcte -include jpege
$   ADD_DEV_MODULES = "''sdcte_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! Decoding (decompression)
$ SDCTD_DEV:
$   IF F$TYPE(sdctd_) .NES. "" THEN RETURN
$   sdctd_="sdctc.obj sjpegc.obj sdctd.obj sjpegd.obj"
$   GOSUB JPEGD_DEV
$   SETMOD sdctd 'sdctd_'
$   ADDMOD sdctd -include jpegd
$   ADD_DEV_MODULES = "''sdctd_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! ---------------- LZW filters ---------------- #
$! These are used by Level 2 in general.
$ LZWE_DEV:
$   IF F$TYPE(lzwe_) .NES. "" THEN RETURN
$   lzwe_ = "slzwce.obj slzwc.obj"
$   SETMOD lzwe 'lzwe_'
$   ADD_DEV_MODULES = "''lzwe_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ SLZWE_DEV:
$   COPY LZWE.DEV SLZWE.DEV
$   RETURN
$ !
$ LZWD_DEV:
$   IF F$TYPE(lzwd_) .NES. "" THEN RETURN
$   lzwd_ = "slzwd.obj slzwc.obj"
$   SETMOD lzwd 'lzwd_'
$   ADD_DEV_MODULES = "''lzwd_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ SLZWD_DEV:
$   COPY LZWD.DEV SLZWD.DEV
$   RETURN
$ !
$ PCXD_DEV:
$   pcxd_ = "spcxd.obj"
$   SETMOD pcxd 'pcxd_'
$   ADD_DEV_MODULES = "''pcxd_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ PDIFF_DEV:
$   pdiff_  = "spdiff.obj"
$   SETMOD pdiff 'pdiff_'
$   ADD_DEV_MODULES = "''pdiff_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ PNGP_DEV:
$   pngp_ = "spngp.obj"
$   SETMOD pngp 'pngp_'
$   ADD_DEV_MODULES = "''pngp_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ RLE_DEV:
$   IF F$TYPE(rle_) .NES. "" THEN RETURN
$   rle_ = "srle.obj"
$   SETMOD rle 'rle_'
$   ADD_DEV_MODULES = "''rle_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ RLD_DEV:
$   IF F$TYPE(rld_) .NES. "" THEN RETURN
$   rld_ = "srld.obj"
$   SETMOD rld 'rld_'
$   ADD_DEV_MODULES = "''rld_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ SZLIBE_DEV:
$   IF F$TYPE(szlibe_) .NES. "" THEN RETURN
$   szlibe_ = "szlibc.obj szlibe.obj"
$   GOSUB ZLIBE_DEV
$   SETMOD szlibe 'szlibe_'
$   ADDMOD szlibe -include zlibe
$   ADD_DEV_MODULES = "''szlibe_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ SZLIBD_DEV:
$   IF F$TYPE(szlibd_) .NES. "" THEN RETURN
$   szlibd_ = "szlibc.obj szlibd.obj"
$   GOSUB ZLIBD_DEV
$   SETMOD szlibd 'szlibd_'
$   ADDMOD szlibd -include zlibd
$   ADD_DEV_MODULES = "''szlibd_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! Command list package.  Currently the higher-level facilities are required,
$! but eventually they will be optional.
$ CLIST_DEV:
$   IF F$TYPE(CLIST_DONE) .NES. "" THEN RETURN
$   GOSUB CLBASE_DEV
$   GOSUB CLPATH_DEV
$   SETMOD clist -include clbase clpath
$   CLIST_DONE = 1
$   RETURN
$ !
$! Base command list facility
$ CLBASE_DEV:
$   IF F$TYPE(clbase_) .NES. "" THEN RETURN
$   clbase1_ = "gxclist.obj gxclbits.obj gxclpage.obj"
$   clbase2_ = "gxclread.obj gxclrect.obj stream.obj"
$   clbase_ = "''clbase1_' ''clbase2_'"
$   GOSUB CL'BAND_LIST_STORAGE'_DEV
$   GOSUB CFE_DEV
$   GOSUB CFD_DEV
$   GOSUB RLE_DEV
$   GOSUB RLD_DEV
$   SETMOD clbase 'clbase1_'
$   ADDMOD clbase -obj 'clbase2_'
$   ADDMOD clbase -include cl'BAND_LIST_STORAGE' cfe cfd rle rld
$   ADD_DEV_MODULES = "''clbase_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! Higher-level command list facilities.
$ CLPATH_DEV:
$   IF F$TYPE(clpath_) .NES. "" THEN RETURN
$   clpath_ = "gxclimag.obj gxclpath.obj"
$   GOSUB PSL2CS_DEV
$   SETMOD clpath 'clpath_'
$   ADDMOD clpath -include psl2cs
$   ADDMOD clpath -init climag clpath
$   ADD_DEV_MODULES = "''clpath_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ CLFILE_DEV:
$   IF F$TYPE(clfile_) .NES. "" THEN RETURN
$   clfile_ = "gxclfile.obj"
$   SETMOD clfile 'clfile_'
$   ADD_DEV_MODULES = "''clfile_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ CLMEMORY_DEV:
$   IF F$TYPE(clmemory_) .NES. "" THEN RETURN
$   clmemory_ = "gxclmem.obj gxcl'BAND_LIST_COMPRESSOR'.obj"
$   GOSUB S'BAND_LIST_COMPRESSOR'E_DEV
$   GOSUB S'BAND_LIST_COMPRESSOR'D_DEV
$   SETMOD clmemory 'clmemory_'
$   ADDMOD clmemory -include S'BAND_LIST_COMPRESSOR'E S'BAND_LIST_COMPRESSOR'D
$   ADDMOD clmemory init cl_'BAND_LIST_COMPRESSOR'
$   ADD_DEV_MODULES = "''clmemory_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ VECTOR_DEV:
$   IF F$TYPE(vector_) .NES. "" THEN RETURN
$   vector_ = "gdevvec.obj"
$   GOSUB BBOX_DEV
$   GOSUB SFILE_DEV
$   SETMOD vector "''vector_'"
$   ADDMOD vector -include bbox sfile
$   ADD_DEV_MODULES = "''vector_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ ISCALE_DEV:
$   iscale_ = "siscale.obj"
$   SETMOD iscale 'iscale_'
$   ADD_DEV_MODULES = "''iscale_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ ROPLIB_DEV:
$   roplib_ = "gdevmrop.obj gsrop.obj gsroptab.obj"
$   SETMOD roplib 'roplib_'
$   ADDMOD roplib -init roplib
$   ADD_DEV_MODULES = "''roplib_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! -------- Composite (PostScript Type 0) font support -------- #
$ CMAPLIB_DEV:
$   cmaplib_="gsfcmap.obj"
$   SETMOD cmaplib "''cmaplib_'"
$   ADD_DEV_MODULES = "''cmaplib_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ PSF0LIB_DEV:
$   psf0lib_ = "gschar0.obj gsfont0.obj"
$   GOSUB CMAPLIB_DEV
$   SETMOD psf0lib 'psf0lib_'
$   ADDMOD psf0lib -include cmaplib
$   ADD_DEV_MODULES = "''psf0lib_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! ---------------- Pattern color ---------------- #
$ PATLIB_DEV:
$   patlib_ = "gspcolor.obj gxclip2.obj gxpcmap.obj"
$   GOSUB CMYKLIB_DEV
$   GOSUB PSL2CS_DEV
$   SETMOD patlib -include cmyklib psl2cs
$   ADDMOD patlib -obj 'patlib_'
$   ADD_DEV_MODULES = "''patlib_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! ---------------- PostScript Type 1 (and Type 4) fonts ---------------- #
$ TYPE1LIB_INI:
$   IF F$TYPE(type1lib_) .NES. "" THEN RETURN
$   type1lib_= "gxtype1.obj gxhint1.obj gxhint2.obj gxhint3.obj"
$   ADD_DEV_MODULES = "''type1lib_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$!  Type 1 charstrings
$ PSF1LIB_DEV:
$   IF F$TYPE(psf1lib_) .NES. "" THEN RETURN
$   GOSUB TYPE1LIB_INI
$   psf1lib_ = "gstype1.obj"
$   SETMOD psf1lib 'psf1lib_'
$   ADDMOD psf1lib 'type1lib_'
$   ADDMOD psf1lib -init gstype1
$   ADD_DEV_MODULES = "''psf1lib_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! Type 2 charstrings
$ PSF2LIB_DEV:
$   IF F$TYPE(psf2lib_) .NES. "" THEN RETURN
$   GOSUB TYPE1LIB_INI
$   psf2lib_ = "gstype2.obj"
$   SETMOD psf2lib 'psf2lib_'
$   ADDMOD psf2lib 'type1lib_'
$   ADDMOD psf2lib -init gstype2
$   ADD_DEV_MODULES = "''psf2lib_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! ---------------- TrueType and PostScript Type 42 fonts ---------------- #
$ TTFLIB_DEV:
$   IF F$TYPE(ttflib_) .NES. "" THEN RETURN
$   ttflib_ = "gstype42.obj"
$   SETMOD ttflib 'ttflib_'
$   ADD_DEV_MODULES = "''ttflib_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ CMYKLIB_DEV:
$   IF F$TYPE(cmyklib_) .NES. "" THEN RETURN
$   cmyklib_ = "gscolor1.obj gsht1.obj"
$   SETMOD cmyklib 'cmyklib_'
$   ADDMOD cmyklib -init gscolor1
$   ADD_DEV_MODULES = "''cmyklib_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ COLIMLIB_DEV:
$   IF F$TYPE(colimlib_) .NES. "" THEN RETURN
$   colimlib_ = "gximage3.obj"
$   SETMOD colimlib 'colimlib_'
$   ADDMOD colimlib -init gximage3
$   ADD_DEV_MODULES = "''colimlib_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ HSBLIB_DEV:
$   hsblib_ = "gshsb.obj"
$   SETMOD hsblib 'hsblib_'
$   ADD_DEV_MODULES = "''hsblib_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ PATH1LIB_DEV:
$   path1lib_ = "gspath1.obj"
$   SETMOD path1lib 'path1lib_'
$   ADD_DEV_MODULES = "''path1lib_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ PSL2CS_DEV:
$   IF F$TYPE(psl2cs_) .NES. "" THEN RETURN
$   psl2cs_ = "gscolor2.obj"
$   SETMOD psl2cs 'psl2cs_'
$   ADD_DEV_MODULES = "''psl2cs_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ PSL2LIB_DEV:
$   IF F$TYPE(psl2lib_) .NES. "" THEN RETURN
$   psl2lib_ = "gximage4.obj gximage5.obj"
$   GOSUB COLIMLIB_DEV
$   GOSUB PSL2CS_DEV
$   SETMOD psl2lib 'psl2lib_'
$   ADDMOD psl2lib -init gximage4 gximage5
$   ADDMOD psl2lib -include colimlib psl2cs
$   ADD_DEV_MODULES = "''psl2lib_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! ---------------- Display Postscript / Level 2 support ---------------- #
$ DPS2LIB_DEV:
$   IF F$TYPE(dps2lib_) .NES. "" THEN RETURN
$   dps2lib_ = "gsdps1.obj"
$   SETMOD dps2lib 'dps2lib_'
$   ADD_DEV_MODULES = "''dps2lib_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! ---------------- Display Postscript extensions ---------------- #
$ DPSLIB_DEV:
$   IF F$TYPE(dpslib_) .NES. "" THEN RETURN
$   dpslib_ = "gsdps.obj"
$   SETMOD dpslib 'dpslib_'
$   ADD_DEV_MODULES = "''dpslib_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! ---------------- CIE color ---------------- #
$ CIELIB_DEV:
$   IF F$TYPE(cielib_) .NES. "" THEN RETURN
$   cielib_ = "gscie.obj gxctable.obj"
$   SETMOD cielib 'cielib_'
$   ADD_DEV_MODULES = "''cielib_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! ---------------- Separation colors ---------------- #
$ SEPRLIB_DEV:
$   seprlib_ = "gscsepr.obj"
$   SETMOD seprlib 'seprlib_'
$   ADD_DEV_MODULES = "''seprlib_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! ---------------- Functions ---------------- #
$! Generic support, and FunctionType 0.
$ FUNCLIB_DEV:
$   IF F$TYPE(funclib_) .NES. "" THEN RETURN
$   funclib_ = "gsdsrc.obj gsfunc.obj gsfunc0.obj"
$   SETMOD funclib "''funclib_'"
$   ADD_DEV_MODULES = "''funclib_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ ISUPPORT_DEV:
$   IF F$TYPE(isupport1_) .NES. "" THEN RETURN
$   isupport1_ = "ialloc.obj igc.obj igcref.obj igcstr.obj"
$   isupport2_ = "ilocate.obj iname.obj isave.obj"
$   isupport_  = "''isupport1_' ''isupport2_'"
$   SETMOD isupport 'isupport1_'
$   ADDMOD isupport -obj 'isupport2_'
$   ADDMOD isupport -init igcref
$   ADD_DEV_MODULES = "''isupport_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ PSBASE_DEV:
$   IF F$TYPE(INT_CONFIG) .NES. "" THEN RETURN
$   INT1  = "idebug.obj idict.obj idparam.obj"
$   INT2  = "iinit.obj interp.obj iparam.obj ireclaim.obj"
$   INT3  = "iscan.obj iscannum.obj istack.obj iutil.obj"
$   INT4  = "scantab.obj sfilter1.obj sstring.obj stream.obj"
$   Z1    = "zarith.obj zarray.obj zcontrol.obj zdict.obj"
$   Z1OPS = "zarith zarray zcontrol zdict"
$   Z2    = "zfile.obj zfileio.obj zfilter.obj zfname.obj zfproc.obj"
$   Z2OPS = "zfile zfileio zfilter zfproc"
$   Z3    = "zgeneric.obj ziodev.obj zmath.obj zmisc.obj zpacked.obj"
$   Z3OPS = "zgeneric ziodev zmath zmisc zpacked"
$   Z4    = "zrelbit.obj zstack.obj zstring.obj zsysvm.obj"
$   Z4OPS = "zrelbit zstack zstring zsysvm"
$   Z5    = "ztoken.obj ztype.obj zvmem.obj"
$   Z5OPS = "ztoken ztype zvmem"
$   Z6    = "zchar.obj zcolor.obj zdevice.obj zfont.obj zfont2.obj"
$   Z6OPS = "zchar zcolor zdevice zfont zfont2"
$   Z7    = "zgstate.obj zht.obj zimage.obj zmatrix.obj zpaint.obj zpath.obj"
$   Z7OPS = "zgstate zht zimage zmatrix zpaint zpath"
$   INT_OBJS = "imainarg.obj gsargs.obj imain.obj ''INT1' ''INT2' ''INT3' ''INT4'"
$   Z_OBJS   = "''Z1' ''Z2' ''Z3' ''Z4' ''Z5' ''Z6' ''Z7'"
$   INT_CONFIG = "gconfig.obj gscdefs.obj iconfig.obj iccinit0.obj"
$   GOSUB ISUPPORT_DEV
$   GOSUB RLD_DEV
$   GOSUB RLE_DEV
$   GOSUB SFILE_DEV
$   SETMOD psbase imainarg.obj gsargs.obj imain.obj
$   ADDMOD psbase -obj 'INT_CONFIG'
$   ADDMOD psbase -obj 'INT1'
$   ADDMOD psbase -obj 'INT2'
$   ADDMOD psbase -obj 'INT3'
$   ADDMOD psbase -obj 'INT4'
$   ADDMOD psbase -obj 'Z1'
$   ADDMOD psbase -oper 'Z1OPS'
$   ADDMOD psbase -obj 'Z2'
$   ADDMOD psbase -oper 'Z2OPS'
$   ADDMOD psbase -obj 'Z3'
$   ADDMOD psbase -oper 'Z3OPS'
$   ADDMOD psbase -obj 'Z4'
$   ADDMOD psbase -oper 'Z4OPS'
$   ADDMOD psbase -obj 'Z5'
$   ADDMOD psbase -oper 'Z5OPS'
$   ADDMOD psbase -obj 'Z6'
$   ADDMOD psbase -oper 'Z6OPS'
$   ADDMOD psbase -obj 'Z7'
$   ADDMOD psbase -oper 'Z7OPS'
$   ADDMOD psbase -iodev stdin stdout stderr lineedit statementedit
$   ADDMOD psbase -include isupport rld rle sfile
$   ADD_DEV_MODULES = "''INT_OBJS'"
$   GOSUB ADD_DEV_MOD
$   ADD_DEV_MODULES = "''Z_OBJS'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ LEVEL1_DEV:
$   GOSUB PSBASE_DEV
$   GOSUB BCP_DEV
$   GOSUB HSB_DEV
$   GOSUB PATH1_DEV
$   GOSUB TYPE1_DEV
$   SETMOD level1 -include psbase bcp hsb path1 type1
$   ADDMOD level1 -emulator """PostScript""" """PostScriptLevel1"""
$   RETURN
$ !
$ COLOR_DEV:
$   IF F$TYPE(COLOR_DONE) .NES. "" THEN RETURN
$   GOSUB CMYKLIB_DEV
$   GOSUB COLIMLIB_DEV
$   GOSUB CMYKREAD_DEV
$   SETMOD color -include cmyklib colimlib cmykread
$   COLOR_DONE = 1
$   RETURN
$ !
$ CMYKREAD_DEV:
$   IF F$TYPE(cmykread_) .NES. "" THEN RETURN
$   cmykread_ = "zcolor1.obj zht1.obj"
$   SETMOD cmykread 'cmykread_'
$   ADDMOD cmykread -oper zcolor1 zht1
$   ADD_DEV_MODULES = "''cmykread_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ HSB_DEV:
$   hsb_ = "zhsb.obj"
$   GOSUB HSBLIB_DEV
$   SETMOD hsb 'hsb_'
$   ADDMOD hsb -include hsblib
$   ADDMOD hsb -oper zhsb
$   ADD_DEV_MODULES = "''hsb_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ PATH1_DEV:
$   path1_ = "zpath1.obj"
$   GOSUB PATH1LIB_DEV
$   SETMOD path1 'path1_'
$   ADDMOD path1 -include path1lib
$   ADDMOD path1 -oper zpath1
$   ADD_DEV_MODULES = "''path1_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ BCP_DEV:
$   bcp_ = "sbcp.obj zfbcp.obj"
$   SETMOD bcp 'bcp_'
$   ADDMOD bcp -oper zfbcp
$   ADD_DEV_MODULES = "''bcp_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ DISKFONT_DEV:
$   SETMOD diskfont -ps gs_diskf
$   RETURN
$ !
$ DOUBLE_DEV:
$   IF F$TYPE(double_) .NES. "" THEN RETURN
$   double_ = "zdouble.obj"
$   SETMOD double 'double_'
$   ADDMOD double -oper zdouble
$   ADD_DEV_MODULES = "''double_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ EPSF_DEV:
$   SETMOD epsf -ps gs_epsf
$   RETURN
$ !
$! ---------------- PostScript Type 1 (and Type 4) fonts ---------------- #
$ TYPE1_DEV:
$   IF F$TYPE(TYPE1_DONE) .NES. "" THEN RETURN
$   GOSUB PSF1LIB_DEV
$   GOSUB PSF1READ_DEV
$   SETMOD type1 -include psf1lib psf1read
$   TYPE1_DONE = 1
$   RETURN
$ !
$ PSF1READ_DEV:
$   IF F$TYPE(psf1read_) .NES. "" THEN RETURN
$   psf1read_ = "seexec.obj zchar1.obj zcharout.obj zfont1.obj zmisc1.obj"
$   SETMOD psf1read 'psf1read_'
$   ADDMOD psf1read -oper zchar1 zfont1 zmisc1
$   ADDMOD psf1read -ps gs_type1
$   ADD_DEV_MODULES = "''psf1read_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! ------------- Compact Font Format and Type 2 charstrings ------------- #
$ CFF_DEV:
$   GOSUB PSL2INT_DEV
$   SETMOD cff -ps gs_cff
$   RETURN
$!
$ TYPE2_DEV:
$   GOSUB TYPE1_DEV
$   GOSUB PSF2LIB_DEV
$   SETMOD type2 -include psf2lib
$   RETURN
$!
$! ---------------- TrueType and PostScript Type 42 fonts ---------------- #
$! Native TrueType support
$ TTFONT_DEV:
$   GOSUB TYPE42_DEV
$   SETMOD ttfont -include type42
$   ADDMOD ttfont -ps gs_mro_e gs_wan_e gs_ttf
$   RETURN
$ !
$! Type 42 (embedded TrueType) support
$ TYPE42_DEV:
$   IF F$TYPE(type42read_) .NES. "" THEN RETURN
$   type42read_ = "zchar42.obj zcharout.obj zfont42.obj"
$   GOSUB TTFLIB_DEV
$   SETMOD type42 'type42read_'
$   ADDMOD type42 -include ttflib
$   ADDMOD type42 -oper zchar42 zfont42
$   ADDMOD type42 -ps gs_typ42
$   ADD_DEV_MODULES = "''type42read_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ GCONFIGF_H:
$   SETMOD ccfonts_ -font "''ccfonts1'"
$   ADDMOD ccfonts_ -font "''ccfonts2'"
$   ADDMOD ccfonts_ -font "''ccfonts3'"
$   ADDMOD ccfonts_ -font "''ccfonts4'"
$   ADDMOD ccfonts_ -font "''ccfonts5'"
$   ADDMOD ccfonts_ -font "''ccfonts6'"
$   ADDMOD ccfonts_ -font "''ccfonts7'"
$   ADDMOD ccfonts_ -font "''ccfonts8'"
$   ADDMOD ccfonts_ -font "''ccfonts9'"
$   ADDMOD ccfonts_ -font "''ccfonts10'"
$   ADDMOD ccfonts_ -font "''ccfonts11'"
$   ADDMOD ccfonts_ -font "''ccfonts12'"
$   ADDMOD ccfonts_ -font "''ccfonts13'"
$   ADDMOD ccfonts_ -font "''ccfonts14'"
$   ADDMOD ccfonts_ -font "''ccfonts15'"
$   GENCONF ccfonts_.dev -n gs -f gconfigf.h
$   RETURN
$ !

$! ======================== PostScript Level 2 ======================== #
$ LEVEL2_DEV:
$   GOSUB CIDFONT_DEV
$   GOSUB CIE_DEV
$   GOSUB CMAPREAD_DEV
$   GOSUB COMPFONT_DEV
$   GOSUB DCT_DEV
$   GOSUB DEVCTRL_DEV
$   GOSUB DPSAND2_DEV
$   GOSUB FILTER_DEV
$   GOSUB LEVEL1_DEV
$   GOSUB PATTERN_DEV
$   GOSUB PSL2LIB_DEV
$   GOSUB PSL2READ_DEV
$   GOSUB SEPR_DEV
$   GOSUB TYPE42_DEV
$   GOSUB XFILTER_DEV
$   SETMOD level2 -include cidfont cie cmapread compfont
$   SETMOD level2 -include dct devctrl dpsand2 filter
$   ADDMOD level2 -include level1 pattern psl2lib psl2read
$   ADDMOD level2 -include sepr type42 xfilter
$   ADDMOD level2 -emulator """PostScript""" """PostScriptLevel2"""
$   RETURN
$ !
$! Define basic Level 2 language support.
$! This is the minimum required for CMap and CIDFont support.
$ PSL2INT_DEV:
$   IF F$TYPE(psl2int_) .NES. "" THEN RETURN
$   psl2int_="iutil2.obj zmisc2.obj zusparam.obj"
$   GOSUB DPS2INT_DEV
$   SETMOD psl2int "''psl2int_'"
$   ADDMOD psl2int -include dps2int
$   ADDMOD psl2int -oper zmisc2 zusparam
$   ADDMOD psl2int -ps gs_lev2 gs_res
$   ADD_DEV_MODULES = "''psl2int_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$!
$! Define full Level 2 support.
$ PSL2READ_DEV:
$   IF F$TYPE(psl2read_) .NES. "" THEN RETURN
$   psl2read_  = "zcolor2.obj zcsindex.obj zht2.obj zimage2.obj"
$   GOSUB PSL2INT_DEV
$   GOSUB DPS2READ_DEV
$   SETMOD psl2read 'psl2read_'
$   ADDMOD psl2read -include psl2int dps2read
$   ADDMOD psl2read -oper zcolor2_l2 zcsindex_l2
$   ADDMOD psl2read -oper zht2_l2 zimage2_l2
$   ADD_DEV_MODULES = "''psl2read_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ DEVCTRL_DEV:
$   devctrl_ = "zdevice2.obj ziodev2.obj zmedia2.obj zdevcal.obj"
$   SETMOD devctrl 'devctrl_'
$   ADDMOD devctrl -oper zdevice2_l2 ziodev2_l2 zmedia2_l2
$   ADDMOD devctrl -iodev null ram calendar
$   ADDMOD devctrl -ps gs_setpd
$   ADD_DEV_MODULES = "''devctrl_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ FDECODE_DEV:
$   IF F$TYPE(fdecode_) .NES. "" THEN RETURN
$   fdecode_ = "scantab.obj sfilter2.obj zfdecode.obj"
$   GOSUB CFD_DEV
$   GOSUB LZWD_DEV
$   GOSUB PDIFF_DEV
$   GOSUB PNGP_DEV
$   GOSUB RLD_DEV
$   SETMOD fdecode 'fdecode_'
$   ADDMOD fdecode -include cfd lzwd pdiff pngp rld
$   ADDMOD fdecode -oper zfdecode
$   ADD_DEV_MODULES = "''fdecode_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ FILTER_DEV:
$   filter_  = "zfilter2.obj"
$   GOSUB FDECODE_DEV
$   GOSUB CFE_DEV
$   GOSUB LZWE_DEV
$   GOSUB RLE_DEV
$   SETMOD filter -include fdecode
$   ADDMOD filter -obj 'filter_'
$   ADDMOD filter -include cfe lzwe rle
$   ADDMOD filter -oper zfilter2
$   ADD_DEV_MODULES = "''filter_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ XFILTER_DEV:
$   xfilter_ = "sbhc.obj sbwbs.obj shcgen.obj smtf.obj zfilterx.obj"
$   GOSUB PCXD_DEV
$   GOSUB PNGP_DEV
$   SETMOD xfilter 'xfilter_'
$   ADDMOD xfilter -include pcxd
$   ADDMOD xfilter -oper zfilterx
$   ADD_DEV_MODULES = "''xfilter_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ BTOKEN_DEV:
$   btoken_ = "iscanbin.obj zbseq.obj"
$   SETMOD btoken 'btoken_'
$   ADDMOD btoken -oper zbseq_l2
$   ADDMOD btoken -ps gs_btokn
$   ADD_DEV_MODULES = "''btoken_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ UPATH_DEV:
$   upath_ = "zupath.obj ibnum.obj"
$   SETMOD upath 'upath_'
$   ADDMOD upath -oper zupath_l2
$   ADD_DEV_MODULES = "''upath_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! -------- Additions common to Display PostScript and Level 2 -------- #
$ DPSAND2_DEV:
$   GOSUB BTOKEN_DEV
$   GOSUB COLOR_DEV
$   GOSUB UPATH_DEV
$   GOSUB DPS2LIB_DEV
$   GOSUB DPS2READ_DEV
$   SETMOD dpsand2 -include btoken color upath dps2lib dps2read
$   RETURN
$ !
$! Note that zvmem2 includes both Level 1 and Level 2 operators.
$ DPS2INT_DEV:
$   IF F$TYPE(dps2int_) .NES. "" THEN RETURN
$   dps2int_="zvmem2.obj zdps1.obj"
$   SETMOD dps2int "''dps2int_'"
$   ADDMOD dps2int -oper zvmem2 zdps1_l2
$   ADDMOD dps2int -ps gs_dps1
$   ADD_DEV_MODULES = "''dps2int_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$!
$ DPS2READ_DEV:
$   IF F$TYPE(dps2read_) .NES. "" THEN RETURN
$   dps2read_ = "ibnum.obj zchar2.obj"
$   GOSUB DPS2INT_DEV
$   SETMOD dps2read 'dps2read_'
$   ADDMOD dps2read -include dps2int
$   ADDMOD dps2read -oper ireclaim_l2 zchar2_l2
$   ADDMOD dps2read -ps gs_dps2
$   ADD_DEV_MODULES = "''dps2read_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! ---------------- Display PostScript ---------------- #
$ DPS_DEV:
$   dps_ = "zdps.obj icontext.obj zcontext.obj"
$   GOSUB DPSLIB_DEV
$   GOSUB LEVEL2_DEV
$   SETMOD dps -include dpslib level2
$   ADDMOD dps -obj 'dps_'
$   ADDMOD dps -oper zcontext zdps
$   ADDMOD dps -ps gs_dps
$   ADD_DEV_MODULES = "''dps_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! ---------------- NeXT Display PostScript ---------------- #
$!**************** NOT READY FOR USE YET ****************#
$!
$! DPSNEXT_DEV:
$!   dpsnext_ = "zdpnext.obj"
$!   GOSUB DPS_DEV
$!   SETMOD dpsnext -include dps
$!   ADDMOD dpsnext -obj "''dpsnext_'"
$!   ADDMOD dpsnext -oper zdpnext
$!   ADDMOD dpsnext -ps gs_dpnxt
$!   ADD_DEV_MODULES = "''dpsnext_'"
$!   GOSUB ADD_DEV_MOD
$!   RETURN
$ !
$ COMPFONT_DEV:
$   GOSUB PSF0LIB_DEV
$   GOSUB PSF0READ_DEV
$   SETMOD compfont -include psf0lib psf0read
$   RETURN
$ !
$ PSF0READ_DEV:
$   psf0read_ = "zchar2.obj zfcmap.obj zfont0.obj"
$   SETMOD psf0read 'psf0read_'
$   ADDMOD psf0read -oper zfont0 zchar2 zfcmap
$   ADD_DEV_MODULES = "''psf0read_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! ---------------- CMap support ---------------- #
$ CMAPREAD_DEV:
$   IF F$TYPE(cmapread_) .NES. "" THEN RETURN
$   cmapread_="zfcmap.obj"
$   GOSUB CMAPLIB_DEV
$   GOSUB PSL2INT_DEV
$   SETMOD cmapread "''cmapread_'"
$   ADDMOD cmapread -include cmaplib psl2int
$   ADDMOD cmapread -oper zfcmap
$   ADDMOD cmapread -ps gs_cmap
$   ADD_DEV_MODULES = "''cmapread_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$!
$ CIDFONT_DEV:
$   IF F$TYPE(cidread_) .NES. "" THEN RETURN
$   cidread_ = "zcid.obj"
$   GOSUB PSF1READ_DEV
$   GOSUB PSL2INT_DEV
$   GOSUB TYPE42_DEV
$   SETMOD cidfont 'cidread_'
$   ADDMOD cidfont -include psf1read psl2int type42
$   ADDMOD cidfont -ps -gs_cidfn
$   ADDMOD cidfont -oper zcid
$   ADD_DEV_MODULES = "''cidread_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ CIE_DEV:
$   IF F$TYPE(cieread_) .NES. "" THEN RETURN
$   cieread_ = "zcie.obj zcrd.obj"
$   GOSUB CIELIB_DEV
$   SETMOD cie 'cieread_'
$   ADDMOD cie -oper zcie_l2 zcrd_l2
$   ADDMOD cie -include cielib
$   ADD_DEV_MODULES = "''cieread_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ PATTERN_DEV:
$   GOSUB PATLIB_DEV
$   GOSUB PATREAD_DEV
$   SETMOD pattern -include patlib patread
$   RETURN
$ !
$ PATREAD_DEV:
$   patread_ = "zpcolor.obj"
$   SETMOD patread 'patread_'
$   ADDMOD patread -oper zpcolor_l2
$   ADD_DEV_MODULES = "''patread_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ SEPR_DEV:
$   seprread_ = "zcssepr.obj"
$   GOSUB SEPRLIB_DEV
$   SETMOD sepr 'seprread_'
$   ADDMOD sepr -oper zcssepr_l2
$   ADDMOD sepr -include seprlib
$   ADD_DEV_MODULES = "''seprread_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! ---------------- Functions ---------------- #
$! Generic support, and FunctionType 0.
$ FUNC_DEV:
$   IF F$TYPE(funcread_) .NES. "" THEN RETURN
$   funcread_ = "zfunc.obj zfunc0.obj"
$   GOSUB FUNCLIB_DEV
$   SETMOD func 'funcread_'
$   ADDMOD func -oper zfunc zfunc0
$   ADDMOD func -include funclib
$   ADD_DEV_MODULES = "''funcread_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! ---------------- DCT filters ---------------- #
$ DCT_DEV:
$   IF F$TYPE(dctc__) .NES. "" THEN RETURN
$   dctc_="zfdctc.obj"
$   GOSUB DCTE_DEV
$   GOSUB DCTD_DEV
$   SETMOD dct -include dcte dctd
$   ADD_DEV_MODULES = "''dctc_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! Encoding (compression)
$ DCTE_DEV:
$   IF F$TYPE(dcte_) .NES. "" THEN RETURN
$   dcte_ = "''dctc_' zfdcte.obj"
$   GOSUB SDCTE_DEV
$   SETMOD dcte -include sdcte
$   ADDMOD dcte -obj 'dcte_'
$   ADDMOD dcte -oper zfdcte
$   ADD_DEV_MODULES = "''dcte_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! Decoding (decompression)
$ DCTD_DEV:
$   IF F$TYPE(dctd_) .NES. "" THEN RETURN
$   dctd_ = "''dctc_' zfdctd.obj"
$   GOSUB SDCTD_DEV
$   SETMOD dctd -include sdctd
$   ADDMOD dctd -obj 'dctd_'
$   ADDMOD dctd -oper zfdctd
$   ADD_DEV_MODULES = "''dctd_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! ---------------- zlib/Flate filters ---------------- #
$ FZLIB_DEV:
$   GOSUB SZLIBE_DEV
$   GOSUB SZLIBD_DEV
$   SETMOD fzlib -include szlibe szlibd
$   ADDMOD fzlib -obj zfzlib.obj
$   ADDMOD fzlib -oper zfzlib
$   ADD_DEV_MODULES = "zfzlib.obj"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! ================================ PDF ================================ #
$ PDFMIN_DEV:
$   GOSUB PSBASE_DEV
$   GOSUB COLOR_DEV
$   GOSUB DPS2LIB_DEV
$   GOSUB DPS2READ_DEV
$   GOSUB FDECODE_DEV
$   GOSUB TYPE1_DEV
$   GOSUB PDFFONTS_DEV
$   GOSUB PSL2LIB_DEV
$   GOSUB PSL2READ_DEV
$   GOSUB PDFREAD_DEV
$   SETMOD pdfmin -include psbase color dps2lib dps2read
$   ADDMOD pdfmin -include fdecode type1
$   ADDMOD pdfmin -include pdffonts psl2lib psl2read pdfread
$   ADDMOD pdfmin -emulator """PDF"""
$   RETURN
$ !
$ PDF_DEV:
$   GOSUB PDFMIN_DEV
$   GOSUB CFF_DEV
$   GOSUB CIDFONT_DEV
$   GOSUB CIE_DEV
$   GOSUB COMPFONT_DEV
$   GOSUB CMAPREAD_DEV
$   GOSUB DCTD_DEV
$   GOSUB FUNC_DEV
$   GOSUB TTFONT_DEV
$   GOSUB TYPE2_DEV
$   SETMOD pdf -include pdfmin cff cidfont cie cmapread
$   ADDMOD pdf -include compfont dctd func ttfont type2
$   RETURN
$ !
$! Reader only
$ PDFFONTS_DEV:
$   SETMOD pdffonts -ps gs_mex_e gs_mro_e gs_pdf_e gs_wan_e
$   RETURN
$ !
$ PDFREAD_DEV:
$   GOSUB FZLIB_DEV
$   SETMOD pdfread -include fzlib
$   ADDMOD pdfread -ps gs_pdf gs_l2img
$   ADDMOD pdfread -ps pdf_base pdf_draw pdf_font pdf_main pdf_sec
$   ADDMOD pdfread -ps pdf_2ps
$   RETURN
$ !
$ JPEGC_DEV:
$   IF F$TYPE(jpegc_) .NES. "" THEN RETURN
$   jpegc_ = "jcomapi.obj jutils.obj sjpegerr.obj jmemmgr.obj"
$   COPY GSJCONF.H JCONFIG.H
$   COPY GSJMOREC.H JMORECFG.H
$   COPY 'JSRCDIR'JMORECFG.H []JMCORIG.H
$   COPY 'JSRCDIR'JERROR.H   []JERROR.H
$   COPY 'JSRCDIR'JINCLUDE.H []JINCLUDE.H
$   COPY 'JSRCDIR'JPEGLIB.H  []JPEGLIB.H
$   COPY 'JSRCDIR'JVERSION.H []JVERSION.H
$   SETMOD JPEGC 'jpegc_'
$   ADD_DEV_MODULES = "''JSRCDIR'jcomapi.obj ''JSRCDIR'jutils.obj " +-
     "sjpegerr.obj ''JSRCDIR'jmemmgr.obj"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ JPEGE_DEV:
$   IF F$TYPE(jpege6) .NES. "" THEN RETURN
$   jpege5 = "jcapi.obj"
$   jpege6 = "jcapimin.obj jcapistd.obj jcinit.obj"
$   jpege_1 = "jccoefct.obj jccolor.obj jcdctmgr.obj"
$   jpege_2 = "jchuff.obj jcmainct.obj jcmarker.obj jcmaster.obj"
$   jpege_3 = "jcparam.obj jcprepct.obj jcsample.obj jfdctint.obj"
$   GOSUB JPEGC_DEV
$   IF JVERSION .EQ. 5 THEN SETMOD jpege 'jpege5'
$   IF JVERSION .EQ. 6 THEN SETMOD jpege 'jpege6'
$   ADDMOD jpege -include jpegc
$   ADDMOD jpege -obj 'jpege_1'
$   ADDMOD jpege -obj 'jpege_2'
$   ADDMOD jpege -obj 'jpege_3'
$   IF JVERSION .EQ. 5 THEN ADD_DEV_MODULES = "''JSRCDIR'jcapi.obj"
$   IF JVERSION .EQ. 6 THEN ADD_DEV_MODULES = "''JSRCDIR'jcapimin.obj " +-
     "''JSRCDIR'jcapistd.obj ''JSRCDIR'jcinit.obj"
$   GOSUB ADD_DEV_MOD
$   ADD_DEV_MODULES = "''JSRCDIR'jccoefct.obj ''JSRCDIR'jccolor.obj " +-
     "''JSRCDIR'jcdctmgr.obj"
$   GOSUB ADD_DEV_MOD
$   ADD_DEV_MODULES = "''JSRCDIR'jchuff.obj ''JSRCDIR'jcmainct.obj " +-
     "''JSRCDIR'jcmarker.obj ''JSRCDIR'jcmaster.obj"
$   GOSUB ADD_DEV_MOD
$   ADD_DEV_MODULES = "''JSRCDIR'jcparam.obj ''JSRCDIR'jcprepct.obj " +-
     "''JSRCDIR'jcsample.obj ''JSRCDIR'jfdctint.obj"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ JPEGD_DEV:
$   IF F$TYPE(jpegd6) .NES. "" THEN RETURN
$   jpegd5 = "jdapi.obj"
$   jpegd6 = "jdapimin.obj jdapistd.obj jdinput.obj jdphuff.obj"
$   jpegd_1 = "jdcoefct.obj jdcolor.obj"
$   jpegd_2 = "jddctmgr.obj jdhuff.obj jdmainct.obj jdmarker.obj"
$   jpegd_3 = "jdmaster.obj jdpostct.obj jdsample.obj jidctint.obj"
$   GOSUB JPEGC_DEV
$   IF JVERSION .EQ. 5 THEN SETMOD jpegd 'jpegd5'
$   IF JVERSION .EQ. 6 THEN SETMOD jpegd 'jpegd6'
$   ADDMOD jpegd -include jpegc
$   ADDMOD jpegd -obj 'jpegd_1'
$   ADDMOD jpegd -obj 'jpegd_2'
$   ADDMOD jpegd -obj 'jpegd_3'
$   IF JVERSION .EQ. 5 THEN ADD_DEV_MODULES = "''JSRCDIR'jdapi.obj"
$   IF JVERSION .EQ. 6 THEN ADD_DEV_MODULES = "''JSRCDIR'jdapimin.obj " +-
     "''JSRCDIR'jdapistd.obj ''JSRCDIR'jdinput.obj ''JSRCDIR'jdphuff.obj"
$   GOSUB ADD_DEV_MOD
$   ADD_DEV_MODULES = "''JSRCDIR'jdcoefct.obj ''JSRCDIR'jdcolor.obj"
$   GOSUB ADD_DEV_MOD
$   ADD_DEV_MODULES = "''JSRCDIR'jddctmgr.obj ''JSRCDIR'jdhuff.obj " +-
     "''JSRCDIR'jdmainct.obj ''JSRCDIR'jdmarker.obj"
$   GOSUB ADD_DEV_MOD
$   ADD_DEV_MODULES = "''JSRCDIR'jdmaster.obj ''JSRCDIR'jdpostct.obj " +-
     "''JSRCDIR'jdsample.obj ''JSRCDIR'jidctint.obj"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ LIBPNG_DEV:
$   IF F$TYPE(png_) .NES. "" THEN RETURN
$   png_1 = "png.obj pngmem.obj pngerror.obj"
$   png_2 = "pngtrans.obj pngwrite.obj pngwtran.obj pngwutil.obj"
$   png_  = "gdevpng.obj gdevpccm.obj gdevprn.obj"
$   COPY 'PSRCDIR'PNG.H     []PNG.H
$   COPY 'PSRCDIR'PNGCONF.H []PNGCONF.H
$   GOSUB ZLIBE_DEV
$   IF PVERSION .EQ. 88 THEN SETMOD libpng "pngio.obj"
$   IF PVERSION .EQ. 89 THEN SETMOD libpng "pngwio.obj"
$   IF PVERSION .GE. 90
$     THEN
$       GOSUB CRC32_DEV
$       SETMOD libpng "pngwio.obj" -include crc32
$     ENDIF
$   ADDMOD libpng -obj 'png_1'
$   ADDMOD libpng -obj 'png_2'
$   ADDMOD libpng -include zlibe
$   ADD_DEV_MODULES = "''PSRCDIR'png.obj ''PSRCDIR'pngmem.obj "
$   IF PVERSION .EQ. 88 THEN ADD_DEV_MODULES = ADD_DEV_MODULES +-
     "''PSRCDIR'pngio.obj ''PSRCDIR'pngerror.obj"
$   IF PVERSION .GT. 89 THEN ADD_DEV_MODULES = ADD_DEV_MODULES +-
     "''PSRCDIR'pngwio.obj ''PSRCDIR'pngerror.obj"
$   GOSUB ADD_DEV_MOD
$   ADD_DEV_MODULES = "''PSRCDIR'pngtrans.obj ''PSRCDIR'pngwrite.obj " +-
     "''PSRCDIR'pngwtran.obj ''PSRCDIR'pngwutil.obj"
$   GOSUB ADD_DEV_MOD
$   ADD_DEV_MODULES = "''png_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! Code common to compression and decompression.
$ ZLIBC_DEV:
$   COPY 'ZSRCDIR'ZCONF.H   []ZCONF.H
$   COPY 'ZSRCDIR'ZLIB.H    []ZLIB.H
$   COPY 'ZSRCDIR'ZUTIL.H   []ZUTIL.H
$   COPY 'ZSRCDIR'DEFLATE.H []DEFLATE.H
$   zlibc_ = "zutil.obj"
$   SETMOD zlibc 'zlibc_'
$   ADD_DEV_MODULES = "''ZSRCDIR'zutil.obj"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! Encoding (compression) code.
$ ZLIBE_DEV:
$   IF F$TYPE(zlibe_) .NES. "" THEN RETURN
$   zlibe_ = "adler32.obj deflate.obj trees.obj"
$   GOSUB ZLIBC_DEV
$   SETMOD zlibe 'zlibe_'
$   ADDMOD zlibe -include zlibc
$   ADD_DEV_MODULES = "''ZSRCDIR'adler32.obj ''ZSRCDIR'deflate.obj " +-
     "''ZSRCDIR'trees.obj"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! The zlib filters per se don't need crc32, but libpng versions starting
$! with 0.90 do.
$ CRC32_DEV:
$   SETMOD crc32 "crc32.obj"
$   ADD_DEV_MODULES = "''ZSRCDIR'crc32.obj"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! Decoding (decompression) code.
$ ZLIBD_DEV:
$   IF F$TYPE(zlibd_) .NES. "" THEN RETURN
$   zlibd1_ = "infblock.obj infcodes.obj inffast.obj"
$   zlibd2_ = "inflate.obj inftrees.obj infutil.obj"
$   zlibd_  = "''zlibd1_' ''zlibd2_'"
$   GOSUB ZLIBC_DEV
$   SETMOD zlibd 'zlibd1_'
$   ADDMOD zlibd -obj 'zlibd2_'
$   ADDMOD zlibe -include zlibc
$   ADD_DEV_MODULES =-
     "''ZSRCDIR'infblock.obj ''ZSRCDIR'infcodes.obj ''ZSRCDIR'inffast.obj " +-
     "''ZSRCDIR'inflate.obj ''ZSRCDIR'inftrees.obj ''ZSRCDIR'infutil.obj"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ X11DEV_INIT:
$   x11_  = "gdevx.obj gdevxini.obj gdevxxf.obj gdevemap.obj"
$   x11alt_ = "''x11_' gdevxalt.obj"
$   XLIBS = "Xt Xext X11"
$   ADD_DEV_MODULES = "''x11alt_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ X11_DEV:
$   IF F$TYPE(x11_) .EQS. "" THEN GOSUB X11DEV_INIT
$   SETDEV x11 'x11_'
$   ADDMOD x11 -lib 'XLIBS'
$   RETURN
$ !
$ X11ALPHA_DEV:
$   IF F$TYPE(x11_) .EQS. "" THEN GOSUB X11DEV_INIT
$   SETDEV x11alpha 'x11alt_'
$   ADDMOD x11alpha -lib 'XLIBS'
$   RETURN
$ !
$ X11CMYK_DEV:
$   IF F$TYPE(x11_) .EQS. "" THEN GOSUB X11DEV_INIT
$   SETDEV x11cmyk 'x11alt_'
$   ADDMOD x11cmyk -lib 'XLIBS'
$   RETURN
$ !
$ X11GRAY2_DEV:
$   IF F$TYPE(x11_) .EQS. "" THEN GOSUB X11DEV_INIT
$   SETDEV x11gray2 'x11alt_'
$   ADDMOD x11gray2 -lib 'XLIBS'
$   RETURN
$ !
$ X11MONO_DEV:
$   IF F$TYPE(x11_) .EQS. "" THEN GOSUB X11DEV_INIT
$   SETDEV x11mono 'x11alt_'
$   ADDMOD x11mono -lib 'XLIBS'
$   RETURN
$ !
$ LN03_INIT:
$   ln03_ = "gdevln03.obj gdevprn.obj"
$   ADD_DEV_MODULES = "''ln03_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ SXLCRT_DEV:
$   IF F$TYPE(ln03_) .EQS. "" THEN GOSUB LN03_INIT
$   SETDEV sxlcrt 'ln03_'
$   RETURN
$ !
$ BJDEV_INIT:
$   bj10e_ = "gdevbj10.obj gdevprn.obj"
$   GOSUB CLIST_DEV
$   ADD_DEV_MODULES = "''bj10e_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ BJ10E_DEV:
$   IF F$TYPE(bj10e_) .EQS. "" THEN GOSUB BJDEV_INIT
$   SETDEV bj10e 'bj10e_'
$   RETURN
$ !
$ BJ200_DEV:
$   IF F$TYPE(bj10e_) .EQS. "" THEN GOSUB BJDEV_INIT
$   SETDEV bj200 'bj10e_'
$   RETURN
$ !
$ HPMONO_INIT:
$   HPPCL     = "gdevprn.obj gdevpcl.obj"
$   HPMONO    = "gdevdjet.obj ''HPPCL'"
$   GOSUB CLIST_DEV
$   ADD_DEV_MODULES = "''HPMONO'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ DESKJET_DEV:
$   IF F$TYPE(HPMONO) .EQS. "" THEN GOSUB HPMONO_INIT
$   SETDEV deskjet 'HPMONO'
$   RETURN
$ !
$ DJET500_DEV:
$   IF F$TYPE(HPMONO) .EQS. "" THEN GOSUB HPMONO_INIT
$   SETDEV djet500 'HPMONO'
$   RETURN
$ !
$ LASERJET_DEV:
$   IF F$TYPE(HPMONO) .EQS. "" THEN GOSUB HPMONO_INIT
$   SETDEV laserjet 'HPMONO'
$   RETURN
$ !
$ LJETPLUS_DEV:
$   IF F$TYPE(HPMONO) .EQS. "" THEN GOSUB HPMONO_INIT
$   SETDEV ljetplus 'HPMONO'
$   RETURN
$ !
$ LJET2P_DEV:
$   IF F$TYPE(HPMONO) .EQS. "" THEN GOSUB HPMONO_INIT
$   SETDEV ljet2p 'HPMONO'
$   RETURN
$ !
$ LJET3_DEV:
$   IF F$TYPE(HPMONO) .EQS. "" THEN GOSUB HPMONO_INIT
$   SETDEV ljet3 'HPMONO'
$   RETURN
$ !
$ LJET4_DEV:
$   IF F$TYPE(HPMONO) .EQS. "" THEN GOSUB HPMONO_INIT
$   SETDEV ljet4 'HPMONO'
$   RETURN
$ !
$ LP2563_DEV:
$   IF F$TYPE(HPMONO) .EQS. "" THEN GOSUB HPMONO_INIT
$   SETDEV lp2563 'HPMONO'
$   RETURN
$ !
$ OCE9050_DEV:
$   IF F$TYPE(HPMONO) .EQS. "" THEN GOSUB HPMONO_INIT
$   SETDEV oce9050 'HPMONO'
$   RETURN
$ !
$ LJ5INIT_INIT:
$   HPPCL     = "gdevprn.obj gdevpcl.obj"
$   ljet5_    = "gdevlj56.obj ''HPPCL'"
$   GOSUB CLIST_DEV
$   ADD_DEV_MODULES = "''ljet5_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ LJ5MONO_DEV:
$   IF F$TYPE(ljet5_) .EQS. "" THEN GOSUB LJ5INIT_INIT
$   SETDEV lj5mono 'ljet5_'
$   RETURN
$ !
$ LJ5GRAY_DEV:
$   IF F$TYPE(ljet5_) .EQS. "" THEN GOSUB LJ5INIT_INIT
$   SETDEV lj5gray 'ljet5_'
$   RETURN
$ !
$ HPDEV_INIT:
$   HPPCL     = "gdevprn.obj gdevpcl.obj"
$   cdeskjet_ = "gdevcdj.obj ''HPPCL'"
$   GOSUB CLIST_DEV
$   ADD_DEV_MODULES = "''cdeskjet_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ CDESKJET_DEV:
$   IF F$TYPE(cdeskjet_) .EQS. "" THEN GOSUB HPDEV_INIT
$   SETDEV cdeskjet 'cdeskjet_'
$   RETURN
$ !
$ CDJCOLOR_DEV:
$   IF F$TYPE(cdeskjet_) .EQS. "" THEN GOSUB HPDEV_INIT
$   SETDEV cdjcolor 'cdeskjet_'
$   RETURN
$ !
$ CDJMONO_DEV:
$   IF F$TYPE(cdeskjet_) .EQS. "" THEN GOSUB HPDEV_INIT
$   SETDEV cdjmono 'cdeskjet_'
$   RETURN
$ !
$ CDJ550_DEV:
$   IF F$TYPE(cdeskjet_) .EQS. "" THEN GOSUB HPDEV_INIT
$   SETDEV cdj550 'cdeskjet_'
$   RETURN
$ !
$ DECLJ250_DEV:
$   IF F$TYPE(cdeskjet_) .EQS. "" THEN GOSUB HPDEV_INIT
$   SETDEV declj250 'cdeskjet_'
$   RETURN
$ !
$ DNJ650C_DEV:
$   IF F$TYPE(cdeskjet_) .EQS. "" THEN GOSUB HPDEV_INIT
$   SETDEV dnj650c 'cdeskjet_'
$   RETURN
$ !
$ LJ4DITH_DEV:
$   IF F$TYPE(cdeskjet_) .EQS. "" THEN GOSUB HPDEV_INIT
$   SETDEV lj4dith 'cdeskjet_'
$   RETURN
$ !
$ PJ_DEV:
$   IF F$TYPE(cdeskjet_) .EQS. "" THEN GOSUB HPDEV_INIT
$   SETDEV pj 'cdeskjet_'
$   RETURN
$ !
$ PJXL_DEV:
$   IF F$TYPE(cdeskjet_) .EQS. "" THEN GOSUB HPDEV_INIT
$   SETDEV pjxl 'cdeskjet_'
$   RETURN
$ !
$ PJXL300_DEV:
$   IF F$TYPE(cdeskjet_) .EQS. "" THEN GOSUB HPDEV_INIT
$   SETDEV pjxl300 'cdeskjet_'
$   RETURN
$ !
$ BJC600_DEV:
$   IF F$TYPE(cdeskjet_) .EQS. "" THEN GOSUB HPDEV_INIT
$   SETDEV bjc600 'cdeskjet_'
$   RETURN
$ !
$ BJC800_DEV:
$   IF F$TYPE(cdeskjet_) .EQS. "" THEN GOSUB HPDEV_INIT
$   SETDEV bjc800 'cdeskjet_'
$   RETURN
$ !
$ ESCP_DEV:
$   IF F$TYPE(cdeskjet_) .EQS. "" THEN GOSUB HPDEV_INIT
$   SETDEV escp 'cdeskjet_'
$   RETURN
$ !
$! --------------- Ugly/Update -> Unified Printer Driver ---------------- ###
$ UNIPRINT_DEV:
$   IF F$TYPE(uniprint_) .NES. "" THEN RETURN
$   uniprint_ = "gdevupd.obj"
$   SETDEV uniprint 'uniprint_'
$   ADD_DEV_MODULES = "''uniprint_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ LN03_DEV:
$   IF F$TYPE(ln03_) .EQS. "" THEN GOSUB LN03_INIT
$   SETDEV ln03 'ln03'
$   RETURN
$ !
$ LA50_DEV:
$   IF F$TYPE(ln03_) .EQS. "" THEN GOSUB LN03_INIT
$   SETDEV la50 'ln03'
$   RETURN
$ !
$ LA70_DEV:
$   IF F$TYPE(ln03_) .EQS. "" THEN GOSUB LN03_INIT
$   SETDEV la70 'ln03'
$   RETURN
$ !
$ LA75_DEV:
$   IF F$TYPE(ln03_) .EQS. "" THEN GOSUB LN03_INIT
$   SETDEV la75 'ln03'
$   RETURN
$ !
$ LA75PLUS_DEV:
$   IF F$TYPE(ln03_) .EQS. "" THEN GOSUB LN03_INIT
$   SETDEV la75plus 'ln03'
$   RETURN
$ !
$ LA70T_DEV:
$   la70t_ = "gdevla7t.obj"
$   SETDEV la70t 'la70t_'
$   ADD_DEV_MODULES = "''la70t_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ TEK4696_DEV:
$   tek4696_ = "gdevtknk.obj gdevprn.obj"
$   SETDEV tek4696 'tek4696_'
$   ADD_DEV_MODULES = "''tek4696_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ DFAX_INIT:
$   dfax_ = "gdevdfax.obj gdevtfax.obj"
$   ADD_DEV_MODULES = "''dfax_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ DFAXLOW_DEV:
$   IF F$TYPE(dfax_) .EQS. "" THEN GOSUB DFAX_INIT
$   SETDEV dfaxlow "''dfax_'"
$   ADDMOD  dfaxlow -include cfe
$   RETURN
$ !
$ DFAXHIGH_DEV:
$   IF F$TYPE(dfax_) .EQS. "" THEN GOSUB DFAX_INIT
$   SETDEV dfaxhigh "''dfax_'"
$   ADDMOD  dfaxhigh -include cfe
$   RETURN
$ !
$! PostScript and EPS writers
$ PSWRITE_INI:
$   IF F$TYPE(pswrite_) .NES. "" THEN RETURN
$   pswrite1_ = "gdevps.obj gdevpsdf.obj gdevpstr.obj"
$   pswrite2_ = "scantab.obj sfilter2.obj"
$   pswrite_  = "''pswrite1_' ''pswrite2_'"
$   ADD_DEV_MODULES = "''pswrite_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ EPSWRITE_DEV:
$   GOSUB PSWRITE_INI
$   GOSUB VECTOR_DEV
$   SETDEV epswrite "''pswrite1_'"
$   ADDMOD epswrite "''pswrite2_'"
$   ADDMOD epswrite -include vector
$   RETURN
$ !
$ PSWRITE_DEV:
$   GOSUB PSWRITE_INI
$   GOSUB VECTOR_DEV
$   SETDEV pswrite "''pswrite1_'"
$   ADDMOD pswrite "''pswrite2_'"
$   ADDMOD pswrite -include vector
$   RETURN
$ !
$! PDF writer
$ PDFWRITE_DEV:
$   pdfwrite1_ = "gdevpdf.obj gdevpdfd.obj gdevpdfi.obj gdevpdfm.obj"
$   pdfwrite2_ = "gdevpdfp.obj gdevpdft.obj gdevpsdf.obj gdevpstr.obj"
$   pdfwrite3_ = "gsflip.obj scantab.obj sfilter2.obj sstring.obj"
$   pdfwrite_  = "''pdfwrite1_' ''pdfwrite2_' ''pdfwrite3_'"
$   GOSUB CMYKLIB_DEV
$   GOSUB CFE_DEV
$   GOSUB DCTE_DEV
$   GOSUB LZWE_DEV
$   GOSUB RLE_DEV
$   GOSUB VECTOR_DEV
$   SETDEV pdfwrite 'pdfwrite1_'
$   ADDMOD pdfwrite 'pdfwrite2_'
$   ADDMOD pdfwrite 'pdfwrite3_'
$   ADDMOD pdfwrite -ps gs_pdfwr
$   ADDMOD pdfwrite -include cmyklib cfe dcte lzwe rle vector
$   ADD_DEV_MODULES = "''pdfwrite_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ PXLDEV_INIT:
$   pxl_ = "gdevpx.obj"
$   GOSUB VECTOR_DEV
$   ADD_DEV_MODULES = "''pxl_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ PXLMONO_DEV:
$   IF F$TYPE(pxl_) .EQS. "" THEN GOSUB PXLDEV_INIT
$   SETDEV pxlmono "''pxl_'"
$   ADDMOD pxlmono -include vector
$   RETURN
$ !
$ PXLCOLOR_DEV:
$   IF F$TYPE(pxl_) .EQS. "" THEN GOSUB PXLDEV_INIT
$   SETDEV pxlcolor "''pxl_'"
$   ADDMOD pxlcolor -include vector
$   RETURN
$ !
$ BITDEV_INIT:
$   bit_ = "gdevbit.obj gdevprn.obj"
$   GOSUB CLIST_DEV
$   ADD_DEV_MODULES = "''bit_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ BIT_DEV:
$   IF F$TYPE(bit_) .EQS. "" THEN GOSUB BITDEV_INIT
$   SETDEV bit 'bit_'
$   RETURN
$ !
$ BITRGB_DEV:
$   IF F$TYPE(bit_) .EQS. "" THEN GOSUB BITDEV_INIT
$   SETDEV bitrgb 'bit_'
$   RETURN
$ !
$ BITCMYK_DEV:
$   IF F$TYPE(bit_) .EQS. "" THEN GOSUB BITDEV_INIT
$   SETDEV bitcmyk 'bit_'
$   RETURN
$ !
$ BMPDEV_INIT:
$   bmp_ = "gdevbmp.obj gdevpccm.obj gdevprn.obj"
$   ADD_DEV_MODULES = "''bmp_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ BMPMONO_DEV:
$   IF F$TYPE(bmp_) .EQS. "" THEN GOSUB BMPDEV_INIT
$   SETDEV bmpmono 'bmp_'
$   RETURN
$ !
$ BMP16_DEV:
$   IF F$TYPE(bmp_) .EQS. "" THEN GOSUB BMPDEV_INIT
$   SETDEV bmp16 'bmp_'
$   RETURN
$ !
$ BMP256_DEV:
$   IF F$TYPE(bmp_) .EQS. "" THEN GOSUB BMPDEV_INIT
$   SETDEV bmp256 'bmp_'
$   RETURN
$ !
$ BMP16M_DEV:
$   IF F$TYPE(bmp_) .EQS. "" THEN GOSUB BMPDEV_INIT
$   SETDEV bmp16m 'bmp_'
$   RETURN
$ !
$ CGMDEV_INIT:
$   cgm_ = "gdevcgm.obj gdevcgml.obj"
$   ADD_DEV_MODULES = "''cgm_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ CGMMONO_DEV:
$   IF F$TYPE(cgm_) .EQS. "" THEN GOSUB CGMDEV_INIT
$   SETDEV cgmmono 'cgm_'
$   RETURN
$ !
$ CGM8_DEV:
$   IF F$TYPE(cgm_) .EQS. "" THEN GOSUB CGMDEV_INIT
$   SETDEV cgm8 'cgm_'
$   RETURN
$ !
$ CGM24_DEV:
$   IF F$TYPE(cgm_) .EQS. "" THEN GOSUB CGMDEV_INIT
$   SETDEV cgm24 'cgm_'
$   RETURN
$ !
$!### ------------------------- JPEG file format ------------------------- ###
$ JPEG_INI:
$   IF F$TYPE(jpeg_) .NES. "" THEN RETURN
$   jpeg_="gdevjpeg.obj"
$   ADD_DEV_MODULES = "''jpeg_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! RGB output
$ JPEG_DEV: 
$   GOSUB JPEG_INI
$   GOSUB SDCTE_DEV
$   SETDEV jpeg "''jpeg_'"
$   ADDMOD jpeg -include sdcte
$   RETURN
$ !
$! Gray output
$ JPEGGRAY_DEV:
$   GOSUB JPEG_INI
$   GOSUB SDCTE_DEV
$   SETDEV jpeggray "''jpeg_'"
$   ADDMOD jpeggray -include sdcte
$   RETURN
$ !
$ PCXDEV_INIT:
$   pcx_ = "gdevpcx.obj gdevpccm.obj gdevprn.obj"
$   GOSUB CLIST_DEV
$   ADD_DEV_MODULES = "''pcx_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ PCXMONO_DEV:
$   IF F$TYPE(pcx_) .EQS. "" THEN GOSUB PCXDEV_INIT
$   SETDEV pcxmono 'pcx_'
$   RETURN
$ !
$ PCXGRAY_DEV:
$   IF F$TYPE(pcx_) .EQS. "" THEN GOSUB PCXDEV_INIT
$   SETDEV pcxgray 'pcx_'
$   RETURN
$ !
$ PCX16_DEV:
$   IF F$TYPE(pcx_) .EQS. "" THEN GOSUB PCXDEV_INIT
$   SETDEV pcx16 'pcx_'
$   RETURN
$ !
$ PCX256_DEV:
$   IF F$TYPE(pcx_) .EQS. "" THEN GOSUB PCXDEV_INIT
$   SETDEV pcx256 'pcx_'
$   RETURN
$ !
$ PCX24B_DEV:
$   IF F$TYPE(pcx_) .EQS. "" THEN GOSUB PCXDEV_INIT
$   SETDEV pcx24b 'pcx_'
$   RETURN
$ !
$ PCXCMYK_DEV:
$   IF F$TYPE(pcx_) .EQS. "" THEN GOSUB PCXDEV_INIT
$   SETDEV pcxcmyk 'pcx_'
$   RETURN
$ !
$! The 2-up PCX device is also here only as an example, and for testing.
$ PCX2UP_DEV:
$   GOSUB PCX256_DEV
$   SETDEV pcx2up "gdevp2up.obj"
$   ADDMOD pcx2up -include pcx256
$   ADD_DEV_MODULES = "gdev2up.obj gdevprn.obj"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$! ### ------------------- Portable Bitmap file formats ------------------- ###
$ PBMDEV_INIT:
$   pxm_ = "gdevpbm.obj gdevprn.obj"
$   GOSUB CLIST_DEV
$   ADD_DEV_MODULES = "''pxm_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ PBM_DEV:
$   IF F$TYPE(pxm_) .EQS. "" THEN GOSUB PBMDEV_INIT
$   SETDEV pbm 'pxm_'
$   RETURN
$ !
$ PBMRAW_DEV:
$   IF F$TYPE(pxm_) .EQS. "" THEN GOSUB PBMDEV_INIT
$   SETDEV pbmraw 'pxm_'
$   RETURN
$ !
$ PGM_DEV:
$   IF F$TYPE(pxm_) .EQS. "" THEN GOSUB PBMDEV_INIT
$   SETDEV pgm 'pxm_'
$   RETURN
$ !
$ PGMRAW_DEV:
$   IF F$TYPE(pxm_) .EQS. "" THEN GOSUB PBMDEV_INIT
$   SETDEV pgmraw 'pxm_'
$   RETURN
$ !
$ PGNM_DEV:
$   IF F$TYPE(pxm_) .EQS. "" THEN GOSUB PBMDEV_INIT
$   SETDEV pgnm 'pxm_'
$   RETURN
$ !
$ PGNMRAW_DEV:
$   IF F$TYPE(pxm_) .EQS. "" THEN GOSUB PBMDEV_INIT
$   SETDEV pgnmraw 'pxm_'
$   RETURN
$ !
$ PPM_DEV:
$   IF F$TYPE(pxm_) .EQS. "" THEN GOSUB PBMDEV_INIT
$   SETDEV ppm 'pxm_'
$   RETURN
$ !
$ PPMRAW_DEV:
$   IF F$TYPE(pxm_) .EQS. "" THEN GOSUB PBMDEV_INIT
$   SETDEV ppmraw 'pxm_'
$   RETURN
$ !
$ PNM_DEV:
$   IF F$TYPE(pxm_) .EQS. "" THEN GOSUB PBMDEV_INIT
$   SETDEV pnm 'pxm_'
$   RETURN
$ !
$ PNMRAW_DEV:
$   IF F$TYPE(pxm_) .EQS. "" THEN GOSUB PBMDEV_INIT
$   SETDEV pnmraw 'pxm_'
$   RETURN
$ !
$ PNGMONO_DEV:
$   IF F$TYPE(png_) .EQS. "" THEN GOSUB LIBPNG_DEV
$   SETDEV pngmono 'png_'
$   ADDMOD pngmono  -include libpng
$   RETURN
$ !
$ PNGGRAY_DEV:
$   IF F$TYPE(png_) .EQS. "" THEN GOSUB LIBPNG_DEV
$   SETDEV pnggray 'png_'
$   ADDMOD pnggray  -include libpng
$   RETURN
$ !
$ PNG16_DEV:
$   IF F$TYPE(png_) .EQS. "" THEN GOSUB LIBPNG_DEV
$   SETDEV png16 'png_'
$   ADDMOD png16  -include libpng
$   RETURN
$ !
$ PNG256_DEV:
$   IF F$TYPE(png_) .EQS. "" THEN GOSUB LIBPNG_DEV
$   SETDEV png256 'png_'
$   ADDMOD png256  -include libpng
$   RETURN
$ !
$ PNG16M_DEV:
$   IF F$TYPE(png_) .EQS. "" THEN GOSUB LIBPNG_DEV
$   SETDEV png16m 'png_'
$   ADDMOD png16m  -include libpng
$   RETURN
$ !
$ PSMONO_DEV:
$   ps_ = "gdevpsim.obj gdevprn.obj"
$   GOSUB CLIST_DEV
$   SETDEV psmono 'ps_'
$   ADD_DEV_MODULES = "''ps_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ PSGRAY_DEV:
$   ps_ = "gdevpsim.obj gdevprn.obj"
$   GOSUB CLIST_DEV
$   SETDEV psgray 'ps_'
$   ADD_DEV_MODULES = "''ps_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ TFAX_DEV:
$   tfax_ = "gdevtfax.obj gdevprn.obj"
$   GOSUB CLIST_DEV
$   GOSUB CFE_DEV
$   GOSUB LZWE_DEV
$   GOSUB RLE_DEV
$   GOSUB TIFFS_DEV
$   SETMOD tfax 'tfax_'
$   ADDMOD tfax -include cfe lzwe rle tiffs
$   ADD_DEV_MODULES = "''tfax_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ FAXG3_DEV:
$   IF F$TYPE(tfax_) .EQS. "" THEN GOSUB TFAX_DEV
$   SETDEV faxg3 -include tfax
$   RETURN
$ !
$ FAXG32D_DEV:
$   IF F$TYPE(tfax_) .EQS. "" THEN GOSUB TFAX_DEV
$   SETDEV faxg32d -include tfax
$   RETURN
$ !
$ FAXG4_DEV:
$   IF F$TYPE(tfax_) .EQS. "" THEN GOSUB TFAX_DEV
$   SETDEV faxg4 -include tfax
$   RETURN
$ !
$ TIFFS_DEV:
$   IF F$TYPE(tiffs_) .NES. "" THEN RETURN
$   tiffs_ = "gdevtifs.obj gdevprn.obj"
$   GOSUB CLIST_DEV
$   SETMOD tiffs 'tiffs_'
$   ADD_DEV_MODULES = "''tiffs_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ TIFFCRLE_DEV:
$   IF F$TYPE(tfax_) .EQS. "" THEN GOSUB TFAX_DEV
$   SETDEV tiffcrle -include tfax
$   RETURN
$ !
$ TIFFG3_DEV:
$   IF F$TYPE(tfax_) .EQS. "" THEN GOSUB TFAX_DEV
$   SETDEV tiffg3 -include tfax
$   RETURN
$ !
$ TIFFG32D_DEV:
$   IF F$TYPE(tfax_) .EQS. "" THEN GOSUB TFAX_DEV
$   SETDEV tiffg32d -include tfax
$   RETURN
$ !
$ TIFFG4_DEV:
$   IF F$TYPE(tfax_) .EQS. "" THEN GOSUB TFAX_DEV
$   SETDEV tiffg4 -include tfax
$   RETURN
$ !
$ TIFFLZW_DEV:
$   IF F$TYPE(tfax_) .EQS. "" THEN GOSUB TFAX_DEV
$   SETDEV tifflzw -include tfax
$   RETURN
$ !
$ TIFFPACK_DEV:
$   IF F$TYPE(tfax_) .EQS. "" THEN GOSUB TFAX_DEV
$   SETDEV tiffpack -include tfax
$   RETURN
$ !
$ TIFF12NC_DEV:
$   tiff12nc_ = "gdevtfnx.obj gdevtifs.obj gdevprn.obj"
$   GOSUB CLIST_DEV
$   SETDEV tiff12nc 'tiff12nc_'
$   ADD_DEV_MODULES = "''tiff12nc_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ TIFF24NC_DEV:
$   tiff24nc_ = "gdevtfnx.obj gdevtifs.obj gdevprn.obj"
$   GOSUB CLIST_DEV
$   SETDEV tiff24nc 'tiff24nc_'
$   ADD_DEV_MODULES = "''tiff24nc_'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ VMS_DEV:
$   vms__ = "gp_nofb.obj gp_vms.obj"
$   SETMOD vms_ 'vms__'
$   ADD_DEV_MODULES = "''vms__'"
$   GOSUB ADD_DEV_MOD
$   RETURN
$ !
$ ADD_DEV_MOD:
$   II = 0
$   ADD_MORE:
$     DEV_NOW = F$EDIT(F$ELEMENT(II," ",ADD_DEV_MODULES),"UPCASE") - ".OBJ"
$     IF DEV_NOW .EQS. " " THEN RETURN
$     II = II + 1
$     ! add delimiters to avoid mistaken identities
$     ! (e.g., a search for "x11" finding "x11alpha")
$     IF F$LOCATE(" "+DEV_NOW+" ",DEV_MODULES) .NE. F$LENGTH(DEV_MODULES) THEN -
        GOTO ADD_MORE
$     NDEVL = 0
$  CHECK_LOOP:
$     IF F$Type(DEV_MODULES'NDEVL') .EQS. "" THEN GOTO ADD_THIS
$     IF F$LOCATE(" "+DEV_NOW+" ",DEV_MODULES'NDEVL') .NE. -
        F$LENGTH(DEV_MODULES'NDEVL') THEN GOTO ADD_MORE
$     NDEVL = NDEVL + 1
$     IF NDEVL .LE. NDEV_MOD THEN GOTO CHECK_LOOP
$  ADD_THIS:
$     DEV_MODULES = DEV_MODULES + DEV_NOW + " "
$     IF F$Length(DEV_MODULES) .GT. 512
$      THEN
$       DEV_MODULES = F$EDIT(DEV_MODULES,"TRIM")
$       DEV_MODULES = F$EDIT(DEV_MODULES,"COMPRESS")
$       DEV_MODULES'NDEV_MOD' = DEV_MODULES + " "
$       NDEV_MOD = NDEV_MOD + 1
$       DEV_MODULES = " "
$      ENDIF
$     GOTO ADD_MORE
$ !
$ DONE:
$ !
$ IF P1 .EQS. "DEBUG" .OR. P2 .EQS. "DEBUG" THEN GOTO AFTER_DEL
$ DELETE *.DEV;*
$ IF F$SEARCH("DEVS.TR")       .NES. "" THEN DELETE DEVS.TR;*
$ IF F$SEARCH("ECHOGS.EXE")    .NES. "" THEN DELETE ECHOGS.EXE;*
$ IF F$SEARCH("GENCONF.EXE")   .NES. "" THEN DELETE GENCONF.EXE;*
$ IF F$SEARCH("GSSETDEV.COM")  .NES. "" THEN DELETE GSSETDEV.COM;*,-
   GSSETMOD.COM;*,GSADDMOD.COM;*
$ IF F$SEARCH("JCONFIG.H")     .NES. "" THEN DELETE JCONFIG.H;*,JMORECFG.H;*,-
   JMCORIG.H;*,JERROR.H;*,JINCLUDE.H;*,JPEGLIB.H;*,JVERSION.H;*
$ IF F$SEARCH("PNG.H")         .NES. "" THEN DELETE PNG.H;*,PNGCONF.H;*
$ IF F$SEARCH("ZCONF.H")       .NES. "" THEN DELETE ZCONF.H;*,ZLIB.H;*,-
   ZUTIL.H;*,DEFLATE.H;*
$AFTER_DEL:
$ IF F$LOGICAL("OPT_FILE")     .NES. "" THEN CLOSE OPT_FILE
$ IF F$LOGICAL("X11")          .NES. "" THEN DEASSIGN X11
$ !
$ ! ALL DONE
$ EXIT
$ !
$ NO_ACTION:
$ !
$ WSO "No such device routine: ''ACTION'"
$ GOTO DONE
