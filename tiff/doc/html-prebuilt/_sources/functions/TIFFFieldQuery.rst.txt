TIFFFieldQuery
==============

Synopsis
--------

.. highlight:: c

::

    #include <tiffio.h>

.. c:function:: const TIFFField* TIFFFieldWithName(TIFF* tif, const char *field_name)

.. c:function:: const TIFFField* TIFFFieldWithTag(TIFF* tif, uint32_t tag)

.. c:function:: const TIFFField* TIFFFindField(TIFF* tif, uint32_t tag, TIFFDataType dt)

.. c:function:: int TIFFFieldIsAnonymous(const TIFFField *fip)

.. c:function:: int TIFFFieldSetGetSize(const TIFFField *fip)

.. c:function:: int TIFFFieldSetGetCountSize(const TIFFField* fip)

Description
-----------

.. TODO: Check explanation and intended use of functions.

:c:func:`TIFFFieldWithName` and :c:func:`TIFFFieldWithTag`
return a pointer to TIFF field information structure `fip` by the tag's
*field_name* or its *tag* number.

:c:func:`TIFFFindField` searches for the TIFF field information structure
`fip` of a given *tag* number and a specific `TIFFDataType dt`.
With dt== :c:macro:`TIFF_ANY` the behaviour is the same than for
*TIFFFieldWithTag()*.

    .. TODO: Check if ``libtiff`` is able to handle tag definitions with two different
             definitions. From the code point of view, I don't believe that.

    Such a `TIFFDataType` dependent search could be useful when the same
    tag is defined twice but with different data types, which is true for
    rare cases like TIFFTAG_XCLIPPATHUNITS. However, the ``libtiff`` does
    currently not support multiple definitions of the same tag.


The following routines return status information about TIFF fields.

    :c:var:`fip` is a field information pointer previously returned by
    `TIFFFindField()`, `TIFFFieldWithTag()`, `TIFFFieldWithName()`.

:c:func:`TIFFFieldIsAnonymous` returns true (nonzero) if the field,
read from file, is unknown to ``libtiff`` and a anonymous field has
been auto-registered. Return is zero "0" if field is known to ``libtiff``.
See  :ref:`Tag_Auto_registration`   for more information.

:c:func:`TIFFFieldSetGetSize` returns the data size in bytes of
the field data type used for ``libtiff`` internal storage.
This is also the data size of the parameter to be provided to
:c:func:`TIFFSetField` and :c:func:`TIFFGetField`. Custom
:c:macro:`TIFF_RATIONAL` values can be stored internally either
as ``float`` or ``double``. :c:func:`TIFFFieldSetGetSize` would
then return "4" or "8", respectively.

:c:func:`TIFFFieldSetGetCountSize` returns size of ``count`` parameter
of :c:func:`TIFFSetField` and :c:func:`TIFFGetField` and also if it is
required:  0=none, 2= :c:type:`uint16_t`, 4= :c:type:`uint32_t`

Diagnostics
-----------

None.

See also
--------

:doc:`libtiff` (3tiff)
