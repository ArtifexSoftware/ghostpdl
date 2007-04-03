all: pcl xps ls_product

debug: pcl_debug ls_debug xps_debug

# only pcl has an install target at this point
install: pcl_install

test: pcl_test ls_test

# legacy targets for backward compatibility
product: pcl

# specific front-end targets

pcl:
	make -C main -f pcl6_gcc.mak	# build PCL and PCLXL. 

pcl_debug: 
	make -C main -f pcl6_gcc.mak debug

fonts:
	mkdir -p /windows/fonts/	# make a font directory. 2 
	cp urwfonts/*.ttf /windows/fonts/	# copy the fonts. 
	touch fonts

pcl_profile:
	make -C main -f pcl6_gcc.mak pg-fp

pcl_install:
	install main/obj/pcl6 /usr/local/bin

pcl_test: 
	cd tools; ../main/obj/pcl6 -dTextAlphaBits=4 owl.pcl tiger.px3 # test with PCL and PXL test file 

# NB - this does not remove the fonts.  blowing away /windows/fonts
# might be unexpected on some systems and we don't enumerate the font
# names here so they could be removed individually.

clean:
	make -C main -f pcl6_gcc.mak clean
	rm -f fonts /usr/local/bin/pcl6


xps_debug: 
	make -C xps -f xps_gcc.mak debug

xps: 
	make -C xps -f xps_gcc.mak 

####  UFST LIBRARY DEPENDENCY RULES ####

ufst:
	make -C ufst/rts/lib -f makefile.artifex

####  LANGUAGE SWITCHING PRODUCT RULES ####

ls_profile:
	make -C language_switch -f pspcl6_gcc.mak pg-fp

ls_product:
	make -C language_switch -f pspcl6_gcc.mak product # build PCL and PCLXL. 

ls_debug: 
	make -C language_switch -f pspcl6_gcc.mak debug

ls_fonts:
	mkdir -p /windows/fonts/	# make a font directory. 2 
	cp urwfonts/*.ttf /windows/fonts/	# copy the fonts. 
	touch fonts

ls_install:
	install language_switch/obj/pspcl6 /usr/local/bin

# test with PCL, PXL and PS file 
ls_test:
	cd tools; ../language_switch/obj/pspcl6 -dTextAlphaBits=4 owl.pcl tiger.px3 ../gs/examples/tiger.ps

check:
	tools/smoke_check.sh


# NB - this does not remove the fonts.  blowing away /windows/fonts
# might be unexpected on some systems and we don't enumerate the font
# names here so they could be removed individually.

ls_clean:
	make -C language_switch -f pspcl6_gcc.mak clean
	rm -f fonts /usr/local/bin/pspcl6

# shortcuts for common build types.

ls_uproduct: ufst
	make -C language_switch -f pspcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" 
	cp *.icc ./language_switch/ufst-obj
	cp wts_* ./language_switch/ufst-obj

ls_udebug: ufst
	make -C language_switch -f pspcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" debug
	cp *.icc ./language_switch/ufst-obj
	cp wts_* ./language_switch/ufst-obj

ls_uclean:
	make -C language_switch -f pspcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" clean
	make -C ufst/rts/lib -f makefile.artifex clean

uproduct: ufst
	make -C main -f pcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" 
	cp *.icc ./language_switch/ufst-obj
	cp wts_* ./language_switch/ufst-obj

udebug: ufst
	make -C main -f pcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" debug
	cp *.icc ./main/ufst-obj
	cp wts_* ./main/ufst-obj

uclean:
	make -C main -f pcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" clean
	make -C ufst/rts/lib -f makefile.artifex clean


all_debug: pcl_debug udebug ls_debug ls_udebug xps_debug


all_clean: clean uclean ls_uclean ls_clean
	make -C ufst/rts/lib -f makefile.artifex clean


.PHONY: all clean test check install product profile pcl pcl_debug pcl_test pcl_install xps xps_debug ls_clean ls_test ls_install ls_product ls_profile ls_udebug udebug ufst
