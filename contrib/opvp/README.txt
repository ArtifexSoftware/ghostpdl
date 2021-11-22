In an effort to remove globals in devices
that ship with Ghostscript, the opvp device was updated
to place it's global values into the device structure.

As part of that commit, a harness was added to enable
testing of the opvp device client API to test for any
issues. To build the harness use  ./build_opv_harness.sh
This will create a debug version of the shared object
libopv.so. The command line
gs -sDEVICE=opvp -sDriver=./contrib/opvp/libopv.so -o output.txt -f ./examples/tiger.eps
can then be used.

This command should create a file called
opvp_command_dump.txt that contains the calls and
the parameters made to the client API. Note that the
harness itself has to rely upon globals. To do otherwise
would require a to change the client API, which we
do not own.