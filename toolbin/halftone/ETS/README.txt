Release notes ETS version 150
8 May 2014
Copyright 2000-2014 Artifex Sotware Inc.

This release removed the SSE and VEC code so that we could focus on 
repairing lingering issues with the C code. 

Robin fixed issues in the actual ETS code.  In particular he fixed issues with 
the "white count test" and the FS weights.  We did a fair amount of testing 
with the various parameters. We also added more useful image outputs in the test 
bed allowing the creation of multiple colorants in PSD format including 16bit.
We also moved to a Visual Studio solution for the project.

Release notes ETS version 138
26 Apr 2006
Copyright 2000-2010 Artifex Sotware Inc.
 
This is the latest unified release of Even Tone Screening. The core
ETS algorithm has been highly tuned for modern inkjet printers such as
the latest EPSON seven color devices. In addition, there are alternate
versions of the core screening algorithm written for both Altivec
(G4/PPC 970) and SSE2 (Pentium 4) instruction sets. The quality of the
results with assembly speedup is comparable, but will not be
bit-identical to the C code.


README for Even Better Screening

EBS is the newest refinement of Even Toned Screening. It is based on
the same fundamental algorithms as ETS, and adds several interesting
features.

1. It's possible to screen all planes in tandem, rather than screening
each plane independent. The result is a more even distribution of
inks, and an avoidance of "interference patterns" between similar fine
structure in the planes.

2. The number of levels of output is adjustable at runtime. Most
modern inkjets support multiple drop sizes, and the number of discrete
drop sizes seems to be increasing.

3. Dot aspect ratios of 1:1, 2:1, and 4:1 are directly supported.

4. An option exists to apply Even Toned smoothing to shadow dots as
well as highlight dots. In print modes where individual white dots
in the shadow areas are visible, this avoids wormy patterns. It is
a runtime option because there is a speed penalty. [ Note: this
option not fully supported in the current release ]

5. More parameters are tunable at runtime, including the amplitude of
added random noise, and the scale for the Even Toned output dependent
feedback signal.

Some usage tips:

The default parameters for even_c1_scale, rand_scale, and do_shadows
are all 0, and should give good results. The reasonable range for
rand_scale is about -4 .. 3. Higher values result in more randomness.
The effects should be quite easy to see. even_c1_scale produces a more
subtle effect. Values of -1 or -2 should produce slightly less regular
patterns in highlights. This might be a good idea on printers with
poor accuracy between passes, for example in bidirectional modes.
As mentioned above, set do_shadows to 1 if wormy patterns are visible
in shadows. At high resolutions on most inkjet printers, the dots are
so close together that individual white pixels cannot be seen.

The planes in the call to even_better_line() should be sorted in
darkest-to-lightest order, for example K, C, M, c, m, Y. The
strengths[] parameter should be an array of approximate darkness
values for the inks, for example { 128, 50, 50, 25, 25, 10 }. It
might be worth fiddling with the strength parameters.


Compiling SSE2

To enable SSE2 in evenbetter-rll.c, enable the #define USE_AVEC
conditional compile.

The SSE2 optimized code is in the assembly language file eb_sse2.s.
In addition, I've included two win32-format object files, with and
without the leading underscore. I believe the former is the one to use
for both MS and Watcom compilers.

You can use the NASM tool to reassemble the eb_sse2.s file. Use the
following command line:

nasm eb_sse2.s -f win32

Most compilers will need an additional underscore prefix in front of
the global symbols. There's an _eb_sse2.s file that's the same as
eb_sse2.s except for this change, and it can be assembled using the
same command line as above, just with _eb_sse2.s as the filename.

The included Makefile_sse2 will compile the test_evenbetter executable
on Linux platforms. This utility acts as a pipe and converts pgm to
pgm.


Release notes for 2003-10-21 Altivec-optimized EvenBetter code drop

1. Compiling

To enable Altivec in evenbetter-rll.c, enable the #define USE_AVEC
conditional compile.

The Altivec optimized code is in the C-language file eb_avec.c, using
the Altivec intrinsics. Most up-to-date compilers should have support
for this. With GCC, use the following command line:

gcc -faltivec -O2 -Wall -Wmissing-prototypes   -c -o eb_avec.o eb_avec.c

Note that the -O3 option was tested and did not succeed, at least on
GCC 3.3, build 1435.

On other compilers, it may be necessary to define the HAVE_ALTIVEC_H
preprocessor macro.

The included Makefile_avec will compile the test_evenbetter executable
on OS X platforms. This utility acts as a pipe and converts pgm to
pgm.

2. Code changes

For the most part, this code uses the same API as before. However,
there is one important change: destination buffers _must_ be aligned
to 16-byte boundaries, and should also have an additional 16 bytes of
padding at the end.

I've provided eb_malloc_aligned() and eb_free_aligned() as handy
utility functions for allocating aligned memory blocks. See the
allocation of "obuf" in the sample code (test_evenbetter.c) for an
example.

3. Performance and quality

Barring bugs, quality should be almost identical to the scalar
version.

I did not implement the test for G4 capability, as it's fairly
dependent on platform specifics. Currently, G4 is always assumed,
so the code is likely to crash when run on G3's. To fix this,
edit the line "using_vectors = 1;" so that vectors are only enabled
when the G4/G5 is detected. See:

http://developer.apple.com/hardware/ve/g3_compatibility.html

Performance is most optimal when the number of planes is a multiple of
4. Screening 5 planes will take about the same time as screening 8.


Tone-dependent randomness scaling

As of version 134, ETS can accept lookup tables to control the tone-
dependent randomness scaling. If you do not wish to use this feature,
you must set the new rand_scale_luts field of EvenBetterParams to
NULL.

Otherwise, rand_scale_luts has the same layout as luts - an array of
pointers, one for each plane, to int arrays of size ET_SRC_MAX + 1.
Each value of the array corresponds to one input tone level; in general,
if you change the luts, you'll want to change the rand_scale_luts
accordingly.

The nominal value is 65536, which is a fairly small level of
randomness.  In general, gray values near round rational numbers (1/2,
1/3, etc.) will need higher levels of randomness in order to avoid
repeating patterns. The current default, as computed in
eb_compute_randshift, peaks at 2 << 19 for 1/4 and 1/2, 2 << 18 for
1/3, and 2 << 17 for 1/6 and 1/9.

Note also that even when using the rand_scale_luts, the rand_scale
parameter still has the effect of globally scaling the randomness
value. Each increment by one of rand_scale is equivalent to doubling
the values in the rand_scale luts. simultaneously doubling the
rand_scale_lut values and decreasing rand_scale by 1 has no effect.

To really fine-tune the tone-dependent randomness for a device, we
recommend running grayscale test files with several different
constant values for the luts, then visually choosing the best value
for each grayscale value in the test file. Different devices will be
sensitive to different patterns, depending on the details of the
weaving.

Lastly, keep in mind that the randomness scaling is quantized to
powers of two in the scalar code, but is smoothly variable in the
vector versions.


Notes on per-channel even_c1_scale settings

As of version 135, this code supports per-channel setting of
even_c1_scale, as a way to control artifacts in "dirty highlights".
Recommended values are -2 for full-strength inks for which light
variants exist (magenta, cyan, and black on EPSON 7-color printer),
and 0 for all other inks.


Notes on mseveneighths

The eb_avec vector version contains an experimental workaround for
problems in "dirty highlights". It is disabled by default, but can
easily be enabled by uncommenting these two lines in eb_avec.c:

	//	f_4 = vec_abs(f_3);
	//	f_3 = vec_madd(f_4, mseveneighths, f_3);

The values in mseveneighths in eb_avec.c can also be adjusted -
while the current value is -.875, values of -.90 or -.95 may be
more effective in controlling the dirty highlights problem. However,
values too near -1 seem to create other artifacts.

Pending feedback from users, this experimental feature will either
be removed from all versions, or added to all versions.


Partial version history

Version 138 contains a fix to eb_malloc_aligned for crashes when the
base allocator is aligned to 4-byte but not 8-byte boundaries. Also,
the .obj files for eb_sse2.s are updated, while in some 137 tarballs
they were stale.

Version 137 mostly contains small changes to the vector code to make
it more like the scalar version.

[There was never a consistent 136 release, although some code did go
out with that version number]

Version 135 adds per-channel control of even_c1_scale.

Version 134 has tone-dependent randomness scaling.

Version 133 has some fixes to make the code compile in C++.

Version 132 fixes some improperly merged files in the 131 release.

Version 131 contains a fix for a subtle problem in the noisy highlight
change in 130, as well as more robust support for > 8bpp deep images
on vector platforms.

Version 130 contains a quality improvement in noisy highlight regions.

Version 129 contains performance tuning for the SSE2 implementation.

Version 128 contains a significant speedup in the prep (Altivec only),
specialized to 8-but LUT's with gamma 1.0, 1.8, or 2.0. There is a new
"gamma" parameter to enable it - a value of 0.0 uses the old LUT-based
code.

In addition, make sure the input buffer is allocated with 16-byte
alignment, and is also padded up to a size which is a multiple of 16
bytes.

This version also unifies the SSE2 release. To enable, undefine
USE_AVEC and define USE_SSE2 at the top of evenbetter-rll.c.

