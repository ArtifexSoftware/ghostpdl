All README files assume that the user is using Java 8.

On Windows, make sure the JDK is installed and the bin directory
inside the installation folder is added to the PATH environment
variable. This exposes command line programs to build Java
applications.

On Linux and Mac, OpenJDK 8 is used.

What's in this directory:

./gsjava	The Java project which provides the bindings to Ghostscript.
./gstest	An old project used for testing gsjava
./gsviewer	A demo Java PDF viewer made using the Java bindings
./jni		Contains the C++ project which backs the Java bindings to Ghostscript
./mtdemo	A demo project used to demonstrate how multithreading can be used with
			the Java bindings