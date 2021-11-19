#!/bin/bash

echo "Build mtdemo"

cd ../gsjava

bash build_linux.sh

cd ../mtdemo

cp ../gsjava/gsjava.jar gsjava.jar

echo "Compiling Java source..."
javac -classpath "../gsjava/bin:." "Main.java" "Worker.java"
echo "Done."
