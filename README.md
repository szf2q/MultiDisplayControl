# MultiDisplayControl（多屏应用控制程序）

## 1. 项目简介

MultiDisplayControl 是一个基于 Qt 的**多显示器大屏统一控制程序**。它以"一屏一窗口"的方式，为每一个物理显示器创建一个无边框全屏窗口，并把各类需要在指定屏幕上展示的内容嵌入其中，再通过全局快捷键在屏幕之间切换内容，用于展览大屏、监控中心、试验台等需要长时间无人值守展示的场景。

支持嵌入的展示内容分三类：

| 类型 | 判定条件（config.ini 中的 `app_N_path`） | 实现方式 |
| --- | --- | --- |
| 桌面应用程序 | 普通本地可执行文件路径（如 `xxx.exe`） | 以管理员身份启动进程，抓取目标窗口句柄（HWND），通过 `QWidget::createWindowContainer` 把外部窗口重设父窗口后嵌入本程序 |
| 网页 | 以 `http://` 或 `https://` 开头 | 使用 `QWebEngineView` 加载，独立 `QWebEngineProfile` 与内存缓存 |
| PPT 演示文稿 | 路径中包含 `.ppt` | 通过 ActiveQt（`QAxWidget`）调用 WPS(`KWPP.Application`) 或 PowerPoint(`PowerPoint.Application`) COM 接口，循环全屏放映后嵌入放映窗口 |

每个屏幕窗口内部以 `QTabWidget` 承载多个应用，程序启动时自动完成启动、定位、嵌入、全屏等全部操作。

## 2. 功能特性

- **多屏全屏布局**：按屏幕几何位置（或主屏优先）排序，自动把配置中的第 N 号窗口放到第 N 块屏幕上，无边框全屏展示。
- **屏幕序号标识**：可在每块屏幕右上角浮层显示屏幕编号（`show_index`），标签页标题同时带上屏幕序号，主屏以 `*` 前缀标识。
- **全局热键切换**：`Alt+数字` 可在屏幕之间"对调"显示内容，`Alt+R` 一键复位到默认屏幕，`Alt+Q` / `Alt+F4` 退出。
- **悬浮标签栏**：屏幕顶部提供独立置顶的 `QTabBar`（可通过 `tab_offset_x` 微调水平位置），点击即可切换当前屏幕显示的应用。
- **外部窗口嵌入**：通过 Win32 窗口样式改写（`WS_CHILD`，去掉标题栏/边框/弹出属性）+ `QWindow::fromWinId` 实现外部 exe 的嵌入与显隐控制。
- **管理员权限自提升**：`runas_administrator=true` 时，若当前非管理员，则通过 `ShellExecuteEx(runas)` 重新以管理员身份启动自身并退出原进程（被嵌入的应用需要通过 `startDetached("runas")` 启动）。
- **PPT 自动放映**：启动前自动结束 WPS 相关进程（`wps.exe`、`wpp.exe`、`wpscloudsvr.exe`、`wpsupdate.exe`）避免冲突，放映过程带 100ms 轮询守护，放映结束会自动重新开始，实现无限循环。
- **Chrome 弹窗清理**：每 500ms 扫描一次 `chrome.exe` 窗口，自动关闭"翻译此页""无法更新 Chrome""要恢复页面吗"等干扰窗口及 `Chrome_SystemMessageWindow` 类窗口。
- **网页调试**：内置启动 `QTWEBENGINE_REMOTE_DEBUGGING=9222`，可通过 `http://127.0.0.1:9222` 远程调试嵌入网页。
- **调试模式**：`debug_mode=true` 时不启动任何应用，仅生成占位标签页显示屏幕序号与屏幕几何信息，并在窗口中保留标签栏，便于排查屏幕映射关系。

## 3. 运行环境与技术栈

| 项 | 说明 |
| --- | --- |
| 语言标准 | C++11（`CONFIG += c++11`） |
| 框架 | Qt 5.14.2 |
| Qt 模块 | `core` `gui` `widgets` `network` `axcontainer`；MSVC 下额外启用 `webenginewidgets` `webchannel` |
| 工具链 | Windows 下 MSVC 或 MinGW（推荐 MSVC，WebEngine/ActiveQt 依赖更完整） |
| 目标平台 | Windows 7 及以上；银河麒麟 V10（Linux 侧仅预留接口，尚未完整实现） |
| Win32 依赖 | `user32`、`kernel32`、`psapi`、`tlhelp32` |
| 第三方库 | `tz.utils.base`（提供 `IniConfiguration` 的 ini 读写实现，需与可执行文件同目录） |

> 说明：`webenginewidgets` 仅在 `win32:msvc` 条件下加入。若使用 MinGW 构建且需要网页展示能力，需自行确认工具链中已提供 Qt WebEngine 模块。

## 4. 目录结构

```
MultiDisplayControl/
├── MultiDisplayControl.pro      # qmake 工程文件（目标名、模块、输出目录、依赖库）
├── main.cpp                     # 程序入口：读配置、启动窗口、注册热键、启动定时任务
├── ExternalAppWindow.h/.cpp     # 核心窗口类：单屏全屏窗口 + 标签页容器 + 应用/网页/PPT 加载
├── ExternalAppWindow.ui         # 窗口界面（centralwidget + QTabWidget）
├── ScreenConfig.h               # 配置数据结构：AppConfig / ScreenConfig / MultiScreenConfig
├── IniConfiguration.h           # ini 配置读写接口（实现由 tz.utils.base 提供）
├── ProcessUtils.h               # 进程操作抽象接口（跨平台）
├── ProcessFactory.h             # 进程工具工厂（按平台返回具体实现）
├── ProcessUtilsWin32Impl.h/.cpp # Windows 平台进程实现（枚举窗口、取路径、启停进程等）
├── ProcessUtilsLinuxImpl.h      # Linux（银河麒麟）平台实现声明（待补充实现）
├── ProcessInfo.h                # 进程信息结构体（pid/hwnd/进程名/窗口标题/类名/路径等）
├── GlobalKeyWatcher.h/.cpp      # 全局热键注册与 Windows 原生消息过滤
├── GlobalVariants.h/.cpp        # 全局变量：屏幕序号 -> 窗口对象映射
├── FullSizeTabWidget.h          # 尺寸自适应填满父窗口的 QTabWidget
├── CustomWidget.h               # 焦点进入时激活外部窗口的 QWidget
├── resource.qrc                 # 资源清单（qss 样式、图标、图片）
├── qss/                         # dark.css 主题及配套图标资源
├── app.manifest                 # Windows 应用程序清单
├── MultiDisplayControl_zh_CN.ts # 中文翻译文件
├── release/                     # 编译输出目录（含 Qt 运行库、config.ini、cache 等）
└── debug/                       # 调试编译输出目录（由构建生成）
```

## 5. 系统架构

```
                    ┌──────────────────────────────────────────────┐
                    │                 main.cpp                     │
                    │  读取 config.ini → 构建 MultiScreenConfig     │
                    │  创建 N 个 ExternalAppWindow                  │
                    │  注册全局热键 + 定时任务                       │
                    └──────────────┬───────────────────────────────┘
                                   │
      ┌────────────────────────────┼──────────────────────────────┐
      │                            │                              │
┌─────▼──────────────┐  ┌──────────▼────────────┐  ┌──────────────▼────────────┐
│ GlobalKeyWatcher   │  │ ExternalAppWindow     │  │ ProcessFactory            │
│ 全局热键注册与拦截   │  │ 单屏全屏窗口 + Tab 容器 │  │ ProcessUtilsWin32 / Linux │
│ Alt+数字/Alt+R/Alt+Q│  │ 应用/网页/PPT 加载      │  │ 进程枚举、启动、结束       │
└────────────────────┘  └───────────────────────┘  └───────────────────────────┘
                                   │
                        ┌──────────▼──────────┐
                        │ GlobalVariants      │
                        │ 屏幕序号 → 窗口映射  │
                        └─────────────────────┘
```

## 6. 核心类说明

### 6.1 `ExternalAppWindow`（[ExternalAppWindow.h](file:///d:/sunzhenfeng/projects/MultiDisplayControl/ExternalAppWindow.h) / [.cpp](file:///d:/sunzhenfeng/projects/MultiDisplayControl/ExternalAppWindow.cpp)）

单块屏幕对应的无边框全屏主窗口，是项目的核心。

- `initWindow()`：按 `sort_screen` 对 `QGuiApplication::screens()` 排序（主屏排到最后），确定本窗口的**默认屏幕** `defaultScreen` 及是否为主屏 `isPrimaryScreen`。
- `setScreen(QScreen*, force)`：把窗口移动到目标屏幕并 `showFullScreen()`，同时同步外部窗口几何尺寸、悬浮标签栏位置和屏幕序号浮层位置。
- `setToDefaultScreen()` / `getDefaultScreen()` / `getCurrentScreen()`：默认屏幕复位与查询，供热键切换逻辑使用。
- `loadApps(bool isReload)`：遍历配置的应用列表，按路径类型分别调用 `loadWebPage()`、`startAndPlayPpt()`、`startExternalProgram()`；结束后初始化标签栏，若无可展示内容则隐藏窗口。
- `startExternalProgram()` / `findProcessInfo()` / `startProcess()`：启动外部程序（管理员权限）→ 轮询 `getProcessInfo(进程名, 窗口标题)` 直至稳定命中窗口（最多 30s，命中后连续确认 5 次）→ 改写窗口样式并以容器窗口嵌入标签页。
- `loadWebPage()`：为每个网页创建独立 `QWebEngineProfile`，缓存目录为 `<exe目录>/cache/<窗口标题>`，使用内存 HTTP 缓存、禁用持久化 Cookie，并放入 `QGraphicsView` 场景以撑满屏幕。
- `startAndPlayPpt()` / `playPpt()`：通过 COM 自动选择 WPS 或 PowerPoint，配置循环放映参数（`LoopUntilStopped`、`AdvanceMode=1` 等）后 `Run()`，并轮询守护放映窗口，嵌入到标签页。
- `tabIndexChanged()`：标签切换时展示对应外部窗口、隐藏其余窗口；Chrome 窗口额外通过 `ShowWindow()` 同步显隐。
- 析构时统一结束所启动的进程并释放资源。

> 说明：Chrome 窗口被强制隐藏后无法重新显示的问题在代码中以 `TODO` 标注，为已知未解决项。

### 6.2 `ProcessUtils` / `ProcessUtilsWin32`（[ProcessUtils.h](file:///d:/sunzhenfeng/projects/MultiDisplayControl/ProcessUtils.h) / [ProcessUtilsWin32Impl.cpp](file:///d:/sunzhenfeng/projects/MultiDisplayControl/ProcessUtilsWin32Impl.cpp)）

进程操作的抽象接口与 Windows 实现，由 `ProcessFactory::newInstance()` 按平台创建。

- `runAsAdministrators()`：检查当前进程是否属于管理员组，不是则 `ShellExecuteEx(runas)` 重新启动自身；返回 `>=0` 表示应退出当前进程。
- `getProcessInfo(进程名)`：调用 `tasklist` 过滤镜像名获取 PID，再按 PID 枚举其可见顶层窗口。
- `getProcessInfo(进程名, 窗口标题)` / `getProcessInfoByWindowTitle(标题)`：通过 `EnumWindows` 枚举可见顶层窗口并按标题模糊匹配。
- `getProcessPath(pid)`：`OpenProcess` + `GetProcessImageFileName`，并把 `\Device\HarddiskVolume` 形式的路径转换为盘符路径。
- `startProcess(路径, 参数, runAsAdmin)`：需要提权时使用 `startDetached("runas")`；Chrome 启动前会移除 `__COMPAT_LAYER`、`RunAsInvoker` 环境变量；启动后最多等待 5s 确认进程已存在。
- `killProcess(...)`：支持按 `QProcess` 对象、PID、进程名三种方式结束进程。

### 6.3 `GlobalKeyWatcher`（[GlobalKeyWatcher.cpp](file:///d:/sunzhenfeng/projects/MultiDisplayControl/GlobalKeyWatcher.cpp)）

单例（静态成员 `GlobalKeyWatcher::watcher`），通过 `RegisterHotKey` 注册全局热键，并安装 `QAbstractNativeEventFilter` 拦截 `WM_HOTKEY` 消息后以信号方式广播：

- `switchScreenKeyPressed(int screenIndex, QString keyMessage)`
- `altAndRPressed()`
- `quitKeyPressed(QString keyMessage)`

## 7. 配置文件说明

配置文件为可执行文件同目录下的 `config.ini`，由 `loadScreenConfig()` 解析（[main.cpp](file:///d:/sunzhenfeng/projects/MultiDisplayControl/main.cpp#L16-L86)）。

### 7.1 全局配置节 `[screen]`

| 配置项 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `count` | int | 0 | 屏幕配置节数量；实际生效数量以有效配置节为准 |
| `show_index` | bool | true | 是否显示屏幕序号（浮层 + 标签页标题前缀） |
| `sort_screen` | bool | true | 是否按屏幕几何位置排序后再与配置序号对应 |
| `debug_mode` | bool | false | 调试模式：不启动任何应用，仅显示占位标签页并保留标签栏 |
| `runas_administrator` | bool | false | 是否以管理员身份运行（非管理员时自动提权重启） |

### 7.2 屏幕配置节 `[screen-N]`（N 从 1 开始）

| 配置项 | 类型 | 说明 |
| --- | --- | --- |
| `app_count` | int | 该屏幕下的应用数量（以实际有效应用为准重新计算） |
| `screen_title` | string | 屏幕名称，窗口标题为 `N号屏-屏幕名称` |
| `tab_offset_x` | int | 悬浮标签栏相对屏幕左边缘的水平偏移（像素，负值按 0 处理） |

### 7.3 应用配置项（`app_N_*`，N 从 1 开始）

| 配置项 | 说明 |
| --- | --- |
| `app_N_path` | 应用路径；`http(s)://` 开头按网页处理，包含 `.ppt` 按 PPT 处理，其余按外部程序处理 |
| `app_N_title` | 窗口标题（用于启动后匹配目标窗口，支持部分匹配），必填 |
| `app_N_tab_name` | 标签页名称；留空时使用 `app_N_title` |
| `app_N_startup_args` | 启动参数（按空格切分） |

`app_N_path` 与 `app_N_title` 任一为空时该应用会被跳过。示例：

```ini
[screen]
count=2
show_index=true
sort_screen=true
debug_mode=false
runas_administrator=false

[screen-1]
app_count=2
screen_title=主屏
tab_offset_x=1400
app_1_path=http://172.20.200.121:11001/TZ200101/index
app_1_title=新型智能反潜无人艇故障诊断与健康管理系统
app_1_tab_name=无人艇
app_1_startup_args=
app_2_path=D:/DMS/EnginePHMDMS[Z12]/EnginePHMDMS[Z12].exe
app_2_title=EnginePHMDMS[Z12]
app_2_tab_name=WZ12
app_2_startup_args=

[screen-2]
app_count=1
screen_title=传动
tab_offset_x=1400
app_1_path=http://172.20.200.121:8081/TZ100308/#/systemOverview
app_1_title=直升机传动系统试验数据监测及分析系统
app_1_tab_name=传动
app_1_startup_args=
```

> 注意：ini 支持 `#` 与 `;` 注释。

## 8. 全局快捷键

| 快捷键 | 功能 |
| --- | --- |
| `Alt+1` ~ `Alt+7`、`Alt+小键盘1` ~ `Alt+小键盘7` | 将第 N 号屏幕的内容切换到主屏显示（主屏与目标屏内容互换） |
| `Alt+R` | 重新把所有窗口复位到各自的默认屏幕（不改变当前窗口内容） |
| `Alt+Q` / `Alt+F4` | 关闭所有窗口并退出程序 |

切换逻辑见 [switchScreen()](file:///d:/sunzhenfeng/projects/MultiDisplayControl/main.cpp#L141-L203) 与 [reloadScreen()](file:///d:/sunzhenfeng/projects/MultiDisplayControl/main.cpp#L212-L230)：若目标屏即主屏则仅复位；否则交换两块屏幕的显示内容，其余窗口复位到默认屏幕。窗口已隐藏或主屏缺失时不做处理。

## 9. 编译与部署

### 9.1 编译

使用 Qt Creator 打开 `MultiDisplayControl.pro`，或命令行执行：

```bash
qmake MultiDisplayControl.pro
make            # Windows + MinGW 使用 mingw32-make；MSVC 使用 nmake / jom
```

构建产物输出到工程同级目录：

| 平台 / 构建类型 | 输出目录 |
| --- | --- |
| Windows Release / Debug | `release` / `debug` |
| 麒麟（x86_64） | `release_kylin_x86` / `debug_kylin_x86` |
| 麒麟（arm64） | `release_kylin_arm` / `debug_kylin_arm` |

中间产物（moc/obj/rcc/ui）统一输出到 `<输出目录>/tmp`。

### 9.2 部署

可执行文件需与以下内容同目录：

1. Qt 运行库（`Qt5Core.dll`、`Qt5Gui.dll`、`Qt5Widgets.dll`、`Qt5Network.dll`、`Qt5WebEngine*`、`QtWebEngineProcess.exe` 及 `platforms/`、`resources/`、`translations/`、`styles/`、`imageformats/`、`iconengines/` 等插件目录，`release/` 目录中已随工程提供）；
2. 第三方库 `tz.utils.base.dll`（IniConfiguration 实现）；
3. 配置文件 `config.ini`；
4. WebEngine 相关 `d3dcompiler_47.dll`、`libEGL.dll`、`libGLESv2.dll`、`opengl32sw.dll` 等。

### 9.3 运行

直接运行 `MultiDisplayControl.exe`：

- 需管理员权限时会在启动阶段弹出 UAC 并自动重启自身；
- 启动后按 `config.ini` 为每块屏幕创建窗口并加载内容，之后进入消息循环等待热键；
- 网页缓存与运行日志（`debug.log`）生成在可执行文件目录下。

## 10. 启动流程

1. 设置 WebEngine 环境变量（关闭 `Win32kLockdown` 以避免 Win10 下 `QtWebEngineProcess.exe` 崩溃、开启 9222 远程调试端口）。
2. 加载 `:/qss/dark.css` 设置全局深色主题。
3. 解析 `config.ini` 生成 `MultiScreenConfig`。
4. 如需管理员权限且当前非管理员，则提权重启自身。
5. 若任一应用路径包含 `.ppt`，先结束 WPS 相关进程。
6. 为每块屏幕创建 `ExternalAppWindow` 并调用 `loadApps()` 启动/嵌入内容，同时登记到 `GlobalVariants::winList`。
7. 初始化全局热键监听，启动 Chrome 干扰窗口清理定时器（500ms）。
8. 500ms 后执行一次 `reloadScreen()` 复位窗口位置，进入事件循环。

## 11. 已知问题与注意事项

- **Chrome 窗口隐藏后无法重新显示**：`tabIndexChanged()` 中已标注 `TODO`，隐藏后的 Chrome 窗口需依赖 `ShowWindow()` 恢复，存在不稳定性。
- **Linux（银河麒麟）支持尚不完整**：`ProcessUtilsLinuxImpl.h` 仅有接口声明，缺少 `.cpp` 实现，且 `axcontainer`（ActiveQt）与 WebEngine 在 Linux 下的可用性需另行处理。
- **依赖 Win32 API**：窗口嵌入、窗口枚举、热键注册、进程路径转换等均基于 Windows API，跨平台移植需重写相关实现。
- **`ProcessFactory.h` 中存在拼写错误**：非 Windows/Linux 分支的 `reutrn nullptr;` 应为 `return nullptr;`（该分支当前不会被编译）。
- **启动耗时**：外部程序启动后需要轮询等待窗口出现（最多 30s），程序整体启动时间取决于被启动应用的响应速度。
- **`.gitignore`** 会忽略 `*.exe`、`*.dll`，因此 `release/` 下的运行库需单独获取或手工保留。
