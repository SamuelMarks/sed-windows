@echo off
setlocal enabledelayedexpansion

REM ============================================================================
REM release_dos.bat - Build & Package GNU sed 4.10 32-bit MS-DOS (OpenWatcom v2)
REM ============================================================================

if "%WATCOM%"=="" (
    if exist "C:\WATCOM\binnt64\wcl386.exe" (
        set "WATCOM=C:\WATCOM"
    ) else if exist "C:\WATCOM\binnt\wcl386.exe" (
        set "WATCOM=C:\WATCOM"
    ) else (
        echo ERROR: WATCOM environment variable is not set and C:\WATCOM not found.
        exit /b 1
    )
)

set "PATH=%WATCOM%\binnt64;%WATCOM%\binnt;%WATCOM%\binw;%PATH%"
set "INCLUDE=%WATCOM%\h"
set "EDPATH=%WATCOM%\eddat"
set "WIPFC=%WATCOM%\wipfc"
set "BUILD_DIR=build_dos"

echo [sed-windows] Configuring GNU sed 4.10 for 32-bit MS-DOS (OpenWatcom)...
cmake -S . -B "%BUILD_DIR%" -G "Watcom WMake" -DCMAKE_SYSTEM_NAME=DOS -DCMAKE_SYSTEM_PROCESSOR=I386 -DCMAKE_C_COMPILER=wcl386 -DCMAKE_C_FLAGS_INIT="-bt=dos -mf -3r -zq" -DSED_TARGET_DOS=ON -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
if errorlevel 1 exit /b 1

echo [sed-windows] Building 32-bit Protected-Mode DOS SED.EXE & C89 tests...
cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 exit /b 1

if exist "%WATCOM%\binw\dos32a.exe" (
    copy /y "%WATCOM%\binw\dos32a.exe" "%BUILD_DIR%\dos32a.exe" >nul
)
if exist "%WATCOM%\binw\dos4gw.exe" (
    copy /y "%WATCOM%\binw\dos4gw.exe" "%BUILD_DIR%\dos4gw.exe" >nul
)

echo [sed-windows] Packaging DOS ZIP archive...
pushd "%BUILD_DIR%"
cpack -G ZIP -C Release
if errorlevel 1 (
    popd
    exit /b 1
)
popd

echo [sed-windows] DOS release build & packaging succeeded in %BUILD_DIR%.
endlocal
