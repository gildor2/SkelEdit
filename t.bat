@echo off
set exe="..\..\Bin_ReleaseEd_Win32\SkelEditor.exe"
rm -f %exe%
bash build_vc.sh
if exist %exe% %exe%

rm -rf Release Debug
