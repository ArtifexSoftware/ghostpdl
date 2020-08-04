The following projects show the use of the Ghostscript API
in a Python environment.  The file gsapi.py contains the
API methods and they have the same names and usage as described
in the Ghostscript API documentation.  

If you are working in a Linux based system, _libgs in gsapi.py is specified to
use libgs.so .  If you are working on Windows, you will be using either gpddll32.dll
or gpdldll64.dll.  You will need to build the appropriate library with the ghostscript
build process.

The file examples.py demonstrates several examples using the gsapi methods.
These include, text extraction, object dependent color conversion, distillation,
running of multiple files with the same GhostPDL instance and feeding 
chunks of data to the interpreter.



