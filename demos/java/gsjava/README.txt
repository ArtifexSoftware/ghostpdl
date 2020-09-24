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

C++ Library:

This library builds to gs_jni.dll and uses the
Java Native Interface (JNI) to bind the functions in
the GSAPI class to Ghoscript as well as handling callbacks,
exceptions, and references.

The bindings in GSAPI are in the header com_artifex_gsjava_GSJAVA.h
and the implementations are in com_artifex_gsjava_GSJAVA.cpp.

Utility methods for throwing exceptions, setting fields, calling
Java methods, and using the Reference<T> class are declared
in jni_util.h.

Example method explaining the implementation for
gsapi_run_string_begin:


/* JNIEnv *env - Provided the JNI. Allows interfacing into calling Java methods,
 * setting Java fields, etc. Different threads have different JNIEnv objects.
 *
 * jclass - Provided by the JNI. It represents the class calling this method.
 *
 * jlong instance - Instance to be passed to gsapi_run_string_begin
 *
 * jint userErrors - User errors to be passed to gsapi_run_string_begin
 *
 * jobject pExitCode - A Reference<Integer> object. This is always the case as the Java
 * code will not allow anything else to be passed. The value which Ghostscript returns
 * in the pExitCode parameter will be placed into here.
 */
JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1run_1string_1begin
	(JNIEnv *env, jclass, jlong instance, jint userErrors, jobject pExitCode)
{
	// Declares the exitCode parameter which will be passed as a pointer
	// to gsapi_run_string_begin
	int exitCode;

	// Different threads have different JNIEnv objects, so each time a JNI method
	// is called, this must be set so the Java callback methods can be called.
	callbacks::setJNIEnv(env);

	// Calls the Ghoscript call gsapi_run_string_begin
	int code = gsapi_run_string_begin((void *)instance, userErrors, &exitCode);

	// If the reference is not NULL, set the value of the reference to the exitCode returned
	// from Ghoscript. It must be converted to the wrapper type java.lang.Integer as Java does not support
	// primitive generic arguments (i.e. int, float, char, etc.)
	if (pExitCode)
		Reference::setValueField(env, pExitCode, toWrapperType(env, (jint)exitCode));

	// Return the error code returned by Ghostscript
	return code;
}

Viewer:

There is an eclipse project containing a viewer using the
Java Ghostscript bindings in gsjava. The eclipse project is
set up such that the gsjava eclipse project must be built
to a jar file and used in the gsviewer eclipse project.

The viewer is designed to load PDF documents, but can also
load other files like PostScript files and distill them into
a PDF document to be opened.

When the document is first loaded, every page is rendered at
a low resolution. These will be displayed as thumbnails on the
left side. All loadingg is done in the com.artifex.gsjava.Document
class. As the user changes the position of the viewport, the
viewer will dynamically load high resolution images based on
what page the user is viewing. This is done in the
com.artifex.gsviewer.ViewerController.SmartLoader class. This
also handles rendering zoomed images if necessary. Only pages
which need to be loaded at a high resolution are loaded, and
zoomed pages are unloaded when no longer necessary to save memory.