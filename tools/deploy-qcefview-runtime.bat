@echo off
rem =============================================================================
rem QCefView / CEF 运行期部署脚本（仅 Windows）
rem
rem 用途：
rem   把 QCefView.dll 与 CEF 运行库按 QCefView 的加载约定部署到程序目录，
rem   部署后 MultiDisplayControl.exe 可直接双击运行，无需手工摆放文件。
rem
rem 用法：
rem   tools\deploy-qcefview-runtime.bat [目标目录]
rem
rem 目标目录的确定顺序（优先级从高到低）：
rem   1. 第一个命令行参数
rem   2. 环境变量 TARGET_DIR
rem   3. 默认值 ..\release（相对本脚本所在目录）
rem
rem 部署布局（由 QCefView 内部实现决定，不可随意调整）：
rem   目标目录\QCefView.dll                        Qt 封装层动态库
rem   目标目录\CefView\CefViewWing.exe             CEF 渲染子进程宿主
rem   目标目录\CefView\libcef.dll 等全部 CEF 运行库
rem
rem 说明：
rem   - 本脚本只负责 CEF 相关运行库；Qt 自身依赖（Qt5*.dll、platforms 等）
rem     仍需另行使用 windeployqt 部署。
rem   - 源文件来自 3rdparty\QCefView\bin，该目录由
rem     tools\build-qcefview-deps.bat 生成，且不纳入版本库。
rem =============================================================================

setlocal enabledelayedexpansion

rem ---- 1. 解析目标目录 ----
set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=%TARGET_DIR%"
if "%TARGET%"=="" set "TARGET=%~dp0..\release"

rem 去掉可能存在的结尾反斜杠，避免 robocopy 路径拼接出错
if "%TARGET:~-1%"=="\" set "TARGET=%TARGET:~0,-1%"

set "SRC_BIN=%~dp0..\3rdparty\QCefView\bin"
set "SRC_CEF=%SRC_BIN%\CefView"

echo [INFO] QCefView 运行库源目录: %SRC_BIN%
echo [INFO] 部署目标目录        : %TARGET%

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

rem ---- 6. 结果校验 ----
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

echo [OK] QCefView 运行库部署完成: %TARGET%
exit /b 0
