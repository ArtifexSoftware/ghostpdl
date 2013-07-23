ITU-T Rec. T.835 (ex T.JXR-5) | ISO/IEC 29199-5

"Information technology – JPEG XR image coding system – Reference software"

==============
 READ-ME FILE
==============

*********************************************************************
*                                                                   *
* NOTE: the software in folder my_getopt-1.5 is used solely for     *
* facilitating handling of the command line interface paramenters,  *
* and it is not part of the algorithimic implementation of JPEG-XR. *
*                                                                   *
*********************************************************************

JPEG XR reference software 1.8 (September 2009)
-------------------------------------------------------------------

This version constitutes changes to the encoder only. Two bugs were fixed:
1.	Fix a crashing bug for tiling.
2.	Fix a bug in the overlap operators. Overlap code was using row indices relative to the tile instead of relative to the image.

No changes were made to the usage text.

JPEG XR reference software 1.7 (July 2009)
-------------------------------------------------------------------

This version constitutes a change to the encoder only. A bug related to using a detailed quantization information file 
without specifying tile sizes (or without tiling) was fixed.

No changes were made to the usage text.

JPEG XR reference software 1.6 (29 May 2009)
-------------------------------------------------------------------

This version constitutes several improvements to both the sample 
encoder and reference decoder.


Changes related to the decoder are as follows:
1.	Add support for upsampling for non zero values of CHROMA_CENTERING_X and CHROMA_CENTERING_Y
2.	Enhance container handling to test conformance relationship of tag based container format and codestream
3.	Add fix by Thomas Richter for issue caused by some compilers treating unsigned long as 64 bit data
4.	Fix a bug in decoding of interleaved YCC/CMYKDIRECT formats

Changes related to the encoder are as follows:
1.	Add support for separate alpha image plane
2.	Add support for YCC OUTPUT_CLR_FMT (YUV444, YUV422, YUV420)
3.	Add support for CMYKDIRECT
4.	Add support for WINDOWING_FLAG
5.	Add support for the following pixel formats - 24bppBGR, 32bppBGRA, 32bppBGR, 32bppPGBGRA, 64bppPRGBA, 128bppPRGBA
6.	Add code to write IFD tags in the image container. The code for optional tags are in a macro which is disabled by default.
7.      Fix a bug related to encoding 40bppCMYKA and 80bppCMYKA using interleaved alpha mode



New usage text:
Usage: Debug\jpegxr.exe <flags> <input_file.jxr>
Usage: e:\rjsourcedepot\multimedia\dmd\CodecDSP\jxrRefCode\current\Debug\jpegxr.exe <flags> <input_file.jxr>
  DECODER FLAGS:
    [-o <path>] [-w] [-P 44|55|66|111] [-L 4|8|16|32|64|128|255]

        -o: selects output file name (.raw/.tif/.pnm)
            (PNM output can be used only for 24bpp RGB and 8bpp gray Output)
            (TIF output can be used for all formats except the following:
             N channels, BGR, RGBE, YCC, CMYKDirect & Premultiplied RGB)
            (RAW output can be used for all formats)
        -w: tests whether LONG_WORD_FLAG needs to be equal to TRUE
             (will still decode the image)
        -P: selects the maximum accepted profile value
             (44:Sub-Baseline|55:Baseline|66:Main|111:Advanced)
        -L: selects the maximum accepted level value
             (4|8|16|32|64|128)

  ENCODER FLAGS: (Temporary (.tmp) files may be used in encoding)
    -c [-o <path>] [-b ALL|NOFLEXBITS|NOHIGHPASS|DCONLY] [-a 0|1|2] [-p]
    [-f YUV444|YUV422|YUV420] [-F bits] [-h] [-m] [-l 0|1|2]
    [-q q1[:q2[:q3]]] [-Q <path>] [-d] [-w] [-U rows:columns]
    [-C width1[:width2>[:width3...]]] [-R height1[:height2[:height3...]]]
    [-P 44|55|66|111] [-L 4|8|16|32|64|128|255] [-s top|left|bottom|right]
    [-r -W width -H height -M 3|4|...|18 [-B 8|16]]

        -c: selects encoding instead of decoding
             this flag is necessary for encoding
        -o: selects the output file name (.jxr)
        -b: selects the bands to encode
             (ALL<Default>|NOFLEXBITS|NOHIGHPASS|DCONLY)
        -a: selects encoder alpha mode
             (0: no alpha|1:interleaved alpha|2:separate alpha)
             Default: For tif input files, based on the information in the
             PhotometricInterpretation and SamplesPerPixel tags in the container,
             the encoder chooses an input pixel format. If the number
             of components is 4 and photometric is 2, RGBA input is inferred and
             the encoder assumes the presence of an alpha channel while encoding.
             If the number of components is 5 and photometric is 5, CMYKA input is
             inferred. In both these cases, the encoder infers a pixel format with
             an alpha channel. In such cases, the default alpha encoder mode is 2.
             For raw input files, when the -M parameter specified by the user is
             9, 10, 11, 12 13, 14, 23, 24, 25, 26 or 28,
             the default alpha encoder mode is 2
             In all other cases, the default alpha encoder mode is 0.
        -p:  selects an input pixel format with a padding channel
             With tif input, when the encoder infers that the input file has an
             alpha channel (see explanation for -a), this flag causes the encoder
             to treat the alpha channel as a padding channel instead
        -f: selects the internal color format
             (YUV444<Default>|YUV422|YUV420)
        -F: selects the number of flexbits to trim
             (0<default> - 15)
        -h: selects hard tile boundaries
             (soft tile boundaries by default)
        -m: encode in frequency order codestream
             (spatial order by default)
        -l: selects the overlapped block filtering
             (0:off|1:HP only<Default>|2:all)

        -q: sets the quantization values separately, or one per band
             (0<default, lossless> - 255)
        -Q: specifies a file containing detailed quantization information
             See sample.qp
        -d: selects quantization for U/V channels derived from Y channel

        -U: selects uniform tile sizes
        -C: selects the number of tile columns and the width of each
        -R: selects the number of tile rows and the height of each

        -w: sets LONG_WORD_FLAG equal to FALSE
        -P: selects the profile value
             (44:Sub-Baseline|55:Baseline|66:Main|111:Advanced)
        -L: selects the level value
             (4|8|16|32|64|128)
        -s: sets the top, left, bottom, and right margins

        -r: selects encoding with RAW images
             must also specify -W, -H and -M, optional -B
        -W: RAW image width when encoding with RAW images
        -H: RAW image height when encoding with RAW images
        -M: RAW image format when encoding with RAW images
            3: 3-channel
            4: 4-channel
            5: 5-channel
            6: 6-channel
            7: 7-channel
            8: 8-channel
            9: 3-channel Alpha
            10: 4-channel Alpha
            11: 5-channel Alpha
            12: 6-channel Alpha
            13: 7-channel Alpha
            14: 8-channel Alpha
            15: 32bppRGBE
            16: 16bppBGR555
            17: 16bppBGR565
            18: 32bppBGR101010
            19: YCC420
            20: YCC422
            21: YCC444
            22: YCC444 Fixed Point
            23: YCC420 Alpha
            24: YCC422 Alpha
            25: YCC444 Alpha
            26: YCC444 Fixed Point Alpha
            27: CMYKDIRECT
            28: CMYKDIRECT Alpha
            29: 24bppBGR
            30: 32bppBGR
            31: 32bppBGRA
            32: 32bppPBGRA
            33: 64bppPRGBA
            34: 128bppPRGBAFloat
        -B: RAW image bit/channel when encoding with RAW images
            8: 8-bit/channel (default)
            10: 10-bit/channel
            16: 16-bit/channel


Raw File Description:
The raw  file makes it possible to store the results of the output formatting process . The raw file output consists of either interleaved or sequential data from each of the channels in the image, without any header information. Buffers are stored in a raster scan order.
If OUTPUT_CLR_FMT is equal to YUV_420, YUV_422, YUV_444 or CMYKDirect, the buffer for each channel is stored sequentially. For example, when OUTPUT_CLR_FMT equals YUV_444 all Y samples are stored in the output file, followed by all U samples, followed by all V samples. If OUTPUT_CLR_FMT is equal to YUV422, half as many bytes used for the Y channel will be used to store the U and V channels. Otherwise, uf OUTPUT_CLR_FMT is equal to YUV420, one quarter as many bytes as used for the Y channel will be used to store the U and V channels. If OUTPUT_CLR_FMT is equal to YUV444 or YUV422 and OUTPUT_BITDEPTH is equal to BD10, 2 bytes are used per sample, and the 10 bits are stored in the LSBs of each 2 byte pair. Samples decoded from the alpha image plane (when present), are concatenated with the output obtained from the primary image.
Otherwise, if OUTPUT_CLR_FMT is not equal to YUV_420, YUV_422, YUV_444 or CMYKDirect, the raw file output consists of interleaved data from each channel. The number of bytes used to store each sample is contingent upon the value of OUTPUT_BITDEPTH and whether the format is a packed output format or not. The samples decoded from the image alpha plane (when present), are interleaved with the samples decoded from the primary image.


JPEG XR reference software 1.5 (13 Apr 2009)
-------------------------------------------------------------------

This version constitutes several improvements to both the sample 
encoder and reference decoder.

Decoder changes:
-Add support for fixed point formats
-Add support for floating point formats
-Add support for half formats
-Add support for separate alpha image plane
-Add support for interleaved alpha image plane
-Add support for YCC OUTPUT_CLR_FMT (YUV444, YUV422, YUV420)
-Add support for CMYKDIRECT
-Add support for n-Component formats
-Add support for RGBE format
-Add support for packed output formats
-Add support for LONG_WORD_FLAG
-Add support for Profiles/Levels
-Fix a bug related to USE_DC_QP_FLAG and USE_LP_QP_FLAG
-Add support for raw image buffer output file format
-Add support for WINDOWING_FLAG
-Fix a bug with respect to the index table and spatial ordered
 bitstreams
-Improve usage text

Encoder changes:
-Fix a bug with CMYK encoding
-Add support for frequency mode
-Add support for tiling
-Add support for fixed point formats
-Add support for floating point formats
-Add support for half formats
-Add support for interleaved alpha image plane
-Add support for n-Component formats
-Add support for RGBE format
-Add support for packed output formats
-Add support for LONG_WORD_FLAG
-Add support for Profiles/Levels
-Improve usage text

New usage text:
Usage: Release\jpegxr.exe <flags> <input_file.jxr>
  DECODER FLAGS:
    [-o <path>] [-w] [-P 44|55|66|111] [-L 4|8|16|32|64|128|255]

        -o: selects output file name (.raw/.tif/.pnm)
        -w: tests whether LONG_WORD_FLAG needs to be equal to TRUE
             (will still decode the image)
        -P: selects the maximum accepted profile value
             (44:Sub-Baseline|55:Baseline|66:Main|111:Advanced)
        -L: selects the maximum accepted level value
             (4|8|16|32|64|128)

  ENCODER FLAGS: (Temporary (.tmp) files may be used in encoding)
    -c [-o <path>] [-b ALL|NOFLEXBITS|NOHIGHPASS|DCONLY] [-a 0|1|2]
    [-f YUV444|YUV422|YUV420] [-F bits] [-h] [-m] [-l 0|1|2]
    [-q q1[:q2[:q3]]] [-Q <path>] [-d] [-w] [-U rows:columns]
    [-C width1[:width2>[:width3...]]] [-R height1[:height2[:height3...]]]
    [-P 44|55|66|111] [-L 4|8|16|32|64|128|255]
    [-r -W width -H height -M 3|4|5|6|7|8|15|16|17|18 [-B 8|16]]

        -c: selects encoding instead of decoding
             this flag is necessary for encoding
        -o: selects the output file name (.jxr)
        -b: selects the bands to encode
             (ALL<Default>|NOFLEXBITS|NOHIGHPASS|DCONLY)
        -a: selects encoder alpha mode
             (0:no alpha|1:interleaved alpha|2:separate alpha)
        -f: selects the internal color format
             (YUV444<Default>|YUV422|YUV420)
        -F: selects the number of flexbits to trim
             (0<default> - 15)
        -h: selects hard tile boundaries
             (soft tile boundaries by default)
        -m: encode in frequency order codestream
             (spatial order by default)
        -l: selects the overlapped block filtering
             (0:off|1:HP only<Default>|2:all)

        -q: sets the quantization values separately, or one per band
             (0<default, lossless> - 255)
        -Q: specifies a file containing detailed quantization information
             See sample.qp
        -d: selects quantization for U/V channels derived from Y channel

        -U: selects uniform tile sizes
        -C: selects the number of tile columns and the width of each
        -R: selects the number of tile rows and the height of each

        -w: sets LONG_WORD_FLAG equal to FALSE
        -P: selects the profile value
             (44:Sub-Baseline|55:Baseline|66:Main|111:Advanced)
        -L: selects the level value
             (4|8|16|32|64|128)

        -r: selects encoding with RAW images
             must also specify -W, -H and -M, optional -B
        -W: RAW image width when encoding with RAW images
        -H: RAW image height when encoding with RAW images
        -M: RAW image format when encoding with RAW images
            3: 3-channel
            4: 4-channel
            5: 5-channel
            6: 6-channel
            7: 7-channel
            8: 8-channel
            9: 3-channel Alpha
            10: 4-channel Alpha
            11: 5-channel Alpha
            12: 6-channel Alpha
            13: 7-channel Alpha
            14: 8-channel Alpha
            15: 32bppRGBE
            16: 16bppBGR555
            17: 16bppBGR565
            18: 32bppBGR101010
        -B: RAW image bit/channel when encoding with RAW images
            8: 8-bit/channel (default)
            16: 16-bit/channel

Raw File Description:
The raw  file makes it possible to store the results of the output formatting process . The raw file output consists of either interleaved or sequential data from each of the channels in the image, without any header information. Buffers are stored in a raster scan order.
If OUTPUT_CLR_FMT is equal to YUV_420, YUV_422, YUV_444 or CMYKDirect, the buffer for each channel is stored sequentially. For example, when OUTPUT_CLR_FMT equals YUV_444 all Y samples are stored in the output file, followed by all U samples, followed by all V samples. If OUTPUT_CLR_FMT is equal to YUV422, half as many bytes used for the Y channel will be used to store the U and V channels. Otherwise, uf OUTPUT_CLR_FMT is equal to YUV420, one quarter as many bytes as used for the Y channel will be used to store the U and V channels. If OUTPUT_CLR_FMT is equal to YUV444 or YUV422 and OUTPUT_BITDEPTH is equal to BD10, 2 bytes are used per sample, and the 10 bits are stored in the LSBs of each 2 byte pair. Samples decoded from the alpha image plane (when present), are concatenated with the output obtained from the primary image.
Otherwise, if OUTPUT_CLR_FMT is not equal to YUV_420, YUV_422, YUV_444 or CMYKDirect, the raw file output consists of interleaved data from each channel. The number of bytes used to store each sample is contingent upon the value of OUTPUT_BITDEPTH and whether the format is a packed output format or not. The samples decoded from the image alpha plane (when present), are interleaved with the samples decoded from the primary image.


JPEG XR reference software 1.4 (31 Dec 2008)
-------------------------------------------------------------------

This version contains a bug fix and integrates the hard tile proposal.
Indentations have been reformatted. It fixes a bug related to decoding of 
multiple tile images with quantization step sizes varying across tiles.

JPEG XR reference software 1.3 (30 Sept 2008)
-------------------------------------------------------------------

This version was circulated for CD ballot.  In has the same
technical design as the 1.2 version.  Some renaming of
variables and strings was performed to replace references to
"HD Photo" (and "hdp") with references to "JPEG XR" (and "jxr").
Some minor header reformatting was performed.

JPEG XR reference software 1.2
-------------------------------------------------------------------

This version integrates the fixed POT filter proposed by
Microsoft. To enable or disable this filter, edit the
line setting FIXED_POT in jxr_priv.h. It is by default enabled
now.

JPEG XR reference software 1.1
-------------------------------------------------------------------

This is an updated/modified version of the 1.0 release of the
reference software, originally provided by Microsoft and Picture
Elements. Modifications for release 1.1 were made by the University
of Stuttgart (Thomas Richter), and are provided under the same
licence terms as release 1.0.

The following modifications have been made for this release:

- The main program prints a summary of its command line arguments
if an input file is missing.

- A new flag "-d" has been added that derives the quantization
parameter for the color planes in the same way the DPK software
derived its quantization parameters from the Y plane. This makes the
reference software backwards compatible to the DPK.



JPEG XR reference software 1.0
-------------------------------------------------------------------


SUMMARY

The jpegxr program is an example program that uses the jpegxr.h
compression/decompression API.

USAGE

jpegxr <flags> <input-file>

FLAGS

* -c

Compress.  Normally, jpegxr decompresses the input file. This flag
requests compression instead.

* -b ALL|NOFLEXBITS|NOHIGHPASS|DCONLY

When compressing, this flag selects the subbands to use. The names are
taken from the JPEG XR standard. The default is '-b ALL'. This is
only useful for compression.

* -f <flag>

Set various encode/decode options.

* -F <bits>

Enable FLEXBIT trimming. If FLEXBITS are enabled (-b ALL) then the
<bits> value is the number of bits to trim. The useful value ranges
from 0 to 15 inclusive. -F 0 keeps the most FLEXBITS, and -F 15 keeps
the fewest bits. This is only useful for compression.

* -l [0|1|2]

Enable overlap pre-filtering. The default is '-l 0' which turns off
filtering. Values 1 and 2 enable first pass only or first and second
pass filters respectively.

* -o <path>

Give the output file destination. If comression is enabled (-c) then
this is the path to the output compressed file. Otherwise, this is the
path where image data is written.

* -q q1:q2:q3...

Give the quantization code for each channel. If only 1 value is
given, then use the same quantization value for all channels. if
multiple values are given, separated by a ':', then each is assigned
to a channel. If fewer values are given then there are channels, then
the last value is used for all the remaining channels. For example,
one can use '-q 2:4' to specify Q=2 for Y and Q=4 for U and V channels
of a color image. The default is '-q 0', which along with '-b ALL'
makes compression lossless. The useful range for this value is 0..255.

* -d

Derived quantization. If this parameter is specified, only one argument
for -q is required and the code derives more suitable quantization values
for the Cb and Cr chroma components itself, similar to the DPK code.
This option improves coding efficency noticably whenever the color
transformation is run.

* -Q <path>

Give an ASCII source file that contains detailed information about the
quantization to use, down to a macroblock level.


ENCODE/DECODE OPTIONS

The -f argument can take any of a variety of options that control
encode decode. The -f flag can appear as often as needed, with one
flag at a time. The currently supported flags are:

   YUV444  -- Encode full color, with no subsampling of UV planes.

   YUV422  -- Encode color by subsampling the UV planes of YUV444.

   YUV420  -- Encode color by subsampling the UV planes of YUV444.

QP_FILE SYNTAX

Comments start with a '#' character and continue to the end of the
line. A comment can start anywhere in the line.

The keywords are:

    DC, LP, HP
    channel
    independent
    separate
    tile
    uniform

A NUMBER is an unsigned decimal value.

A file consists of a number of tile descriptors, one tile descriptor
for each expected tile in the image. A tile descripter is:

   tile ( <n>, <n> ) { tile_comp_mode tile_body }

A tile_comp_mode is one of the keywords: uniform, separate, or
independent. The tile_body is an unordered list of tile_items:

   channel <n> { channel_body }
   LP [ map_list ]
   HP [ map_list ]

The channel_body gives channel-specific information. How many channels
depends on the number of channels and the tile_comp_mode. A channel
body is one each of these channel items:

   DC { <n> }
   LP { <n>... }
   HP { <n>... }

--END