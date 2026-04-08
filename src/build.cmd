@echo off
setlocal

set "DEVICE_FLAG="

if /i "%~1"=="--device" set "DEVICE_FLAG=/DCOMPILE_FOR_DEVICE"
if /i "%~1"=="device" set "DEVICE_FLAG=/DCOMPILE_FOR_DEVICE"

if not "%DEVICE_FLAG%"=="" (
    echo Building WITH device support...
) else (
    echo Building WITHOUT device support ^(run with '--device' to enable^)...
)

rem MSVC Equivalent Flags:
rem /nologo     : Suppress startup banner
rem /std:c++17  : C++17 standard
rem /W4         : High warning level (close to -Wall -Wextra)
rem /WX         : Treat warnings as errors (-Werror)
rem /EHsc       : Standard C++ exception handling
rem /sdl        : Additional security features (-fstack-protector-strong)
rem /O2         : Optimize for maximum speed
rem /Zi         : Generate diagnostic/debug info (useful with ASAN)
rem /fsanitize=address : Address sanitizer

cl.exe /nologo /std:c++17 /W4 /WX /EHsc /sdl /O2 /Zi /fsanitize=address %DEVICE_FLAG% main.cpp /Femain.exe
