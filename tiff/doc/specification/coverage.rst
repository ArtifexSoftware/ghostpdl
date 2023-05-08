LibTIFF Coverage of the TIFF 6.0 Specification
==============================================

The library is capable of dealing with images that are written to
follow the 5.0 or 6.0 TIFF spec.  There is also considerable support
for some of the more esoteric portions of the 6.0 TIFF spec.

.. list-table:: Core requirements
    :widths: 5 20
    :header-rows: 0

    * - Core requirements
      - Both ``MM`` and ``II`` byte orders are handled.
        Both packed and separated planar configuration of samples.
        Any number of samples per pixel (memory permitting).
        Any image width and height (memory permitting).
        Multiple subfiles can be read and written.
        Editing is **not** supported in that related subfiles (e.g.
        a reduced resolution version of an image) are not automatically
        updated.

        Tags handled: ``ExtraSamples``, ``ImageWidth``,
        ``ImageLength``, ``NewSubfileType``, ``ResolutionUnit``.
        ``Rowsperstrip``, ``StripOffsets``, ``StripByteCounts``,
        ``XResolution``, ``YResolution``

    * - Tiled Images
      - ``TileWidth``, ``TileLength``, ``TileOffsets``,
        ``TileByteCounts``

    * - Image Colorimetry Information
      - ``WhitePoint``, ``PrimaryChromaticities``, ``TransferFunction``,
        ``ReferenceBlackWhite``

    * - Class B for bilevel images
      - ``SamplesPerPixel`` = 1

        ``BitsPerSample`` = 1

        ``Compression`` = 1 (none), 2 (CCITT 1D), or 32773 (PackBits)

        ``PhotometricInterpretation`` = 0 (Min-is-White), 1 (Min-is-Black)

    * - Class G for grayscale images
      - ``SamplesPerPixel`` = 1

        ``BitsPerSample`` = 4, 8

        ``Compression`` = 1 (none) 5 (LZW)

        ``PhotometricInterpretation`` = 0 (Min-is-White), 1 (Min-is-Black)

    * - Class P for palette color images
      - ``SamplesPerPixel`` = 1

        ``BitsPerSample`` = 1-8

        ``Compression`` = 1 (none) 5 (LZW)

        ``PhotometricInterpretation`` = 3 (Palette RGB)

        ``ColorMap``

    * - Class R for RGB full color images
      - ``SamplesPerPixel`` = 3

        ``BitsPerSample`` = <8,8,8>

        ``PlanarConfiguration`` = 1, 2

        ``Compression`` = 1 (none) 5 (LZW)

        ``PhotometricInterpretation`` = 2 (RGB)

    * - Class F for facsimile
      - (*Class B tags plus...*)

        ``Compression`` = 3 (CCITT Group 3), 4 (CCITT Group 4)

        ``FillOrder`` = 1 (MSB), 2 (LSB)

        ``Group3Options`` = 1 (2d encoding), 4 (zero fill), 5 (2d+fill)

        ``ImageWidth`` = 1728, 2048, 2482

        ``NewSubFileType`` = 2

        ``ResolutionUnit`` = 2 (Inch), 3 (Centimeter)

        ``PageNumber``,
        ``XResolution``,
        ``YResolution``,
        ``Software``,
        ``BadFaxLines``,
        ``CleanFaxData``,
        ``ConsecutiveBadFaxLines``,
        ``DateTime``,
        ``DocumentName``,
        ``ImageDescription``,
        ``Orientation``

    * - Class S for separated images
      - ``SamplesPerPixel`` = 4

        ``PlanarConfiguration`` = 1, 2

        ``Compression`` = 1 (none), 5 (LZW)

        ``PhotometricInterpretation`` = 5 (Separated)

        ``InkSet`` = 1 (CMYK)

        ``DotRange``,
        ``InkNames``,
        ``DotRange``,
        ``TargetPrinter``

    * - Class Y for YCbCr images
      - ``SamplesPerPixel`` = 3

        ``BitsPerSample`` = <8,8,8>

        ``PlanarConfiguration`` = 1, 2

        ``Compression`` = 1 (none),  5 (LZW), 7 (JPEG)

        ``PhotometricInterpretation`` = 6 (YCbCr)

        ``YCbCrCoefficients``,
        ``YCbCrSubsampling``,
        ``YCbCrPositioning``

        (*colorimetry info from Appendix H; see above*)

    * - Class "JPEG" for JPEG images (per TTN2)
      - ``PhotometricInterpretation`` = 1 (grayscale), 2 (RGB), 5 (CMYK), 6 (YCbCr)

        (*Class Y tags if YCbCr*)

        (*Class S tags if CMYK*)

        ``Compression`` = 7 (JPEG)

In addition, the library supports some optional compression algorithms
that are, in some cases, of dubious value.

.. list-table:: Compression algorithms
    :widths: 5 20
    :header-rows: 1

    * - Compression tag value
      - Compression algorithm
    * - 32766
      - NeXT 2-bit encoding
    * - 32809
      - ThunderScan 4-bit encoding
    * - 32909
      - Pixar companded 11-bit ZIP encoding
    * - 32946
      - PKZIP-style Deflate encoding (experimental)
    * - 34676
      - SGI 32-bit Log Luminance encoding (experimental)
    * - 34677
      - SGI 24-bit Log Luminance encoding (experimental)

Note that there is no support for the JPEG-related tags defined
in the 6.0 specification; the JPEG support is based on the post-6.0
proposal given in TIFF Technical Note #2.

.. note::

    For more information on the experimental Log Luminance encoding
    consult the materials available at
    http://www.anyhere.com/gward/pixformat/tiffluv.html.

The following table shows the tags that are recognized
and how they are used by the library.  If no use is indicated,
then the library reads and writes the tag, but does not use it internally.

.. list-table:: Tags used by libtiff
    :widths: 5 1 1 5
    :header-rows: 1

    * - Tag Name
      - Value
      - R/W<
      - Library's Use (Comments)

    * - ``NewSubFileType``
      - 254
      - R/W
      - none (called ``SubFileType`` in :file:`<tiff.h>`)

    * - ``SubFileType``
      - 255
      - R/W
      - none (called ``OSubFileType`` in :file:`<tiff.h>`)

    * - ``ImageWidth``
      - 256
      - R/W
      - lots

    * - ``ImageLength``
      - 257
      - R/W
      - lots

    * - ``BitsPerSample``
      - 258
      - R/W
      - lots

    * - ``Compression``
      - 259
      - R/W
      - to select appropriate codec

    * - ``PhotometricInterpretation``
      - 262
      - R/W
      - lots

    * - ``Thresholding``
      - 263
      - R/W
      -

    * - ``CellWidth``
      - 264
      -
      - parsed but ignored

    * - ``CellLength``
      - 265
      -
      - parsed but ignored

    * - ``FillOrder``
      - 266
      - R/W
      - control bit order

    * - ``DocumentName``
      - 269
      - R/W
      -

    * - ``ImageDescription``
      - 270
      - R/W
      -

    * - ``Make``
      - 271
      - R/W
      -

    * - ``Model``
      - 272
      - R/W
      -

    * - ``StripOffsets``
      - 273
      - R/W
      - data i/o

    * - ``Orientation``
      - 274
      - R/W
      -

    * - ``SamplesPerPixel``
      - 277
      - R/W
      - lots

    * - ``RowsPerStrip``
      - 278
      - R/W
      - data i/o

    * - ``StripByteCounts``
      - 279
      - R/W
      - data i/o

    * - ``MinSampleValue``
      - 280
      - R/W
      -

    * - ``MaxSampleValue``
      - 281
      - R/W
      -

    * - ``XResolution``
      - 282
      - R/W
      -

    * - ``YResolution``
      - 283
      - R/W
      - used by Group 3 2d encoder

    * - ``PlanarConfiguration``
      - 284
      - R/W
      - data i/o

    * - ``PageName``
      - 285
      - R/W
      -

    * - ``XPosition``
      - 286
      - R/W
      -

    * - ``YPosition``
      - 286
      - R/W
      -

    * - ``FreeOffsets``
      - 288
      -
      - parsed but ignored

    * - ``FreeByteCounts``
      - 289
      -
      - parsed but ignored

    * - ``GrayResponseUnit``
      - 290
      -
      - parsed but ignored

    * - ``GrayResponseCurve``
      - 291
      -
      - parsed but ignored

    * - ``Group3Options``
      - 292
      - R/W
      - used by Group 3 codec

    * - ``Group4Options``
      - 293
      - R/W
      -

    * - ``ResolutionUnit``
      - 296
      - R/W
      - used by Group 3 2d encoder

    * - ``PageNumber``
      - 297
      - R/W
      -

    * - ``ColorResponseUnit``
      - 300
      -
      - parsed but ignored

    * - ``TransferFunction``
      - 301
      - R/W
      -

    * - ``Software``
      - 305
      - R/W
      -

    * - ``DateTime``
      - 306
      - R/W
      -

    * - ``Artist``
      - 315
      - R/W
      -

    * - ``HostComputer``
      - 316
      - R/W
      -

    * - ``Predictor``
      - 317
      - R/W
      - used by LZW codec

    * - ``WhitePoint``
      - 318
      - R/W
      -

    * - ``PrimaryChromacities``
      - 319
      - R/W
      -

    * - ``ColorMap``
      - 320
      - R/W
      -

    * - ``TileWidth``
      - 322
      - R/W
      - data i/o

    * - ``TileLength``
      - 323
      - R/W
      - data i/o

    * - ``TileOffsets``
      - 324
      - R/W
      - data i/o

    * - ``TileByteCounts``
      - 324
      - R/W
      - data i/o

    * - ``BadFaxLines``
      - 326
      - R/W
      -

    * - ``CleanFaxData``
      - 327
      - R/W
      -

    * - ``ConsecutiveBadFaxLines``
      - 328
      - R/W
      -

    * - ``SubIFD``
      - 330
      - R/W
      - subimage descriptor support

    * - ``InkSet``
      - 332
      - R/W
      -

    * - ``InkNames``
      - 333
      - R/W
      -

    * - ``DotRange``
      - 336
      - R/W
      -

    * - ``TargetPrinter``
      - 337
      - R/W
      -

    * - ``ExtraSamples``
      - 338
      - R/W
      - lots

    * - ``SampleFormat``
      - 339
      - R/W
      -

    * - ``SMinSampleValue``
      - 340
      - R/W
      -

    * - ``SMaxSampleValue``
      - 341
      - R/W
      -

    * - ``JPEGTables``
      - 347
      - R/W
      - used by JPEG codec

    * - ``YCbCrCoefficients``
      - 529
      - R/W
      - used by ``TIFFReadRGBAImage`` support

    * - ``YCbCrSubsampling``
      - 530
      - R/W
      - tile/strip size calculations

    * - ``YCbCrPositioning``
      - 531
      - R/W
      -

    * - ``ReferenceBlackWhite``
      - 532
      - R/W
      -

    * - ``Matteing``
      - 32995
      - R
      - none (obsoleted by ``ExtraSamples`` tag)

    * - ``DataType``
      - 32996
      - R
      - none (obsoleted by ``SampleFormat`` tag)

    * - ``ImageDepth``
      - 32997
      - R/W
      - tile/strip calculations

    * - ``TileDepth``
      - 32998
      - R/W
      - tile/strip calculations

    * - ``StoNits``
      - 37439
      - R/W
      -

The ``Matteing`` and ``DataType``
tags have been obsoleted by the 6.0
``ExtraSamples`` and ``SampleFormat`` tags.
Consult the documentation on the
``ExtraSamples`` tag and Associated Alpha for elaboration.  Note however
that if you use Associated Alpha, you are expected to save data that is
pre-multipled by Alpha.  If this means nothing to you, check out
Porter & Duff's paper in the '84 SIGGRAPH proceedings: "Compositing Digital
Images".

The ``ImageDepth``
tag is a non-standard, but registered tag that specifies
the Z-dimension of volumetric data.  The combination of ``ImageWidth``,
``ImageLength``, and ``ImageDepth``,
defines a 3D volume of pixels that are
further specified by ``BitsPerSample`` and
``SamplesPerPixel``.  The ``TileDepth``
tag (also non-standard, but registered) can be used to specified a
subvolume "tiling" of a volume of data.

The Colorimetry, and CMYK tags are additions that appear in TIFF 6.0.
Consult the TIFF 6.0 specification and :doc:`index`.

The JPEG-related tag is specified in
:doc:`technote2`, which defines
a revised JPEG-in-TIFF scheme (revised over the appendix that was
part of the TIFF 6.0 specification).
