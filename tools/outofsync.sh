#!/bin/sh

cvs status 2> /dev/null | grep ^File: | grep -v "Up-to-date$"
