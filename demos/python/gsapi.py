#! /usr/bin/env python3

'''
Python version of the C API in psi/iapi.h.

Copyright (C) 2001-2023 Artifex Software, Inc.

Overview:

    Implemented using Python's ctypes module.

    All functions have the same name as the C function that they wrap.

    Functions raise a GSError exception if the underlying function returned a
    negative error code.

    Functions that don't have out-params return None. Out-params are returned
    directly (using tuples if there are more than one).

    See examples.py for sample usage.

Example usage:

    On Linux/OpenBSD/MacOS:
        Build the ghostscript shared library:
            make sodebug

        Run gsapi.py as a test script:
            GSAPI_LIBDIR=sodebugbin ./demos/python/gsapi.py

    On Windows:
        Build ghostscript dll, for example:
            devenv.com windows/GhostPDL.sln /Build Debug /Project ghostscript

        Run gsapi.py as a test script in a cmd.exe window:
            set GSAPI_LIBDIR=debugbin&& python ./demos/python/gsapi.py

        Run gsapi.py as a test script in a PowerShell window:
            cmd /C "set GSAPI_LIBDIR=debugbin&& python ./demos/python/gsapi.py"

Specifying the Ghostscript shared library:

    Two environmental variables can be used to specify where to find the
    Ghostscript shared library.

    GSAPI_LIB sets the exact path of the ghostscript shared library, else
    GSAPI_LIBDIR sets the directory containing the ghostscript shared
    library. If neither is defined we will use the OS's default location(s) for
    shared libraries.

    If GSAPI_LIB is not defined, the leafname of the shared library is inferred
    from the OS type - libgs.so on Unix, libgs.dylib on MacOS, gsdll64.dll on
    Windows 64.

Requirements:

    Should work on python-2.5+ and python-3.0+, but this might change in
    future.

Limitations as of 2020-07-21:

    Only very limited testing on has been done.

    Tested on Linux, OpenBSD and Windows.

    Only tested with python-3.7 and 2.7.

    We don't provide gsapi_add_fs() or gsapi_remove_fs().

    We only provide display_callback V2, without V3's
    display_adjust_band_height and display_rectangle_request.

'''

import ctypes
import os
import platform
import sys


_lib = os.environ.get('GSAPI_LIB')
if not _lib:
    if platform.system() in ('Linux', 'OpenBSD'):
        _lib = 'libgs.so'
    elif platform.system() == 'Darwin':
        _lib = 'libgs.dylib'
    elif platform.system() == 'Windows':
        if sys.maxsize == 2**31 - 1:
            _lib = 'gsdll32.dll'
        elif sys.maxsize == 2**63 - 1:
            _lib = 'gsdll64.dll'
        else:
            raise Exception('Unrecognised sys.maxsize=0x%x' % sys.maxsize)
    else:
        raise Exception('Unrecognised platform.system()=%s' % platform.system())

    _libdir = os.environ.get('GSAPI_LIBDIR')
    if _libdir:
        _lib = os.path.join(_libdir, _lib)

try:
    _libgs = ctypes.CDLL(_lib)
except Exception as e:
    print('gsapi.py: Failed to load Ghostscript library "%s".' % _lib)
    raise


class GSError(Exception):
    '''
    Exception type for all errors from underlying C library.
    '''
    def __init__(self, gs_error):
        self.gs_error = gs_error
    def __str__(self):
        return 'Ghostscript exception %i: %s' % (
                self.gs_error,
                _gs_error_text(self.gs_error),
                )

class gsapi_revision_t:
    def __init__(self, product, copyright, revision, revisiondate):
        self.product = product
        self.copyright = copyright
        self.revision = revision
        self.revisiondate = revisiondate
    def __str__(self):
        return 'product=%r copyright=%r revision=%r revisiondate=%r' % (
                self.product,
                self.copyright,
                self.revision,
                self.revisiondate,
                )

def gsapi_revision():
    '''
    Returns a gsapi_revision_t.
    '''
    # [unicode: we assume that underlying gsapi_revision() returns utf-8
    # strings.]
    _r = _gsapi_revision_t()
    e = _libgs.gsapi_revision(ctypes.byref(_r), ctypes.sizeof(_r))
    if e < 0:
        raise GSError(e)
    r = gsapi_revision_t(
            _r.product.decode('utf-8'),
            _r.copyright.decode('utf-8'),
            _r.revision,
            _r.revisiondate,
            )
    return r


def gsapi_new_instance(caller_handle=None):
    '''
    Returns an <instance> to be used with other gsapi_*() functions.

    caller_handle:
        Typically unused, but is passed to callbacks e.g. via
        gsapi_set_stdio(). Must be convertible to a C void*, so None or an
        integer is ok but other types such as strings will fail.
    '''
    instance = ctypes.c_void_p()
    e = _libgs.gsapi_new_instance(
            ctypes.byref(instance),
            ctypes.c_void_p(caller_handle),
            )
    if e < 0:
        raise GSError(e)
    return instance


def gsapi_delete_instance(instance):
    e = _libgs.gsapi_delete_instance(instance)
    if e < 0:
        raise GSError(e)


def gsapi_set_stdio(instance, stdin_fn, stdout_fn, stderr_fn):
    '''
    stdin_fn:
        If not None, will be called with (caller_handle, text, len_):
            caller_handle:
                As passed originally to gsapi_new_instance().
            text:
                A ctypes.LP_c_char of length <len_>.

        [todo: wrap this to be easier to use from Python?]

    stdout_fn and stderr_fn:
        If not None, called with (caller_handle, text):
            caller_handle:
                As passed originally to gsapi_new_instance().
            text:
                A Python bytes object.
        Should return the number of bytes of <text> that they handled; for
        convenience None is converted to len(text).
    '''
    # [unicode: we do not do any encoding or decoding; stdin_fn should encode
    # and stdout_fn and stderr_fn should decode. ]
    def make_out(fn):
        if not fn:
            return None
        def out(caller_handle, text, len_):
            text2 = text[:len_]  # converts from ctypes.LP_c_char to bytes.
            ret = fn(caller_handle, text2)
            if ret is None:
                return len_
            return ret
        return _stdio_fn(out)
    def make_in(fn):
        if not fn:
            return None
        return _stdio_fn(fn)

    stdout_fn2 = make_out(stdout_fn)
    stderr_fn2 = make_out(stderr_fn)
    stdin_fn2  = make_in(stdin_fn)
    e = _libgs.gsapi_set_stdio(instance, stdout_fn2, stdout_fn2, stdout_fn2)
    if e < 0:
        raise GSError(e)
    # Need to keep references to call-back functions.
    global _gsapi_set_stdio_refs
    _gsapi_set_stdio_refs = stdin_fn2, stdout_fn2, stderr_fn2


def gsapi_set_poll(instance, poll_fn):
    '''
    poll_fn:
        Will be called with (caller_handle) where <caller_handle> is as passed
        to gsapi_new_instance().
    Not tested.
    '''
    poll_fn2 = _poll_fn(poll_fn)
    e = _libgs.gsapi_set_poll(instance, poll_fn2)
    if e < 0:
        raise GSError(e)
    global _gsapi_set_poll_refs
    _gsapi_set_poll_refs = poll_fn2


class display_callback:
    def __init__(self,
            version_major = 0,
            version_minor = 0,
            display_open = 0,
            display_preclose = 0,
            display_close = 0,
            display_presize = 0,
            display_size = 0,
            display_sync = 0,
            display_page = 0,
            display_update = 0,
            display_memalloc = 0,
            display_memfree = 0,
            display_separation = 0,
            display_adjust_band_height = 0,
            ):
        self.version_major              = version_major
        self.version_minor              = version_minor
        self.display_open               = display_open
        self.display_preclose           = display_preclose
        self.display_close              = display_close
        self.display_presize            = display_presize
        self.display_size               = display_size
        self.display_sync               = display_sync
        self.display_page               = display_page
        self.display_update             = display_update
        self.display_memalloc           = display_memalloc
        self.display_memfree            = display_memfree
        self.display_separation         = display_separation
        self.display_adjust_band_height = display_adjust_band_height


def gsapi_set_display_callback(instance, callback):
    '''
    callback:
        Must be a <display_callback> instance.
    '''
    assert isinstance(callback, display_callback)
    callback2 = _display_callback()
    callback2.size = ctypes.sizeof(callback2)
    # Copy from <callback> into <callback2>.
    for name, type_ in _display_callback._fields_:
        if name == 'size':
            continue
        value = getattr(callback, name)
        value2 = type_(value)
        setattr(callback2, name, value2)

    e = _libgs.gsapi_set_display_callback(instance, ctypes.byref(callback2))
    if e < 0:
        raise GSError(e)
    # Ensure that we keep references to callbacks.
    global _gsapi_set_display_callback_refs
    _gsapi_set_display_callback_refs = callback2


def gsapi_set_default_device_list(instance, list_):
    '''
    list_:
        Must be a string.
    '''
    # [unicode: we assume that underlying gsapi_set_default_device_list() is
    # expecting list_ to be in utf-8 encoding.]
    assert isinstance(list_, str)
    list_2 = list_.encode('utf-8')
    e = _libgs.gsapi_set_default_device_list(instance, list_2, len(list_))
    if e < 0:
        raise GSError(e)


def gsapi_get_default_device_list(instance):
    '''
    Returns a string.
    '''
    # [unicode: we assume underlying gsapi_get_default_device_list() returns
    # strings encoded as latin-1.]
    list_ = ctypes.POINTER(ctypes.c_char)()
    len_ = ctypes.c_int()
    e = _libgs.gsapi_get_default_device_list(
            instance,
            ctypes.byref(list_),
            ctypes.byref(len_),
            )
    if e < 0:
        raise GSError(e)
    return list_[:len_.value].decode('latin-1')


GS_ARG_ENCODING_LOCAL = 0
GS_ARG_ENCODING_UTF8 = 1
GS_ARG_ENCODING_UTF16LE = 2


def gsapi_set_arg_encoding(instance, encoding):
    '''
    encoding:
        Must be one of:
            GS_ARG_ENCODING_LOCAL
            GS_ARG_ENCODING_UTF8
            GS_ARG_ENCODING_UTF16LE
    '''
    assert encoding in (
            GS_ARG_ENCODING_LOCAL,
            GS_ARG_ENCODING_UTF8,
            GS_ARG_ENCODING_UTF16LE,
            )
    global _encoding
    e = _libgs.gsapi_set_arg_encoding(instance, encoding)
    if e < 0:
        raise GSError(e)
    if encoding == GS_ARG_ENCODING_LOCAL:
        # This is probably wrong on Windows.
        _encoding = 'utf-8'
    elif encoding == GS_ARG_ENCODING_UTF8:
        _encoding = 'utf-8'
    elif encoding == GS_ARG_ENCODING_UTF16LE:
        _encoding = 'utf-16-le'


def gsapi_init_with_args(instance, args):
    '''
    args:
        A list/tuple of strings.
    '''
    # [unicode: we assume that underlying gsapi_init_with_args()
    # expects strings in args[] to be encoded in encoding set by
    # gsapi_set_arg_encoding().]

    # Create copy of args in format expected by C.
    argc = len(args)
    argv = (_pchar * (argc + 1))()
    for i, arg in enumerate(args):
        enc_arg = arg.encode(_encoding)
        argv[i] = ctypes.create_string_buffer(enc_arg)
    argv[argc] = None

    e = _libgs.gsapi_init_with_args(instance, argc, argv)
    if e < 0:
        raise GSError(e)


def gsapi_run_string_begin(instance, user_errors=0):
    '''
    Returns <exit_code>.
    '''
    pexit_code = ctypes.c_int()
    e = _libgs.gsapi_run_string_begin(instance, user_errors, ctypes.byref(pexit_code))
    if e < 0:
        raise GSError(e)
    return pexit_code.value


def gsapi_run_string_continue(instance, str_, user_errors=0):
    '''
    <str_> should be either a python string or a bytes object. If the former,
    it is converted into a bytes object using utf-8 encoding.

    We don't raise exception for gs_error_NeedInput.

    Returns <exit_code>.
    '''
    if isinstance(str_, str):
        str_ = str_.encode('utf-8')
    assert isinstance(str_, bytes)
    pexit_code = ctypes.c_int()
    e = _libgs.gsapi_run_string_continue(
            instance,
            str_,
            len(str_),
            user_errors,
            ctypes.byref(pexit_code),
            )
    if e == gs_error_NeedInput.num:
        # This is normal, so we don't raise.
        pass
    elif e < 0:
        raise GSError(e)
    return pexit_code.value


def gsapi_run_string_end(instance, user_errors=0):
    '''
    Returns <exit_code>.
    '''
    pexit_code = ctypes.c_int()
    e = _libgs.gsapi_run_string_end(
            instance,
            user_errors,
            ctypes.byref(pexit_code),
            )
    if e < 0:
        raise GSError(e)
    return pexit_code.value


def gsapi_run_string_with_length(instance, str_, length, user_errors=0):
    '''
    <str_> should be either a python string or a bytes object. If the former,
    it is converted into a bytes object using utf-8 encoding.

    Returns <exit_code>.
    '''
    return gsapi_run_string(instance, str_[:length], user_errors=0)


def gsapi_run_string(instance, str_, user_errors=0):
    '''
    <str_> should be either a python string or a bytes object. If the former,
    it is converted into a bytes object using utf-8 encoding.

    Returns <exit_code>.
    '''
    if isinstance(str_, str):
        str_ = str_.encode('utf-8')
    assert isinstance(str_, bytes)
    pexit_code = ctypes.c_int()
    # We use gsapi_run_string_with_length() because str_ might contain zeros.
    e = _libgs.gsapi_run_string_with_length(
            instance,
            str_,
            len(str_),
            user_errors,
            ctypes.byref(pexit_code),
            )
    if e < 0:
        raise GSError(e)
    return pexit_code.value


def gsapi_run_file(instance, filename, user_errors=0):
    '''
    Returns <exit_code>.
    '''
    # [unicode: we assume that underlying gsapi_run_file() expects <filename>
    # to be encoded in encoding set by gsapi_set_arg_encoding().]
    pexit_code = ctypes.c_int()
    filename2 = filename.encode(_encoding)
    e = _libgs.gsapi_run_file(instance, filename2, user_errors, ctypes.byref(pexit_code))
    if e < 0:
        raise GSError(e)
    return pexit_code.value


def gsapi_exit(instance):
    e = _libgs.gsapi_exit(instance)
    if e < 0:
        raise GSError(e)


gs_spt_invalid = -1
gs_spt_null    = 0 # void * is NULL.
gs_spt_bool    = 1 # void * is NULL (false) or non-NULL (true).
gs_spt_int     = 2 # void * is a pointer to an int.
gs_spt_float   = 3 # void * is a float *.
gs_spt_name    = 4 # void * is a char *.
gs_spt_string  = 5 # void * is a char *.
gs_spt_long    = 6 # void * is a long *.
gs_spt_i64     = 7 # void * is an int64_t *.
gs_spt_size_t  = 8 # void * is a size_t *.
gs_spt_parsed  = 9 # void * is a pointer to a char * to be parsed.
gs_spt__end    = 10

gs_spt_more_to_come = 2**31


def gsapi_set_param(instance, param, value, type_=None):
    '''
    We behave much like the underlying gsapi_set_param() C function, except
    that we also support automatic inference of type type_ arg by looking at
    the type of <value>.

    param:
        Name of parameter, either a bytes or a str; if str it is encoded using
        latin-1.
    value:
        A bool, int, float, bytes or str. If str, it is encoded into a bytes
        using utf-8.

        If <type_> is not None, <value> must be convertible to the Python type
        implied by <type_>:

            type_           Python type(s)
            -----------------------------------------
            gs_spt_null     [Ignored]
            gs_spt_bool     bool
            gs_spt_int      int
            gs_spt_float    float
            gs_spt_name     [Error]
            gs_spt_string   (bytes, str)
            gs_spt_long     int
            gs_spt_i64      int
            gs_spt_size_t   int
            gs_spt_parsed   (bytes, str)

        We raise an exception if <type_> is an integer type and <value> is
        outside its range.
    type_:
        If None, we choose something suitable for type of <value>:

            Python type of <value>  type_
            -----------------------------
            bool                    gs_spt_bool
            int                     gs_spt_i64
            float                   gs_spt_float
            bytes                   gs_spt_parsed
            str                     gs_spt_parsed (encoded with utf-8)

            If <value> is None, we use gs_spt_null.

        Otherwise type_ must be a gs_spt_* except for gs_spt_invalid and
        gs_spt_name (we don't allow psapi_spt_name because the underlying C
        does not copy the string, so cannot be safely used from Python).
    '''
    # [unicode: we assume that underlying gsapi_set_param() expects <param> and
    # string <value> to be encoded as latin-1.]

    if isinstance(param, str):
        param = param.encode('latin-1')
    assert isinstance(param, bytes)

    if type_ is None:
        # Choose a gs_spt_* that matches the Python type of <value>.
        if value is None:
            type_ = gs_spt_null
        elif isinstance(value, bool):
            type_ = gs_spt_bool
        elif isinstance(value, int):
            type_ = gs_spt_i64
        elif isinstance(value, float):
            type_ = gs_spt_float
        elif isinstance(value, (bytes, str)):
            type_ = gs_spt_parsed
        else:
            raise Exception('Unrecognised Python type (must be bool, int, float, bytes or str): %s' % type(value))

    # Make a <value2> suitable for the underlying C gsapi_set_param() function.
    #
    if type_ == gs_spt_null:
        # special-case, we pass value2=None.
        value2 = None
    elif type_ == gs_spt_name:
        # Unsupported.
        raise Exception('gs_spt_name is not supported from Python')
    elif type_ in (gs_spt_string, gs_spt_parsed):
        # String.
        value2 = value
        if isinstance(value2, str):
            value2 = value2.encode('utf-8')
        assert isinstance(value2, bytes)
    else:
        # Bool/int/float.
        type2 = None
        if type_ == gs_spt_bool:
            type2 = ctypes.c_int
        elif type_ == gs_spt_int:
            type2 = ctypes.c_int
        elif type_ == gs_spt_float:
            type2 = ctypes.c_float
        elif type_ == gs_spt_long:
            type2 = ctypes.c_long
        elif type_ == gs_spt_i64:
            type2 = ctypes.c_int64
        elif type_ == gs_spt_size_t:
            type2 = ctypes.c_size_t
        else:
            assert 0, 'unrecognised gs_spt_ value: %s' % type_
        value2 = type2(value)
        if type_ not in (gs_spt_float, gs_spt_bool):
            # Check for out-of-range integers.
            if value2.value != value:
                raise Exception('Value %s => %s is out of range for type %s (%s)' % (
                    value, value2.value, type_, type2))
        value2 = ctypes.byref(value2)

    e = _libgs.gsapi_set_param(instance, param, value2, type_)
    if e < 0:
        raise GSError(e)


def gsapi_get_param(instance, param, type_=None, encoding=None):
    '''
    Returns value of specified parameter, or None if parameter type is
    gs_spt_null.

    param:
        Name of parameter, either a bytes or str; if a str it is encoded using
        latin-1.
    type:
        A gs_spt_* constant or None. If None we try each gs_spt_* until one
        succeeds; if none succeeds we raise the last error.
    encoding:
        Only affects string values. If None we return a bytes object, otherwise
        it should be the encoding to use to decode into a string, e.g. 'utf-8'.
    '''
    # [unicode: we assume that underlying gsapi_get_param() expects <param> to
    # be encoded as latin-1.]
    #
    param2 = param
    if isinstance(param2, str):
        param2 = param2.encode('latin-1')
    assert isinstance(param2, bytes)

    def _get_simple(value_type):
        value = value_type()
        e = _libgs.gsapi_get_param(instance, param2, ctypes.byref(value), type_)
        if e < 0:
            raise GSError(e)
        return value.value

    if type_ is None:
        # Try each type until one succeeds. We raise the last error if no type
        # works.
        for type_ in range(0, gs_spt__end):
            try:
                ret = gsapi_get_param(instance, param2, type_, encoding)
                return ret
            except GSError as e:
                last_error = e
        raise last_error

    elif type_ == gs_spt_null:
        e = _libgs.gsapi_get_param(instance, param2, None, type_)
        if e < 0:
            raise GSError(e)
        return None

    elif type_ == gs_spt_bool:
        ret = _get_simple(ctypes.c_int)
        return ret != 0
    elif type_ == gs_spt_int:
        return _get_simple(ctypes.c_int)
    elif type_ == gs_spt_float:
        return _get_simple(ctypes.c_float)
    elif type_ == gs_spt_long:
        return _get_simple(ctypes.c_long)
    elif type_ == gs_spt_i64:
        return _get_simple(ctypes.c_int64)
    elif type_ == gs_spt_size_t:
        return _get_simple(ctypes.c_size_t)

    elif type_ in (gs_spt_name, gs_spt_string, gs_spt_parsed):
        # Value is a string, so get required buffer size.
        e = _libgs.gsapi_get_param(instance, param2, None, type_)
        if e < 0:
            raise GSError(e)
        value = ctypes.create_string_buffer(e)
        e = _libgs.gsapi_get_param(instance, param2, ctypes.byref(value), type_)
        if e < 0:
            raise GSError(e)
        ret = value.value
        if encoding:
            ret = ret.decode(encoding)
        return ret

    else:
        raise Exception('Unrecognised type_=%s' % type_)


def gsapi_enumerate_params(instance):
    '''
    Yields (key, value) for each param. <key> is decoded as latin-1.
    '''
    # [unicode: we assume that param names are encoded as latin-1.]
    iterator = ctypes.c_void_p()
    key = ctypes.c_char_p()
    type_ = ctypes.c_int()
    while 1:
        e = _libgs.gsapi_enumerate_params(
                instance,
                ctypes.byref(iterator),
                ctypes.byref(key),
                ctypes.byref(type_),
                )
        if e == 1:
            break
        if e:
            raise GSError(e)
        yield key.value.decode('latin-1'), type_.value


GS_PERMIT_FILE_READING = 0
GS_PERMIT_FILE_WRITING = 1
GS_PERMIT_FILE_CONTROL = 2


def gsapi_add_control_path(instance, type_, path):
    # [unicode: we assume that underlying gsapi_add_control_path() expects
    # <path> to be encoded in encoding set by gsapi_set_arg_encoding().]
    path2 = path.encode(_encoding)
    e = _libgs.gsapi_add_control_path(instance, type_, path2)
    if e < 0:
        raise GSError(e)


def gsapi_remove_control_path(instance, type_, path):
    # [unicode: we assume that underlying gsapi_remove_control_path() expects
    # <path> to be encoded in encoding set by gsapi_set_arg_encoding().]
    path2 = path.encode(_encoding)
    e = _libgs.gsapi_remove_control_path(instance, type_, path2)
    if e < 0:
        raise GSError(e)


def gsapi_purge_control_paths(instance, type_):
    e = _libgs.gsapi_purge_control_paths(instance, type_)
    if e < 0:
        raise GSError(e)


def gsapi_activate_path_control(instance, enable):
    e = _libgs.gsapi_activate_path_control(instance, enable)
    if e < 0:
        raise GSError(e)


def gsapi_is_path_control_active(instance):
    e = _libgs.gsapi_is_path_control_active(instance)
    if e < 0:
        raise GSError(e)



# Implementation details.
#

_Error_num_to_error = dict()
class _Error:
    def __init__(self, num, desc):
        self.num = num
        self.desc = desc
        _Error_num_to_error[self.num] = self

gs_error_ok                 = _Error(   0, 'ok')
gs_error_unknownerror       = _Error(  -1, 'unknown error')
gs_error_dictfull           = _Error(  -2, 'dict full')
gs_error_dictstackoverflow  = _Error(  -3, 'dict stack overflow')
gs_error_dictstackunderflow = _Error(  -4, 'dict stack underflow')
gs_error_execstackoverflow  = _Error(  -5, 'exec stack overflow')
gs_error_interrupt          = _Error(  -6, 'interrupt')
gs_error_invalidaccess      = _Error(  -7, 'invalid access')
gs_error_invalidexit        = _Error(  -8, 'invalid exit')
gs_error_invalidfileaccess  = _Error(  -9, 'invalid fileaccess')
gs_error_invalidfont        = _Error( -10, 'invalid font')
gs_error_invalidrestore     = _Error( -11, 'invalid restore')
gs_error_ioerror            = _Error( -12, 'ioerror')
gs_error_limitcheck         = _Error( -13, 'limit check')
gs_error_nocurrentpoint     = _Error( -14, 'no current point')
gs_error_rangecheck         = _Error( -15, 'range check')
gs_error_stackoverflow      = _Error( -16, 'stack overflow')
gs_error_stackunderflow     = _Error( -17, 'stack underflow')
gs_error_syntaxerror        = _Error( -18, 'syntax error')
gs_error_timeout            = _Error( -19, 'timeout')
gs_error_typecheck          = _Error( -20, 'type check')
gs_error_undefined          = _Error( -21, 'undefined')
gs_error_undefinedfilename  = _Error( -22, 'undefined filename')
gs_error_undefinedresult    = _Error( -23, 'undefined result')
gs_error_unmatchedmark      = _Error( -24, 'unmatched mark')
gs_error_VMerror            = _Error( -25, 'VMerror')

gs_error_configurationerror = _Error( -26, 'configuration error')
gs_error_undefinedresource  = _Error( -27, 'undefined resource')
gs_error_unregistered       = _Error( -28, 'unregistered')
gs_error_invalidcontext     = _Error( -29, 'invalid context')
gs_error_invalidid          = _Error( -30, 'invalid id')

gs_error_hit_detected       = _Error( -99, 'hit detected')
gs_error_Fatal              = _Error(-100, 'Fatal')
gs_error_Quit               = _Error(-101, 'Quit')
gs_error_InterpreterExit    = _Error(-102, 'Interpreter Exit')
gs_error_Remap_Color        = _Error(-103, 'Remap Color')
gs_error_ExecStackUnderflow = _Error(-104, 'Exec Stack Underflow')
gs_error_VMreclaim          = _Error(-105, 'VM reclaim')
gs_error_NeedInput          = _Error(-106, 'Need Input')
gs_error_NeedFile           = _Error(-107, 'Need File')
gs_error_Info               = _Error(-110, 'Info')
gs_error_handled            = _Error(-111, 'handled')

def _gs_error_text(gs_error):
    '''
    Returns text description of <gs_error>. See base/gserrors.h.
    '''
    e = _Error_num_to_error.get(gs_error)
    if e:
        return e.desc
    return 'no error'


# The encoding that we use when passing strings to the underlying gsapi_*() C
# functions. Changed by gsapi_set_arg_encoding().
#
# This default is probably incorrect on Windows.
#
_encoding = 'utf-8'

class _gsapi_revision_t(ctypes.Structure):
    _fields_ = [
            ('product',         ctypes.c_char_p),
            ('copyright',       ctypes.c_char_p),
            ('revision',        ctypes.c_long),
            ('revisiondate',    ctypes.c_long),
            ]


_stdio_fn = ctypes.CFUNCTYPE(
        ctypes.c_int,                   # return
        ctypes.c_void_p,                # caller_handle
        ctypes.POINTER(ctypes.c_char),  # str
        ctypes.c_int,                   # len
        )

_gsapi_set_stdio_refs = None


# ctypes representation of int (*poll_fn)(void* caller_handle).
#
_poll_fn = ctypes.CFUNCTYPE(
        ctypes.c_int,       # return
        ctypes.c_void_p,    # caller_handle
        )

_gsapi_set_poll_refs = None


# ctypes representation of display_callback.
#
class _display_callback(ctypes.Structure):
    _fields_ = [
            ('size', ctypes.c_int),
            ('version_major', ctypes.c_int),
            ('version_minor', ctypes.c_int),
            ('display_open',
                    ctypes.CFUNCTYPE(ctypes.c_int,
                            ctypes.c_void_p,    # handle
                            ctypes.c_void_p,    # device
                            )),
            ('display_preclose',
                    ctypes.CFUNCTYPE(ctypes.c_int,
                            ctypes.c_void_p,    # handle
                            ctypes.c_void_p,    # device
                            )),
            ('display_close',
                    ctypes.CFUNCTYPE(ctypes.c_int,
                            ctypes.c_void_p,    # handle
                            ctypes.c_void_p,    # device
                            )),
            ('display_presize',
                    ctypes.CFUNCTYPE(ctypes.c_int,
                            ctypes.c_void_p,    # handle
                            ctypes.c_void_p,    # device
                            ctypes.c_int,       # width
                            ctypes.c_int,       # height
                            ctypes.c_int,       # raster
                            ctypes.c_uint,      # format
                            )),
            ('display_size',
                    ctypes.CFUNCTYPE(ctypes.c_int,
                            ctypes.c_void_p,    # handle
                            ctypes.c_void_p,    # device
                            ctypes.c_int,       # width
                            ctypes.c_int,       # height
                            ctypes.c_int,       # raster
                            ctypes.c_uint,      # format
                            ctypes.c_char_p,    # pimage
                            )),
            ('display_sync',
                    ctypes.CFUNCTYPE(ctypes.c_int,
                            ctypes.c_void_p,    # handle
                            ctypes.c_void_p,    # device
                            )),
            ('display_page',
                    ctypes.CFUNCTYPE(ctypes.c_int,
                            ctypes.c_void_p,    # handle
                            ctypes.c_void_p,    # device
                            ctypes.c_int,       # copies
                            ctypes.c_int,       # flush
                            )),
            ('display_update',
                    ctypes.CFUNCTYPE(ctypes.c_int,
                            ctypes.c_void_p,    # handle
                            ctypes.c_void_p,    # device
                            ctypes.c_int,       # x
                            ctypes.c_int,       # y
                            ctypes.c_int,       # w
                            ctypes.c_int,       # h
                            )),
            ('display_memalloc',
                    ctypes.CFUNCTYPE(ctypes.c_int,
                            ctypes.c_void_p,    # handle
                            ctypes.c_void_p,    # device
                            ctypes.c_ulong,     # size
                            )),
            ('display_memfree',
                    ctypes.CFUNCTYPE(ctypes.c_int,
                            ctypes.c_void_p,    # handle
                            ctypes.c_void_p,    # device
                            ctypes.c_void_p,    # mem
                            )),
            ('display_separation',
                    ctypes.CFUNCTYPE(ctypes.c_int,
                            ctypes.c_void_p,    # handle
                            ctypes.c_void_p,    # device
                            ctypes.c_int,       # component
                            ctypes.c_char_p,    # component_name
                            ctypes.c_ushort,    # c
                            ctypes.c_ushort,    # m
                            ctypes.c_ushort,    # y
                            ctypes.c_ushort,    # k
                            )),
            ]


_libgs.gsapi_set_display_callback.argtypes = (
        ctypes.c_void_p,                    # instance
        ctypes.POINTER(_display_callback),  # callback
        )


_gsapi_set_display_callback_refs = None


# See:
#
#   https://stackoverflow.com/questions/58598012/ctypes-errors-with-argv
#
_pchar = ctypes.POINTER(ctypes.c_char)
_ppchar = ctypes.POINTER(_pchar)

_libgs.gsapi_init_with_args.argtypes = (
        ctypes.c_void_p,    # instance
        ctypes.c_int,       # argc
        _ppchar,            # argv
        )


if 0:
    # Not implemented yet:
    #   gsapi_add_fs()
    #   gsapi_remove_fs()
    #
    class gsapi_fs_t(ctypes.Structure): # lgtm [py/unreachable-statement]
        _fields_ = [
                ('open_file',
                        ctypes.CFUNCTYPE(ctypes.c_int,
                                ctypes.c_pvoid, # const gs_memory_t *mem
                                ctypes.c_pvoid, # secret
                                ctypes.c_char_p,    # fname
                                ctypes.c_char_p,    # mode
                                )),
                ]



if __name__ == '__main__':

    # test
    #

    print('Running some very simple and incomplete tests...')

    print('libgs: %s' % _libgs)

    revision  = gsapi_revision()
    print('libgs.gsapi_revision() ok: %s' % revision)

    instance = gsapi_new_instance()
    print('gsapi_new_instance() ok: %s' % instance)

    gsapi_set_arg_encoding(instance, GS_ARG_ENCODING_UTF8)
    print('gsapi_set_arg_encoding() ok.')

    def stdout_fn(caller_handle, bytes_):
        sys.stdout.write(bytes_.decode('latin-1'))
    gsapi_set_stdio(instance, None, stdout_fn, None)
    print('gsapi_set_stdio() ok.')

    d = display_callback()
    gsapi_set_display_callback(instance, d)
    print('gsapi_set_display_callback() ok.')

    gsapi_set_default_device_list(instance, 'bmp256 bmp32b bmpgray bmpmono bmpsep1 bmpsep8 ccr cdeskjet cdj1600 cdj500')
    print('gsapi_set_default_device_list() ok.')

    l = gsapi_get_default_device_list(instance)
    print('gsapi_get_default_device_list() ok: l=%s' % l)

    gsapi_init_with_args(instance, ['gs',])
    print('gsapi_init_with_args() ok')

    gsapi_set_param(instance, "foo", 100, gs_spt_i64)

    try:
        gsapi_get_param(instance, "foo", gs_spt_i64)
    except GSError as e:
        assert e.gs_error == gs_error_undefined.num, e.gs_error
    else:
        assert 0, 'expected gsapi_get_param() to fail'

    # Check specifying invalid type raises exception.
    try:
        gsapi_get_param(instance, None, -1)
    except Exception as e:
        pass
    else:
        assert 0

    # Check specifying invalid param name raises exception.
    try:
        gsapi_get_param(instance, -1, None)
    except Exception as e:
        pass
    else:
        assert 0

    # Check we can write 64-bit value.
    gsapi_set_param(instance, 'foo', 2**40, None)

    # Check specifying out-of-range raises exception.
    try:
        gsapi_set_param(instance, 'foo', 2**40, gs_spt_int)
    except Exception as e:
        print(e)
        assert 'out of range' in str(e)
    else:
        assert 0

    # Check specifying out-of-range raises exception.
    try:
        gsapi_set_param(instance, 'foo', 2**70, None)
    except Exception as e:
        print(e)
        assert 'out of range' in str(e)
    else:
        assert 0

    print('Checking that we can set and get NumCompies and get same result back')
    gsapi_set_param(instance, "NumCopies", 10, gs_spt_i64)
    v = gsapi_get_param(instance, "NumCopies", gs_spt_i64)
    assert v == 10

    for value in False, True:
        gsapi_set_param(instance, "DisablePageHandler", value)
        v = gsapi_get_param(instance, "DisablePageHandler")
        assert v is value

    for value in 32, True, 3.14:
        gsapi_set_param(instance, "foo", value);
        print('gsapi_set_param() %s ok.' % value)
        try:
            gsapi_get_param(instance, 'foo')
        except GSError as e:
            pass
        else:
            assert 0, 'expected gsapi_get_param() to fail'

    value = "hello world"
    gsapi_set_param(instance, "foo", value, gs_spt_string)
    print('gsapi_set_param() %s ok.' % value)

    gsapi_set_param(instance, "foo", 123, gs_spt_bool)

    try:
        gsapi_set_param(instance, "foo", None, gs_spt_bool)
    except Exception:
        pass
    else:
        assert 0

    # Enumerate all params and print name/value.
    print('gsapi_enumerate_params():')
    for param, type_ in gsapi_enumerate_params(instance):
        value = gsapi_get_param(instance, param, type_)
        value2 = gsapi_get_param(instance, param)
        assert value2 == value, 'value=%s value2=%s' % (value, value2)
        value3 = gsapi_get_param(instance, param, encoding='utf-8')
        print('    %-24s type_=%-5s %r %r' % (param, type_, value, value3))
        assert not isinstance(value, str)
        if isinstance(value, bytes):
            assert isinstance(value3, str)

    print('Finished')
