Terminology
===========

.. image:: images/strike.gif
    :width: 128
    :alt: strike

The following definitions are used throughout this documentation.
They are consistent with the terminology used in the TIFF 6.0 specification.

Sample:
    The unit of information stored in an image; often called a
    channel elsewhere.  Sample values are numbers, usually unsigned
    integers, but possibly in some other format if the SampleFormat
    tag is specified in a TIFF

Pixel:
    A collection of one or more samples that go together.

Row:
    An Nx1 rectangular collection of pixels.

Tile:
    An NxM rectangular organization of data (or pixels).

Strip:
    A tile whose width is the full image width.

Compression:
    A scheme by which pixel or sample data are stored in
    an encoded form, specifically with the intent of reducing the
    storage cost.

Codec:
    Software that implements the decoding and encoding algorithms
    of a compression scheme.

In order to better understand how TIFF works (and consequently this
software) it is important to recognize the distinction between the
physical organization of image data as it is stored in a TIFF and how
the data is interpreted and manipulated as pixels in an image.  TIFF
supports a wide variety of storage and data compression schemes that
can be used to optimize retrieval time and/or minimize storage space.
These on-disk formats are independent of the image characteristics; it
is the responsibility of the TIFF reader to process the on-disk storage
into an in-memory format suitable for an application.  Furthermore, it
is the responsibility of the application to properly interpret the
visual characteristics of the image data.  TIFF defines a framework for
specifying the on-disk storage format and image characteristics with
few restrictions.  This permits significant complexity that can be
daunting.  Good applications that handle TIFF work by handling as wide
a range of storage formats as possible, while constraining the
acceptable image characteristics to those that make sense for the
application.
