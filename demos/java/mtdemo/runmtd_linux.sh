#!/bin/bash

export LD_PRELOAD=/usr/lib/libgpdl.so.9

java -cp "gsjava.jar;." Main "$ARG1"