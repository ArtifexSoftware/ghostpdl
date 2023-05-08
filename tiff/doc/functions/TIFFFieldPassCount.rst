TIFFFieldPassCount
==================

Synopsis
--------

.. highlight:: c

::

    #include <tiffio.h>

.. c:function:: int TIFFFieldPassCount(const TIFFField* fip)

Description
-----------

:c:func:`TIFFFieldPassCount` returns true (nonzero) if
:c:func:`TIFFGetField` and :c:func:`TIFFSetField`
expect a :c:var:`count` value to be passed before the actual data pointer.

:c:var:`fip` is a field information pointer previously returned by
:c:func:`TIFFFindField`,
:c:func:`TIFFFieldWithTag`,
:c:func:`TIFFFieldWithName`.

When a :c:var:`count` is required, it will be of type :c:type:`uint32_t`
if :c:func:`TIFFFieldReadCount` reports :c:macro:`TIFF_VARIABLE2`,
and of type :c:type:`uint16_t` otherwise.  (This distinction is
critical for use of :c:func:`TIFFGetField`, but normally not so for
use of :c:func:`TIFFSetField`.)

Return values
-------------

:c:func:`TIFFFieldPassCount` returns an integer that is always 1 (true)
or 0 (false).

See also
--------

:doc:`libtiff`
