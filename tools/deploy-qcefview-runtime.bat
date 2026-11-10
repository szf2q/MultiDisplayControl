@echo off
rem =============================================================================
rem QCefView / CEF 运行期部署脚本（仅 Windows）
rem
rem 用途：
rem   把 QCefView.dll 与 CEF 运行库按 QCefView 的加载约定部署到程序目录，
rem   可选地再用 windeployqt 补齐 Qt 自身依赖，使 MultiDisplayControl.exe
rem   可直接双击运行。
rem
rem 用法：
rem   tools\deploy-qcefview-runtime.bat [目标目录] [--with-qt | --no-qt]
rem
rem 参数说明：
rem   目标目录    部署目标；省略时依次取环境变量 TARGET_DIR、默认 ..\release
rem   --with-qt   强制部署 Qt 依赖，找不到 windeployqt.exe 时报错退出
rem   --no-qt     跳过 Qt 依赖部署
rem   （都不指定时自动判断：能找到 windeployqt.exe 且目标目录有 exe 才部署）
rem
rem 相关环境变量：
rem   TARGET_DIR   目标目录（命令行参数优先）
rem   QT_BIN_DIR   直接指定包含 windeployqt.exe 的 Qt bin 目录
rem
rem 部署布局（由 QCefView 内部实现决定，不可随意调整）：
rem   目标目录\QCefView.dll                        Qt 封装层动态库
rem   目标目录\CefView\CefViewWing.exe             CEF 渲染子进程宿主
rem   目标目录\CefView\libcef.dll 等全部 CEF 运行库
rem   目标目录\Qt5*.dll、platforms\ 等             Qt 依赖（可选步骤）
rem
rem 说明：
rem   源文件来自 3rdparty\QCefView\bin，该目录由
rem   tools\build-qcefview-deps.bat 生成，且不纳入版本库。
rem =============================================================================

setlocal enabledelayedexpansion

rem 先备份脚本所在目录：下面的参数解析会调用 shift，而 shift 会改变 %0，
rem 导致之后再取 %~dp0 得到错误路径（实测会退化成上级目录）。
set "SCRIPT_DIR=%~dp0"

rem ---- 1. 解析命令行参数 ----
set "TARGET="
set "QT_MODE=auto"

:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="--with-qt" set "QT_MODE=on"
if /i "%~1"=="--no-qt" set "QT_MODE=off"
if /i "%~1"=="--with-qt" goto next_arg
if /i "%~1"=="--no-qt" goto next_arg
if not defined TARGET set "TARGET=%~1"
:next_arg
shift
goto parse_args
:args_done

if "%TARGET%"=="" set "TARGET=%TARGET_DIR%"
if "%TARGET%"=="" set "TARGET=%SCRIPT_DIR%..\release"

rem 去掉可能存在的结尾反斜杠，避免 robocopy 路径拼接出错
if "%TARGET:~-1%"=="\" set "TARGET=%TARGET:~0,-1%"

set "SRC_BIN=%SCRIPT_DIR%..\3rdparty\QCefView\bin"
set "SRC_CEF=%SRC_BIN%\CefView"

echo [INFO] QCefView 运行库源目录: %SRC_BIN%
echo [INFO] 部署目标目录        : %TARGET%
echo [INFO] Qt 依赖部署模式      : %QT_MODE%

rem ---- 2. 校验源文件是否齐备 ----
if not exist "%SRC_BIN%\QCefView.dll" (
  echo [ERROR] 缺少 QCefView.dll，请先执行 tools\build-qcefview-deps.bat
  exit /b 1
)
if not exist "%SRC_CEF%\CefViewWing.exe" (
  echo [ERROR] 缺少 CefView\CefViewWing.exe，请先执行 tools\build-qcefview-deps.bat
  exit /b 1
)

rem ---- 3. 校验目标目录是否像程序目录（存在 exe 才提示，不阻断）----
if not exist "%TARGET%" (
  echo [ERROR] 目标目录不存在: %TARGET%
  exit /b 1
)
if not exist "%TARGET%\MultiDisplayControl.exe" (
  echo [WARN] 目标目录下未找到 MultiDisplayControl.exe，请确认路径是否正确
)

rem ---- 4. 部署 QCefView.dll 到 exe 同级目录 ----
echo [INFO] 复制 QCefView.dll ...
robocopy "%SRC_BIN%" "%TARGET%" QCefView.dll /NJH /NJS /NP /NDL /NFL
if errorlevel 8 (
  echo [ERROR] QCefView.dll 复制失败
  exit /b 1
)

rem ---- 5. 部署 CEF 运行库到 CefView\ 子目录 ----
rem QCefView 内部会 SetDllDirectory^("<exe目录>/CefView"^)，
rem 并把 browser_subprocess_path 指向 CefView\CefViewWing.exe，因此该目录结构固定。
echo [INFO] 复制 CEF 运行库到 CefView\ ...
robocopy "%SRC_CEF%" "%TARGET%\CefView" /E /XF *.pdb /NJH /NJS /NP /NDL /NFL
if errorlevel 8 (
  echo [ERROR] CEF 运行库复制失败
  exit /b 1
)

rem ---- 6. 可选步骤：部署 Qt 自身依赖 ----
if /i "%QT_MODE%"=="off" (
  echo [INFO] 已指定 --no-qt，跳过 Qt 依赖部署
  goto verify
)
if not exist "%TARGET%\MultiDisplayControl.exe" (
  echo [INFO] 目标目录缺少 MultiDisplayControl.exe，跳过 Qt 依赖部署
  goto verify
)

rem windeployqt 查找顺序：QT_BIN_DIR -> PATH -> QTDIR/QT_HOME -> 全盘扫描 Qt 安装目录
set "WINDEPLOYQT="
if defined QT_BIN_DIR if exist "%QT_BIN_DIR%\windeployqt.exe" set "WINDEPLOYQT=%QT_BIN_DIR%\windeployqt.exe"
if not defined WINDEPLOYQT for /f "delims=" %%W in ('where windeployqt.exe 2^>nul') do if not defined WINDEPLOYQT set "WINDEPLOYQT=%%W"
if not defined WINDEPLOYQT call :find_windeployqt

if not defined WINDEPLOYQT (
  if /i "%QT_MODE%"=="on" (
    echo [ERROR] 未找到 windeployqt.exe，请用 QT_BIN_DIR 指定 Qt 的 bin 目录
    exit /b 1
  )
  echo [INFO] 未找到 windeployqt.exe，跳过 Qt 依赖部署（可用 QT_BIN_DIR 指定）
  goto verify
)

echo [INFO] 部署 Qt 依赖: %WINDEPLOYQT%
"%WINDEPLOYQT%" --release --no-translations "%TARGET%\MultiDisplayControl.exe"
if errorlevel 1 (
  echo [ERROR] windeployqt 执行失败
  exit /b 1
)

:verify
rem ---- 7. 结果校验 ----
echo [INFO] 校验关键文件 ...
set "MISSING=0"
for %%F in (
  "%TARGET%\QCefView.dll"
  "%TARGET%\CefView\CefViewWing.exe"
  "%TARGET%\CefView\libcef.dll"
  "%TARGET%\CefView\chrome_elf.dll"
  "%TARGET%\CefView\icudtl.dat"
  "%TARGET%\CefView\resources.pak"
  "%TARGET%\CefView\v8_context_snapshot.bin"
) do (
  if exist %%F (
    for %%S in (%%F) do echo   [OK] %%~nxS  %%~zS 字节
  ) else (
    echo   [MISS] %%F
    set "MISSING=1"
  )
)

if "%MISSING%"=="1" (
  echo [ERROR] 部署不完整，请检查源目录是否被裁剪
  exit /b 1
)

rem Qt 依赖属于可选步骤，缺失时只提示不失败
if not exist "%TARGET%\Qt5Core.dll" (
  echo [WARN] 未检测到 Qt5Core.dll，程序仍无法独立启动，请加 --with-qt 重新部署
) else (
  echo   [OK] Qt5Core.dll
)

echo [OK] 运行期部署完成: %TARGET%
exit /b 0

rem =============================================================================
rem windeployqt 自动探测子程序
rem 说明：优先使用 QTDIR/QT_HOME，再遍历各盘符下的 Qt 安装目录
rem       （兼容 <根>\Qt\<版本>\<kit> 与 <根>\Qt<版本>\<kit> 两种布局）
rem =============================================================================

:find_windeployqt
if not defined WINDEPLOYQT if defined QTDIR call :try_windeployqt_bin "%QTDIR%\bin"
if not defined WINDEPLOYQT if defined QT_HOME call :try_windeployqt_bin "%QT_HOME%\bin"
if not defined WINDEPLOYQT for %%D in (C D E F G H I J K L M N O P Q R S T U V W X Y Z) do call :scan_wd_drive "%%D:"
if not defined WINDEPLOYQT if defined USERPROFILE call :scan_wd_subdir "%USERPROFILE%"
goto :eof

:scan_wd_drive
rem %~1 = 盘符根目录，例如 "D:"
if defined WINDEPLOYQT goto :eof
if not exist "%~1\" goto :eof
rem <盘符>:\Qt 与 <盘符>:\Qt<版本>
for /d %%Q in ("%~1\Qt*") do call :scan_wd_root "%%~fQ"
rem <盘符>:\<目录>\Qt 与 <盘符>:\<目录>\Qt<版本>
for /d %%A in ("%~1\*") do call :scan_wd_subdir "%%~fA"
goto :eof

:scan_wd_subdir
rem %~1 = 可能包含 Qt 目录的上级目录，例如 E:\InstalledApplication
if defined WINDEPLOYQT goto :eof
for /d %%Q in ("%~1\Qt*") do call :scan_wd_root "%%~fQ"
goto :eof

:scan_wd_root
rem %~1 = Qt 根目录或 kit 目录本身
if defined WINDEPLOYQT goto :eof
call :try_windeployqt_bin "%~1\bin"
if defined WINDEPLOYQT goto :eof
rem 本项目使用的 kit 优先
call :try_windeployqt_bin "%~1\5.14.2\msvc2017_64\bin"
if defined WINDEPLOYQT goto :eof
for /f "delims=" %%V in ('dir /b /o-n /ad "%~1" 2^>nul') do call :scan_wd_version "%~1\%%V"
goto :eof

:scan_wd_version
rem %~1 = <Qt 根目录>\<版本>，kit 在其下一层
if defined WINDEPLOYQT goto :eof
call :try_windeployqt_bin "%~1\bin"
if defined WINDEPLOYQT goto :eof
for %%K in (msvc2017_64 msvc2019_64 msvc2022_64 msvc2015_64) do if not defined WINDEPLOYQT call :try_windeployqt_bin "%~1\%%K\bin"
if defined WINDEPLOYQT goto :eof
for /d %%K in ("%~1\msvc*_64") do call :try_windeployqt_bin "%%~fK\bin"
goto :eof

:try_windeployqt_bin
rem 仅当目录中存在 windeployqt.exe 且属于 Qt 5 时采用，避免误用 Qt 6 的部署工具
if defined WINDEPLOYQT goto :eof
if not exist "%~1\windeployqt.exe" goto :eof
if not exist "%~1\..\lib\cmake\Qt5Core" goto :eof
set "WINDEPLOYQT=%~1\windeployqt.exe"
goto :eof
