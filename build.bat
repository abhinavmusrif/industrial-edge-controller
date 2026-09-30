@echo off
setlocal
echo =======================================================
echo  Building Industrial Edge Controller (Windows Native)
echo =======================================================

cd /d "%~dp0"
if not exist "build" mkdir build
if not exist "logs" mkdir logs

findstr /C:"/mnt/" build\CMakeCache.txt >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [INFO] Cleaning Linux CMake cache for Windows build...
    rmdir /S /Q build
    mkdir build
)

if exist "C:\Users\hoids\w64devkit\bin\g++.exe" (
    set "PATH=C:\Users\hoids\w64devkit\bin;%PATH%"
    cmake -B build -S . -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
) else (
    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
)

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed!
    exit /b %ERRORLEVEL%
)

echo.
echo [INFO] Running test suite...
if exist "build\bin\run_tests.exe" (
    build\bin\run_tests.exe
) else if exist "build\run_tests.exe" (
    build\run_tests.exe
) else if exist "build\Release\run_tests.exe" (
    build\Release\run_tests.exe
)

echo.
echo [SUCCESS] Windows build completed successfully!
