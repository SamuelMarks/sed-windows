@echo off
setlocal enabledelayedexpansion

REM ============================================================================
REM release.bat - Build & Package GNU sed 4.10 Native Windows (MSVC) Release
REM ============================================================================

set "BUILD_DIR=build_msvc"
set "ARCH=%~1"
if "%ARCH%"=="" set "ARCH=x64"

echo [sed-windows] Configuring GNU sed 4.10 for Windows (%ARCH%)...
cmake -S . -B "%BUILD_DIR%" -A "%ARCH%" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
if errorlevel 1 exit /b 1

echo [sed-windows] Building Release binaries and strict-C89 test suites...
cmake --build "%BUILD_DIR%" --config Release --parallel
if errorlevel 1 exit /b 1

echo [sed-windows] Running CTest verification suite...
ctest --test-dir "%BUILD_DIR%" -C Release --output-on-failure
if errorlevel 1 exit /b 1

echo [sed-windows] Packaging release archives...
pushd "%BUILD_DIR%"
cpack -C Release
if errorlevel 1 (
    popd
    exit /b 1
)
popd

echo [sed-windows] Release build & packaging succeeded in %BUILD_DIR%.
endlocal
