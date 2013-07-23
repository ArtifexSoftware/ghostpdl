# Wrappers for GS builds

ifeq ($(RUN_CLANG_STATIC_ANALYZER),YES)
	# We'd like to use the following so we actually get usable output
	# reported in the GUI, but it fails - see the script for why.
	#CC="$(PROJECT_DIR)/../clang_wrapper.sh"
	# Instead we use the following exe, which is even more of an unholy
	# hack.
	CC="$(PROJECT_DIR)/../clang_wrapper"
	#CC="clang"
	CLANG_WRAPPER=clang_wrapper
else
	CLANG_WRAPPER=
endif

# Add all the flags we want to pass on into recursive invocations of make here
PASSON="CC=$(CC)"

clang_wrapper: clang_wrapper.c
	gcc clang_wrapper.c -o clang_wrapper

# Ghostscript configure stage
gs-conf-: ../gs/Makefile ../gs/.configured

../gs/Makefile: Makefile
	echo "Configuring ghostscript"
	../gs/autogen.sh --without-opvp --with-libiconv=no

../gs/.configured:
	touch ../gs/.configured

gs-conf-clean:
	touch ../gs/.configured
	-rm ../gs/.configured

# GhostPDL configure stage
pdl-conf-: ../.configured

../.configured: Makefile
	echo "Configuring ghostpdl"
	../autogen.sh
	touch ../.configured

pdl-conf-clean:
	touch ../.configured
	-rm ../.configured

# Ghostscript build targets

gs-Debug-: $(CLANG_WRAPPER)
	echo "Building Ghostscript (Debug)..."
	$(MAKE) -C ../gs -f Makefile debug DEBUG=1 TDEBUG=1 GENDIR="./debugobj" $(PASSON)

gs-Release-: $(CLANG_WRAPPER)
	echo "Building Ghostscript (Release)..."
	$(MAKE) -C ../gs -f Makefile $(PASSON)

gs-Debug-clean: gs-conf-clean $(CLANG_WRAPPER)
	echo "Cleaning Ghostscript (Debug)..."
	$(MAKE) -C ../gs -f Makefile debugclean GENDIR="./debugobj" $(PASSON)

gs-Release-clean: gs-conf-clean $(CLANG_WRAPPER)
	echo "Cleaning Ghostscript (Release)..."
	$(MAKE) -C ../gs -f Makefile clean $(PASSON)

# Wrappers for PCL builds

pcl-Debug-: $(CLANG_WRAPPER)
	echo "Building GhostPCL (Debug)..."
	$(MAKE) -C .. pcl-debug $(PASSON)

pcl-Release-: $(CLANG_WRAPPER)
	echo "Building GhostPCL (Release)..."
	$(MAKE) -C .. pcl $(PASSON)

pcl-Debug-clean: pdl-conf-clean $(CLANG_WRAPPER)
	echo "Cleaning GhostPCL (Debug)..."
	$(MAKE) -C .. pcl-debug-clean $(PASSON)

pcl-Release-clean: pdl-conf-clean $(CLANG_WRAPPER)
	echo "Cleaning GhostPCL (Release)..."
	$(MAKE) -C .. pcl-clean $(PASSON)

# Wrappers for LS builds

ls-Debug-: $(CLANG_WRAPPER)
	echo "Building Language Switch (Debug)..."
	$(MAKE) -C .. ls-debug $(PASSON)

ls-Release-: $(CLANG_WRAPPER)
	echo "Building Language Switch (Release)..."
	$(MAKE) -C .. ls-product $(PASSON)

ls-Debug-clean: pdl-conf-clean $(CLANG_WRAPPER)
	echo "Cleaning Language Switch (Debug)..."
	$(MAKE) -C .. ls-debug-clean $(PASSON)

ls-Release-clean: pdl-conf-clean $(CLANG_WRAPPER)
	echo "Cleaning Language Switch (Release)..."
	$(MAKE) -C .. ls-clean $(PASSON)

# Wrappers for SVG builds

svg-Debug-: $(CLANG_WRAPPER)
	echo "Building SVG (Debug)..."
	$(MAKE) -C .. svg-debug $(PASSON)

svg-Release-: $(CLANG_WRAPPER)
	echo "Building SVG (Release)..."
	$(MAKE) -C .. svg $(PASSON)

svg-Debug-clean: pdl-conf-clean $(CLANG_WRAPPER)
	echo "Cleaning SVG (Debug)..."
	$(MAKE) -C .. svg-debug-clean $(PASSON)

svg-Release-clean: pdl-conf-clean $(CLANG_WRAPPER)
	echo "Cleaning SVG (Release)..."
	$(MAKE) -C .. svg-clean $(PASSON)

# Wrappers for XPS builds

xps-Debug-: $(CLANG_WRAPPER)
	echo "Building XPS (Debug)..."
	$(MAKE) -C .. xps-debug $(PASSON)

xps-Release-: $(CLANG_WRAPPER)
	echo "Building XPS (Release)..."
	$(MAKE) -C .. xps $(PASSON)

xps-Debug-clean: pdl-conf-clean $(CLANG_WRAPPER)
	echo "Cleaning XPS (Debug)..."
	$(MAKE) -C .. xps-debug-clean $(PASSON)

xps-Release-clean: pdl-conf-clean $(CLANG_WRAPPER)
	echo "Cleaning XPS (Release)..."
	$(MAKE) -C .. xps-clean $(PASSON)
