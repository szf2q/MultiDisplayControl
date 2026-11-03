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
set "VCVARS=D:\VS2019BT\VC\Auxiliary\Build\vcvars64.bat"
set "QT_SDK_DIR=D:\Qt\5.14.2\msvc2017_64"
set "CMAKE_EXE=D:\Qt\Tools\CMake_64\bin\cmake.exe"
set "NINJA_DIR=D:\Qt\Tools\Ninja"
set "BUILD_DIR=%TEMP%\mdc_qcef_build"

set "ROOT=%~dp0.."
set "QCEF_DIR=%ROOT%\3rdparty\QCefView"
set "PATCH_FILE=%~dp0qcefview-cef109-win7.patch"

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
