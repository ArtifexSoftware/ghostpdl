pcl6: fonts
	make -C main -f pcl6_gcc.mak	# build PCL and PCLXL. 

fonts:
	mkdir -p /windows/fonts/	# make a font directory. 2 
	cp urwfonts/*.ttf /windows/fonts/	# copy the fonts. 
	touch fonts

install:
	install main/obj/pcl6 /usr/local/bin

test: pcl6
	cd tools; ../main/obj/pcl6 -sDEVICE=x11alpha owl.pcl frs96.pxl # test with PCL and PXL test file 

# NB - this does not remove the fonts.  blowing away /windows/fonts
# might be unexpected on some systems and we don't enumerate the font
# names here so they could be removed individually.

clean:
	make -C main -f pcl6_gcc.mak clean
	rm -rf fonts main/obj /usr/local/bin/pcl6

PHONY: clean test install pcl6
