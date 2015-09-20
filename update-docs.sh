#!/bin/sh
inkscape -z  doc/intro-scheme.svg -e doc/intro-scheme.png -d 65
doxygen doc/Doxyfile
