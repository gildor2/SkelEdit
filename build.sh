#!/bin/bash

project="skeledit.project"
resfile="resource.bin"

# compile scripts
Tools/ucc --type=typeinfo.bin Core Anim Editor || exit

[ "$PLATFORM" ] || PLATFORM="vc-win32"

# force PLATFORM=linux under Linux OS
[ "$OSTYPE" == "linux-gnu" ] || [ "$OSTYPE" == "linux" ] && PLATFORM="linux"
#[ "$PLATFORM" == "linux" ] && PLATFORM="linux64"

if [ "${PLATFORM:0:3}" == "vc-" ]; then
	# Visual C++ compiler
	# setup default compiler version
	[ "$vc_ver" ] || vc_ver=latest
	# Find Visual Studio
	. vc32tools $VC32TOOLS_OPTIONS --version=$vc_ver --check
	[ -z "$found_vc_year" ] && exit 1				# nothing found
	# Adjust vc_ver to found one
	vc_ver=$found_vc_year
#	echo "Found: $found_vc_year $workpath [$vc_ver]"
	GENMAKE_OPTIONS=VC_VER=$vc_ver					# specify compiler for genmake script
fi

# build resources archive
rm -f $resfile
pkzipc -add $resfile -lev=9 -nozip -silent xrc/*.png xrc/*.xrc

# generate makefile
makefile="obj/SkelEdit-$PLATFORM.mak"
if ! [ -d obj ]; then
	mkdir obj
fi
Tools/genmake $project TARGET=$PLATFORM $GENMAKE_OPTIONS > $makefile

# build
# if $target is empty, whole project will be built, otherwise a single file
case "$PLATFORM" in
	"vc-win32")
		Make $makefile $target || exit 1
		;;
	"vc-win64")
		Make $makefile $target || exit 1
		;;
	linux*)
		make -j 4 -f $makefile $target || exit 1	# use 4 jobs for build
		;;
	*)
		echo "Unknown PLATFORM=\"$PLATFORM\""
		exit 1
esac
