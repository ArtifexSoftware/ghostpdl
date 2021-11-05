                                api_test
                                ~~~~~~~~

This is a simple VS2019 project that loads the gpdl dll and drives
it via the gsapi functions.

The first test feeds a variety of input formats into gpdl to create
output PDF files. Next, mixtures of different format files are fed
into the same instance, hence generating output PDF files collated
from different sources.

Finally, the display device is driven in a range of different
formats, testing different alignments, chunky/planar formats,
spot colors, and full page/rectangle request modes.

These tests take a while to run, and will result in the outputs
being given as apitest*.{pnm,pam,png,pdf}.

A good quick test of the results is to run:

 md5sum apitest*

and you should see that the bitmaps produced group nicely into
having the same md5sum values according to their color depths.

The same code should compile and run on unix, but has not been
tested there. Some fiddling to load the DLL may be required.

Building with GHOSTPDL=0 will allow the Ghostscript DLL to be
tested. The VS2019 project will need to be edited to use the
appropriate .lib file too.

On unix, the file can be built using:

 make api_test

run using:

 make run_api_test

and cleaned up using:

 make post_api_test


                            multi_test
                            ~~~~~~~~~~

This is a simple VS2019 project that loads the gpdl dll and drives
multiple instances of it from the gsapi functions.

It is intended as a simple demonstration of how to use multiple
instances of the ghostscript library at once; in this case many
ghostscript instances run in parallel within a single process
to convert several different documents at a time.

Building with GHOSTPDL=0 will allow the Ghostscript DLL to be
tested. The VS2019 project will need to be edited to use the
appropriate .lib file too.

On unix, the file can be built using:

 make multi_test

run using:

 make run_multi_test

and cleaned up using:

 make post_multi_test
