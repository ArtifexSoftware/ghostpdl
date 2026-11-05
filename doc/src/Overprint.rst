.. Copyright (C) 2001-2025 Artifex Software, Inc.
.. All Rights Reserved.

.. title:: The Overprint Compositor


.. include:: header.rst

.. _Overprint.html:


============================================
Ghostscript Overprint Compositor (gsovrc.c)
============================================

The overprint compositor in Ghostscript is a specialized core graphics library subsystem. It provides a device-independent layer for processing PostScript and PDF overprint (``/OP``, ``/op``) and overprint modes (``/OPM``). It is implemented primarily within ``base/gsovrc.c`` and functions by pushing a transient compositor device onto the device rendering chain.

Architectural Structure & File Structure
========================================

The subsystem is located within the Ghostscript graphics library (``base/`` directory) and comprises the following files:

* **``base/gsovrc.c``**: Implements the main state management, creation, and setup functions for the overprint compositor device.
* **``base/gsovrc.h``**: Defines the public API exposed to the interpreter and graphics state manager for overprint operations.
* **``base/gxoprect.c`` / ``.h``**: Contains optimized rectangle and polygon fill primitives modified specifically to handle overprint semantics.

The compositor is implemented using Ghostscript's generic compositor framework (``base/gscompt.h`` and ``base/gxcomp.h``). When an overprint graphics state is initialized, this framework constructs a forwarding device that sits directly above the target device or a rendering command list (``clist``) band buffer.

Core Implementation Logic
=========================

The primary goal of ``gsovrc.c`` is channel preservation. While a normal graphics operation replaces background pixel data completely (knockout), the overprint compositor handles the blending mathematically based on active color spaces.

1. **Overprint Device Creation:**
   The interpreter invokes ``gs_create_overprint()`` when it encounters an active overprint state (``/OP true`` or ``/OPM 1``).

2. **Color Masking Generation:**
   The compositor evaluates the current source color space against the device's native color components (e.g., DeviceCMYK, Spot/Separation colors). If Illustrator-style overprint mode (``OPM = 1``) is active, ``gsovrc.c`` creates a bitmask of the color channels. Channels that have a zero value in the source color are masked out, allowing the corresponding target background channels to remain unchanged.

3. **Command-List (clist) Serialization:**
   For banded printing devices, ``gsovrc.c`` serializes the overprint state parameters into the ``clist``. The rendering phase then reads these back to safely process transparency and overprint in bands.

Key Internal Functions & Structures
===================================

Developers interacting with or debugging the ``gsovrc.c`` architecture will encounter these essential hooks:

``gs_create_overprint_compositor()``
    The main factory function used to instantiate a new overprint compositor forwarding device instance based on the current graphics state parameters.

``c_overprint_initialize()``
    Internal lifecycle method that binds the compositor to the underlying target device and matches its color characteristics (color depths, color channel maps).

``gx_overprint_composite_color()``
    Handles the per-pixel or per-component color merging loop, ensuring that unwritten channels do not overwrite existing bitmap backing memory.

Integration with Memory Devices
===============================

To improve performance, Ghostscript's memory devices (``gxdevmem.h``) feature a higher-speed set of special fill routines that work in tandem with ``gsovrc.c``. When rendering directly to memory structures, the compositor hands off operations to these optimized primitives to reduce the performance overhead usually associated with multi-channel bitmask operations.


Implementation notes for driver writers
=======================================

Normally overprint is enabled/disabled within the source postscript/PDF files themselves.

Operation is largely handled in the core, by a overprint compositor being created and pushed onto the device chain. The system checks whether such a compositor is in effect by calling a device special op, gxdso_overprint_active.

We have experimental code intended to support devices being able to turn overprint on or off themselves. This is somewhat specialised, and unlikely to be of interest to most device authors.

For those that do wish to implement this, their device should capture gxdso_overprint_active and return 1, to force the overprint mechanisms to work.

In addition, they should capture gxdso_overprint_adjust. This will be called whenever overprint needs to be reevaluated, and a structure will be passed in with the current overprint parameters.

The device can then change these parameters as it sees fit to modify the overprint settings.

In particular, all forwarding/compsitor devices (such as overprint, clist, and pdf14) should implement gxdso_overprint_adjust to update the `op_was_active` field; they should set the passed on value to 1 if overprint was previously active or not. The target output device at the end of the chain should leave `op_was_active` untouched, and should modify the other params (`retain_any_comps`, `effective_opm`, and/or `drawn_comps`) as appropriate, returning 1 if it modified them, and 0 otherwise. This is required to ensure that we correctly disable overprint when it is no longer needed after having been turned on.


.. include:: footer.rst
