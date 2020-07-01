#! /usr/bin/env python3

'''
Python version of the C API in psi/iapi.h, using ctypes.

Overview:

    All functions have the same name as the C function that they wrap.

    All functions return an integer error code unless otherwise stated. [The
    exceptions are usually for out-parameters, which are returned directly as
    part of a tuple.]

Usage:

    make sodebug
    LD_LIBRARY_PATH=sodebugbin ./toolbin/gsapi.py

Requirements:

    Requires python3.7 or later.

Limitations as of 2020-06-18:

    Only tested on Linux.

    Only very limited testing on has been done.

    We don't provide gsapi_add_fs() or gsapi_remove_fs().

'''

import collections
import ctypes
import sys



gsapi_revision_t = collections.namedtuple('gsapi_revision_t',
        'product copyright revision revisiondate'
        )


def gsapi_revision():
    '''
    Returns (e, r) where <r> is a gsapi_revision_t.
    '''
    _r = _gsapi_revision_t()
    e = _libgs.gsapi_revision(ctypes.byref(_r), ctypes.sizeof(_r))
    if e:
        return e, None
    r = gsapi_revision_t(
            _r.product.decode('latin-1'),
            _r.copyright.decode('latin-1'),
            _r.revision,
            _r.revisiondate,
            )
    return e, r


def gsapi_new_instance(caller_handle):
    '''
    Returns (e, instance).
    '''
    instance = ctypes.c_void_p()
    e = _libgs.gsapi_new_instance(ctypes.byref(instance), ctypes.c_void_p(caller_handle))
    return e, instance


def gsapi_delete_instance(instance):
    e = _libgs.gsapi_delete_instance(instance)
    return e


def gsapi_set_stdio(instance, stdin_fn, stdout_fn, stderr_fn):
    stdin_fn2  = _stdio_fn(stdin_fn)  if stdin_fn  else None
    stdout_fn2 = _stdio_fn(stdout_fn) if stdout_fn else None
    stderr_fn2 = _stdio_fn(stderr_fn) if stderr_fn else None
    e = _libgs.gsapi_set_stdio(instance, stdout_fn2, stdout_fn2, stdout_fn2)
    if not e:
        # Need to keep references to call-back functions.
        global _gsapi_set_stdio_refs
        _gsapi_set_stdio_refs = stdin_fn2, stdout_fn2, stderr_fn2
    return e


def gsapi_set_poll(instance, poll_fn):
    poll_fn2 = _poll_fn(poll_fn)
    e = _libgs.gsapi_set_poll(instance, poll_fn2)
    if not e:
        global _gsapi_set_poll_refs
        _gsapi_set_poll_refs = poll_fn2
    return e


display_callback = collections.namedtuple('display_callback',
    ' size'
    ' version_major'
    ' version_minor'
    ' display_open'
    ' display_preclose'
    ' display_close'
    ' display_presize'
    ' display_size'
    ' display_sync'
    ' display_page'
    ' display_update'
    ' display_memalloc'
    ' display_memfree'
    ' display_separation'
    ,
    defaults=[0]*14,
    )


def gsapi_set_display_callback(instance, callback):
    assert isinstance(callback, display_callback)
    callback2 = _display_callback()

    # Copy from <callback> into <callback2>.
    for name, type_ in _display_callback._fields_:
        value = getattr(callback, name)
        value2 = type_(value)
        setattr(callback2, name, value2)

    e = _libgs.gsapi_set_display_callback(instance, ctypes.byref(callback2))
    if not e:
        # Ensure that we keep references to callbacks.
        global _gsapi_set_display_callback_refs
        _gsapi_set_display_callback_refs = callback2
    return e


def gsapi_set_default_device_list(instance, list_):
    assert isinstance(list_, str)
    e = _libgs.gsapi_set_default_device_list(
            instance,
            list_.encode('latin-1'),
            len(list_),
            )
    return e


;def gsapi_get_default_device_list(instance):
    '''
    Returns (e, list) where <list> is a string.
    '''
    list_ = ctypes.POINTER(ctypes.c_char)()
    len_ = ctypes.c_int()
    e = _libgs.gsapi_get_default_device_list(
            instance,
            ctypes.byref(list_),
            ctypes.byref(len_),
            )
    if e:
        return e, ''
    return e, list_[:len_.value]


GS_ARG_ENCODING_LOCAL = 0
GS_ARG_ENCODING_UTF8 = 1
GS_ARG_ENCODING_UTF16LE = 2


def gsapi_set_arg_encoding(instance, encoding):
    e = _libgs.gsapi_set_arg_encoding(instance, encoding)
    return e


def gsapi_init_with_args(instance, args):
    # Create copy of args in format expected by C.
    argc = len(args)
    argv = (_pchar * (argc + 1))()
    for i, arg in enumerate(args):
        enc_arg = arg.encode('utf-8')
        argv[i] = ctypes.create_string_buffer(enc_arg)
    argv[argc] = None

    e = _libgs.gsapi_init_with_args(instance, argc, argv)
    return e


def gsapi_run_string_begin(instance, user_errors):
    '''
    Returns (e, exit_code).
    '''
    pexit_code = ctypes.c_int()
    e = _libgs.gsapi_run_string_begin(instance, user_errors, ctypes.byref(pexit_code))
    return e, pexit_code.value


def gsapi_run_string_continue(instance, str_, user_errors):
    '''
    Returns (e, exit_code).
    '''
    pexit_code = ctypes.c_int()
    e = _libgs.gsapi_run_string_continue(
            instance,
            str_,
            len(str_),
            user_errors,
            ctypes.byref(pexit_code),
            )
    return e, pexit_code.value


def gsapi_run_string_end(instance, user_errors):
    '''
    Returns (e, exit_code).
    '''
    pexit_code = ctypes.c_int()
    e = _libgs.gsapi_run_string_end(instance, user_errors, ctypes.byref(pexit_code))
    return e, pexit_code.value


def gsapi_run_string_with_length(instance, str_, length, user_errors):
    '''
    Returns (e, exit_code).
    '''
    pexit_code = ctypes.c_int()
    e = _libgs.gsapi_run_string_with_length(
            instance,
            str_,
            length,
            user_errors,
            ctypes.byref(pexit_code),
            )
    return e, pexit_code.value


def gsapi_run_string(instance, str_, user_errors):
    '''
    Returns (e, exit_code).
    '''
    pexit_code = ctypes.c_int()
    e = _libgs.gsapi_run_string(instance, str_, user_errors, ctypes.byref(pexit_code))
    return e, pexit_code.value


def gsapi_run_file(instance, filename, user_errors):
    '''
    Returns (e, exit_code).
    '''
    pexit_code = ctypes.c_int()
    e = _libgs.gsapi_run_file(instance, filename, user_errors, ctypes.byref(pexit_code))
    return e, pexit_code.value


def gsapi_exit(instance):
    e = _libgs.gsapi_exit(instance)
    return e


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


def gsapi_set_param(instance, param, value):
    param2 = param.encode('latin-1')
    if 0: pass
    elif isinstance(value, bool):
        type2 = gs_spt_bool
        value2 = ctypes.byref(ctypes.c_bool(value))
    elif isinstance(value, int):
        type2 = gs_spt_i64
        value2 = ctypes.byref(ctypes.c_longlong(value))
    elif isinstance(value, float):
        type2 = gs_spt_float
        value2 = ctypes.byref(ctypes.c_float(value))
    elif isinstance(value, str):
        # We use gs_spt_string, not psapi_spt_name, because the latter doesn't
        # copy the string.
        type2 = gs_spt_string
        value2 = ctypes.c_char_p(value.encode('latin-1'))
    else:
        assert 0, 'unrecognised type: %s' % type(value)
    e = _libgs.gsapi_set_param(instance, type2, param2, value2)
    return e


GS_PERMIT_FILE_READING = 0
GS_PERMIT_FILE_WRITING = 1
GS_PERMIT_FILE_CONTROL = 2


def gsapi_add_control_path(instance, type_, path):
    e = _libgs.gsapi_add_control_path(instance, type_, path)
    return e


def gsapi_remove_control_path(instance, type_, path):
    e = _libgs.gsapi_remove_control_path(instance, type_, path)
    return e


def gsapi_purge_control_paths(instance, type_):
    e = _libgs.gsapi_purge_control_paths(instance, type_)
    return e


def gsapi_activate_path_control(instance, enable):
    e = _libgs.gsapi_activate_path_control(instance, enable)
    return e


def gsapi_is_path_control_active(instance):
    e = gsapi.gsapi_is_path_control_active(instance)
    return e



# Implementation details.
#


_libgs = ctypes.CDLL('libgs.so')


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
    class gsapi_fs_t(ctypes.Structure):
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

    e, revision  = gsapi_revision()
    print('libgs.gsapi_revision => e=%s revision=%s' % (e, revision))
    assert not e

    e, revision = gsapi_revision()
    print('gsapi_revision() => e=%s: %s' % (e, revision))
    assert not e


    e, instance = gsapi_new_instance(0)
    print('gsapi_new_instance => e=%s: %s' % (e, instance))
    assert not e

    e = gsapi_set_arg_encoding(instance, GS_ARG_ENCODING_UTF8)
    print('gsapi_set_arg_encoding => e=%s' % e)
    assert not e

    def stdout_fn(caller_handle, str_, len_):
        sys.stdout.write(str_[:len_].decode('latin-1').replace('\n', '\n*** '))
        return len_
    e = gsapi_set_stdio(instance, None, stdout_fn, None)
    print('gsapi_set_stdio => e=%s' % e)
    assert not e

    d = display_callback()
    e = gsapi_set_display_callback(instance, d)
    print('gsapi_set_display_callback => e=%s' % e)
    assert not e

    e = gsapi_set_default_device_list(instance, 'bmp256 bmp32b bmpgray bmpmono bmpsep1 bmpsep8 ccr cdeskjet cdj1600 cdj500')
    print('gsapi_set_default_device_list => e=%s' % e)
    assert not e

    e, l = gsapi_get_default_device_list(instance)
    print('gsapi_get_default_device_list => e=%s l=%s' % (e, l))
    assert not e

    e = gsapi_init_with_args(instance, ['gs',])
    print('gsapi_init_with_args => e=%s' % e)
    assert not e

    for value in 32, True, 3.14, 'hello world':
        e = gsapi_set_param(instance, "foo", value);
        print('gsapi_set_param %s => e=%s' % (value, e))
        assert not e
