$ !   Copyright (C) 1989, 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
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
$ VERIFY = 'F$VERIFY(F$INTEGER(F$LOGICAL("GS_VERIFY")))'
$ !
$ ! "makefile" for Ghostscript, VMS / VMS C / DECwindows (X11) configuration.
$ !
$ !
$ ! Execute this command file while in the Ghostscript directory!
$ !
$ !
$ ! To build a debugging configuration, issue the command:
$ !
$ !          $ @VMS-CC.MAK DEBUG
$ !
$ ! Do not  abbreviate "DEBUG".  To specify an alternate directory for
$ ! GS_LIB_DEFAULT, issue the command:
$ !
$ !          $ @VMS-CC.MAK directory
$ !
$ ! with "directory" a fully qualified directory name.  "directory" cannot
$ ! be DEBUG (otherwise it will be confused with the DEBUG switch).  Both
$ ! the DEBUG switch and a directory name may be specified on the same
$ ! command line and in either order.
$ !
$ !
$ ! Declare local DCL symbols used by VMS.MAK
$ !
$ CC_DEF     = ""         ! Compiler /DEFINEs, US
$!!! CC_DEF     = "A4"       ! Compiler /DEFINEs, Europe
$ L_QUAL     = ""         ! Qualifiers for the link command
$ CC_COMMAND = "CC"       ! Compile command
$ X_INCLUDE  =  ""        ! Used with Gnu C to find path to X11 include files
$ !
$ GS_DOCDIR      = F$ENVIRONMENT("DEFAULT")
$ GS_INIT        = "GS_INIT.PS"
$ GS_LIB_DEFAULT = F$ENVIRONMENT("DEFAULT")
$ SEARCH_HERE_FIRST = 1
$ CONFIG         = ""
$ FEATURE_DEVS   = "level2.dev pdf.dev"
$ COMPILE_INITS  = "0"
$ BAND_LIST_STORAGE = "file"
$ BAND_LIST_COMPRESSOR = "zlib"
$ FILE_IMPLEMENTATION = "stdio"
$ DEVICE_DEVS    = "x11.dev x11alpha.dev x11cmyk.dev x11mono.dev"
$ DEVICE_DEVS3   = "deskjet.dev djet500.dev laserjet.dev ljetplus.dev ljet2p.dev ljet3.dev ljet4.dev"
$ DEVICE_DEVS4   = "cdeskjet.dev cdjcolor.dev cdjmono.dev cdj550.dev pj.dev pjxl.dev pjxl300.dev"
$ DEVICE_DEVS6   = "bj10e.dev bj200.dev bjc600.dev bjc800.dev"
$ DEVICE_DEVS7   = "faxg3.dev faxg32d.dev faxg4.dev"
$ DEVICE_DEVS8   = "pcxmono.dev pcxgray.dev pcx16.dev pcx256.dev pcx24b.dev pcxcmyk.dev"
$ DEVICE_DEVS9   = "pbm.dev pbmraw.dev pgm.dev pgmraw.dev pgnm.dev pgnmraw.dev pnm.dev pnmraw.dev ppm.dev ppmraw.dev"
$ DEVICE_DEVS10  = "tiffcrle.dev tiffg3.dev tiffg32d.dev tiffg4.dev tifflzw.dev tiffpack.dev"
$ DEVICE_DEVS11  = "tiff12nc.dev tiff24nc.dev"
$ DEVICE_DEVS12  = "psmono.dev psgray.dev bit.dev bitrgb.dev bitcmyk.dev"
$ DEVICE_DEVS13  = "pngmono.dev pnggray.dev png16.dev png256.dev png16m.dev"
$ DEVICE_DEVS14  = "jpeg.dev jpeggray.dev"
$ DEVICE_DEVS15  = "pdfwrite.dev"
$ !
$ !
$ ! Give ourself a healthy search list for C include files
$ !
$ DEFINE C$INCLUDE 'F$ENVIRONMENT("DEFAULT"),DECW$INCLUDE,SYS$LIBRARY
$ DEFINE VAXC$INCLUDE C$INCLUDE
$ DEFINE SYS SYS$LIBRARY
$ !
$ !
$ ! Option file to use when linking genarch.c
$ !
$ CREATE RTL.OPT
SYS$SHARE:VAXCRTL.EXE/SHARE
$ !
$ !
$ ! Now build everything
$ !
$ @VMS.MAK 'P1 'P2 'P3 'P4 'P5 'P6 'P7 'P8
$ !
$ !
$ DONE:
$ ! Cleanup
$ !
$ IF F$LOGICAL("C$INCLUDE")    .NES. "" THEN DEASSIGN C$INCLUDE"
$ IF F$LOGICAL("VAXC$INCLUDE") .NES. "" THEN DEASSIGN VAXC$INCLUDE
$ IF F$LOGICAL("SYS")          .NES. "" THEN DEASSIGN SYS
$ IF F$SEARCH("RTL.OPT")       .NES. "" THEN DELETE RTL.OPT;*
$ !
$ ! ALL DONE
$ !
$ VERIFY = 'F$VERIFY(VERIFY)
