#!/bin/bash

project="skeledit.project"
makefile="makefile.mak"
resfile="resource.bin"

# compile scripts
Tools/ucc --type=typeinfo.bin Core Anim Editor || exit

# build resources archive
rm -f $resfile
pkzipc -add $resfile -lev=9 -nozip -silent xrc/*.png xrc/*.xrc

# update makefile
Tools/genmake $project TARGET=vc-win32 > $makefile

# build
vc32tools --version=2015 --make $makefile
