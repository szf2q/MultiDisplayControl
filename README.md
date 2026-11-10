# MultiDisplayControl（多屏应用控制程序）

## 1. 项目简介

MultiDisplayControl 是一个基于 Qt 的**多显示器大屏统一控制程序**。它以"一屏一窗口"的方式，为每一个物理显示器创建一个无边框全屏窗口，并把各类需要在指定屏幕上展示的内容嵌入其中，再通过全局快捷键在屏幕之间切换内容，用于展览大屏、监控中心、试验台等需要长时间无人值守展示的场景。

支持嵌入的展示内容分三类：

| 类型         | 判定条件（`config.json` 中的 `software[].path`） | 实现方式                                                                                                                              |
| ------------ | ------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------- |
| 桌面应用程序 | 普通本地可执行文件路径（如 `xxx.exe`）           | 以当前用户权限启动进程（不会弹 UAC 提权），抓取目标窗口句柄（HWND），通过纯 Win32 `SetParent` 把外部窗口嵌入本程序                    |
| 网页         | 以 `http://` 或 `https://` 开头                  | 使用 `QCefView`（CEF 126 内核）加载，全进程共用 `cefCache` 缓存目录                                                                   |
| PPT 演示文稿 | 扩展名为 `.ppt` / `.pptx` / `.pps` / `.ppsx`     | 通过 ActiveQt（`QAxWidget`）调用 WPS(`KWPP.Application`) 或 PowerPoint(`PowerPoint.Application`) COM 接口，循环全屏放映后嵌入放映窗口 |

每个屏幕窗口内部以 `QTabWidget` 承载多个应用，程序启动时自动完成启动、定位、嵌入、全屏等全部操作。

## 2. 功能特性

- **多屏全屏布局**：按屏幕几何位置（或主屏优先）排序，自动把配置中的第 N 号窗口放到第 N 块屏幕上，无边框全屏展示。
- **可视化管理界面**：`Alt+M` 全屏显示/隐藏管理窗口，支持软件清单管理（桌面程序/网页/PPT，清单内直接显示软件预览缩略图）、屏幕数量与布局管理、拖拽分配软件到屏幕、同屏多软件调整标签顺序；保存后立即热应用，无需重启。同一软件只能绑定到一块屏幕：已绑定的条目在清单中显示「已绑定 N号屏」标记且无法再拖出，从屏幕移除后标记消失、恢复可拖拽。新增或编辑软件时可自动运行一次目标程序，抓取第一个界面作为预览图（保存在 `previews/` 目录）并自动填充窗口标题；网页通过临时 CEF 窗口抓取首帧生成预览，PPT 通过 COM 导出第一页。
- **配置单文件**：`config.json` 是唯一的数据源，保存时自动备份为 `config.json.bak`；不再写出 `config.ini`，仅在 `config.json` 不存在（旧部署升级）时从旧版 `config.ini` 导入一次并另存为 `config.json`。
- **屏幕序号标识**：可在每块屏幕右上角浮层显示屏幕编号（`show_index`），标签页标题同时带上屏幕序号，主屏以 `*` 前缀标识。
- **全局热键切换**：`Alt+数字` 可在屏幕之间"对调"显示内容，`Alt+R` 一键复位到默认屏幕，`Alt+F5` 只重新加载鼠标所在屏幕当前标签页的应用（不切换标签），`Alt+Tab` / `Alt+Shift+Tab` 循环切换鼠标所在屏幕的标签页，`Alt+F12` 自动重启主程序，`Alt+Q` / `Alt+F4` 退出。
- **悬浮标签栏**：屏幕顶部提供独立置顶的 `QTabBar`（可通过 `tab_offset_x` 微调水平位置），点击即可切换当前屏幕显示的应用。
- **外部窗口嵌入**：普通桌面程序通过 Win32 窗口样式改写（`WS_CHILD`，保留自身标题栏/边框）+ `SetParent` 嵌入；Chrome 窗口仍使用 `QWindow::fromWinId` 包装。
- **管理员权限自提升**：`runas_administrator=true` 时，若当前非管理员，则通过 `ShellExecuteEx(runas)` 重新以管理员身份启动自身并退出原进程。被嵌入的外部应用始终以当前用户权限启动，不会单独弹 UAC。
- **PPT 自动放映**：启动时只清理没有可见窗口的 WPS 残留实例（`wps.exe`、`wpp.exe`），不会结束用户正在编辑的文档；放映过程带 50ms 轮询守护，放映结束会自动重新开始，实现无限循环。
- **Chrome 弹窗清理**：每 500ms 扫描一次 `chrome.exe` 窗口，自动关闭"翻译此页""无法更新 Chrome""要恢复页面吗"等干扰窗口及 `Chrome_SystemMessageWindow` 类窗口。
- **网页调试**：可在 `main.cpp` 中通过 `cefConfig.setRemoteDebuggingPort(9222)` 打开 CEF 远程调试（默认关闭），通过 `http://127.0.0.1:9222` 调试嵌入网页。
- **调试模式**：`debug_mode=true` 时不启动任何应用，仅生成占位标签页显示屏幕序号与屏幕几何信息，并在窗口中保留标签栏，便于排查屏幕映射关系。

## 3. 运行环境与技术栈

| 项         | 说明                                                                  |
| ---------- | --------------------------------------------------------------------- |
| 语言标准   | C++11（`CONFIG += c++11`）                                            |
| 框架       | Qt 5.14.2                                                             |
| Qt 模块    | `core` `gui` `widgets` `network` `axcontainer`                        |
| 工具链     | Windows 下必须使用 MSVC（CEF 官方二进制由 MSVC 构建，MinGW 无法链接） |
| 目标平台   | Windows 10 及以上；银河麒麟 V10（Linux 侧仅预留接口，尚未完整实现）   |
| Win32 依赖 | `user32`、`kernel32`、`psapi`、`tlhelp32`                             |

> 说明：网页能力依赖 `3rdparty/QCefView` 与 `3rdparty/cef`，构建前必须就位，否则工程直接构建失败；可用 `tools/build-qcefview-deps.bat` 一键生成。

## 4. 目录结构

```
MultiDisplayControl/
├── MultiDisplayControl.pro      # qmake 工程文件（目标名、模块、输出目录、依赖库）
├── main.cpp                     # 程序入口：读配置、启动窗口、注册热键、启动定时任务
├── ConfigStore.h/.cpp           # 配置读写：config.json 读写，旧版 config.ini 仅用于导入迁移
├── DisplayRuntime.h/.cpp        # 运行时：按配置创建/更新屏幕窗口，保存后增量热更新
├── ManagementWindow.h/.cpp      # 管理界面主窗口：两栏全屏布局、保存/重新加载/热应用
├── SoftwareLibraryPanel.h/.cpp  # 管理界面-软件清单面板（预览缩略图、新增/编辑/删除/搜索/拖拽源）
├── SoftwareCapture.h/.cpp       # 管理界面-软件预览图采集（启动一次程序并抓取首个窗口）
├── ScreenLayoutCanvas.h/.cpp    # 管理界面-屏幕布局画布（屏幕卡片、拖拽分配、标签排序）
├── ExternalAppWindow.h/.cpp     # 核心窗口类：单屏全屏窗口 + 标签页容器 + 应用/网页/PPT 加载
├── ExternalAppWindow.ui         # 窗口界面（centralwidget + QTabWidget）
├── ScreenConfig.h               # 配置数据结构：AppConfig / ScreenConfig / MultiScreenConfig
├── IniConfiguration.h/.cpp      # 旧版 ini 配置读写封装（基于 QSettings，工程内实现）
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
├── MultiDisplayControl_zh_CN.ts # 中文翻译文件
├── release/                     # 编译输出目录（含 Qt 运行库、config.json、cache 等）
└── debug/                       # 调试编译输出目录（由构建生成）
```

## 5. 系统架构

```
                    ┌──────────────────────────────────────────────┐
                    │                 main.cpp                     │
                    │  读取 config.json → 构建 MultiScreenConfig    │
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
- `loadApps(bool isReload)`：四阶段加载——并行预启动本屏外部程序 → 按配置顺序创建网页标签并登记待等待的外部程序 → 并行等待窗口就绪并嵌入 → 最后串行加载 PPT；结束后归位到配置首项并初始化标签栏，若无可展示内容则隐藏窗口。
- `reloadCurrentTab()`：故障恢复用，只重载当前选中的标签页——外部程序先结束旧进程再重新启动并原地替换标签，网页重建 `CefWebView` 并重新加载，PPT 放映保持不变（避免与现有 COM 放映实例冲突）；同屏其它标签页不受影响。
- `startExternalProgram()` / `findProcessInfo()` / `startProcess()`：以当前用户权限启动外部程序 → 优先按 exe 路径定位窗口（进程名 + 标题匹配兜底），轮询直至稳定命中窗口（命中后连续确认 3 次）→ 以宿主控件嵌入标签页。
- 嵌入桌面应用时默认保留其标题栏与边框；管理界面开启「嵌入应用去掉边框」后，嵌入时会清除 `WS_CAPTION`/`WS_BORDER`，并按 `AdjustWindowRectEx` 反算外框尺寸、用负偏移把客户区对齐到宿主控件左上角，使客户区完整铺满整块屏幕、四周不留黑边（不清除 `WS_THICKFRAME`，避免外部 Qt 程序崩溃）。
- `loadWebPage()`：通过 `CefWebView`（QCefView/CEF 126）加载网页，全进程共用 `<exe目录>/cefCache` 缓存目录。
- `startAndPlayPpt()` / `playPpt()`：通过 COM 自动选择 WPS 或 PowerPoint，配置循环放映参数（`LoopUntilStopped`、`AdvanceMode=1` 等）后 `Run()`，并轮询守护放映窗口，嵌入到标签页。
- `tabIndexChanged()`：标签切换时展示对应外部窗口、隐藏其余窗口；Chrome 窗口额外通过 `ShowWindow()` 同步显隐。
- 析构时统一结束所启动的进程并释放资源。

> 说明：Chrome 窗口被强制隐藏后无法重新显示的问题在代码中以 `TODO` 标注，为已知未解决项。

### 6.2 `ProcessUtils` / `ProcessUtilsWin32`（[ProcessUtils.h](file:///d:/sunzhenfeng/projects/MultiDisplayControl/ProcessUtils.h) / [ProcessUtilsWin32Impl.cpp](file:///d:/sunzhenfeng/projects/MultiDisplayControl/ProcessUtilsWin32Impl.cpp)）

进程操作的抽象接口与 Windows 实现，由 `ProcessFactory::newInstance()` 按平台创建。

- `runAsAdministrators()`：检查当前进程是否属于管理员组，不是则 `ShellExecuteEx(runas)` 重新启动自身；返回 `>=0` 表示应退出当前进程。
- `getProcessInfo(进程名)`：通过 Toolhelp32 进程快照按镜像名匹配 PID（不再调用 `tasklist` 子进程），再按 PID 枚举其可见顶层窗口。
- `getProcessInfo(进程名, 窗口标题)` / `getProcessInfoByWindowTitle(标题)`：通过 `EnumWindows` 枚举可见顶层窗口并按标题模糊匹配。
- `getProcessPath(pid)`：`OpenProcess` + `GetProcessImageFileName`，并把 `\Device\HarddiskVolume` 形式的路径转换为盘符路径。
- `startProcess(路径, 参数, runAsAdmin)`：以当前用户权限分离启动进程（`runAsAdmin` 参数当前不生效，也不会弹 UAC）；Chrome 启动前会移除 `__COMPAT_LAYER`、`RunAsInvoker` 环境变量；启动后最多等待 5s 确认进程已存在。
- `killProcess(...)`：支持按 `QProcess` 对象、PID、进程名三种方式结束进程。

### 6.3 `GlobalKeyWatcher`（[GlobalKeyWatcher.cpp](file:///d:/sunzhenfeng/projects/MultiDisplayControl/GlobalKeyWatcher.cpp)）

单例（静态成员 `GlobalKeyWatcher::watcher`），通过 `RegisterHotKey` 注册全局热键，并安装 `QAbstractNativeEventFilter` 拦截 `WM_HOTKEY` 消息后以信号方式广播；`Alt + Tab` 属于系统保留组合键无法用 `RegisterHotKey` 注册，改用 `WH_KEYBOARD_LL` 低级键盘钩子拦截后转发到主线程：

- `switchScreenKeyPressed(int screenIndex, QString keyMessage)` — `Alt + 1~7` / `Alt + 小键盘 1~7`
- `altAndRPressed()` — `Alt + R`
- `altAndF5Pressed()` — `Alt + F5`，重新加载鼠标所在屏幕当前标签页的应用（不切换标签）
- `restartKeyPressed()` — `Alt + F12`，自动重启主程序（先结束后各屏应用，再以 `--restart` 参数拉起新实例并等待单实例锁释放）
- `altTabPressed(bool reverse)` — `Alt + Tab` / `Alt + Shift + Tab`，循环切换鼠标所在屏幕的标签页
- `quitKeyPressed(QString keyMessage)` — `Alt + Q` / `Alt + F4`
- `helpKeyPressed()` — `Alt + H`，弹出/关闭快捷键说明浮层（置顶、无边框，点击浮层外部或按 Esc 关闭）
- `manageKeyPressed()` — `Alt + M`，打开/隐藏多屏控制管理界面

全局配置 `global.disableWinKey` 开启时，同一低级键盘钩子会吞掉左右 Win 键的按下与抬起事件，系统收不到 Win 键状态，因此 `Win`、`Win+D`、`Win+R` 等组合键均失效，可避免全屏展示时误触系统界面；该开关在管理界面「禁用 Win 键」中切换，保存后立即生效，无需重启主程序。

## 7. 配置文件说明

程序只读取可执行文件同目录下的 `config.json`（管理界面保存的唯一数据源）。管理界面保存时会先把已存在的 `config.json` 备份为 `config.json.bak`，再写入新的 `config.json`，不再生成 `config.ini`。若 `config.json` 不存在（旧部署升级），程序会自动读取同目录下的旧版 `config.ini` 完成一次导入，并把导入结果另存为 `config.json`，此后以 `config.json` 为准。

> `config.ini` 的配置项说明保留在下方，仅用于理解旧配置文件的含义。

### 7.1 全局配置节 `[screen]`

| 配置项                | 类型 | 默认值 | 说明                                                                                                                                                                                       |
| --------------------- | ---- | ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `count`               | int  | 0      | 屏幕配置节数量；实际生效数量以有效配置节为准                                                                                                                                               |
| `show_index`          | bool | true   | 是否显示屏幕序号（浮层 + 标签页标题前缀）                                                                                                                                                  |
| `sort_screen`         | bool | true   | 是否按屏幕几何位置排序后再与配置序号对应                                                                                                                                                   |
| `debug_mode`          | bool | false  | 调试模式：不启动任何应用，仅显示占位标签页并保留标签栏                                                                                                                                     |
| `runas_administrator` | bool | false  | 是否以管理员身份运行（非管理员时自动提权重启）                                                                                                                                             |
| `disable_win_key`     | bool | false  | 是否禁用 Windows 徽标键（`config.json` 中为 `global.disableWinKey`），开启后 Win 键及 Win+D、Win+R 等系统组合键失效                                                                        |
| `strip_app_border`    | bool | false  | 是否去掉被嵌入桌面应用的标题栏与边框（`config.json` 中为 `global.stripAppBorder`）；开启后嵌入时清除 `WS_CAPTION`/`WS_BORDER` 并重新校正窗口尺寸，使客户区铺满整块屏幕，修改后需重启主程序 |

### 7.2 屏幕配置节 `[screen-N]`（N 从 1 开始）

| 配置项         | 类型   | 说明                                                      |
| -------------- | ------ | --------------------------------------------------------- |
| `app_count`    | int    | 该屏幕下的应用数量（以实际有效应用为准重新计算）          |
| `screen_title` | string | 屏幕名称，窗口标题为 `N号屏-屏幕名称`                     |
| `tab_offset_x` | int    | 悬浮标签栏相对屏幕左边缘的水平偏移（像素，负值按 0 处理） |

### 7.3 应用配置项（`app_N_*`，N 从 1 开始）

| 配置项               | 说明                                                                               |
| -------------------- | ---------------------------------------------------------------------------------- |
| `app_N_path`         | 应用路径；`http(s)://` 开头按网页处理，包含 `.ppt` 按 PPT 处理，其余按外部程序处理 |
| `app_N_title`        | 窗口标题（用于启动后匹配目标窗口，支持部分匹配），必填                             |
| `app_N_tab_name`     | 标签页名称；留空时使用 `app_N_title`                                               |
| `app_N_startup_args` | 启动参数（按空格切分）                                                             |

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

| 快捷键                                           | 功能                                                                                   |
| ------------------------------------------------ | -------------------------------------------------------------------------------------- |
| `Alt+1` ~ `Alt+7`、`Alt+小键盘1` ~ `Alt+小键盘7` | 将第 N 号屏幕的内容切换到主屏显示（主屏与目标屏内容互换）                              |
| `Alt+R`                                          | 重新把所有窗口复位到各自的默认屏幕（不改变当前窗口内容）                               |
| `Alt+F5`                                         | 重新加载鼠标所在屏幕当前标签页的应用（外部程序重启、网页重新加载），其它标签页不受影响 |
| `Alt+M`                                          | 打开/隐藏多屏控制管理界面（软件清单、屏幕布局、拖拽分配、保存热应用）                  |
| `Alt+F12`                                        | 自动重启 MultiDisplayControl（结束各屏应用后重新启动自身，用于让需重启的配置立即生效） |
| `Alt+H`                                          | 显示/关闭快捷键说明浮层                                                                |
| `Alt+Q` / `Alt+F4`                               | 关闭所有窗口并退出程序                                                                 |

`Alt+Tab` / `Alt+Shift+Tab` 用于循环切换鼠标所在屏幕的标签页；只有该屏存在两个及以上标签时才由程序接管，否则放行给 Windows 自带的窗口切换。

切换逻辑见 [switchScreen()](file:///d:/sunzhenfeng/projects/MultiDisplayControl/main.cpp#L141-L203) 与 [reloadScreen()](file:///d:/sunzhenfeng/projects/MultiDisplayControl/main.cpp#L212-L230)：若目标屏即主屏则仅复位；否则交换两块屏幕的显示内容，其余窗口复位到默认屏幕。窗口已隐藏或主屏缺失时不做处理。

## 9. 编译与部署

### 9.1 编译

使用 Qt Creator 打开 `MultiDisplayControl.pro`，或命令行执行：

```bash
qmake MultiDisplayControl.pro
make            # Windows + MinGW 使用 mingw32-make；MSVC 使用 nmake / jom
```

构建产物输出到工程同级目录：

| 平台 / 构建类型         | 输出目录                                |
| ----------------------- | --------------------------------------- |
| Windows Release / Debug | `release` / `debug`                     |
| 麒麟（x86_64）          | `release_kylin_x86` / `debug_kylin_x86` |
| 麒麟（arm64）           | `release_kylin_arm` / `debug_kylin_arm` |

中间产物（moc/obj/rcc/ui）统一输出到 `<输出目录>/tmp`。

### 9.2 部署

可执行文件需与以下内容同目录：

1. Qt 运行库（`Qt5Core.dll`、`Qt5Gui.dll`、`Qt5Widgets.dll`、`Qt5Network.dll`、`Qt5WebEngine*`、`QtWebEngineProcess.exe` 及 `platforms/`、`resources/`、`translations/`、`styles/`、`imageformats/`、`iconengines/` 等插件目录，`release/` 目录中已随工程提供）；
2. 配置文件 `config.json`（旧部署保留 `config.ini` 时会在首次启动自动导入）；
3. WebEngine 相关 `d3dcompiler_47.dll`、`libEGL.dll`、`libGLESv2.dll`、`opengl32sw.dll` 等。

### 9.3 运行

直接运行 `MultiDisplayControl.exe`：

- 需管理员权限时会在启动阶段弹出 UAC 并自动重启自身；
- 程序为单实例运行：重复启动会提示“程序已在运行”并直接退出，避免热键注册冲突与外部程序被重复拉起；
- 配置中存在 PPT 时，启动只清理上次运行遗留的、没有可见窗口的 WPS 进程，不会结束用户正在编辑的 WPS 文档；
- 启动后按 `config.json` 为每块屏幕创建窗口并加载内容，之后进入消息循环等待热键；
- 网页缓存与运行日志（`debug.log`）生成在可执行文件目录下。

## 10. 启动流程

1. 设置 CEF 运行环境（缓存目录 `cefCache` 等），加载 `:/qss/dark.css` 全局深色主题。
2. `ConfigStore::load()` 读取配置：以 `config.json` 为唯一数据源，缺失时从旧版 `config.ini` 导入，生成 `MultiScreenConfig`。
3. 如需管理员权限且当前非管理员，则提权重启自身。
4. `DisplayRuntime::start()` 为每块屏幕创建 `ExternalAppWindow`，并行预启动外部程序后按配置顺序加载标签页，并登记到 `GlobalVariants::winList`。
5. 创建 `ManagementWindow` 管理界面（默认隐藏），把 `Alt+M` 与管理界面显隐、保存后的增量热应用连接起来。
6. 初始化全局热键监听，启动 Chrome 干扰窗口清理定时器（500ms）。
7. 500ms 后执行一次 `reloadScreen()` 复位窗口位置，进入事件循环。

## 11. 已知问题与注意事项

- **Chrome 窗口隐藏后无法重新显示**：`tabIndexChanged()` 中已标注 `TODO`，隐藏后的 Chrome 窗口需依赖 `ShowWindow()` 恢复，存在不稳定性。
- **外部程序提权未生效**：`startProcess()` 的 `runAsAdmin` 参数当前不生效，外部程序始终以当前用户权限启动（不弹 UAC）；如业务确需提权，须改用 `ShellExecuteEx` 的 `runas` 动词并接受每个应用一次 UAC 确认。
- **Linux（银河麒麟）支持尚不完整**：`ProcessUtilsLinuxImpl.h` 仅有接口声明，缺少 `.cpp` 实现，且 `axcontainer`（ActiveQt）在 Linux 下的可用性需另行处理；经确认本期暂不考虑，留作后续独立任务。
- **依赖 Win32 API**：窗口嵌入、窗口枚举、热键注册、进程路径转换等均基于 Windows API，跨平台移植需重写相关实现。
- **启动耗时**：外部程序启动后需要轮询等待窗口出现，程序整体启动时间取决于被启动应用的响应速度。
- **`.gitignore`** 会忽略 `*.exe`、`*.dll`，`release/` 下的运行库属于部署产物，需通过 `git add -f` 显式维护。
