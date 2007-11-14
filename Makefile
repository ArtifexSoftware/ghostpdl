all: pcl xps ls_product

debug: pcl_debug ls_debug xps_debug

clean: pcl_clean ls_clean xps_clean

test: pcl_test ls_test

# only pcl has an install target at this point
install: pcl_install

uninstall: pcl_uninstall

product: pcl

# specific front-end targets

pcl:
	$(MAKE) -C main -f pcl6_gcc.mak	product # build PCL and PCLXL. 

pcl_debug: 
	$(MAKE) -C main -f pcl6_gcc.mak debug

fonts:
	mkdir -p /windows/fonts/	# make a font directory. 2 
	cp urwfonts/*.ttf /windows/fonts/	# copy the fonts. 
	touch fonts

pcl_profile:
	$(MAKE) -C main -f pcl6_gcc.mak pg-fp

pcl_install:
	install main/obj/pcl6 /usr/local/bin

pcl_uninstall:
	rm -f /usr/local/bin/pcl6

pcl_test: 
	./main/obj/pcl6 -dTextAlphaBits=4 \
		tools/owl.pcl tools/tiger.px3 # test with PCL and PXL test files 

# NB - this does not remove the fonts.  blowing away /windows/fonts
# might be unexpected on some systems and we don't enumerate the font
# names here so they could be removed individually.

pcl_clean:
	$(MAKE) -C main -f pcl6_gcc.mak clean
	rm -f fonts

xps_debug:
	$(MAKE) -C xps -f xps_gcc.mak debug

xps: 
	$(MAKE) -C xps -f xps_gcc.mak	# build XPS

xps_clean:
	$(MAKE) -C xps -f xps_gcc.mak clean

####  UFST LIBRARY DEPENDENCY RULES ####

ufst:
	$(MAKE) -C ufst/rts/lib -f makefile.artifex

####  LANGUAGE SWITCHING PRODUCT RULES ####

ls_profile:
	$(MAKE) -C language_switch -f pspcl6_gcc.mak pg-fp

ls_product:
	$(MAKE) -C language_switch -f pspcl6_gcc.mak product # build PS/PCL/XL. 

ls_debug: 
	$(MAKE) -C language_switch -f pspcl6_gcc.mak debug

ls_fonts:
	mkdir -p /windows/fonts/	# make a font directory. 2 
	cp urwfonts/*.ttf /windows/fonts/	# copy the fonts. 
	touch fonts

ls_install:
	install language_switch/obj/pspcl6 /usr/local/bin

# test with PCL, PXL and PS file 
ls_test:
	./language_switch/obj/pspcl6 -dTextAlphaBits=4 \
		tools/owl.pcl tools/tiger.px3 gs/examples/tiger.eps

check:
	tools/smoke_check.sh


# NB - this does not remove the fonts.  blowing away /windows/fonts
# might be unexpected on some systems and we don't enumerate the font
# names here so they could be removed individually.

ls_clean:
	$(MAKE) -C language_switch -f pspcl6_gcc.mak clean
	rm -f fonts /usr/local/bin/pspcl6

# shortcuts for common build types.

ls_uproduct: ufst
	$(MAKE) -C language_switch -f pspcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" 
	cp *.icc ./language_switch/ufst-obj
	cp wts_* ./language_switch/ufst-obj

ls_udebug: ufst
	$(MAKE) -C language_switch -f pspcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" debug
	cp *.icc ./language_switch/ufst-obj
	cp wts_* ./language_switch/ufst-obj

ls_uclean:
	$(MAKE) -C language_switch -f pspcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" clean
	$(MAKE) -C ufst/rts/lib -f makefile.artifex clean

uproduct: ufst
	$(MAKE) -C main -f pcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" product
	cp *.icc ./main/ufst-obj
	cp wts_* ./main/ufst-obj

udebug: ufst
	$(MAKE) -C main -f pcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-debugobj" debug
	cp *.icc ./main/ufst-debugobj
	cp wts_* ./main/ufst-debugobj

uclean:
	$(MAKE) -C main -f pcl6_gcc.mak PL_SCALER=ufst GENDIR="./ufst-obj" clean
	$(MAKE) -C ufst/rts/lib -f makefile.artifex clean

all_debug: pcl_debug udebug ls_debug ls_udebug xps_debug

all_clean: clean uclean ls_uclean ls_clean
	$(MAKE) -C ufst/rts/lib -f makefile.artifex clean

.PHONY: all clean test check install uninstall product profile pcl pcl_debug pcl_test pcl_install pcl_uninstall pcl_clean xps xps_debug ls_clean ls_test ls_install ls_product ls_profile ls_udebug udebug ufst
