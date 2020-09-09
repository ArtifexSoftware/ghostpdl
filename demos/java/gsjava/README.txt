Required libraries for this library:
gpdldll64.dll
gs_jni.dll

Java Library:

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
Ghoscript instance in the Reference<Long>'s value field.

The com.artifex.gsjava.GSInstance is a
utility class. The GSInstance class stores an instance
of Ghostscript and a client handle. The methods in this
class are named the same in which they are documented without
the need to pass the "instance" parameter through.

The com.artifex.gsjava.devices package contains several
classes representing Ghoscript devices. Using the classes
in this package is the greatest amount of separation from
using the raw Ghoscript calls and it requires no direct
calls through the GSAPI class. All device classes in this
package are extensions of Device which provides a way
of setting up a Ghoscript device and setting parameters.
Subclasses have utility methods to set individual parameters
through method calls instead of using a String and parameter
type to specify the parameter name and type, respectively.

Example code segment to set up an instance of Ghoscript:

import com.artifex.gsjava.*;
import com.artifex.gsjava.util.*;

public static void main(String[] args) {
	// Stores a reference to the Ghoscript instance
	Reference<Long> instanceReference = new Reference<>();

	// Creates a new Ghoscript instance with a NULL caller handle,
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

	// Deletes the Ghoscript instance
	GSAPI.gsapi_delete_instance(instance);
}
