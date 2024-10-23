.. Copyright (C) 2001-2024 Artifex Software, Inc.
.. All Rights Reserved.

.. title:: ZUGFeRD

.. include:: header.rst

.. _ZUGFeRD.html:


Creating ZUGFeRD files
=====================================================================

**ZUGFeRD** (also known as Factur-X, XRechnung) files are PDF/A-3 documents that contain both human-readable and machine-readable invoice data, making them suitable for use in e-invoicing. The **ZUGFeRD** standard, which stands for "Zentraler User Guide des Forums elektronische Rechnung Deutschland," was developed in Germany to streamline electronic invoicing between businesses (B2B) and with public entities (B2G).




What is ZUGFeRD?
----------------------

**ZUGFeRD** is unique because it combines two formats into one invoice:

- Human-readable PDF/A-3: A visual representation of the invoice, just like a regular PDF.
- Machine-readable XML: Embedded within the PDF, this XML file contains structured invoice data, which can be automatically processed by accounting software.

By including both formats, **ZUGFeRD** allows the recipient to either view and print the invoice or process the structured data electronically, eliminating the need for manual data entry.



How to create a ZUGFeRD PDF with Ghostscript
----------------------------------------------


.. note::

    We will assume you have been supplied with, or understand how to make, the XML portion for your invoice.

    Ghostscript is just responsible for ensuring that the generated PDF is PDF/A compliant and for embedding the XML - it does not analyse or validate the XML structure.


To create such an e-invoice we can use the `zugferd.ps` **PostScript** file (found in the ``lib`` folder) along with some command line options as follows:


.. code-block:: bash

   gs
   --permit-file-read=/usr/home/me/zugferd/
   -sDEVICE=pdfwrite
   -dPDFA=3
   -sColorConversionStrategy=RGB
   -sZUGFeRDXMLFile=/usr/home/me/zugferd/invoice.xml
   -sZUGFeRDProfile=/usr/home/me/zugferd/default_rgb.icc
   -sZUGFeRDVersion=2p1
   -sZUGFeRDConformanceLevel=BASIC
   -o /usr/home/me/zugferd/output.pdf
   /usr/home/me/zugferd/zugferd.ps
   /usr/home/me/zugferd/input.pdf


Let's go through these command line options in more detail:

``--permit-file-read``
    Grant access to the folder we allow **Ghostscript** to read from.

``-sDEVICE``
    Select a device to write a PDF.

``-dPDFA``
    Define the PDF/A version. See: :ref:`Creating a PDF/A document <VectorDevices_Creating_PDFA>` for available options. Note: version ``3`` is required for **ZUGFeRD**!

``-sColorConversionStrategy``
    Define the color conversion strategy. See: :ref:`Creating a PDF/A document <VectorDevices_Creating_PDFA>` for available options. Note: this should be consistent with the ICC profile defined in ``-sZUGFeRDProfile``. So for example if this value is ``RGB`` then the associated ICC profile should be an RGB profile.

``-sZUGFeRDXMLFile``
    Define the path to the invoice XML file.

``-sZUGFeRDProfile``
    Define the path to the ICC profile.

``-sZUGFeRDVersion``
    Allows selection of the version of the ZUGFeRD standard, possible values are ``rc``, ``1p0``, ``2p0``, ``2p1``. The default is ``2p1`` which is the 2.1 version of ZUGFeRD.

``-sZUGFeRDConformanceLevel``
    Defines the level of conformance with the standard, possible values are ``MINIMUM``, ``BASIC``, ``BASIC WL``, ``EN 16931``, ``EXTENDED`` and ``XRECHNUNG``. The default is ``BASIC``.

``-o``
    Defines the output PDF file.


.. note::

    The final two arguments are the full path and filename of the ``zugferd.ps`` file, and the full path and filename of the input PDF file respectively.




.. External links

