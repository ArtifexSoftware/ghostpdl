#!/bin/bash

export LD_PRELOAD=./libgpdl.dylib

java -cp "gsjava.jar:." Main "$ARG1"
