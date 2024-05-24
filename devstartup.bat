@echo off
cd build
call "shell.bat"
cd ..\misc
call "emacs.bat"
cd ..
cd build
cmd /k