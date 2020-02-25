#!/usr/bin/python3

'''
Reads memento squeeze output from stdin, and writes brief info to stdout or
file.

E.g.:
    MEMENTO_SQUEEZEAT=1 ./membin/gpdl -sDEVICE=bit -o /dev/null examples/tiger.eps 2>&1 | toolbin/squeeze2text.py -o squeeze.txt

Args:

    -a <filename>
        Append output to <filename>. Write log to stdout.
    -o <filename>
        Write output to <filename>. Write log to stdout.
    -p <N>
        Write progress information to log, every <N> blocks.

If -o is not specified, we write output to stdout, and log to stderr.

If -p is not specified, default is -p 1.

'''

import io
import re
import sys

if __name__ == '__main__':

    verbose = False
    progress_n = 1
    num_segv = 0
    num_leak = 0
    out = sys.stdout
    log = sys.stderr

    args = iter( sys.argv[1:])
    while 1:
        try:
            arg = next(args)
        except StopIteration:
            break
        if arg == '-p':
            progress_n = int( next(args))
        elif arg == '-a':
            filename = next(args)
            out = open( filename, 'a')
            log = sys.stdout
        elif arg == '-o':
            filename = next(args)
            out = open( filename, 'w')
            log = sys.stdout
        else:
            raise Exception( 'unrecognised arg: %r' % arg)

    # Use latin_1 encoding to ensure we can read all 8-bit bytes from stdin as
    # characters.
    f = io.TextIOWrapper( sys.stdin.buffer, encoding='latin_1')
    progress_n_next = 0

    def print_progress():
        print( 'memento_n=%s. num_segv=%s num_leak=%s' % (
                    memento_n,
                    num_segv,
                    num_leak
                    ),
                file=log,
                )

    for line in f:
        m = re.match( '^Memory squeezing @ ([0-9]+)$', line)
        if m:
            memento_n = int( m.group(1))
            if memento_n >= progress_n_next:
                print_progress()
                progress_n_next = int(memento_n / progress_n + 1) * progress_n
                log.flush() # Otherwise buffered and we see no output for ages.
        elif line.startswith( 'SEGV at:'):
            num_segv += 1
            print( 'memento_n=%s: segv' % memento_n, file=out)
        elif line.startswith( 'Allocated blocks'):
            num_leak += 1
            print( 'memento_n=%s: leak' % memento_n, file=out)
    print_progress()
