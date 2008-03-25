#!/bin/bash

project="Test.project"
makefile="makefile.mak"
resfile="resource.bin"

# compile scripts
Tools/ucc --type=typeinfo.bin Core Anim Editor || exit

# build resources archive
rm -rf $resfile > /dev/null
pkzipc -add $resfile -lev=9 -nozip -silent xrc/*.png xrc/*.xrc

# update makefile when needed
Tools/genmake $project TARGET=vc-win32 > $makefile

# build
vc32tools --version=8 --make $makefile
