@echo off
rem =============================================================================
rem QCefView v1.2.1 + CEF 109??? Windows 7???????
rem
rem ???????????  tools\build-qcefview-deps.bat
rem
rem ?????
rem   - ??? Visual Studio ?????? MSVC v142 ????? Windows SDK?
rem   - ??? Qt 5.14.2 msvc2017_64?CMake >= 3.21?Ninja?git
rem   - ??? github.com??? QCefView / CefViewCore??
rem     cef-builds.spotifycdn.com??? CEF ????? 105MB?
rem
rem Required by MultiDisplayControl.pro (QCefView/CEF backend is always enabled)
rem   3rdparty\cef              CEF 109 SDK?? Release\libcef.lib
rem   3rdparty\QCefView\lib     QCefView.lib?????
rem   3rdparty\QCefView\bin     QCefView.dll ? CefView\ ????
rem
rem ???????????????????????
rem =============================================================================
setlocal

rem ---- 0. Locate the toolchain (every path below is auto-detected) ----
rem Optional overrides from the environment, used when the auto-detection
rem cannot see the install (network share, unmounted drive, ...):
rem   VCVARS       full path of vcvars64.bat
rem   QT_SDK_DIR   Qt kit folder, the one containing bin\qmake.exe
rem   CMAKE_EXE    full path of cmake.exe
rem   NINJA_DIR    folder containing ninja.exe
set "BUILD_DIR=%TEMP%\mdc_qcef_build"

set "ROOT=%~dp0.."
set "QCEF_DIR=%ROOT%\3rdparty\QCefView"
set "PATCH_FILE=%~dp0qcefview-cef109-win7.patch"

if not defined VCVARS call :find_vcvars
if defined VCVARS call :derive_vs_install
if not defined QT_SDK_DIR call :find_qt_sdk
if not defined CMAKE_EXE call :find_cmake
if not defined NINJA_DIR call :find_ninja

echo [INFO] vcvars64.bat : %VCVARS%
echo [INFO] Qt SDK       : %QT_SDK_DIR%
echo [INFO] CMake        : %CMAKE_EXE%
echo [INFO] Ninja dir    : %NINJA_DIR%
echo.

if not defined VCVARS goto err_toolchain
if not defined QT_SDK_DIR goto err_qt
if not defined CMAKE_EXE goto err_cmake
if not defined NINJA_DIR goto err_ninja

rem ---- 1. ?? MSVC ? Ninja ???? ----
call "%VCVARS%" >nul 2>&1
if errorlevel 1 (
  echo [ERROR] vcvars64.bat ?????%VCVARS%
  exit /b 1
)
set "PATH=%NINJA_DIR%;%PATH%"

rem ---- 2. ?? QCefView v1.2.1 ??????????----
if not exist "%QCEF_DIR%\CMakeLists.txt" (
  echo [INFO] ?? QCefView v1.2.1 ...
  git clone --branch v1.2.1 --depth 1 https://github.com/CefView/QCefView.git "%QCEF_DIR%"
  if errorlevel 1 (
    echo [ERROR] QCefView ????
    exit /b 1
  )
)

rem ---- 3. ???????CEF ?? 109 + ?? C4005??????? ----
rem ?? check ?????????????????????????
git -C "%QCEF_DIR%" apply --reverse --check "%PATCH_FILE%" >nul 2>&1
if not errorlevel 1 (
  echo [INFO] ??????????
) else (
  echo [INFO] ?????? ...
  git -C "%QCEF_DIR%" apply "%PATCH_FILE%"
  if errorlevel 1 (
    echo [ERROR] ?????????? 3rdparty\QCefView ??????
    exit /b 1
  )
)

rem ---- 4. CMake ??????Release / x64?----
"%CMAKE_EXE%" -S "%QCEF_DIR%" -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DPROJECT_ARCH=x86_64 -DBUILD_DEMO=OFF -DSTATIC_CRT=OFF -DQT_SDK_DIR=%QT_SDK_DIR:\=/%
if errorlevel 1 (
  echo [ERROR] CMake ????
  exit /b 1
)

rem ?? CEF ??????????????????? --parallel??????
"%CMAKE_EXE%" --build "%BUILD_DIR%" --parallel 3
if errorlevel 1 (
  echo [ERROR] QCefView ????
  exit /b 1
)

rem ---- 5. ????? .pro ????? ----
set "CEF_SDK="
for /d %%D in ("%BUILD_DIR%\_deps\cefviewcore-src\dep\cef_binary_*") do set "CEF_SDK=%%~fD"
if not defined CEF_SDK (
  echo [ERROR] ??????? CEF SDK ??
  exit /b 1
)

rem CEF SDK ??????? Debug?????? Release?? tests?????????
robocopy "%CEF_SDK%" "%ROOT%\3rdparty\cef" /E /XD Debug tests /NFL /NDL /NJH /NJS /NP
rem QCefView ??????????
robocopy "%BUILD_DIR%\output\Release\lib" "%ROOT%\3rdparty\QCefView\lib" QCefView.lib /NFL /NDL /NJH /NJS /NP
rem QCefView ????QCefView.dll ? CefView\ ?????????
robocopy "%BUILD_DIR%\output\Release\bin" "%ROOT%\3rdparty\QCefView\bin" /E /XF *.pdb /NFL /NDL /NJH /NJS /NP

echo [OK] Dependencies are ready, run qmake and build MultiDisplayControl now.
exit /b 0

rem =============================================================================
rem Toolchain detection
rem =============================================================================

:err_toolchain
echo [ERROR] Visual Studio (MSVC v142 toolset) was not found.
echo         Install "Visual Studio Build Tools 2019" with the C++ workload,
echo         or set VCVARS to the full path of vcvars64.bat.
exit /b 1

:err_qt
echo [ERROR] Qt SDK was not found, a Qt 5 (msvc*_64) kit is required.
echo         Install Qt 5.14.2 msvc2017_64, or set QT_SDK_DIR to the kit folder.
exit /b 1

:err_cmake
echo [ERROR] cmake.exe was not found.
echo         Install CMake 3.21 or newer, or set CMAKE_EXE to its full path.
exit /b 1

:err_ninja
echo [ERROR] ninja.exe was not found.
echo         Install Ninja or the Qt Tools component, or set NINJA_DIR to the
echo         folder containing ninja.exe.
exit /b 1

:find_vcvars
rem 1) The vswhere database: prefer VS2019 (v142), then the newest install
set "VSWHERE_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE_PATH%" goto vcvars_scan
for /f "usebackq delims=" %%i in (`"%VSWHERE_PATH%" -version "[16.0,17.0)" -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find "VC\Auxiliary\Build\vcvars64.bat" 2^>nul`) do if not defined VCVARS set "VCVARS=%%i"
if defined VCVARS goto :eof
for /f "usebackq delims=" %%i in (`"%VSWHERE_PATH%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find "VC\Auxiliary\Build\vcvars64.bat" 2^>nul`) do if not defined VCVARS set "VCVARS=%%i"
if defined VCVARS goto :eof

:vcvars_scan
rem 2) Environment of an already opened developer prompt
if not defined VSINSTALLDIR goto vcvars_env_skip
if exist "%VSINSTALLDIR%VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%VSINSTALLDIR%VC\Auxiliary\Build\vcvars64.bat"
:vcvars_env_skip
if defined VCVARS goto :eof

rem 3) Scan every drive for custom / side-by-side install locations
for %%d in (C D E F G H I J K L M N O P Q R S T U V W X Y Z) do call :scan_vs_drive "%%d:"
goto :eof

:scan_vs_drive
rem %~1 = a drive root such as "E:"
if defined VCVARS goto :eof
if not exist "%~1\" goto :eof
call :scan_vs_root "%~1"
for /d %%a in ("%~1\*") do call :scan_vs_root "%%~fa"
goto :eof

:scan_vs_root
rem %~1 = a folder that may directly contain "Microsoft Visual Studio"
if defined VCVARS goto :eof
if not exist "%~1\Microsoft Visual Studio" goto :eof
for /f "delims=" %%y in ('dir /b /o-n /ad "%~1\Microsoft Visual Studio" 2^>nul') do call :scan_vs_editions "%~1\Microsoft Visual Studio\%%y"
goto :eof

:scan_vs_editions
rem %~1 = ...\Microsoft Visual Studio\<version>, one folder per edition
if defined VCVARS goto :eof
for /d %%e in ("%~1\*") do call :try_vcvars "%%~fe"
goto :eof

:try_vcvars
rem Accept a VS edition only if it ships the x64 environment setup script
if defined VCVARS goto :eof
if not exist "%~1\VC\Auxiliary\Build\vcvars64.bat" goto :eof
set "VCVARS=%~1\VC\Auxiliary\Build\vcvars64.bat"
goto :eof

:derive_vs_install
rem VS_INSTALL = VS root, derived from <VS>\VC\Auxiliary\Build\vcvars64.bat
set "VS_INSTALL="
set "VCVARS_DIR="
for %%f in ("%VCVARS%") do set "VCVARS_DIR=%%~dpf"
if not defined VCVARS_DIR goto :eof
for %%f in ("%VCVARS_DIR%..\..\..") do set "VS_INSTALL=%%~ff"
goto :eof

:find_qt_sdk
rem 1) Environment variables set by Qt installers and IDEs
if defined QTDIR call :try_qt_kit "%QTDIR%"
if defined QT_SDK_DIR goto :eof
if defined QT_HOME call :try_qt_kit "%QT_HOME%"
if defined QT_SDK_DIR goto :eof

rem 2) qmake already available on PATH
where qmake >nul 2>&1
if errorlevel 1 goto qt_path_done
for /f "delims=" %%q in ('where qmake 2^>nul') do call :try_qt_kit_parent "%%q"
:qt_path_done
if defined QT_SDK_DIR goto :eof

rem 3) Scan every drive plus the user profile for Qt install roots
for %%d in (C D E F G H I J K L M N O P Q R S T U V W X Y Z) do call :scan_qt_drive "%%d:"
if defined QT_SDK_DIR goto :eof
if not defined USERPROFILE goto :eof
call :scan_qt_subdir "%USERPROFILE%"
goto :eof

:scan_qt_drive
rem %~1 = a drive root such as "D:"
if defined QT_SDK_DIR goto :eof
if not exist "%~1\" goto :eof
rem <drive>:\Qt and <drive>:\Qt<version>
for /d %%q in ("%~1\Qt*") do call :scan_qt_root "%%~fq"
rem <drive>:\<folder>\Qt and <drive>:\<folder>\Qt<version>
for /d %%a in ("%~1\*") do call :scan_qt_subdir "%%~fa"
goto :eof

:scan_qt_subdir
rem %~1 = a folder that may contain a Qt folder, e.g. E:\InstalledApplication
if defined QT_SDK_DIR goto :eof
for /d %%q in ("%~1\Qt*") do call :scan_qt_root "%%~fq"
goto :eof

:scan_qt_root
rem %~1 = a Qt root folder or a kit folder itself
if defined QT_SDK_DIR goto :eof
call :try_qt_kit "%~1"
if defined QT_SDK_DIR goto :eof
rem The kit this project is built with, try it before anything else
call :try_qt_kit "%~1\5.14.2\msvc2017_64"
if defined QT_SDK_DIR goto :eof
for /f "delims=" %%v in ('dir /b /o-n /ad "%~1" 2^>nul') do call :scan_qt_version "%~1\%%v"
goto :eof

:scan_qt_version
rem %~1 = <Qt root>\<version>, kits live one level deeper
if defined QT_SDK_DIR goto :eof
call :try_qt_kit "%~1"
if defined QT_SDK_DIR goto :eof
for %%k in (msvc2017_64 msvc2019_64 msvc2022_64 msvc2015_64) do if not defined QT_SDK_DIR call :try_qt_kit "%~1\%%k"
if defined QT_SDK_DIR goto :eof
for /d %%k in ("%~1\msvc*_64") do call :try_qt_kit "%%~fk"
goto :eof

:try_qt_kit
rem Accept a Qt kit only if it is Qt 5 and has bin\qmake.exe
if defined QT_SDK_DIR goto :eof
set "CANDIDATE=%~1"
if "%CANDIDATE:~-1%"=="\" set "CANDIDATE=%CANDIDATE:~0,-1%"
if not exist "%CANDIDATE%\bin\qmake.exe" goto :eof
if not exist "%CANDIDATE%\lib\cmake\Qt5Core" goto :eof
set "QT_SDK_DIR=%CANDIDATE%"
goto :eof

:try_qt_kit_parent
rem Derive the kit folder from a qmake.exe path by dropping bin\qmake.exe
if defined QT_SDK_DIR goto :eof
for %%d in ("%~dp1..") do call :try_qt_kit "%%~fd"
goto :eof

:find_cmake
rem 1) cmake.exe already on PATH
for /f "delims=" %%c in ('where cmake.exe 2^>nul') do if not defined CMAKE_EXE set "CMAKE_EXE=%%c"
if defined CMAKE_EXE goto :eof

rem 2) Qt Tools shipped next to the detected kit: <QtRoot>\Tools\CMake_64
if not defined QT_SDK_DIR goto cmake_vs
for %%r in ("%QT_SDK_DIR%\..\..") do set "QT_ROOT=%%~fr"
if exist "%QT_ROOT%\Tools\CMake_64\bin\cmake.exe" set "CMAKE_EXE=%QT_ROOT%\Tools\CMake_64\bin\cmake.exe"
if defined CMAKE_EXE goto :eof

rem 3) CMake bundled with Visual Studio
:cmake_vs
if not defined VS_INSTALL goto cmake_program_files
if exist "%VS_INSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "CMAKE_EXE=%VS_INSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if defined CMAKE_EXE goto :eof

rem 4) Default standalone CMake install locations
:cmake_program_files
if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "CMAKE_EXE=%ProgramFiles%\CMake\bin\cmake.exe"
if defined CMAKE_EXE goto :eof
if exist "%ProgramFiles(x86)%\CMake\bin\cmake.exe" set "CMAKE_EXE=%ProgramFiles(x86)%\CMake\bin\cmake.exe"
goto :eof

:find_ninja
rem 1) ninja.exe already on PATH
for /f "delims=" %%n in ('where ninja.exe 2^>nul') do call :set_ninja_dir "%%n"
if defined NINJA_DIR goto :eof

rem 2) Qt Tools shipped next to the detected kit: <QtRoot>\Tools\Ninja
if not defined QT_ROOT if defined QT_SDK_DIR for %%r in ("%QT_SDK_DIR%\..\..") do set "QT_ROOT=%%~fr"
if not defined QT_ROOT goto ninja_vs
if exist "%QT_ROOT%\Tools\Ninja\ninja.exe" set "NINJA_DIR=%QT_ROOT%\Tools\Ninja"
if defined NINJA_DIR goto :eof

rem 3) Ninja bundled with Visual Studio CMake
:ninja_vs
if not defined VS_INSTALL goto :eof
if exist "%VS_INSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" set "NINJA_DIR=%VS_INSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
goto :eof

:set_ninja_dir
rem Keep the folder part of a ninja.exe path, without the trailing backslash
if defined NINJA_DIR goto :eof
set "NINJA_CANDIDATE=%~dp1"
if "%NINJA_CANDIDATE:~-1%"=="\" set "NINJA_CANDIDATE=%NINJA_CANDIDATE:~0,-1%"
set "NINJA_DIR=%NINJA_CANDIDATE%"
goto :eof
