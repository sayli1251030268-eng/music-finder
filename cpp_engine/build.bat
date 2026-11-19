@echo off
echo Compiling TuneFinder C++ Data Structures Engine...
g++ -O2 -std=c++14 -Wall "%~dp0main.cpp" -o "%~dp0ds_engine.exe"
if %ERRORLEVEL% NEQ 0 (
    echo Compilation FAILED! Make sure MinGW g++ is installed and in PATH.
    exit /b 1
)
echo Successfully compiled: %~dp0ds_engine.exe
