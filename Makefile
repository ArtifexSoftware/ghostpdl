all: pcl xps svg ls-product

all-lib: pcl-lib xps-lib svg-lib ls-lib

debug: pcl-debug xps-debug svg-debug ls-debug

memento: pcl-memento xps-memento svg-memento ls-memento

clean: pcl-clean xps-clean svg-clean ls-clean

debug-clean: pcl-debug-clean xps-debug-clean svg-debug-clean ls-debug-clean

memento-clean: pcl-memento-clean xps-memento-clean svg-memento-clean ls-memento-clean

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

pcl-lib-shared: tiff
	$(MAKE) -C main -f pcl6_gcc.mak	pdl-product-solib # build PCL and PCLXL shared lib

pcl-debug:  tiff
	$(MAKE) -C main -f pcl6_gcc.mak pdl-debug GENDIR="./debugobj"

pcl-memento:  tiff
	$(MAKE) -C main -f pcl6_gcc.mak pdl-memento GENDIR="./memobj"

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

pcl-debug-clean: tiff_clean
	$(MAKE) -C main -f pcl6_gcc.mak pdl-clean GENDIR="./debugobj"
	rm -f fonts

pcl-memento-clean: tiff_clean
	$(MAKE) -C main -f pcl6_gcc.mak pdl-clean GENDIR="./memobj"
	rm -f fonts

xps-debug: tiff
	$(MAKE) -C xps -f xps_gcc.mak pdl-debug GENDIR="./debugobj"

xps-memento: tiff
	$(MAKE) -C xps -f xps_gcc.mak pdl-memento GENDIR="./memobj"

xps:  tiff
	$(MAKE) -C xps -f xps_gcc.mak pdl-product # build XPS

xps-lib:  tiff
	$(MAKE) -C xps -f xps_gcc.mak pdl-product-lib # build XPS lib

xps-clean: tiff_clean
	$(MAKE) -C xps -f xps_gcc.mak pdl-clean

xps-debug-clean: tiff_clean
	$(MAKE) -C xps -f xps_gcc.mak pdl-clean GENDIR="./debugobj"

xps-memento-clean: tiff_clean
	$(MAKE) -C xps -f xps_gcc.mak pdl-clean GENDIR="./memobj"

xps-test:
	./xps/obj/gxps tools/tiger.xps

svg-debug: tiff
	$(MAKE) -C svg -f svg_gcc.mak pdl-debug GENDIR="./debugobj"

svg-memento: tiff
	$(MAKE) -C svg -f svg_gcc.mak pdl-memento GENDIR="./memobj"

svg:  tiff
	$(MAKE) -C svg -f svg_gcc.mak pdl-product # build SVG

svg-lib:  tiff
	$(MAKE) -C svg -f svg_gcc.mak pdl-product-lib # build SVG lib

svg-clean: tiff_clean
	$(MAKE) -C svg -f svg_gcc.mak pdl-clean

svg-debug-clean: tiff_clean
	$(MAKE) -C svg -f svg_gcc.mak pdl-clean GENDIR="./debugobj"

svg-memento-clean: tiff_clean
	$(MAKE) -C svg -f svg_gcc.mak pdl-clean GENDIR="./memobj"

svg-test:
	./svg/obj/gsvg tools/tiger.svg

mupdf-debug:
	$(MAKE) -C mupdf -f bovine_gcc.mak pdl-debug GENDIR="./debugobj"

mupdf:
	$(MAKE) -C mupdf -f bovine_gcc.mak pdl-product

mupdf-clean:
	$(MAKE) -C mupdf -f bovine_gcc.mak pdl-clean

mupdf-debug-clean:
	$(MAKE) -C mupdf -f bovine_gcc.mak pdl-clean GENDIR="./debugobj"


ufst_built:
	$(MAKE) -C ufst/rts/lib -f makefile.artifex
	touch ufst_built	

####  UFST LIBRARY DEPENDENCY RULES ####

ufst: ufst_built

####  LANGUAGE SWITCHING PRODUCT RULES ####

#### Configure the tiff library - this is poor, we shouldn't call a configure
#### script from a makefile - once ghostpdls get their own configures, do it there.

tiff:
	cd ./gs/tiff && ./configure --disable-jbig
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

ls-memento:  tiff
	$(MAKE) -C language_switch -f pspcl6_gcc.mak pdl-memento GENDIR="./memobj"

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
	#rm -f fonts /usr/local/bin/pspcl6

ls-debug-clean: tiff_clean
	$(MAKE) -C language_switch -f pspcl6_gcc.mak pdl-clean GENDIR="./debugobj"

# shortcuts for common build types.

ls-uproduct: ufst tiff
	$(MAKE) -C language_switch -f pspcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" pdl-product

ls-udebug: ufst tiff
	$(MAKE) -C language_switch -f pspcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-debugobj" pdl-debug

ls-umemento: ufst tiff
	$(MAKE) -C language_switch -f pspcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-memobj" pdl-memento

ls-uclean: tiff_clean
	$(MAKE) -C language_switch -f pspcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" pdl-clean
	$(MAKE) -C ufst/rts/lib -f makefile.artifex clean
	rm -f ufst_built

ls-udebug-clean: tiff_clean
	$(MAKE) -C language_switch -f pspcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" pdl-clean
	$(MAKE) -C ufst/rts/lib -f makefile.artifex debug-clean
	rm -f ufst_built

ls-umemento-clean: tiff_clean
	$(MAKE) -C language_switch -f pspcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-memobj" pdl-clean
	$(MAKE) -C ufst/rts/lib -f makefile.artifex debug-clean
	rm -f ufst_built

uproduct: ufst tiff
	$(MAKE) -C main -f pcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" pdl-product

udebug: ufst tiff
	$(MAKE) -C main -f pcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-debugobj" pdl-debug

umemento: ufst tiff
	$(MAKE) -C main -f pcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-memobj" pdl-memento

uclean: tiff_clean
	$(MAKE) -C main -f pcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" pdl-clean
	$(MAKE) -C ufst/rts/lib -f makefile.artifex clean
	rm -f ufst_built

udebug-clean: tiff_clean
	$(MAKE) -C main -f pcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-debugobj" pdl-clean
	$(MAKE) -C ufst/rts/lib -f makefile.artifex clean
	rm -f ufst_built

umemento-clean: tiff_clean
	$(MAKE) -C main -f pcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-memobj" pdl-clean
	$(MAKE) -C ufst/rts/lib -f makefile.artifex clean
	rm -f ufst_built

all-debug: pcl-debug udebug ls-debug ls-udebug xps-debug

all-memento: pcl-memento umemento ls-memento ls-umemento xps-memento

all-clean: clean clean-debug clean-memento ls-clean ls-debug-clean ls-memento-clean uclean udebug-clean umemento-clean ls-uclean ls-udebug-clean ls-umemento-clean
	$(MAKE) -C ufst/rts/lib -f makefile.artifex clean

.PHONY: all clean test check install uninstall product profile pcl pcl-debug pcl-memento pcl-test pcl-install pcl-uninstall pcl-clean pcl-debug-clean pcl-memento-clean xps xps-debug xps-memento svg svg-debug svg-memento ls-clean ls-debug-clean ls-memento-clean ls-test ls-install ls-product ls-profile ls-udebug ls-umemento udebug umemento ufst mupdf
