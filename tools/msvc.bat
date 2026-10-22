@echo off
rem Runs the given command line inside an MSVC x64 developer environment.
rem Usage (PowerShell):  cmd /c "`"<repo>\tools\msvc.bat`" cl /nologo ..."
set "PATH=%PATH%;C:\Program Files (x86)\Microsoft Visual Studio\Installer"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
%*
