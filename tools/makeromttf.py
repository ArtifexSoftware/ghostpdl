#!/usr/bin/env python

# Creates C program data files for each ttf file given on the command
# line.  Also a C header file is generated.  The C files are compiled
# using gcc and archived into a large library using gnu ar, the final
# result can be linked with pcl.  It requires ttf_windows_font_name, a
# C program which fetches a name from each Truetype font and modifies
# it to a name PCL expects.  DEVELOPERS ONLY.

#  This program does not do any error checking, clean up data files or
#  anything that might be construed as user friendly - please use with
#  caution.

# ./makeromttf.py /windows/fonts/*.ttf

if __name__ == '__main__':
    import sys, os, tempfile

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
        # exercise - should do this in python
        wf = os.popen('ttf_windows_file_name ' + file)
        # read the whole damn thing.  If this get too cumbersome we
        # can switch to string i/o which will use a disk
        ttfont = fp.read()
        fp.close()
        font_name = wf.readlines()[0][:-1]
        wf.close()

        c_file = os.path.basename(file) + ".c"
        fp = open(c_file, 'wb')
        # no spaces in C variables.
        variable_name = font_name.replace(' ', '_')
        tmp_str = "const unsigned char " + variable_name + "[] = {\n"
        # 6 dummy bytes
        tmp_str += "%d,%d,%d,%d,%d,%d," % (0, 0, 0, 0, 0, 0)
        fp.write(tmp_str)
        for byte in ttfont:
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
    fp.write("{\"\", 0}};")
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
