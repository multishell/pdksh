@echo off
if "%1" == "/h" goto help
if "%1" == "/?" goto help
if "%1" == "-h" goto help
set verbose=no
set debug=no
:parse
if "%1" == "/v" goto verbose
if "%1" == "-v" goto verbose
if "%1" == "/d" goto gdebug
if "%1" == "-d" goto gdebug
if exist os2 goto start
echo You are in the wrong directory.  Change to source directory.
echo Copy this file and run again or type os2\configure.
goto end
:verbose
set verbose=yes
shift
goto parse
:gdebug
set debug=yes
shift
goto parse
:start
if exist conftest.c erase conftest.c
if exist confdefs.h erase confdefs.h
if %verbose% == yes echo Searching for sed
for %%i in (%path%) do if exist %%i\sed.exe goto s_success
echo No sed in search path.  Copying Makefile for gcc. You will need
echo to edit it if you are not using gcc.
copy os2\Makefile Makefile
goto copystuff
:help
echo Run configure to set up for os/2 compilation
echo usage: configure [[/v|-v][/d|-d]|/h|-h|/?]
echo        where /v means verbose output
echo		  /d means compile with symbols (debug)
goto end
:s_success
if %verbose% == yes echo checking for compiler
for %%i in (%path%) do if exist %%i\gcc.exe goto g_success
rem for the future we'll use sed processing
for %%i in (%path%) do if exist %%i\bcc.exe goto b_success
for %%i in (%path%) do if exist %%i\icc.exe goto i_success
echo compiler not found. Check your path
goto end
:b_success
echo Borland C compiler found.  Configuration not complete for
echo this compiler.  You may need to edit the Makefile
set CC=bcc
set CPP=cpp
goto createstuff
:i_success
echo IBM C compiler found.  Configuration not complete for
echo this compiler.  You may need to edit the Makefile.
set CC=icc -q -Sm -Gm -Gt -Spl -D__STDC__
set CPP=cpp
goto createstuff
:g_success
echo GNU C compiler found.  This is the standard configuration.
copy os2\Makefile Makefile
if %debug% == no goto copystuff
set CC=gcc -g
set CPP=cpp
:createstuff
echo Creating files for you.
echo s/@CC@/%CC%/> os2\make.tmp
echo s/@CPP@/%CPP%/>> os2\make.tmp
copy os2\make.tmp+os2\make.sed os2\make.tmp
sed -f os2\make.tmp Makefile.in >  Makefile
del os2\make.tmp
:copystuff
if %verbose% == yes echo Copying config.h and config.status files. 
copy os2\config.h config.h
copy os2\config.status config.status
if not exist os2.c copy os2\os2.c os2.c
:end


