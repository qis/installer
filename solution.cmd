@echo off
set __SOURCE_PATH=%~dp0
set __SOURCE_PATH=%__SOURCE_PATH:~0,-1%

cmake -G "Visual Studio 16 2019" -A x64 -B "%__SOURCE_PATH%\build" -DCMAKE_CONFIGURATION_TYPES="Debug;Release" ^
  -DCMAKE_TOOLCHAIN_FILE:PATH="%__SOURCE_PATH%\res\toolchain.cmake" ^
  -DCMAKE_INSTALL_PREFIX:PATH="%__SOURCE_PATH%" "%__SOURCE_PATH%"
if %errorlevel% == 0 (
  cmake --open "%__SOURCE_PATH%\build"
) else (
  pause
)

set __SOURCE_PATH=
