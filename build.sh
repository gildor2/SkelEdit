#!/bin/bash

project="skeledit.project"
resfile="resource.bin"

# compile scripts
Tools/ucc --type=typeinfo.bin Core Anim Editor || exit

[ "$PLATFORM" ] || PLATFORM="vc-win32"

# force PLATFORM=linux under Linux OS
[ "$OSTYPE" == "linux-gnu" ] || [ "$OSTYPE" == "linux" ] && PLATFORM="linux"
#[ "$PLATFORM" == "linux" ] && PLATFORM="linux64"

# build resources archive
rm -f $resfile
pkzipc -add $resfile -lev=9 -nozip -silent xrc/*.png xrc/*.xrc

# generate makefile
makefile="obj/SkelEdit-$PLATFORM.mak"
if ! [ -d obj ]; then
	mkdir obj
fi
Tools/genmake $project TARGET=$PLATFORM > $makefile

# build
vc32tools --version=2015 --make $makefile
