all: pcl xps svg ls-product

all-lib: pcl-lib xps-lib svg-lib ls-lib

debug: pcl-debug xps-debug svg-debug ls-debug

clean: pcl-clean xps-clean svg-clean ls-clean

test: pcl-test ls-test xps-test svg-test

# only pcl has an install target at this point
install: pcl-install

uninstall: pcl-uninstall

product: pcl

# specific front-end targets

pcl: tiff
	$(MAKE) -C main -f pcl6_gcc.mak	pdl-product # build PCL and PCLXL. 

pcl-lib: tiff
	$(MAKE) -C main -f pcl6_gcc.mak	pdl-product-lib # build PCL and PCLXL lib

pcl-debug:  tiff
	$(MAKE) -C main -f pcl6_gcc.mak pdl-debug GENDIR="./debugobj"

fonts:
	mkdir -p /windows/fonts/	# make a font directory. 2 
	cp urwfonts/*.ttf /windows/fonts/	# copy the fonts. 
	touch fonts

pcl-profile: tiff
	$(MAKE) -C main -f pcl6_gcc.mak GENDIR="./pgobj" pdl-pg

pcl-install:
	install main/obj/pcl6 /usr/local/bin

pcl-uninstall:
	rm -f /usr/local/bin/pcl6

pcl-test: 
	./main/obj/pcl6 -dTextAlphaBits=4 \
		tools/owl.pcl tools/tiger.px3 # test with PCL and PXL test files 
# NB - this does not remove the fonts.  blowing away /windows/fonts
# might be unexpected on some systems and we don't enumerate the font
# names here so they could be removed individually.

pcl-clean: tiff_clean
	$(MAKE) -C main -f pcl6_gcc.mak pdl-clean
	rm -f fonts

xps-debug: tiff
	$(MAKE) -C xps -f xps_gcc.mak pdl-debug GENDIR="./debugobj"

xps:  tiff
	$(MAKE) -C xps -f xps_gcc.mak pdl-product # build XPS

xps-lib:  tiff
	$(MAKE) -C xps -f xps_gcc.mak pdl-product-lib # build XPS lib

xps-clean: tiff_clean
	$(MAKE) -C xps -f xps_gcc.mak pdl-clean

xps-test:
	./xps/obj/gxps tools/tiger.xps

svg-debug: tiff
	$(MAKE) -C svg -f svg_gcc.mak pdl-debug GENDIR="./debugobj"

svg:  tiff
	$(MAKE) -C svg -f svg_gcc.mak pdl-product # build SVG

svg-lib:  tiff
	$(MAKE) -C svg -f svg_gcc.mak pdl-product-lib # build SVG lib

svg-clean: tiff_clean
	$(MAKE) -C svg -f svg_gcc.mak pdl-clean

svg-test:
	./svg/obj/gsvg tools/tiger.svg

mupdf-debug:
	$(MAKE) -C mupdf -f bovine_gcc.mak pdl-debug GENDIR="./debugobj"

mupdf:
	$(MAKE) -C mupdf -f bovine_gcc.mak pdl-product

mupdf-clean:
	$(MAKE) -C mupdf -f bovine_gcc.mak pdl-clean


ufst_built:
	$(MAKE) -C ufst/rts/lib -f makefile.artifex
	touch ufst_built	

####  UFST LIBRARY DEPENDENCY RULES ####

ufst: ufst_built

####  LANGUAGE SWITCHING PRODUCT RULES ####

#### Configure the tiff library - this is poor, we shouldn't call a configure
#### script from a makefile - once ghostpdls get their own configures, do it there.

tiff:
	cd ./gs/tiff && ./configure
	touch tiff

tiff_clean:
	# equally unpleasant, we have to explicitly delete tif_config.h
	rm -f gs/tiff/libtiff/tif_config.h gs/tiff/libtiff/tiffconf.h
	rm -f tiff

#### tiff ####

ls-profile: tiff
	$(MAKE) -C language_switch -f pspcl6_gcc.mak GENDIR="./pgobj" pdl-pg

ls-product: tiff
	$(MAKE) -C language_switch -f pspcl6_gcc.mak pdl-product # build PS/PCL/XL. 

ls-lib: tiff
	$(MAKE) -C language_switch -f pspcl6_gcc.mak pdl-product-lib # build PS/PCL/XL lib.

ls-debug:  tiff
	$(MAKE) -C language_switch -f pspcl6_gcc.mak pdl-debug GENDIR="./debugobj"

ls-fonts:
	mkdir -p /windows/fonts/	# make a font directory. 2 
	cp urwfonts/*.ttf /windows/fonts/	# copy the fonts. 
	touch fonts

ls-install:
	install language_switch/obj/pspcl6 /usr/local/bin

# test with PCL, PXL and PS file 
ls-test:
	(cd ./gs/lib/; \
	../../language_switch/obj/pspcl6 -dTextAlphaBits=4 \
	../../tools/owl.pcl ../../tools/tiger.px3 ../examples/tiger.eps)

check:
	tools/smoke_check.sh


# NB - this does not remove the fonts.  blowing away /windows/fonts
# might be unexpected on some systems and we don't enumerate the font
# names here so they could be removed individually.

ls-clean: tiff_clean
	$(MAKE) -C language_switch -f pspcl6_gcc.mak pdl-clean
	rm -f fonts /usr/local/bin/pspcl6

# shortcuts for common build types.

ls-uproduct: ufst tiff
	$(MAKE) -C language_switch -f pspcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" pdl-product
	cp *.icc ./language_switch/ufst-obj
	cp wts_* ./language_switch/ufst-obj

ls-udebug: ufst tiff
	$(MAKE) -C language_switch -f pspcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" pdl-debug
	cp *.icc ./language_switch/ufst-obj
	cp wts_* ./language_switch/ufst-obj

ls-uclean: tiff_clean
	$(MAKE) -C language_switch -f pspcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" pdl-clean
	$(MAKE) -C ufst/rts/lib -f makefile.artifex clean
	rm -f ufst_built

uproduct: ufst tiff
	$(MAKE) -C main -f pcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" pdl-product
	cp *.icc ./main/ufst-obj
	cp wts_* ./main/ufst-obj

udebug: ufst tiff
	$(MAKE) -C main -f pcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-debugobj" pdl-debug
	cp *.icc ./main/ufst-debugobj
	cp wts_* ./main/ufst-debugobj

uclean: tiff_clean
	$(MAKE) -C main -f pcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" pdl-clean
	$(MAKE) -C ufst/rts/lib -f makefile.artifex clean
	rm -f ufst_built

all-debug: pcl-debug udebug ls-debug ls-udebug xps-debug

all-clean: clean uclean ls-uclean ls-clean
	$(MAKE) -C ufst/rts/lib -f makefile.artifex clean

.PHONY: all clean test check install uninstall product profile pcl pcl-debug pcl-test pcl-install pcl-uninstall pcl-clean xps xps-debug svg svg-debug ls-clean ls-test ls-install ls-product ls-profile ls-udebug udebug ufst mupdf
