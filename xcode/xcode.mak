# Wrappers for PCL builds

pcl-debug-:
	echo "Building GhostPCL (Debug)..."
	$(MAKE) -C ../main -f pcl6_gcc.mak pdl-debug GENDIR="./debugobj"

pcl-release-:
	echo "Building GhostPCL (Release)..."
	$(MAKE) -C ../main -f pcl6_gcc.mak pdl-product

pcl-debug-clean:
	echo "Cleaning GhostPCL (Debug)..."
	$(MAKE) -C ../main -f pcl6_gcc.mak pdl-clean GENDIR="./debugobj"

pcl-release-clean:
	echo "Cleaning GhostPCL (Release)..."
	$(MAKE) -C ../main -f pcl6_gcc.mak pdl-clean

# Wrappers for LS builds

ls-debug-:
	echo "Building Language Switch (Debug)..."
	$(MAKE) -C ../language_switch -f pspcl6_gcc.mak pdl-debug GENDIR="./debugobj"

ls-release-:
	echo "Building Language Switch (Release)..."
	$(MAKE) -C ../language_switch -f pspcl6_gcc.mak pdl-product

ls-debug-clean:
	echo "Cleaning Language Switch (Debug)..."
	$(MAKE) -C ../language_switch -f pspcl6_gcc.mak pdl-clean GENDIR="./debugobj"

ls-release-clean:
	echo "Cleaning Language Switch (Release)..."
	$(MAKE) -C ../language_switch -f pspcl6_gcc.mak pdl-clean

# Wrappers for SVG builds

svg-debug-:
	echo "Building SVG (Debug)..."
	$(MAKE) -C ../svg -f svg_gcc.mak pdl-debug GENDIR="./debugobj"

svg-release-:
	echo "Building SVG (Release)..."
	$(MAKE) -C ../svg -f svg_gcc.mak pdl-product

svg-debug-clean:
	echo "Cleaning SVG (Debug)..."
	$(MAKE) -C ../svg -f svg_gcc.mak pdl-clean GENDIR="./debugobj"

svg-release-clean:
	echo "Cleaning SVG (Release)..."
	$(MAKE) -C ../svg -f svg_gcc.mak pdl-clean

# Wrappers for XPS builds

xps-debug-:
	echo "Building XPS (Debug)..."
	$(MAKE) -C ../xps -f xps_gcc.mak pdl-debug GENDIR="./debugobj"

xps-release-:
	echo "Building XPS (Release)..."
	$(MAKE) -C ../xps -f xps_gcc.mak pdl-product

xps-debug-clean:
	echo "Cleaning XPS (Debug)..."
	$(MAKE) -C ../xps -f xps_gcc.mak pdl-clean GENDIR="./debugobj"

xps-release-clean:
	echo "Cleaning XPS (Release)..."
	$(MAKE) -C ../xps -f xps_gcc.mak pdl-clean
