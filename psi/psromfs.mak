#  Copyright (C) 2001-2023 Artifex Software, Inc.
#  All Rights Reserved.
#
#  This software is provided AS-IS with no warranty, either express or
#  implied.
#
#  This software is distributed under license and may not be copied,
#  modified or distributed except as expressly authorized under the terms
#  of the license contained in the file LICENSE in this distribution.
#
#  Refer to licensing information at http://www.artifex.com or contact
#  Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
#  CA 94129, USA, for further information.
#
# mkromfs macros for PostScript %rom% when COMPILE_INITS=1

# The following list of files needed by the interpreter is maintained here.
# This changes infrequently, but is a potential point of bitrot, but since
# unix-inst.mak uses this macro, problems should surface when testing installed
# versions.

#	Resource files go into Resource/...
#	The init files are in the %rom%Resource/Init/ directory
#       Any EXTRA_INIT_FILES go into %rom%lib/

PDF_RESOURCE_LIST=CMap$(D)*

MISC_INIT_FILES=FCOfontmap-PCLPS2 -C cidfmap \
 FAPIcidfmap FAPIconfig FAPIfontmap Fontmap Fontmap.GS xlatmap \
 gs_diskn.ps gs_dscp.ps gs_trap.ps \
 -B gs_cet.ps

# In the below list, the Font contents are _not_ compressed since it doesn't help.
PS_RESOURCE_LIST=SubstCID$(D)* CIDFSubst$(D)* CIDFont$(D)* -C $(PDF_RESOURCE_LIST) ColorSpace$(D)* Decoding$(D)* Encoding$(D)* -c -C IdiomSet$(D)* ProcSet$(D)* -P $(PSRESDIR)$(D)Init$(D) -d Resource/Init/ -B $(MISC_INIT_FILES)

PS_FONT_RESOURCE_LIST=-B -b Font$(D)*

#	Notes: gs_cet.ps is only needed to match Adobe CPSI defaults
PS_ROMFS_ARGS=-c \
  -d Resource/Init/ -P $(PSRESDIR)$(D)Init$(D) -g gs_init.ps $(iconfig_h) \
  -d Resource/ -P $(PSRESDIR)$(D) $(PS_RESOURCE_LIST) \
  -d lib/ -P $(PSLIBDIR)$(D) $(EXTRA_INIT_FILES)

PS_FONT_ROMFS_ARGS=-d Resource/ -P $(PSRESDIR)$(D) $(PS_FONT_RESOURCE_LIST)

# If you add a file remember to add it here. If you forget then builds from
# clean will work (as all files in the directory are included), but rebuilds
# after changes to unlisted files will not cause the romfs to be regenerated.

# A list of all of the files in Resource/CIDFont dependencies for COMPILE_INITS=1
PS_CIDFONT_DEPS=

# A list of all of the files in Resource/CMap dependencies for COMPILE_INITS=1
PS_CMAP_DEPS=\
	$(PSRESDIR)$(D)CMap$(D)78-EUC-H \
	$(PSRESDIR)$(D)CMap$(D)78-EUC-V \
	$(PSRESDIR)$(D)CMap$(D)78-H \
	$(PSRESDIR)$(D)CMap$(D)78-RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)78-RKSJ-V \
	$(PSRESDIR)$(D)CMap$(D)78-V \
	$(PSRESDIR)$(D)CMap$(D)78ms-RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)78ms-RKSJ-V \
	$(PSRESDIR)$(D)CMap$(D)83pv-RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)90ms-RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)90ms-RKSJ-UCS2 \
	$(PSRESDIR)$(D)CMap$(D)90ms-RKSJ-V \
	$(PSRESDIR)$(D)CMap$(D)90msp-RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)90msp-RKSJ-V \
	$(PSRESDIR)$(D)CMap$(D)90pv-RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)90pv-RKSJ-UCS2 \
	$(PSRESDIR)$(D)CMap$(D)90pv-RKSJ-UCS2C \
	$(PSRESDIR)$(D)CMap$(D)90pv-RKSJ-V \
	$(PSRESDIR)$(D)CMap$(D)Add-H \
	$(PSRESDIR)$(D)CMap$(D)Add-RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)Add-RKSJ-V \
	$(PSRESDIR)$(D)CMap$(D)Add-V \
	$(PSRESDIR)$(D)CMap$(D)Adobe-CNS1-0 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-CNS1-1 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-CNS1-2 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-CNS1-3 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-CNS1-4 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-CNS1-5 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-CNS1-6 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-CNS1-B5pc \
	$(PSRESDIR)$(D)CMap$(D)Adobe-CNS1-ETenms-B5 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-CNS1-H-CID \
	$(PSRESDIR)$(D)CMap$(D)Adobe-CNS1-H-Host \
	$(PSRESDIR)$(D)CMap$(D)Adobe-CNS1-H-Mac \
	$(PSRESDIR)$(D)CMap$(D)Adobe-CNS1-UCS2 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-GB1-0 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-GB1-1 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-GB1-2 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-GB1-3 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-GB1-4 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-GB1-5 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-GB1-GBK-EUC \
	$(PSRESDIR)$(D)CMap$(D)Adobe-GB1-GBpc-EUC \
	$(PSRESDIR)$(D)CMap$(D)Adobe-GB1-H-CID \
	$(PSRESDIR)$(D)CMap$(D)Adobe-GB1-H-Host \
	$(PSRESDIR)$(D)CMap$(D)Adobe-GB1-H-Mac \
	$(PSRESDIR)$(D)CMap$(D)Adobe-GB1-UCS2 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Japan1-0 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Japan1-1 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Japan1-2 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Japan1-3 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Japan1-4 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Japan1-5 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Japan1-6 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Japan1-90ms-RKSJ \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Japan1-90pv-RKSJ \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Japan1-H-CID \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Japan1-H-Host \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Japan1-H-Mac \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Japan1-PS-H \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Japan1-PS-V \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Japan1-UCS2 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Japan2-0 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Korea1-0 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Korea1-1 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Korea1-2 \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Korea1-H-CID \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Korea1-H-Host \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Korea1-H-Mac \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Korea1-KSCms-UHC \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Korea1-KSCpc-EUC \
	$(PSRESDIR)$(D)CMap$(D)Adobe-Korea1-UCS2 \
	$(PSRESDIR)$(D)CMap$(D)B5-H \
	$(PSRESDIR)$(D)CMap$(D)B5-V \
	$(PSRESDIR)$(D)CMap$(D)B5pc-H \
	$(PSRESDIR)$(D)CMap$(D)B5pc-UCS2 \
	$(PSRESDIR)$(D)CMap$(D)B5pc-UCS2C \
	$(PSRESDIR)$(D)CMap$(D)B5pc-V \
	$(PSRESDIR)$(D)CMap$(D)CNS-EUC-H \
	$(PSRESDIR)$(D)CMap$(D)CNS-EUC-V \
	$(PSRESDIR)$(D)CMap$(D)CNS01-RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)CNS02-RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)CNS03-RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)CNS04-RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)CNS05-RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)CNS06-RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)CNS07-RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)CNS1-H \
	$(PSRESDIR)$(D)CMap$(D)CNS1-V \
	$(PSRESDIR)$(D)CMap$(D)CNS15-RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)CNS2-H \
	$(PSRESDIR)$(D)CMap$(D)CNS2-V \
	$(PSRESDIR)$(D)CMap$(D)ETHK-B5-H \
	$(PSRESDIR)$(D)CMap$(D)ETHK-B5-V \
	$(PSRESDIR)$(D)CMap$(D)ETen-B5-H \
	$(PSRESDIR)$(D)CMap$(D)ETen-B5-UCS2 \
	$(PSRESDIR)$(D)CMap$(D)ETen-B5-V \
	$(PSRESDIR)$(D)CMap$(D)ETenms-B5-H \
	$(PSRESDIR)$(D)CMap$(D)ETenms-B5-V \
	$(PSRESDIR)$(D)CMap$(D)EUC-H \
	$(PSRESDIR)$(D)CMap$(D)EUC-V \
	$(PSRESDIR)$(D)CMap$(D)Ext-H \
	$(PSRESDIR)$(D)CMap$(D)Ext-RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)Ext-RKSJ-V \
	$(PSRESDIR)$(D)CMap$(D)Ext-V \
	$(PSRESDIR)$(D)CMap$(D)GB-EUC-H \
	$(PSRESDIR)$(D)CMap$(D)GB-EUC-V \
	$(PSRESDIR)$(D)CMap$(D)GB-H \
	$(PSRESDIR)$(D)CMap$(D)GB-RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)GB-V \
	$(PSRESDIR)$(D)CMap$(D)GBK-EUC-H \
	$(PSRESDIR)$(D)CMap$(D)GBK-EUC-UCS2 \
	$(PSRESDIR)$(D)CMap$(D)GBK-EUC-V \
	$(PSRESDIR)$(D)CMap$(D)GBK2K-H \
	$(PSRESDIR)$(D)CMap$(D)GBK2K-V \
	$(PSRESDIR)$(D)CMap$(D)GBKp-EUC-H \
	$(PSRESDIR)$(D)CMap$(D)GBKp-EUC-V \
	$(PSRESDIR)$(D)CMap$(D)GBT-EUC-H \
	$(PSRESDIR)$(D)CMap$(D)GBT-EUC-V \
	$(PSRESDIR)$(D)CMap$(D)GBT-H \
	$(PSRESDIR)$(D)CMap$(D)GBT-RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)GBT-V \
	$(PSRESDIR)$(D)CMap$(D)GBTpc-EUC-H \
	$(PSRESDIR)$(D)CMap$(D)GBTpc-EUC-V \
	$(PSRESDIR)$(D)CMap$(D)GBpc-EUC-H \
	$(PSRESDIR)$(D)CMap$(D)GBpc-EUC-UCS2 \
	$(PSRESDIR)$(D)CMap$(D)GBpc-EUC-UCS2C \
	$(PSRESDIR)$(D)CMap$(D)GBpc-EUC-V \
	$(PSRESDIR)$(D)CMap$(D)H \
	$(PSRESDIR)$(D)CMap$(D)HK-RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)HKdla-B5-H \
	$(PSRESDIR)$(D)CMap$(D)HKdla-B5-V \
	$(PSRESDIR)$(D)CMap$(D)HKdlb-B5-H \
	$(PSRESDIR)$(D)CMap$(D)HKdlb-B5-V \
	$(PSRESDIR)$(D)CMap$(D)HKgccs-B5-H \
	$(PSRESDIR)$(D)CMap$(D)HKgccs-B5-V \
	$(PSRESDIR)$(D)CMap$(D)HKm314-B5-H \
	$(PSRESDIR)$(D)CMap$(D)HKm314-B5-V \
	$(PSRESDIR)$(D)CMap$(D)HKm471-B5-H \
	$(PSRESDIR)$(D)CMap$(D)HKm471-B5-V \
	$(PSRESDIR)$(D)CMap$(D)HKscs-B5-H \
	$(PSRESDIR)$(D)CMap$(D)HKscs-B5-V \
	$(PSRESDIR)$(D)CMap$(D)Hankaku \
	$(PSRESDIR)$(D)CMap$(D)Hiragana \
	$(PSRESDIR)$(D)CMap$(D)Hojo-EUC-H \
	$(PSRESDIR)$(D)CMap$(D)Hojo-EUC-V \
	$(PSRESDIR)$(D)CMap$(D)Hojo-H \
	$(PSRESDIR)$(D)CMap$(D)Hojo-RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)Hojo-V \
	$(PSRESDIR)$(D)CMap$(D)Identity-H \
	$(PSRESDIR)$(D)CMap$(D)Identity-UTF16-H \
	$(PSRESDIR)$(D)CMap$(D)Identity-UTF16-V \
	$(PSRESDIR)$(D)CMap$(D)Identity-V \
	$(PSRESDIR)$(D)CMap$(D)KSC-EUC-H \
	$(PSRESDIR)$(D)CMap$(D)KSC-EUC-V \
	$(PSRESDIR)$(D)CMap$(D)KSC-H \
	$(PSRESDIR)$(D)CMap$(D)KSC-Johab-H \
	$(PSRESDIR)$(D)CMap$(D)KSC-Johab-V \
	$(PSRESDIR)$(D)CMap$(D)KSC-RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)KSC-V \
	$(PSRESDIR)$(D)CMap$(D)KSC2-RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)KSCms-UHC-H \
	$(PSRESDIR)$(D)CMap$(D)KSCms-UHC-HW-H \
	$(PSRESDIR)$(D)CMap$(D)KSCms-UHC-HW-V \
	$(PSRESDIR)$(D)CMap$(D)KSCms-UHC-UCS2 \
	$(PSRESDIR)$(D)CMap$(D)KSCms-UHC-V \
	$(PSRESDIR)$(D)CMap$(D)KSCpc-EUC-H \
	$(PSRESDIR)$(D)CMap$(D)KSCpc-EUC-UCS2 \
	$(PSRESDIR)$(D)CMap$(D)KSCpc-EUC-UCS2C \
	$(PSRESDIR)$(D)CMap$(D)KSCpc-EUC-V \
	$(PSRESDIR)$(D)CMap$(D)Katakana \
	$(PSRESDIR)$(D)CMap$(D)NWP-H \
	$(PSRESDIR)$(D)CMap$(D)NWP-V \
	$(PSRESDIR)$(D)CMap$(D)RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)RKSJ-V \
	$(PSRESDIR)$(D)CMap$(D)Roman \
	$(PSRESDIR)$(D)CMap$(D)TCVN-RKSJ-H \
	$(PSRESDIR)$(D)CMap$(D)UCS2-90ms-RKSJ \
	$(PSRESDIR)$(D)CMap$(D)UCS2-90pv-RKSJ \
	$(PSRESDIR)$(D)CMap$(D)UCS2-B5pc \
	$(PSRESDIR)$(D)CMap$(D)UCS2-ETen-B5 \
	$(PSRESDIR)$(D)CMap$(D)UCS2-GBK-EUC \
	$(PSRESDIR)$(D)CMap$(D)UCS2-GBpc-EUC \
	$(PSRESDIR)$(D)CMap$(D)UCS2-KSCms-UHC \
	$(PSRESDIR)$(D)CMap$(D)UCS2-KSCpc-EUC \
	$(PSRESDIR)$(D)CMap$(D)UniCNS-UCS2-H \
	$(PSRESDIR)$(D)CMap$(D)UniCNS-UCS2-V \
	$(PSRESDIR)$(D)CMap$(D)UniCNS-UTF16-H \
	$(PSRESDIR)$(D)CMap$(D)UniCNS-UTF16-V \
	$(PSRESDIR)$(D)CMap$(D)UniCNS-UTF32-H \
	$(PSRESDIR)$(D)CMap$(D)UniCNS-UTF32-V \
	$(PSRESDIR)$(D)CMap$(D)UniCNS-UTF8-H \
	$(PSRESDIR)$(D)CMap$(D)UniCNS-UTF8-V \
	$(PSRESDIR)$(D)CMap$(D)UniGB-UCS2-H \
	$(PSRESDIR)$(D)CMap$(D)UniGB-UCS2-V \
	$(PSRESDIR)$(D)CMap$(D)UniGB-UTF16-H \
	$(PSRESDIR)$(D)CMap$(D)UniGB-UTF16-V \
	$(PSRESDIR)$(D)CMap$(D)UniGB-UTF32-H \
	$(PSRESDIR)$(D)CMap$(D)UniGB-UTF32-V \
	$(PSRESDIR)$(D)CMap$(D)UniGB-UTF8-H \
	$(PSRESDIR)$(D)CMap$(D)UniGB-UTF8-V \
	$(PSRESDIR)$(D)CMap$(D)UniHojo-UCS2-H \
	$(PSRESDIR)$(D)CMap$(D)UniHojo-UCS2-V \
	$(PSRESDIR)$(D)CMap$(D)UniHojo-UTF16-H \
	$(PSRESDIR)$(D)CMap$(D)UniHojo-UTF16-V \
	$(PSRESDIR)$(D)CMap$(D)UniHojo-UTF32-H \
	$(PSRESDIR)$(D)CMap$(D)UniHojo-UTF32-V \
	$(PSRESDIR)$(D)CMap$(D)UniHojo-UTF8-H \
	$(PSRESDIR)$(D)CMap$(D)UniHojo-UTF8-V \
	$(PSRESDIR)$(D)CMap$(D)UniJIS-UCS2-H \
	$(PSRESDIR)$(D)CMap$(D)UniJIS-UCS2-HW-H \
	$(PSRESDIR)$(D)CMap$(D)UniJIS-UCS2-HW-V \
	$(PSRESDIR)$(D)CMap$(D)UniJIS-UCS2-V \
	$(PSRESDIR)$(D)CMap$(D)UniJIS-UTF16-H \
	$(PSRESDIR)$(D)CMap$(D)UniJIS-UTF16-V \
	$(PSRESDIR)$(D)CMap$(D)UniJIS-UTF32-H \
	$(PSRESDIR)$(D)CMap$(D)UniJIS-UTF32-V \
	$(PSRESDIR)$(D)CMap$(D)UniJIS-UTF8-H \
	$(PSRESDIR)$(D)CMap$(D)UniJIS-UTF8-V \
	$(PSRESDIR)$(D)CMap$(D)UniJIS2004-UTF16-H \
	$(PSRESDIR)$(D)CMap$(D)UniJIS2004-UTF16-V \
	$(PSRESDIR)$(D)CMap$(D)UniJIS2004-UTF32-H \
	$(PSRESDIR)$(D)CMap$(D)UniJIS2004-UTF32-V \
	$(PSRESDIR)$(D)CMap$(D)UniJIS2004-UTF8-H \
	$(PSRESDIR)$(D)CMap$(D)UniJIS2004-UTF8-V \
	$(PSRESDIR)$(D)CMap$(D)UniJISPro-UCS2-HW-V \
	$(PSRESDIR)$(D)CMap$(D)UniJISPro-UCS2-V \
	$(PSRESDIR)$(D)CMap$(D)UniJISPro-UTF8-V \
	$(PSRESDIR)$(D)CMap$(D)UniJISX0213-UTF32-H \
	$(PSRESDIR)$(D)CMap$(D)UniJISX0213-UTF32-V \
	$(PSRESDIR)$(D)CMap$(D)UniJISX02132004-UTF32-H \
	$(PSRESDIR)$(D)CMap$(D)UniJISX02132004-UTF32-V \
	$(PSRESDIR)$(D)CMap$(D)UniKS-UCS2-H \
	$(PSRESDIR)$(D)CMap$(D)UniKS-UCS2-V \
	$(PSRESDIR)$(D)CMap$(D)UniKS-UTF16-H \
	$(PSRESDIR)$(D)CMap$(D)UniKS-UTF16-V \
	$(PSRESDIR)$(D)CMap$(D)UniKS-UTF32-H \
	$(PSRESDIR)$(D)CMap$(D)UniKS-UTF32-V \
	$(PSRESDIR)$(D)CMap$(D)UniKS-UTF8-H \
	$(PSRESDIR)$(D)CMap$(D)UniKS-UTF8-V \
	$(PSRESDIR)$(D)CMap$(D)V \
	$(PSRESDIR)$(D)CMap$(D)WP-Symbol

# A list of all of the files in Resource/ColorSpace dependencies for COMPILE_INITS=1
PS_COLORSPACE_DEPS=\
	$(PSRESDIR)$(D)ColorSpace$(D)DefaultCMYK \
	$(PSRESDIR)$(D)ColorSpace$(D)DefaultGray \
	$(PSRESDIR)$(D)ColorSpace$(D)DefaultRGB \
	$(PSRESDIR)$(D)ColorSpace$(D)TrivialCMYK \
	$(PSRESDIR)$(D)ColorSpace$(D)sGray \
	$(PSRESDIR)$(D)ColorSpace$(D)sRGB

# A list of all of the files in Resource/Decoding
PS_DECODING_DEPS=\
	$(PSRESDIR)$(D)Decoding$(D)FCO_Dingbats \
	$(PSRESDIR)$(D)Decoding$(D)FCO_Symbol \
	$(PSRESDIR)$(D)Decoding$(D)FCO_Unicode \
	$(PSRESDIR)$(D)Decoding$(D)FCO_Wingdings \
	$(PSRESDIR)$(D)Decoding$(D)Latin1 \
	$(PSRESDIR)$(D)Decoding$(D)StandardEncoding \
	$(PSRESDIR)$(D)Decoding$(D)Unicode

# A list of all of the files in Resource/Encoding
PS_ENCODING_DEPS=\
	$(PSRESDIR)$(D)Encoding$(D)Wingdings

# A list of all of the files in Resource/Font
PS_FONT_DEPS=\
	$(PSRESDIR)$(D)Font$(D)C059-BdIta \
	$(PSRESDIR)$(D)Font$(D)C059-Bold \
	$(PSRESDIR)$(D)Font$(D)C059-Italic \
	$(PSRESDIR)$(D)Font$(D)C059-Roman \
	$(PSRESDIR)$(D)Font$(D)D050000L \
	$(PSRESDIR)$(D)Font$(D)NimbusMonoPS-Bold \
	$(PSRESDIR)$(D)Font$(D)NimbusMonoPS-BoldItalic \
	$(PSRESDIR)$(D)Font$(D)NimbusMonoPS-Italic \
	$(PSRESDIR)$(D)Font$(D)NimbusMonoPS-Regular \
	$(PSRESDIR)$(D)Font$(D)NimbusRoman-Bold \
	$(PSRESDIR)$(D)Font$(D)NimbusRoman-BoldItalic \
	$(PSRESDIR)$(D)Font$(D)NimbusRoman-Italic \
	$(PSRESDIR)$(D)Font$(D)NimbusRoman-Regular \
	$(PSRESDIR)$(D)Font$(D)NimbusSans-Bold \
	$(PSRESDIR)$(D)Font$(D)NimbusSans-BoldItalic \
	$(PSRESDIR)$(D)Font$(D)NimbusSansNarrow-BoldOblique \
	$(PSRESDIR)$(D)Font$(D)NimbusSansNarrow-Bold \
	$(PSRESDIR)$(D)Font$(D)NimbusSansNarrow-Oblique \
	$(PSRESDIR)$(D)Font$(D)NimbusSansNarrow-Regular \
	$(PSRESDIR)$(D)Font$(D)NimbusSans-Italic \
	$(PSRESDIR)$(D)Font$(D)NimbusSans-Regular \
	$(PSRESDIR)$(D)Font$(D)P052-Bold \
	$(PSRESDIR)$(D)Font$(D)P052-BoldItalic \
	$(PSRESDIR)$(D)Font$(D)P052-Italic \
	$(PSRESDIR)$(D)Font$(D)P052-Roman \
	$(PSRESDIR)$(D)Font$(D)StandardSymbolsPS \
	$(PSRESDIR)$(D)Font$(D)URWBookman-Demi \
	$(PSRESDIR)$(D)Font$(D)URWBookman-DemiItalic \
	$(PSRESDIR)$(D)Font$(D)URWBookman-Light \
	$(PSRESDIR)$(D)Font$(D)URWBookman-LightItalic \
	$(PSRESDIR)$(D)Font$(D)URWGothic-Book \
	$(PSRESDIR)$(D)Font$(D)URWGothic-BookOblique \
	$(PSRESDIR)$(D)Font$(D)URWGothic-Demi \
	$(PSRESDIR)$(D)Font$(D)URWGothic-DemiOblique \
	$(PSRESDIR)$(D)Font$(D)Z003-MediumItalic

# A list of all of the files in Resource/IdimSet
PS_IDIOMSET_DEPS=

# A list of all of the files in Resource/ProcSet
PS_PROCSET_DEPS=

# A list of all the files in Resource/Init/ as dependencies for COMPILE_INITS=1.
PS_INIT_DEPS=\
	$(PSRESDIR)$(D)Init$(D)cidfmap \
	$(PSRESDIR)$(D)Init$(D)FCOfontmap-PCLPS2 \
	$(PSRESDIR)$(D)Init$(D)Fontmap \
	$(PSRESDIR)$(D)Init$(D)Fontmap.GS \
	$(PSRESDIR)$(D)Init$(D)gs_agl.ps \
	$(PSRESDIR)$(D)Init$(D)gs_btokn.ps \
	$(PSRESDIR)$(D)Init$(D)gs_cff.ps \
	$(PSRESDIR)$(D)Init$(D)gs_cidcm.ps \
	$(PSRESDIR)$(D)Init$(D)gs_ciddc.ps \
	$(PSRESDIR)$(D)Init$(D)gs_cidfm.ps \
	$(PSRESDIR)$(D)Init$(D)gs_cidfn.ps \
	$(PSRESDIR)$(D)Init$(D)gs_cidtt.ps \
	$(PSRESDIR)$(D)Init$(D)gs_cmap.ps \
	$(PSRESDIR)$(D)Init$(D)gs_cspace.ps \
	$(PSRESDIR)$(D)Init$(D)gs_dbt_e.ps \
	$(PSRESDIR)$(D)Init$(D)gs_diskn.ps \
	$(PSRESDIR)$(D)Init$(D)gs_dps1.ps \
	$(PSRESDIR)$(D)Init$(D)gs_dps2.ps \
	$(PSRESDIR)$(D)Init$(D)gs_dscp.ps \
	$(PSRESDIR)$(D)Init$(D)gs_epsf.ps \
	$(PSRESDIR)$(D)Init$(D)gs_fapi.ps \
	$(PSRESDIR)$(D)Init$(D)gs_fntem.ps \
	$(PSRESDIR)$(D)Init$(D)gs_fonts.ps \
	$(PSRESDIR)$(D)Init$(D)gs_frsd.ps \
	$(PSRESDIR)$(D)Init$(D)gs_icc.ps \
	$(PSRESDIR)$(D)Init$(D)gs_il1_e.ps \
	$(PSRESDIR)$(D)Init$(D)gs_img.ps \
	$(PSRESDIR)$(D)Init$(D)gs_lev2.ps \
	$(PSRESDIR)$(D)Init$(D)gs_ll3.ps \
	$(PSRESDIR)$(D)Init$(D)gs_mex_e.ps \
	$(PSRESDIR)$(D)Init$(D)gs_mgl_e.ps \
	$(PSRESDIR)$(D)Init$(D)gs_mro_e.ps \
	$(PSRESDIR)$(D)Init$(D)gs_pdfwr.ps \
	$(PSRESDIR)$(D)Init$(D)gs_pdf_e.ps \
	$(PSRESDIR)$(D)Init$(D)gs_res.ps \
	$(PSRESDIR)$(D)Init$(D)gs_resmp.ps \
	$(PSRESDIR)$(D)Init$(D)gs_setpd.ps \
	$(PSRESDIR)$(D)Init$(D)gs_statd.ps \
	$(PSRESDIR)$(D)Init$(D)gs_std_e.ps \
	$(PSRESDIR)$(D)Init$(D)gs_sym_e.ps \
	$(PSRESDIR)$(D)Init$(D)gs_trap.ps \
	$(PSRESDIR)$(D)Init$(D)gs_ttf.ps \
	$(PSRESDIR)$(D)Init$(D)gs_typ32.ps \
	$(PSRESDIR)$(D)Init$(D)gs_typ42.ps \
	$(PSRESDIR)$(D)Init$(D)gs_type1.ps \
	$(PSRESDIR)$(D)Init$(D)gs_wan_e.ps \
	$(PSRESDIR)$(D)Init$(D)pdf_main.ps \
	$(PSRESDIR)$(D)Init$(D)xlatmap

PS_SUBSTCID_DEPS=\
	$(PSRESDIR)$(D)SubstCID$(D)CNS1-WMode \
	$(PSRESDIR)$(D)SubstCID$(D)GB1-WMode \
	$(PSRESDIR)$(D)SubstCID$(D)Japan1-WMode \
	$(PSRESDIR)$(D)SubstCID$(D)Korea1-WMode

PS_MISC_DEPS=\
	$(PSRESDIR)$(D)Init$(D)FCOfontmap-PCLPS2 \
	$(PSRESDIR)$(D)Init$(D)cidfmap \
	$(PSRESDIR)$(D)Init$(D)gs_cet.ps

PS_ROMFS_DEPS=$(PSSRCDIR)$(D)psromfs.mak $(gconfig_h) $(gs_tr) \
	$(PDF_RESOURCE_DEPS) $(PS_COLORSPACE_DEPS) $(PS_DECODING_DEPS) $(PS_ENCODING_DEPS) \
	$(PS_FONT_DEPS) $(PS_IDIOMSET_DEPS) $(PS_PROCSET_DEPS) $(PS_INIT_DEPS) $(PS_SUBSTCID_DEPS) \
	$(PS_MISC_DEPS)
