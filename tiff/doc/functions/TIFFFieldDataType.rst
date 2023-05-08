TIFFFieldDataType
=================

Synopsis
--------

.. highlight:: c

::

    #include <tiffio.h>

.. c:function:: TIFFDataType TIFFFieldDataType(const TIFFField* fip)

Description
-----------

:c:func:`TIFFFieldDataType` returns the data type stored in a TIFF field.
*fip* is a field information pointer previously returned by
:c:func:`TIFFFindField`, :c:func:`TIFFFieldWithTag`,
or :c:func:`TIFFFieldWithName`.

Return values
-------------

:c:func:`TIFFFieldDataType` returns a member of the enum type
:c:type:`TIFFDataType`.

See also
--------

:doc:`libtiff` (3tiff)
