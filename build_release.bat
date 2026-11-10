@echo off
setlocal

REM ============================================================
REM  Generic Qt + MSVC build script for Windows
REM
REM  Usage:
REM    Put this file next to the qmake project (*.pro) and run it.
REM    To reuse it in another project, edit ONLY the CONFIG section
REM    below; the detection and build logic needs no change.
REM
REM  Notes:
REM    1. Keep this file ASCII-only and CRLF. Non-ASCII text inside a
REM       .bat file is decoded with the console code page (936 by
REM       default), so a UTF-8 saved script shows mojibake and bad
REM       byte pairing may even make cmd run a comment as a command.
REM    2. Never test "(x86)" paths inside an "if (...)" block. Use a
REM       goto label or a "call :label" subroutine instead, because
REM       the closing parenthesis in the path closes the block first.
REM ============================================================

REM ============================================================
REM                 CONFIG SECTION (per project)
REM ============================================================

REM Project name, used by the window title and log output
set "PROJECT_NAME=MultiDisplayControl"
REM qmake project file, in the same folder as this script
set "PRO_FILE=MultiDisplayControl.pro"
REM Build configuration: release or debug
set "BUILD_CONFIG=release"
REM Target architecture: x64 or x86
set "TARGET_ARCH=x64"
REM Output exe path relative to this script, empty means no check
set "OUTPUT_EXE=release\MultiDisplayControl.exe"

REM Folder holding obj/moc/qrc files to delete before build, empty = skip
set "OBJ_TMP_DIR=release\tmp"
REM Skip qmake when the Makefile already exists: yes / no
set "SKIP_QMAKE_IF_MAKEFILE=yes"
REM Add ATL/MFC include and lib paths (required by ActiveQt): yes / no
set "NEED_ATLMFC=yes"

REM Optional extra MSVC toolset candidates, highest priority first.
REM Normally NOT needed: detection already covers VCINSTALLDIR,
REM VSINSTALLDIR, the vswhere database and a scan of every drive root.
REM Only fill this in for installs that live on a network share or on a
REM drive that is not mounted while the build runs. Space separated.
set "MSVC_FALLBACKS="
REM Optional extra Qt kit candidates, the folder containing bin\qmake.exe.
REM Normally NOT needed: detection already covers QTDIR/QT_HOME/QT_ROOT,
REM qmake found on PATH and a scan of every drive root (both
REM <root>\Qt\<ver>\<kit> and <root>\Qt\<ver> layouts). Space separated.
set "QT_FALLBACKS="

REM Extra environment for third-party libraries, empty means skip
REM Directories appended to PATH, separated by semicolon
set "EXTRA_PATH="
REM Directories appended to INCLUDE, separated by semicolon
set "EXTRA_INCLUDE="
REM Directories appended to LIB, separated by semicolon
set "EXTRA_LIB="

REM ============================================================
REM                    CONFIG SECTION END
REM ============================================================

cd /d "%~dp0"
title %PROJECT_NAME% Build

echo.
echo ============================================================
echo   %PROJECT_NAME% Build
echo   Auto-detect Qt / MSVC / Windows SDK
echo ============================================================
echo.

REM ==================== Resolve config ====================
REM Derive MSVC host tools folder and library folder name from arch
if /i "%TARGET_ARCH%"=="x86" goto arch_x86
set "MSVC_HOST_DIR=Hostx64\x64"
set "ARCH_LIB=x64"
goto arch_done
:arch_x86
set "MSVC_HOST_DIR=Hostx86\x86"
set "ARCH_LIB=x86"
:arch_done

REM Derive Makefile name and qmake argument from build configuration
if /i "%BUILD_CONFIG%"=="debug" goto cfg_debug
set "MAKEFILE_NAME=Makefile.Release"
set "QMAKE_CONFIG_FLAG=CONFIG+=release"
goto cfg_done
:cfg_debug
set "MAKEFILE_NAME=Makefile.Debug"
set "QMAKE_CONFIG_FLAG=CONFIG+=debug"
:cfg_done

echo [CONFIG] Project      : %PROJECT_NAME%
echo [CONFIG] .pro file    : %PRO_FILE%
echo [CONFIG] Build config : %BUILD_CONFIG% / %TARGET_ARCH%
echo [CONFIG] Output exe   : %OUTPUT_EXE%
echo.

REM ==================== Check project file ====================
if exist "%PRO_FILE%" goto detect_msvc
echo [ERROR] %PRO_FILE% not found.
echo         Run this script from the project root directory,
echo         or fix PRO_FILE in the CONFIG section.
pause
exit /b 1

:detect_msvc
REM ==================== Auto-detect MSVC ====================
echo [INFO] Detecting MSVC compiler...

set MSVC_DIR=
set VS_INSTALL=
set VSWHERE_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe

REM 1) Environment of an already opened "x64 Native Tools" prompt
if not defined VCToolsInstallDir goto vcenv_skip
call :try_msvc "%VCToolsInstallDir%"
:vcenv_skip
if defined MSVC_DIR goto msvc_done
if not defined VSINSTALLDIR goto vsenv_skip
call :try_vs_install "%VSINSTALLDIR%"
:vsenv_skip
if defined MSVC_DIR goto msvc_done

REM 2) Visual Studio installer database, every install, newest first.
REM    This also finds installs that live outside Program Files.
if not exist "%VSWHERE_PATH%" goto vswhere_skip
for /f "usebackq delims=" %%i in (`"%VSWHERE_PATH%" -all -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do call :try_vs_install "%%i"
:vswhere_skip
if defined MSVC_DIR goto msvc_done

REM 3) Scan every drive for custom / side-by-side install locations
call :scan_msvc_drives
if defined MSVC_DIR goto msvc_done

REM 4) Last resort: user supplied MSVC_FALLBACKS
if not defined MSVC_FALLBACKS goto msvc_done
for %%p in (%MSVC_FALLBACKS%) do call :try_msvc "%%~p"

:msvc_done
if defined MSVC_DIR goto msvc_ok
echo [ERROR] MSVC compiler not found!
echo         Install Visual Studio Build Tools with the C++ desktop workload,
echo         or add a valid path to MSVC_FALLBACKS in the CONFIG section.
pause
exit /b 1
:msvc_ok
REM Walk up from <VS>\VC\Tools\MSVC\<ver> to the Visual Studio root
for %%d in ("%MSVC_DIR%\..\..\..\..") do set "VS_INSTALL=%%~fd"
echo [OK] MSVC found: %MSVC_DIR%
echo.

REM ==================== Auto-detect Windows SDK ====================
echo [INFO] Detecting Windows SDK...

set SDK_DIR=
set SDK_VERSION=
set SDK_ROOT=%ProgramFiles(x86)%\Windows Kits\10

if not exist "%SDK_ROOT%\Include" goto sdk_fail
set "SDK_DIR=%SDK_ROOT%"

REM Pick the newest version that has um and ucrt headers plus arch libs
for /f "delims=" %%v in ('dir /b /o-n /ad "%SDK_DIR%\Include" 2^>nul ^| findstr /b "10."') do call :try_sdk "%%v"

if defined SDK_VERSION goto sdk_ok
goto sdk_fail

:sdk_ok
echo [OK] Windows SDK found: %SDK_VERSION%
echo.
goto detect_qt

:sdk_fail
echo [ERROR] Windows SDK 10 not found!
echo         Install the Windows SDK from the Visual Studio Installer.
pause
exit /b 1

:detect_qt
REM ==================== Auto-detect Qt ====================
echo [INFO] Detecting Qt ...

set QT_DIR=

REM 1) Well known environment variables, set by Qt installers and IDEs
call :try_qt_env
if defined QT_DIR goto qt_done

REM 2) qmake already available on PATH
where qmake >nul 2>&1
if errorlevel 1 goto qt_path_skip
for /f "delims=" %%q in ('where qmake 2^>nul') do call :try_qt_parent "%%q"
:qt_path_skip
if defined QT_DIR goto qt_done

REM 3) Scan every drive for custom install locations
call :scan_qt_drives
if defined QT_DIR goto qt_done

REM 4) Last resort: user supplied QT_FALLBACKS
if not defined QT_FALLBACKS goto qt_done
for %%p in (%QT_FALLBACKS%) do call :try_qt "%%~p"

:qt_done
if defined QT_DIR goto qt_ok
echo [ERROR] Qt not found!
echo         Tried QTDIR/QT_HOME/QT_ROOT, qmake on PATH, a scan of all
echo         drives and QT_FALLBACKS.
echo         Install the proper Qt kit or add it to QT_FALLBACKS.
pause
exit /b 1
:qt_ok
echo [OK] Qt found: %QT_DIR%
echo.

REM ==================== Setup Environment ====================
echo [STEP 1/4] Setting up build environment...

set "PATH=%MSVC_DIR%\bin\%MSVC_HOST_DIR%;%QT_DIR%\bin;%SDK_DIR%\bin\%SDK_VERSION%\%ARCH_LIB%;%PATH%"
set "INCLUDE=%MSVC_DIR%\include;%SDK_DIR%\Include\%SDK_VERSION%\ucrt;%SDK_DIR%\Include\%SDK_VERSION%\um;%SDK_DIR%\Include\%SDK_VERSION%\shared;%SDK_DIR%\Include\%SDK_VERSION%\winrt;%SDK_DIR%\Include\%SDK_VERSION%\cppwinrt"
set "LIB=%MSVC_DIR%\lib\%ARCH_LIB%;%SDK_DIR%\Lib\%SDK_VERSION%\ucrt\%ARCH_LIB%;%SDK_DIR%\Lib\%SDK_VERSION%\um\%ARCH_LIB%"
set "LIBPATH=%MSVC_DIR%\lib\%ARCH_LIB%"

REM ATL/MFC paths, needed by ActiveQt and other COM based modules
if /i not "%NEED_ATLMFC%"=="yes" goto extra_env

set "ATLMFC_INC=%VS_INSTALL%\ATLMFC\include"
if not exist "%ATLMFC_INC%" goto atl_inc_skip
set "INCLUDE=%ATLMFC_INC%;%INCLUDE%"
:atl_inc_skip

set "ATLMFC_LIB=%VS_INSTALL%\ATLMFC\lib\%ARCH_LIB%"
if not exist "%ATLMFC_LIB%" goto atl_lib_skip
set "LIB=%ATLMFC_LIB%;%LIB%"
set "LIBPATH=%ATLMFC_LIB%;%LIBPATH%"
:atl_lib_skip

:extra_env
REM Append project specific third-party paths
if not defined EXTRA_PATH goto extra_path_skip
set "PATH=%EXTRA_PATH%;%PATH%"
:extra_path_skip
if not defined EXTRA_INCLUDE goto extra_include_skip
set "INCLUDE=%EXTRA_INCLUDE%;%INCLUDE%"
:extra_include_skip
if not defined EXTRA_LIB goto extra_lib_skip
set "LIB=%EXTRA_LIB%;%LIB%"
:extra_lib_skip

echo [OK] Environment ready
echo.

REM ==================== qmake ====================
echo [STEP 2/4] Running qmake...

if /i not "%SKIP_QMAKE_IF_MAKEFILE%"=="yes" goto do_qmake
if not exist "%MAKEFILE_NAME%" goto do_qmake
echo [INFO] %MAKEFILE_NAME% already exists, skip qmake.
echo        Delete Makefile.* to force regenerate.
goto qmake_done

:do_qmake
qmake "%PRO_FILE%" -spec win32-msvc "%QMAKE_CONFIG_FLAG%"
if errorlevel 1 goto qmake_fail
echo [OK] qmake done
goto qmake_done

:qmake_fail
echo.
echo [ERROR] qmake failed!
pause
exit /b 1

:qmake_done
echo.

REM ==================== Clean old objects ====================
echo [STEP 3/4] Compiling...

if "%OBJ_TMP_DIR%"=="" goto do_nmake
if not exist "%OBJ_TMP_DIR%" goto do_nmake
echo [INFO] Cleaning old object files...
if exist "%OBJ_TMP_DIR%\*.obj" del /q "%OBJ_TMP_DIR%\*.obj"
if exist "%OBJ_TMP_DIR%\moc_*.cpp" del /q "%OBJ_TMP_DIR%\moc_*.cpp"
if exist "%OBJ_TMP_DIR%\qrc_*.cpp" del /q "%OBJ_TMP_DIR%\qrc_*.cpp"

:do_nmake
REM ==================== nmake ====================
nmake -f %MAKEFILE_NAME%
if errorlevel 1 goto build_fail

echo.
echo [STEP 4/4] Build complete!
echo.

REM ==================== Show result ====================
if "%OUTPUT_EXE%"=="" goto build_success
if not exist "%OUTPUT_EXE%" goto build_fail

for %%A in ("%OUTPUT_EXE%") do set SIZE_B=%%~zA
set /a SIZE_KB=%SIZE_B% / 1024
set /a SIZE_MB=%SIZE_KB% / 1024

echo [OK] Output file:  %cd%\%OUTPUT_EXE%
echo [OK] File size:    %SIZE_MB% MB (%SIZE_KB% KB)
echo.

:build_success
echo ============================================================
echo   Build Success!
echo ============================================================
echo.
pause
exit /b 0

:build_fail
echo.
echo ============================================================
echo   Build FAILED!
echo   Scroll up to see the error messages.
echo ============================================================
echo.
pause
exit /b 1

REM ============================================================
REM                       Subroutines
REM ============================================================

:try_vs_install
REM Find the newest usable MSVC toolset under a Visual Studio root
if defined MSVC_DIR goto :eof
if not exist "%~1\VC\Tools\MSVC" goto :eof
for /f "delims=" %%v in ('dir /b /o-n /ad "%~1\VC\Tools\MSVC" 2^>nul') do call :try_msvc "%~1\VC\Tools\MSVC\%%v"
goto :eof

:try_msvc
REM Accept a candidate MSVC toolset only if cl.exe exists for the arch.
REM A trailing backslash is stripped so "dir\bin" never becomes "dir\\bin".
if defined MSVC_DIR goto :eof
set "CANDIDATE=%~1"
if "%CANDIDATE:~-1%"=="\" set "CANDIDATE=%CANDIDATE:~0,-1%"
if not exist "%CANDIDATE%\bin\%MSVC_HOST_DIR%\cl.exe" goto :eof
set "MSVC_DIR=%CANDIDATE%"
goto :eof

:try_sdk
REM Accept a candidate SDK version only if it is complete for the arch
if defined SDK_VERSION goto :eof
if not exist "%SDK_DIR%\Include\%~1\um" goto :eof
if not exist "%SDK_DIR%\Include\%~1\ucrt" goto :eof
if not exist "%SDK_DIR%\Lib\%~1\um\%ARCH_LIB%" goto :eof
set "SDK_VERSION=%~1"
goto :eof

:try_qt
REM Accept a candidate Qt kit only if it contains bin\qmake.exe.
REM A trailing backslash is stripped so "dir\bin" never becomes "dir\\bin".
if defined QT_DIR goto :eof
set "CANDIDATE=%~1"
if "%CANDIDATE:~-1%"=="\" set "CANDIDATE=%CANDIDATE:~0,-1%"
if not exist "%CANDIDATE%\bin\qmake.exe" goto :eof
set "QT_DIR=%CANDIDATE%"
goto :eof

:try_qt_env
REM Try the environment variables commonly pointing at a Qt kit
if defined QTDIR call :try_qt "%QTDIR%"
if defined QT_DIR goto :eof
if defined QT_HOME call :try_qt "%QT_HOME%"
if defined QT_DIR goto :eof
if defined QT_ROOT call :try_qt "%QT_ROOT%"
goto :eof

:try_qt_parent
REM Derive the Qt root from a qmake.exe path by dropping bin\qmake.exe
if defined QT_DIR goto :eof
for %%d in ("%~dp1..") do call :try_qt "%%~fd"
goto :eof

REM ---------------- Drive scan: MSVC ----------------
REM Looks for "<any folder>\Microsoft Visual Studio\<year>\<edition>"
REM anywhere on the fixed drives, so VS installed to a non default
REM folder (e.g. E:\InstalledApplication\... or a BuildTools copy)
REM is found without editing this script.

:scan_msvc_drives
for %%d in (C D E F G H I J K L M N O P Q R S T U V W X Y Z) do call :scan_msvc_drive "%%d:"
goto :eof

:scan_msvc_drive
REM %~1 = a drive root such as "E:"
if defined MSVC_DIR goto :eof
if not exist "%~1\" goto :eof
REM <drive>:\Microsoft Visual Studio
call :scan_msvc_root "%~1"
REM <drive>:\<folder>\Microsoft Visual Studio
for /d %%a in ("%~1\*") do call :scan_msvc_root "%%~fa"
goto :eof

:scan_msvc_root
REM %~1 = a folder that may directly contain "Microsoft Visual Studio"
if defined MSVC_DIR goto :eof
if not exist "%~1\Microsoft Visual Studio" goto :eof
call :scan_vs_tree "%~1\Microsoft Visual Studio"
goto :eof

:scan_vs_tree
REM %~1 = ...\Microsoft Visual Studio, one folder per VS version
for /d %%y in ("%~1\*") do call :scan_vs_editions "%%~fy"
goto :eof

:scan_vs_editions
REM %~1 = ...\Microsoft Visual Studio\<version>, one folder per edition
for /d %%e in ("%~1\*") do call :try_vs_install "%%~fe"
goto :eof

REM ---------------- Drive scan: Qt ----------------
REM Recognises both common install layouts:
REM   <root>\Qt\<version>\<kit>\bin\qmake.exe   (official online installer)
REM   <root>\<version>\<kit>\bin\qmake.exe      (Qt folder matched by Qt*)

:scan_qt_drives
for %%d in (C D E F G H I J K L M N O P Q R S T U V W X Y Z) do call :scan_qt_drive "%%d:"
REM Per user install inside the profile folder (default for the online installer)
if not defined USERPROFILE goto :eof
call :scan_qt_subdir "%USERPROFILE%"
goto :eof

:scan_qt_drive
REM %~1 = a drive root such as "D:"
if defined QT_DIR goto :eof
if not exist "%~1\" goto :eof
REM <drive>:\Qt and <drive>:\Qt<version>
for /d %%q in ("%~1\Qt*") do call :scan_qt_root "%%~fq"
REM <drive>:\<folder>\Qt and <drive>:\<folder>\Qt<version>
for /d %%a in ("%~1\*") do call :scan_qt_subdir "%%~fa"
goto :eof

:scan_qt_subdir
REM %~1 = a folder that may contain a Qt folder, e.g. E:\InstalledApplication
if defined QT_DIR goto :eof
for /d %%q in ("%~1\Qt*") do call :scan_qt_root "%%~fq"
goto :eof

:scan_qt_root
REM %~1 = a Qt root folder or a kit folder itself
if defined QT_DIR goto :eof
call :try_qt "%~1"
if defined QT_DIR goto :eof
REM Kits first, newest version folder first
for /f "delims=" %%v in ('dir /b /o-n /ad "%~1" 2^>nul') do call :scan_qt_version "%~1\%%v"
goto :eof

:scan_qt_version
REM %~1 = <Qt root>\<version>, kits live one level deeper
if defined QT_DIR goto :eof
call :try_qt "%~1"
if defined QT_DIR goto :eof
for /d %%k in ("%~1\*") do call :try_qt "%%~fk"
goto :eof
