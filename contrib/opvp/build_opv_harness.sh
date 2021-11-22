#!/bin/bash

gcc -Werror -Wall -c -fpic -g opvpharness.c
gcc -shared -o libopv.so opvpharness.o