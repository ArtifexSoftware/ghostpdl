#!/bin/bash

echo "Build..."
javac -classpath "../gsjava/bin;." "Main.java" "Worker.java"
echo "Done."