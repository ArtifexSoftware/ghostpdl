#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Tue Jul 21 10:05:07 2020
Example use of gsapi for various tasks.

@author: Michael Vrhel
"""

try:
    import gsapi
except Exception:
    print('Failure to import gsapi. Check shared library path')
    raise

import os

ghostpdl_root = os.path.abspath('%s/../../..' % __file__)
print('ghostpdl_root=%s' % ghostpdl_root)

def run_gpdl(params, path):
    instance = gsapi.gsapi_new_instance(0)
    gsapi.gsapi_set_arg_encoding(instance, gsapi.GS_ARG_ENCODING_UTF8)
    gsapi.gsapi_add_control_path(instance, gsapi.GS_PERMIT_FILE_READING, path)
    gsapi.gsapi_init_with_args(instance, params)
    end_gpdl(instance)

def init_gpdl(params):
    instance = gsapi.gsapi_new_instance(0)
    gsapi.gsapi_set_arg_encoding(instance, gsapi.GS_ARG_ENCODING_UTF8)
    gsapi.gsapi_init_with_args(instance, params)
    return instance

def run_file(instance, filename):
    exitcode = gsapi.gsapi_run_file(instance, filename, None)
    return exitcode

def end_gpdl(instance):
    gsapi.gsapi_exit(instance)
    gsapi.gsapi_delete_instance(instance)

# run multiple files through same instance
def multiple_files():

    out_filename = 'multi_file_output_%d.png'

    params =['gs', '-dNOPAUSE', '-dBATCH', '-sDEVICE=pngalpha',
             '-r72', '-o', out_filename]
    instance = init_gpdl(params)
    run_file(instance, '%s/examples/tiger.eps' % ghostpdl_root)
    run_file(instance, '%s/examples/snowflak.ps' % ghostpdl_root)
    run_file(instance, '%s/examples/annots.pdf' % ghostpdl_root)

    end_gpdl(instance)

# Extract text from source file
def extract_text():

    in_filename = '%s/examples/alphabet.ps' % ghostpdl_root
    out_filename = 'alphabet.txt'
    print('Extracting text from %s to %s' % (in_filename, out_filename))

    params =['gs', '-dNOPAUSE', '-dBATCH','-sDEVICE=txtwrite',
             '-dTextFormat=3','-o', out_filename, '-f', in_filename]
    run_gpdl(params, in_filename)

# Perform different color conversions on text, graphic, and image content
# through the use of different destination ICC profiles
def object_dependent_color_conversion():

    in_filename = '%s/examples/text_graph_image_cmyk_rgb.pdf' % ghostpdl_root
    out_filename = 'rendered_profile.tif'
    image_icc = '%s/toolbin/color/icc_creator/effects/cyan_output.icc' % ghostpdl_root
    graphic_icc = '%s/toolbin/color/icc_creator/effects/magenta_output.icc' % ghostpdl_root
    text_icc = '%s/toolbin/color/icc_creator/effects/yellow_output.icc' % ghostpdl_root
    print('Object dependent color conversion on %s to %s' % (in_filename, out_filename))

    params =['gs', '-dNOPAUSE', '-dBATCH', '-sDEVICE=tiff32nc',
             '-r72','-sImageICCProfile=' + image_icc,
             '-sTextICCProfile=' + text_icc,
             '-sGraphicICCProfile=' + graphic_icc,
             '-o', out_filename, '-f', in_filename]

    # Include ICC profile location to readable path
    run_gpdl(params, '../../toolbin/color/icc_creator/effects/')

# Perform different color conversions on text, graphic, and image content
# through the use of different rendering intents
def object_dependent_rendering_intent():

    in_filename = '%s/examples/text_graph_image_cmyk_rgb.pdf' % ghostpdl_root
    out_filename = 'rendered_intent.tif'
    output_icc_profile = '%s/toolbin/color/src_color/cmyk_des_renderintent.icc' % ghostpdl_root
    print('Object dependent rendering intents on %s to %s' % (in_filename, out_filename))

    params =['gs', '-dNOPAUSE', '-dBATCH', '-sDEVICE=tiff32nc',
             '-r72', '-sOutputICCProfile=' + output_icc_profile,
             '-sImageICCIntent=0', '-sTextICCIntent=1',
             '-sGraphicICCIntent=2', '-o', out_filename,
             '-f', in_filename]

    # Include ICC profile location to readable path
    run_gpdl(params, '../../toolbin/color/src_color/')

# Distill
def distill():

    in_filename = '%s/examples/tiger.eps' % ghostpdl_root
    out_filename = 'tiger.pdf'
    print('Distilling %s to %s' % (in_filename, out_filename))

    params =['gs', '-dNOPAUSE', '-dBATCH', '-sDEVICE=pdfwrite',
             '-o', out_filename, '-f', in_filename]
    run_gpdl(params, in_filename)

# Transparency in Postscript
def trans_ps():

    in_filename = '%s/examples/transparency_example.ps' % ghostpdl_root
    out_filename = 'transparency.png'
    print('Rendering Transparency PS file %s to %s' % (in_filename, out_filename))

    params =['gs', '-dNOPAUSE', '-dBATCH', '-sDEVICE=pngalpha',
             '-dALLOWPSTRANSPARENCY', '-o', out_filename, '-f', in_filename]
    run_gpdl(params, in_filename)

# Run string to feed chunks
def run_string():

    size = 1024;
    in_filename = '%s/examples/tiger.eps' % ghostpdl_root
    out_filename = 'tiger_byte_fed.png'
    params =['gs', '-dNOPAUSE', '-dBATCH', '-sDEVICE=pngalpha',
              '-o', out_filename]

    instance = gsapi.gsapi_new_instance(0)

    gsapi.gsapi_set_arg_encoding(instance, gsapi.GS_ARG_ENCODING_UTF8)
    gsapi.gsapi_init_with_args(instance, params)

    gsapi.gsapi_run_string_begin(instance, 0)

    with open(in_filename,"rb") as f:
        while True:
            data = f.read(size)
            if not data:
                break
            gsapi.gsapi_run_string_continue(instance, data, 0)

    exitcode = gsapi.gsapi_run_string_end(instance, 0)

    end_gpdl(instance)

    return exitcode


# Examples
print('***********Text extraction***********');
extract_text()
print('***********Color conversion***********')
object_dependent_color_conversion()
print('***********Rendering intent***********')
object_dependent_rendering_intent()
print('***********Distillation***************')
distill()
print('***********Postscript with transparency********')
trans_ps()
print('***********Multiple files********')
multiple_files()
print('***********Run string********')
run_string()
wait = input("press enter to exit")
