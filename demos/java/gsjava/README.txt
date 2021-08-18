This library contains direct bindings to Ghostscript as
well as several utility methods to make using the
Ghostscript calls easier.

The direct Ghostscript calls are found in
com.artifex.gsjava.GSAPI and are named in the same way
in which they are documented on https://www.ghostscript.com/doc/current/API.htm

The com.artifex.gsjava.callbacks package contains
interfaces which store interfaces and abstract classes
used to implement Ghostscript callbacks in Java. These
can be passed to Ghostscript through the GSAPI class.

The com.artifex.gsjava.util package contains several
classes to assist in passing parameters and handling
native values. The Reference<T> and ByteArrayReference are
the only classes which need to be used, however the others
are avaliable if needed. The Reference class acts as
a pointer parameter in a C function. An example use of this
is in gsapi_new_instance which takes a Reference<Long> as
a parameter. Calling the method would place the new
Ghostscript instance in the Reference<Long>'s value field.

The com.artifex.gsjava.GSInstance is a
utility class. The GSInstance class stores an instance
of Ghostscript and a client handle. The methods in this
class are named the same in which they are documented without
the need to pass the "instance" parameter through.

The com.artifex.gsjava.devices package contains several
classes representing Ghostscript devices. Using the classes
in this package is the greatest amount of separation from
using the raw Ghostscript calls and it requires no direct
calls through the GSAPI class. All device classes in this
package are extensions of Device which provides a way
of setting up a Ghostscript device and setting parameters.
Subclasses have utility methods to set individual parameters
through method calls instead of using a String and parameter
type to specify the parameter name and type, respectively.

Example code segment to set up an instance of Ghostscript:

import com.artifex.gsjava.*;
import com.artifex.gsjava.util.*;

public static void main(String[] args) {
	// Stores a reference to the Ghostscript instance
	Reference<Long> instanceReference = new Reference<>();

	// Creates a new Ghostscript instance with a NULL caller handle,
	// placing the instance into instanceReference
	int code = GSAPI.gsapi_new_instance(instanceReference, GSAPI.GS_NULL);

	if (code != GSAPI.GS_ERROR_OK) {
		// An error has occured.
		return;
	}

	// Returns the value the instanceReference references.
	// This value is equivalent to a void * and has the same
	// size in bytes.
	long instance = instanceReference.getValue();

	// Deletes the Ghostscript instance
	GSAPI.gsapi_delete_instance(instance);
}

To build the library, build_linux.sh can be used on Linux,
build_darwin.sh can be used on Mac, and build_win32.bat on Windows.

All of these scripts do not compile Ghostscript. However,
build_linux.sh and build_darwin.sh will compile the C++
library unlike build_win32.bat.

Viewer:

There is an Eclipse project containing a viewer using the
Java Ghostscript bindings in gsjava. The Eclipse project is
set up such that the gsjava Eclipse project must be built
to a jar file and used in the gsviewer Eclipse project.

The viewer is designed to load PDF documents, but can also
load other files like PostScript files and distill them into
a PDF document to be opened.

When the document is first loaded, every page is rendered at
a low resolution. These will be displayed as thumbnails on the
left side. All loading is done in the com.artifex.gsjava.Document
class. As the user changes the position of the viewport, the
viewer will dynamically load high resolution images based on
what page the user is viewing. This is done in the
com.artifex.gsviewer.ViewerController.SmartLoader class. This
also handles rendering zoomed images if necessary. Only pages
which need to be loaded at a high resolution are loaded, and
zoomed pages are unloaded when no longer necessary to save memory.
