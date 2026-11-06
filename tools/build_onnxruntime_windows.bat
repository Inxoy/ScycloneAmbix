@echo off
rem Builds a full static onnxruntime 1.14.1 (all CPU operators, static MSVC runtime /MT) and
rem merges it into modules\onnxruntime-full\lib\onnxruntime-win-x64.lib, which CMakeLists.txt
rem uses instead of the reduced library when it exists. The converted RAVE models need it.
rem
rem Needs: Visual Studio (Build Tools) with C++, git, Python 3.8+ on PATH (or as argument 1).
rem Takes about 30 minutes and ~3 GB in %USERPROFILE%\ortb (a short path: the onnxruntime
rem sources are deep and hit the 260 character limit elsewhere).
rem
rem     tools\build_onnxruntime_windows.bat [python.exe]

setlocal
set "REPO=%~dp0.."
set "WORK=%USERPROFILE%\ortb"
set "PYTHON=%~1"
if "%PYTHON%"=="" set "PYTHON=python"

for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath`) do set "VS=%%i"
if "%VS%"=="" (echo Visual Studio not found & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
rem Visual Studio's own CMake and Ninja: other CMake builds (e.g. Strawberry Perl's) fail the
rem TLS downloads or pick up gcc
set "VSCMAKE=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake"
set "PATH=%VSCMAKE%\CMake\bin;%VSCMAKE%\Ninja;%PATH%"

if not exist "%WORK%\onnxruntime" (
    git -c core.longpaths=true clone --depth 1 --branch v1.14.1 --recurse-submodules --shallow-submodules ^
        https://github.com/microsoft/onnxruntime.git "%WORK%\onnxruntime" || exit /b 1
)

rem newer MSVC versions no longer include <chrono> through <mutex>
findstr /c:"#include <chrono>" "%WORK%\onnxruntime\include\onnxruntime\core\platform\ort_mutex.h" >nul || ^
    powershell -NoProfile -Command "$f='%WORK%\onnxruntime\include\onnxruntime\core\platform\ort_mutex.h'; $t=[IO.File]::ReadAllText($f).Replace('#pragma once', \"#pragma once`n#include <chrono>\"); [IO.File]::WriteAllText($f, $t)"

pushd "%WORK%\onnxruntime"
"%PYTHON%" tools\ci_build\build.py --build_dir "%WORK%\build" --config Release --parallel --skip_tests ^
    --skip_submodule_sync --cmake_generator Ninja --enable_msvc_static_runtime --compile_no_warning_as_error ^
    --cmake_extra_defines onnxruntime_BUILD_UNIT_TESTS=OFF CMAKE_POLICY_VERSION_MINIMUM=3.5 ^
    CMAKE_C_COMPILER=cl CMAKE_CXX_COMPILER=cl || (popd & exit /b 1)
popd
rem re2 (used by the tokenizer operator) is only built for the final link otherwise
cmake --build "%WORK%\build\Release" --target re2 || exit /b 1

rem one library, as the reduced build: onnxruntime + abseil, flatbuffers, onnx, protobuf-lite
set "B=%WORK%\build\Release"
set "RSP=%WORK%\merge.rsp"
if exist "%RSP%" del "%RSP%"
for %%f in ("%B%\onnxruntime_*.lib") do if /i not "%%~nxf"=="onnxruntime_providers_shared.lib" echo "%%f">>"%RSP%"
for /r "%B%\_deps\abseil_cpp-build" %%f in (*.lib) do echo "%%f">>"%RSP%"
echo "%B%\_deps\flatbuffers-build\flatbuffers.lib">>"%RSP%"
echo "%B%\_deps\onnx-build\onnx.lib">>"%RSP%"
echo "%B%\_deps\onnx-build\onnx_proto.lib">>"%RSP%"
echo "%B%\_deps\protobuf-build\libprotobuf-lite.lib">>"%RSP%"
echo "%B%\_deps\re2-build\re2.lib">>"%RSP%"

if not exist "%REPO%\modules\onnxruntime-full\lib" mkdir "%REPO%\modules\onnxruntime-full\lib"
lib.exe /NOLOGO /OUT:"%REPO%\modules\onnxruntime-full\lib\onnxruntime-win-x64.lib" @"%RSP%" || exit /b 1
echo Wrote modules\onnxruntime-full\lib\onnxruntime-win-x64.lib, reconfigure CMake to use it.
