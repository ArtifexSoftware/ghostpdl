# -*- coding: utf-8 -*-
"""
Created on Tue Jul 21 10:05:07 2020
Example use of gsapi for various tasks.

@author: Michael Vrhel
"""

try:
    import gsapi
except Exception as err:
    print('Failure to import gsapi. Check shared library path')
    print(err.args)

def run_gpdl(params, path):

    try:
        e, instance = gsapi.gsapi_new_instance(0)
        if e:
            raise Warning('gsapi_new_instance failure')
    except Exception as err:
        print(err.args)
        return

    try:
        e = gsapi.gsapi_set_arg_encoding(instance, gsapi.GS_ARG_ENCODING_UTF8)
        if e:
            raise Warning('gsapi_set_arg_encoding failure')

        if path != None:
            e = gsapi.gsapi_add_control_path(instance, gsapi.GS_PERMIT_FILE_READING, path)
            if e:
                raise Warning('gsapi_add_control_path failure')

        e = gsapi.gsapi_init_with_args(instance, params)
        if e:
            raise Warning('gsapi_init_with_args failure')

    except Exception as err:
        print(err.args)
        return

    end_gpdl(instance)
    return

def init_gpdl(params):

    try:
        e, instance = gsapi.gsapi_new_instance(0)
        if e:
            raise Warning('gsapi_new_instance failure')
    except Exception as err:
        print(err.args)
        return None

    try:
        e = gsapi.gsapi_set_arg_encoding(instance, gsapi.GS_ARG_ENCODING_UTF8)
        if e:
            raise Warning('gsapi_set_arg_encoding failure')

        e = gsapi.gsapi_init_with_args(instance, params)
        if e:
            raise Warning('gsapi_init_with_args failure')

    except Exception as err:
        print(err.args)
        return None

    return instance

def run_file(instance, filename):

    try:
        (e, exitcode) = gsapi.gsapi_run_file(instance, filename, None)
        if e:
            raise Warning('gsapi_run_file failure')

    except Exception as err:
        print(err.args)

    return

def end_gpdl(instance):

    try:
        e = gsapi.gsapi_exit(instance)
        if e:
            raise Warning('gsapi_exit failure')
    
        e = gsapi.gsapi_delete_instance(instance)
        if e:
            raise Warning('gsapi_delete_instance failure')

    except Exception as err:
        print(err.args)

    return

# run multiple files through same instance
def multiple_files():

    out_filename = 'multi_file_output_%d.png'

    params =['gs', '-dNOPAUSE', '-dBATCH', '-sDEVICE=pngalpha',
             '-r72', '-o', out_filename]
    instance = init_gpdl(params)
    run_file(instance, '../../examples/tiger.eps')
    run_file(instance, '../../examples/snowflak.ps')
    run_file(instance, '../../examples/annots.pdf')

    end_gpdl(instance)

# Extract text from source file
def extract_text():

    in_filename = '../../examples/alphabet.ps'
    out_filename = 'alphabet.txt'
    print('Extracting text from %s to %s' % (in_filename, out_filename))

    params =['gs', '-dNOPAUSE', '-dBATCH','-sDEVICE=txtwrite',
             '-dTextFormat=3','-o', out_filename, '-f', in_filename]
    run_gpdl(params, None)

    return

# Perform different color conversions on text, graphic, and image content
# through the use of different destination ICC profiles
def object_dependent_color_conversion():

    in_filename = '../../examples/text_graph_image_cmyk_rgb.pdf'
    out_filename = 'rendered_profile.tif'
    image_icc = '../../toolbin/color/icc_creator/effects/cyan_output.icc'
    graphic_icc = '../../toolbin/color/icc_creator/effects/magenta_output.icc'
    text_icc = '../../toolbin/color/icc_creator/effects/yellow_output.icc'
    print('Object dependent color conversion on %s to %s' % (in_filename, out_filename))

    params =['gs', '-dNOPAUSE', '-dBATCH', '-sDEVICE=tiff32nc',
             '-r72','-sImageICCProfile=' + image_icc,
             '-sTextICCProfile=' + text_icc,
             '-sGraphicICCProfile=' + graphic_icc,
             '-o', out_filename, '-f', in_filename]

    # Include ICC profile location to readable path
    run_gpdl(params, '../../toolbin/color/icc_creator/effects/')

    return

# Perform different color conversions on text, graphic, and image content
# through the use of different rendering intents
def object_dependent_rendering_intent():

    in_filename = '../../examples/text_graph_image_cmyk_rgb.pdf'
    out_filename = 'rendered_intent.tif'
    output_icc_profile = '../../toolbin/color/src_color/cmyk_des_renderintent.icc'
    print('Object dependent rendering intents on %s to %s' % (in_filename, out_filename))

    params =['gs', '-dNOPAUSE', '-dBATCH', '-sDEVICE=tiff32nc',
             '-r72', '-sOutputICCProfile=' + output_icc_profile,
             '-sImageICCIntent=0', '-sTextICCIntent=1',
             '-sGraphicICCIntent=2', '-o', out_filename,
             '-f', in_filename]

    # Include ICC profile location to readable path
    run_gpdl(params, '../../toolbin/color/src_color/')

    return

# Distill
def distill():

    in_filename = '../../examples/tiger.eps'
    out_filename = 'tiger.pdf'
    print('Distilling %s to %s' % (in_filename, out_filename))

    params =['gs', '-dNOPAUSE', '-dBATCH', '-sDEVICE=pdfwrite',
             '-o', out_filename, '-f', in_filename]
    run_gpdl(params, None)

    return

# Transparency in Postscript
def trans_ps():

    in_filename = '../../examples/transparency_example.ps'
    out_filename = 'transparency.png'
    print('Rendering Transparency PS file %s to %s' % (in_filename, out_filename))

    params =['gs', '-dNOPAUSE', '-dBATCH', '-sDEVICE=pngalpha',
             '-dALLOWPSTRANSPARENCY', '-o', out_filename, '-f', in_filename]
    run_gpdl(params, None)

    return

# Run string to feed chunks
def run_string():

    f = None
    size = 1024;
    in_filename = '../../examples/tiger.eps'
    out_filename = 'tiger_byte_fed.png'
    params =['gs', '-dNOPAUSE', '-dBATCH', '-sDEVICE=pngalpha',
              '-o', out_filename]

    try:
        e, instance = gsapi.gsapi_new_instance(0)
        if e:
            raise Warning('gsapi_new_instance failure')
    except Exception as err:
        print(err.args)
        return

    try:
        e = gsapi.gsapi_set_arg_encoding(instance, gsapi.GS_ARG_ENCODING_UTF8)
        if e:
            raise Warning('gsapi_set_arg_encoding failure')
    
        e = gsapi.gsapi_init_with_args(instance, params)
        if e:
            raise Warning('gsapi_init_with_args failure')

        [e, exitcode] = gsapi.gsapi_run_string_begin(instance, 0)
        if e:
            raise Warning('gsapi_run_string_begin failure')

        f = open(in_filename,"rb")
        while True:
            data = f.read(size)
            if not data:
                break
            [e, exitcode] = gsapi.gsapi_run_string_continue(instance, data, 0)
            if e:
                raise Warning('gsapi_run_string_continue failure')

        [e, exitcode] = gsapi.gsapi_run_string_end(instance, 0)
        if e:
            raise Warning('gsapi_run_string_end failure')

    except Exception as err:
        print(err.args)

    end_gpdl(instance)
    if f != None:
        f.close()
    return


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