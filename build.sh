#!/bin/bash

project="Test.project"
makefile="makefile.mak"

# update makefile when needed
# [ $makefile -ot $project ] &&
	Tools/genmake $project TARGET=vc-win32 > $makefile

# build
vc32tools --version=8 --make $makefile
