This library builds to gs_jni.dll and uses the
Java Native Interface (JNI) to bind the functions in
the GSAPI class to Ghostscript as well as handling callbacks,
exceptions, and references.

The bindings in GSAPI are in the header com_artifex_gsjava_GSJAVA.h
and the implementations are in com_artifex_gsjava_GSJAVA.cpp.

Utility methods for throwing exceptions, setting fields, calling
Java methods, and using the Reference<T> class are declared
in jni_util.h.

To build, this project requires the header jni.h, which defines
all JNI functions, and jni_md.h, which defines all system-specific
integer types. Each of these headers requires a JDK install.

The headers are typically found in the following directories:

For Windows:

jni.h:
C:\Program Files\Java\<JDK Install>\include\jni.h

jni_md.h:
C:\Program Files\Java\<JDK Install>\include\win32\jni_md.h


For Linux:

jni.h:
/lib/jvm/<JDK Install>/include/jni.h

jni_md.h:
/lib/jvm/<JDK Install>/include/linux/jni_md.h


For Mac:

jni.h:
/Library/Java/JavaVirtualMachines/<JDK Install>/Contents/Home/include/jni.h

jni_md.h:
/Library/Java/JavaVirtualMachines/<JDK Install>/Contents/Home/include/darwin/jni_md.h

So, a setup fully capable of building for any system should have an
include directory resembling this, where each copy of jni_md.h comes
from its respective system:

* jni.h
* win32\jni_md.h
* linux/jni_md.h
* darwin/jni_md.h

To build on Windows,
Ensure Ghostscript has already been built, and then open
the gs_jni.sln (for Visual Studio 2019), and build the gs_jni
project. This will compile to gs_jni.dll. For use in the Java interface,
this DLL must be placed somewhere on the Java PATH. On Windows, this
can be placed in the working directory, next to gsjava.jar (see
..\..\gsjava\README.txt for information on how to build this).

To build on Linux,
Ensure Ghostscript has already been built, and then run
the build_linux.sh script.

To build on Mac,
Ensure Ghostscript has already been built, and then run
the build_darwin.sh script.

Notes on function implementations and a brief introduction on
how to use the JNI:

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

	// Calls the Ghostscript call gsapi_run_string_begin
	int code = gsapi_run_string_begin((void *)instance, userErrors, &exitCode);

	// If the reference is not NULL, set the value of the reference to the exitCode returned
	// from Ghostscript. It must be converted to the wrapper type java.lang.Integer as Java does not support
	// primitive generic arguments (i.e. int, float, char, etc.)
	if (pExitCode)
		Reference::setValueField(env, pExitCode, toWrapperType(env, (jint)exitCode));

	// Return the error code returned by Ghostscript
	return code;
}
