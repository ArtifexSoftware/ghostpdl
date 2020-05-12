#!/usr/bin/python

import os, string

# not used may use later
def get_line_directives_table(file):
    # not intended to be portable or useful outside of gs universe.
    CPP_FLAGS="-I../gs/base/ -I../gs/psi -I../pl/ -I../pcl -I../pxl -I../main/obj -DDEBUG"
    CPP="gcc -E"
    CPP_COMMAND=CPP + " " + CPP_FLAGS + " " + file
    CPP_IN=os.popen(CPP_COMMAND, 'r')
    lines = []
    while (1):
        line = CPP_IN.readline()
        if not line:
            break;
        # chop newline
        line = line[:-1]

        # check again for line since we might have chopped the only
        # character.  check for lines starting with # and ending with
        # 1 (see cpp docs).
        
        if line and line[0] == "#" and line[-1] == "1":
            # second field 1:-1 leaves out enclosing quotes.
            lines.append(os.path.basename(string.splitfields(line)[2][1:-1]))
    for line in lines:
        print line

def get_includes(file, skip_system_files=1):
    includes = []
    for line in fileinput.input(file):
        if line[0:len("#include")] == "#include":
            myin = string.splitfields(line)[1]
            if skip_system_files and myin[0] == '<' and myin[-1] == '>':
                print "skipping system header " + myin
                continue
            else:
                includes.append(myin[1:-1])
    return includes
            
def find_all_files(dir, pattern):
    lines=[]
    FIND_COMMAND = "find " + dir + " -name " + pattern
    for line in os.popen(FIND_COMMAND).readlines():
        lines.append(line[:-1])
    return lines
    

import fileinput


def find_target(makefile, target):
    parsed_target = []
    parsing_target = 0
    fp = open(makefile)
    for line in fp.readlines():
        if line.find(target) >= 0 and not parsing_target:
            parsing_target = 1
            parsed_target.append(line)
            continue
        if parsing_target:
            if line[0] not in string.whitespace:
                break
            parsed_target.append(line)
    fp.close()
    return makefile, parsed_target
        
# return the filename and the targets from all the makefiles that
# contain the target.
def find_targets(target, makefiles):
    # target is a c file - want .$(OBJ) file
    target = target[:-1] + "$(OBJ):"
    target = os.path.basename(target)
    parsed_targets = []
    # list of lines
    for line in fileinput.input(makefiles):
        if line.find(')' + target) >= 0:
            parsed_targets.append(find_target(fileinput.filename(), target))
    fileinput.close()
    return parsed_targets

import re

def get_deps_from_target(target):
    # find .h files sans definition.
    reg=re.compile('\.h')
    for line in target:
        for h in reg.findall(line):
            print "naked .h found in " + target[0][0:target[0].index(":")]
    # not quite right.
    reg=re.compile('[A-Za-z0-9]+_+h')
    deps=[]
    for line in target:
        targ_list=reg.findall(line)
        for targ in targ_list:
            deps.append(targ)
    return deps

def compare_lists(c_file, makefile, from_makefile, from_source):
    canon_from_source = []
    for x in from_source:
        canon_from_source.append(x.replace('.', '_'))

#    print canon_from_source
#    print from_makefile

    for mline in from_makefile:
        if mline not in canon_from_source:
            print mline + " from " + makefile + " not in " + c_file
    for sline in canon_from_source:
        if sline not in from_makefile:
            print sline + " from " + c_file + " not in " + makefile
    
if __name__ == '__main__':
#    headers=get_line_directives_table("../pcl/pcjob.c")
#    print headers
     C_FILES=find_all_files("../", "\\*.c")
     MAKEFILES=find_all_files("../", "\\*.mak")
     for c_file in C_FILES:
         target_list = find_targets(c_file, MAKEFILES)
         for (f, t) in target_list:
#             print "found target for " + c_file + " in " + f
#             print "target is "
#             print t
             compare_lists(c_file, f, get_deps_from_target(t), get_includes(c_file))
         else:
             "no target for " + c_file
#    C_FILES=find_all_files("../", "*.c")
#    MAKE_FILES=find_all_files("../", "*.mak")
#    H_FILES=find_all_files("../", "*.h")


