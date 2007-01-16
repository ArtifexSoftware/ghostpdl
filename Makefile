product:
	make -C main -f pcl6_gcc.mak	# build PCL and PCLXL. 

debug: 
	make -C main -f pcl6_gcc.mak debug

fonts:
	mkdir -p /windows/fonts/	# make a font directory. 2 
	cp urwfonts/*.ttf /windows/fonts/	# copy the fonts. 
	touch fonts

profile:
	make -C main -f pcl6_gcc.mak pg-fp

install:
	install main/obj/pcl6 /usr/local/bin

test: 
	cd tools; ../main/obj/pcl6 -dTextAlphaBits=4 owl.pcl tiger.px3 # test with PCL and PXL test file 

# NB - this does not remove the fonts.  blowing away /windows/fonts
# might be unexpected on some systems and we don't enumerate the font
# names here so they could be removed individually.

clean:
	make -C main -f pcl6_gcc.mak clean
	rm -f fonts /usr/local/bin/pcl6


.PHONY: xps

xps_debug: 
	make -C main -f pcl6_gcc.mak XPS_INCLUDED=TRUE debug

xps: 
	make -C main -f pcl6_gcc.mak XPS_INCLUDED=TRUE 

met: 
	make -C main -f pcl6_gcc.mak MET_INCLUDED=TRUE 
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

ls_udebug: ufst
	make -C language_switch -f pspcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" debug

ls_uclean:
	make -C language_switch -f pspcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" clean
	make -C ufst/rts/lib -f makefile.artifex clean

uproduct: ufst
	make -C main -f pcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" 

udebug: ufst
	make -C main -f pcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" debug

uclean:
	make -C main -f pcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" clean
	make -C ufst/rts/lib -f makefile.artifex clean


all_debug: debug udebug ls_debug ls_udebug 


all_clean: clean uclean ls_uclean ls_clean
	make -C ufst/rts/lib -f makefile.artifex clean


.PHONY: clean test install product profile ls_clean ls_test ls_install ls_product ls_profile ls_udebug udebug ufst check
