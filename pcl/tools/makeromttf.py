#!/usr/bin/env python

# Creates C program data files for each ttf file given on the command
# line.  Also a C header file is generated.  The C files are compiled
# using gcc and archived into a large library using gnu ar, the final
# result can be linked with pcl.  DEVELOPERS ONLY.

#  This program does not do any error checking, clean up data files or
#  anything that might be construed as user friendly - please use with
#  caution.

# ./makeromttf.py /windows/fonts/*.ttf

FILTER_NONE=0
FILTER_ZLIB=1
FILTER_GZIP=2

import tempfile, os, zlib

def font_filter(ttfont, filter):
    if (filter == FILTER_NONE):
        return ttfont
    if (filter == FILTER_ZLIB):
        return zlib.compress(ttfont)
    if (filter == FILTER_GZIP):
        outfile=tempfile.mktemp()
        infile=tempfile.mktemp()
        open(infile, "wb").write(ttfont)
        command="gzip -c < %s > %s" % (infile, outfile)
        os.system(command)
        filtered_font=open(outfile, "rb").read()
        os.remove(outfile)
        os.remove(infile)
        return filtered_font
    return None
    
import struct

def find_table(ttfont, table_name):
    # index error not handled.
    num_tables = struct.unpack(">H", ttfont[4:6])[0]
    for table in range(num_tables):
        this_table_offset= 12 + (table * 16)
        if (ttfont[this_table_offset:this_table_offset+len(table_name)] == table_name):
            table_length = struct.unpack(">L", ttfont[this_table_offset+12:this_table_offset+16])[0]
            table_offset = struct.unpack(">L", ttfont[this_table_offset+8:this_table_offset+12])[0]
            return ttfont[table_offset:table_offset+table_length]

    return None

def get_name(ttfont):
    name_table = find_table(ttfont, "name")
    if (name_table):
        storageOffset = struct.unpack(">H", name_table[4:6])[0]
        name_recs = name_table[6:]
        windows_name_len = struct.unpack(">H", name_recs[12*4+8:12*4+10])[0];
        windows_name_offset = struct.unpack(">H", name_recs[12*4+10:12*4+12])[0];
        # at last
        windows_name = name_table[storageOffset+windows_name_offset:storageOffset+windows_name_offset+windows_name_len]
        return_string = ""
        for ch in windows_name:
            if (ord(ch) in range(32, 127)):
                return_string += ch
                
        return return_string
    return None
            

if __name__ == '__main__':
    import sys

    if not sys.argv[1:]:
        print "Usage: %s pxl files" % sys.argv[0]

    files = sys.argv[1:]
    font_table_dict = {}
    font_cfile_name = {}
    for file in files:
        try:
            fp = open(file, 'rb')
        except:
            print "Cannot find file %s" % file
            continue
        # read the whole damn thing.  If this get too cumbersome we
        # can switch to string i/o which will use a disk
        ttfont = fp.read()
        fp.close()
        font_name = get_name(ttfont)

        c_file = os.path.basename(file) + ".c"
        fp = open(c_file, 'wb')
        # no spaces in C variables.
        variable_name = font_name.replace(' ', '_')
        tmp_str = "const unsigned char " + variable_name + "[] = {\n"
        fp.write(tmp_str)
        # 6 dummy bytes
        tmp_str = "%c%c%c%c%c%c" % (0, 0, 0, 0, 0, 0)
        ttfont = tmp_str + ttfont
        for byte in font_filter(ttfont, FILTER_ZLIB):
            array_entry = "%d," % ord(byte)
            fp.write(array_entry)
        tmp_str = '};\n'
        fp.write(tmp_str)
        font_table_dict[variable_name] = font_name
        font_cfile_name[variable_name] = c_file
        fp.close()

    # Generate a header file with externs for each font
    print "put romfnttab.h and libttffont.a in the pl directory"
    fp = open("romfnttab.h", 'wb')
    # write out the externs
    for k in font_table_dict.keys():
        tmp_str = "extern const unsigned char " + k + ";\n"
        fp.write(tmp_str)

    tmp_str = """typedef struct pcl_font_variable_name {
                 const char font_name[40];
                 const unsigned char *data;
             } pcl_font_variable_name_t;
    
             const pcl_font_variable_name_t pcl_font_variable_name_table[] = {"""
    fp.write(tmp_str)
    # build the table
    for k in font_table_dict.keys():
        fp.write("{\n")
        # font name
        tmp_str = "\"" + font_table_dict[k] + "\","
        fp.write(tmp_str)
        tmp_str = "&" + k + "},"
        fp.write(tmp_str)
        
    # table terminator
    fp.write("{\"\", 0}};\n")
    fp.close()

    # compile the code
    for k in font_cfile_name.keys():
        os.system("gcc -c " + font_cfile_name[k]);

    # archive the objects
    object_names = ""
    for k in font_cfile_name.keys():
        object_names = object_names + font_cfile_name[k][:-1] + 'o' + " "
    os.system("rm libttffont.a")
    os.system("ar rcv libttffont.a " + object_names)
