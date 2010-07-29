# Wrappers for GS builds

ifeq ($(RUN_CLANG_STATIC_ANALYZER),YES)
	CC="$(PROJECT_DIR)/clang_wrapper.sh"
endif

# Add all the flags we want to pass on into recursive invocations of make here
PASSON="CC=$(CC)"

gs-debug-:
	echo "Building Ghostscript (Debug)..."
	$(MAKE) -C ../gs -f base/macosx.mak debug DEBUG=1 TDEBUG=1 GENDIR="./debugobj" $(PASSON)

gs-release-:
	echo "Building Ghostscript (Release)..."
	$(MAKE) -C ../gs -f base/macosx.mak $(PASSON)

gs-debug-clean:
	echo "Cleaning Ghostscript (Debug)..."
	$(MAKE) -C ../gs -f base/macosx.mak debugclean GENDIR="./debugobj" $(PASSON)

gs-release-clean:
	echo "Cleaning Ghostscript (Release)..."
	$(MAKE) -C ../gs -f base/macosx.mak clean $(PASSON)

# Wrappers for PCL builds

pcl-debug-:
	echo "Building GhostPCL (Debug)..."
	$(MAKE) -C ../main -f pcl6_gcc.mak pdl-debug GENDIR="./debugobj" $(PASSON)

pcl-release-:
	echo "Building GhostPCL (Release)..."
	$(MAKE) -C ../main -f pcl6_gcc.mak pdl-product $(PASSON)

pcl-debug-clean:
	echo "Cleaning GhostPCL (Debug)..."
	$(MAKE) -C ../main -f pcl6_gcc.mak pdl-clean GENDIR="./debugobj" $(PASSON)

pcl-release-clean:
	echo "Cleaning GhostPCL (Release)..."
	$(MAKE) -C ../main -f pcl6_gcc.mak pdl-clean $(PASSON)

# Wrappers for LS builds

ls-debug-:
	echo "Building Language Switch (Debug)..."
	$(MAKE) -C ../language_switch -f pspcl6_gcc.mak pdl-debug GENDIR="./debugobj" $(PASSON)

ls-release-:
	echo "Building Language Switch (Release)..."
	$(MAKE) -C ../language_switch -f pspcl6_gcc.mak pdl-product $(PASSON)

ls-debug-clean:
	echo "Cleaning Language Switch (Debug)..."
	$(MAKE) -C ../language_switch -f pspcl6_gcc.mak pdl-clean GENDIR="./debugobj" $(PASSON)

ls-release-clean:
	echo "Cleaning Language Switch (Release)..."
	$(MAKE) -C ../language_switch -f pspcl6_gcc.mak pdl-clean $(PASSON)

# Wrappers for SVG builds

svg-debug-:
	echo "Building SVG (Debug)..."
	$(MAKE) -C ../svg -f svg_gcc.mak pdl-debug GENDIR="./debugobj" $(PASSON)

svg-release-:
	echo "Building SVG (Release)..."
	$(MAKE) -C ../svg -f svg_gcc.mak pdl-product $(PASSON)

svg-debug-clean:
	echo "Cleaning SVG (Debug)..."
	$(MAKE) -C ../svg -f svg_gcc.mak pdl-clean GENDIR="./debugobj" $(PASSON)

svg-release-clean:
	echo "Cleaning SVG (Release)..."
	$(MAKE) -C ../svg -f svg_gcc.mak pdl-clean $(PASSON)

# Wrappers for XPS builds

xps-debug-:
	echo "Building XPS (Debug)..."
	$(MAKE) -C ../xps -f xps_gcc.mak pdl-debug GENDIR="./debugobj" $(PASSON)

xps-release-:
	echo "Building XPS (Release)..."
	$(MAKE) -C ../xps -f xps_gcc.mak pdl-product $(PASSON)

xps-debug-clean:
	echo "Cleaning XPS (Debug)..."
	$(MAKE) -C ../xps -f xps_gcc.mak pdl-clean GENDIR="./debugobj" $(PASSON)

xps-release-clean:
	echo "Cleaning XPS (Release)..."
	$(MAKE) -C ../xps -f xps_gcc.mak pdl-clean $(PASSON)
