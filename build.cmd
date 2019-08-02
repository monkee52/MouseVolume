@ECHO OFF

IF EXIST "out" RMDIR /S /Q "out"

MKDIR "out"
MKDIR "out\bin"

TASKKILL /F /IM "MouseVolume.exe"

cl.exe /O2 /c "MouseVolume.c" /Fo"out\MouseVolume.obj"
rc.exe /fo"out\MouseVolume.res" "MouseVolume.rc"
link.exe /SUBSYSTEM:WINDOWS /RELEASE /OUT:"out\bin\MouseVolume.exe" /DEBUG /PDB:"out\MouseVolume.pdb" "out\MouseVolume.obj" "out\MouseVolume.res" /NODEFAULTLIB "user32.lib" "kernel32.lib"
