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

####  LANGUAGE SWITCHING PRODUCT RULES ####

ls_profile:
	make -C language_switch -f pspcl6_gcc.mak pg-fp

ls_product:
	make -C language_switch -f pspcl6_gcc.mak	# build PCL and PCLXL. 

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

# NB - this does not remove the fonts.  blowing away /windows/fonts
# might be unexpected on some systems and we don't enumerate the font
# names here so they could be removed individually.

ls_clean:
	make -C language_switch -f pspcl6_gcc.mak clean
	rm -f fonts /usr/local/bin/pspcl6


PHONY: clean test install product profile ls_clean ls_test ls_install ls_product ls_profile 
