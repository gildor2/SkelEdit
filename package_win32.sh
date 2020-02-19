#!/bin/bash
archive="SkelEdit.rar"
filelist="SkelEdit.exe SkelEdit.exe.manifest readme.txt wx*.dll resource.bin typeinfo.bin"

for i in $filelist; do
	if [ ! -f $i ]; then
		echo "ERROR: unable to find \"$i\""
		exit 1
	fi
done

#if grep -q -E "(XMemDecompress|Huxley)" umodel.exe; then
#	echo "ERROR: this is a private build"
#	exit
#fi

rm -f $archive
rar a $archive $filelist
