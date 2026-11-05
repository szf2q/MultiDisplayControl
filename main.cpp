#include <QApplication>

#include <windows.h>
#include <QDebug>
#include <QDir>
#include <QMap>
#include <QMessageBox>
#include <QNetworkProxyFactory>
#include <QProcess>
#include <QSharedMemory>
#include <QSharedPointer>
#include <QTimer>
#include <QVector>
#include <QCefConfig.h>
#include <QCefContext.h>
#include <QCursor>
#include <QtConcurrent/QtConcurrent>
#include "ConfigStore.h"
#include "DisplayRuntime.h"
#include "ExternalAppWindow.h"
#include "GlobalKeyWatcher.h"
#include "GlobalVariants.h"
#include "ManagementWindow.h"
#include "ScreenConfig.h"
#include "ShortcutHelpDialog.h"
void setAppStyle()
{
    QFile file(":/qss/dark.css");
    if (file.exists() && file.open(QIODevice::ReadOnly))
    {
        QByteArray array = file.readAll();
        qApp->setStyleSheet(QString::fromUtf8(array));
        file.close();
    }
}

void closeUndesiredWindowTimer(QTimer &timer)
{
    // 使用共享指针在 lambda 中按值捕获运行标志，避免按引用捕获局部变量导致的悬垂引用（未定义行为）
    QSharedPointer<bool> isTimerRunning = QSharedPointer<bool>::create(false);
    timer.setTimerType(Qt::PreciseTimer);

    // 使用共享指针管理 ProcessUtils 实例，lambda 捕获后随 lambda 生命周期自动释放，避免内存泄漏
    QSharedPointer<ProcessUtils> processUtils(ProcessFactory::newInstance());
    QObject::connect(&timer, &QTimer::timeout, [processUtils, isTimerRunning]() {
        if (*isTimerRunning)
        {
            return;
        }
        *isTimerRunning = true;
        QVector<ProcessInfo> processList = processUtils->getProcessInfo("chrome.exe");
        for (ProcessInfo process : processList)
        {
            if (process.hwnd == NULL)
            {
                continue;
            }
            // 使用 Unicode 版 API 读取标题与类名：旧实现用 GetWindowTextA + fromLocal8Bit，
            // 在 UTF-8 执行字符集下中文标题会变成乱码，导致下面的匹配规则永远不生效
            wchar_t text[512] = {0};
            GetWindowTextW(process.hwnd, text, 512);
            wchar_t className[512] = {0};
            GetClassNameW(process.hwnd, className, 512);
            QString tmpTitle = QString::fromWCharArray(text);
            QString tmpClass = QString::fromWCharArray(className);
            if (tmpTitle.isEmpty())
            {
                continue;
            }

            if (tmpTitle.contains("翻译此页") || tmpTitle.contains("无法更新 Chrome") ||
                tmpTitle.contains("要恢复页面吗") || tmpClass.contains("Chrome_SystemMessageWindow"))
            {
                PostMessage(process.hwnd, WM_CLOSE, 0, 0);
                continue;
            }
        }
        processList.clear();
        *isTimerRunning = false;
    });
    // 每轮都会枚举全部 chrome 进程与顶层窗口，间隔过短会占用主线程；
    // 关闭翻译/更新弹窗对时效性要求不高，1 秒一轮足够
    timer.start(1000);
}

void switchScreen(QMap<QString, ExternalAppWindow *> &winList, const int &screenIndex, const QString &keyMessage)
{
    qDebug() << "捕获到组合键: " << keyMessage;
    int screenCount = QGuiApplication::screens().size();
    if (screenIndex < 1 || screenIndex > screenCount)
    {
        return;
    }
    //        qDebug() << "screenCount = " << screenCount;
    ExternalAppWindow *primaryWin = nullptr;
    int primaryWinIndex = -1;
    for (int i = 1; i <= screenCount; i++)
    {
        ExternalAppWindow *tmpWin = winList.value(QString::number(i), nullptr);
        if (tmpWin == nullptr)
        {
            continue;
        }
        if (tmpWin->isPrimaryScreen)
        {
            primaryWin = tmpWin;
            primaryWinIndex = tmpWin->config.screenIndex;
            break;
        }
    }
    qDebug() << "primaryWinIndex = " << primaryWinIndex;
    if (primaryWin == nullptr)
    {
        qDebug() << "primaryWin = nullptr";
        return;
    }
    // 按屏幕序号取目标窗体，先判空再访问，避免窗体未创建时解引用空指针崩溃
    ExternalAppWindow *targetWin = winList.value(QString::number(screenIndex), nullptr);
    if (targetWin == nullptr)
    {
        qDebug() << "targetWin = nullptr, screenIndex = " << screenIndex;
        return;
    }
    if (targetWin->isHidden() || primaryWin->isHidden())
    {
        qDebug() << "primaryWin isHidden";
        return;
    }
    if (primaryWinIndex != screenIndex)
    {
        //            qDebug() << "primaryWinIndex != num， num = " << num;
        targetWin->setScreen(primaryWin->getDefaultScreen());
        primaryWin->setScreen(targetWin->getDefaultScreen());
    }
    else
    {
        //            qDebug() << "set primary to default, num = " << num;
        primaryWin->setScreen(primaryWin->getDefaultScreen());
    }
    for (int i = 1; i <= screenCount; i++)
    {
        //            qDebug() << "set default screen, i = " << i;
        ExternalAppWindow *tmpWin = winList.value(QString::number(i), nullptr);
        if (tmpWin == nullptr || tmpWin->isHidden())
        {
            continue;
        }
        if (primaryWinIndex != screenIndex && (i == screenIndex || i == primaryWinIndex))
        {
            continue;
        }
        tmpWin->setToDefaultScreen();
    }
}

void screenSwitchWatcher(GlobalKeyWatcher &watcher, QMap<QString, ExternalAppWindow *> &winList)
{
    QObject::connect(&watcher, &GlobalKeyWatcher::switchScreenKeyPressed,
                     [&winList](const int &screenIndex, const QString &keyMessage) {
                         switchScreen(winList, screenIndex, keyMessage);
                     });
}
void reloadScreen(QMap<QString, ExternalAppWindow *> &winList)
{
    int screenCount = QGuiApplication::screens().size();
    for (int i = 1; i <= screenCount; i++)
    {
        qDebug() << "set default screen, i = " << i;
        ExternalAppWindow *tmpWin = winList.value(QString::number(i), nullptr);
        if (tmpWin == nullptr)
        {
            continue;
        }
        if (tmpWin->isHidden())
        {
            qDebug() << "app window is hidden, window title = " << tmpWin->windowTitle();
            continue;
        }
        tmpWin->setToDefaultScreen(true);
    }
}
void reloadScreenWatcher(GlobalKeyWatcher &watcher, QMap<QString, ExternalAppWindow *> &winList)
{
    QObject::connect(&watcher, &GlobalKeyWatcher::altAndRPressed, [&winList]() {
        qDebug() << "捕获到组合键: Alt + R，重新设置各窗口的显示位置";
        reloadScreen(winList);
    });
}

/**
 * @brief 重新加载鼠标所在屏幕中当前选中标签页的应用
 * @param winList 屏幕序号到窗口对象的映射
 *
 * 以鼠标位置确定目标屏幕，再找到当前位于该屏幕上的窗体，只重载该窗体当前选中的标签页，
 * 同屏其它标签页不受影响。这样某个应用崩溃或界面异常时，把鼠标移到该屏幕按 Alt+F5
 * 即可单独恢复该应用，无需重启整个控制程序，也不会打断同屏其它应用。
 */
void reloadCurrentScreen(QMap<QString, ExternalAppWindow *> &winList)
{
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (screen == nullptr)
    {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen == nullptr)
    {
        qDebug() << "reload current screen failed, screen is null";
        return;
    }
    for (QMap<QString, ExternalAppWindow *>::iterator it = winList.begin(); it != winList.end(); ++it)
    {
        ExternalAppWindow *win = it.value();
        if (win == nullptr || win->isHidden())
        {
            continue;
        }
        if (win->getCurrentScreen() == screen)
        {
            qDebug() << "reload current tab, window title = " << win->windowTitle();
            win->reloadCurrentTab();
            return;
        }
    }
    qDebug() << "reload current screen failed, no window on screen: " << screen->geometry();
}

/**
 * @brief 注册 Alt+F5 快捷键的响应：重新加载鼠标所在屏幕中当前标签页的应用
 * @param watcher 全局热键监听器
 * @param winList 屏幕序号到窗口对象的映射
 */
void reloadCurrentScreenWatcher(GlobalKeyWatcher &watcher, QMap<QString, ExternalAppWindow *> &winList)
{
    QObject::connect(&watcher, &GlobalKeyWatcher::altAndF5Pressed, [&winList]() {
        qDebug() << "捕获到组合键: Alt + F5，重新加载当前标签页的应用";
        reloadCurrentScreen(winList);
    });
}

/**
 * @brief 注册 Alt+Tab 快捷键的响应：切换鼠标所在屏幕的标签页
 * @param watcher 全局热键监听器
 * @param winList 屏幕序号到窗口对象的映射
 *
 * 与 Alt+F5 一致，以鼠标位置确定目标屏幕，只切换该屏幕上的标签页，
 * 避免多屏场景下影响其它屏幕的展示内容。
 */
void switchTabWatcher(GlobalKeyWatcher &watcher, QMap<QString, ExternalAppWindow *> &winList)
{
    QObject::connect(&watcher, &GlobalKeyWatcher::altTabPressed, [&winList](bool reverse) {
        QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (screen == nullptr)
        {
            screen = QGuiApplication::primaryScreen();
        }
        if (screen == nullptr)
        {
            return;
        }
        for (QMap<QString, ExternalAppWindow *>::iterator it = winList.begin(); it != winList.end(); ++it)
        {
            ExternalAppWindow *win = it.value();
            if (win == nullptr || win->isHidden())
            {
                continue;
            }
            if (win->getCurrentScreen() == screen)
            {
                win->switchToNextTab(reverse ? -1 : 1);
                return;
            }
        }
    });
}

void closeAppsWatcher(GlobalKeyWatcher &watcher, QMap<QString, ExternalAppWindow *> &winList)
{
    QObject::connect(&watcher, &GlobalKeyWatcher::quitKeyPressed, [&winList](const QString &keyMessage) {
        qDebug() << "捕获到组合键: " << keyMessage << "，关闭所有窗体，退出程序";
        qDeleteAll(winList);
        winList.clear();
        QApplication::quit();
        QApplication::exit(0);
    });
}

/**
 * @brief 注册 Alt+F12 快捷键的响应：以 --restart 参数重新拉起自身后退出当前实例
 *
 * 先拉起新实例再销毁窗口：新实例带 --restart 参数启动，会在单实例校验处等待旧实例
 * 释放共享内存后再继续；把启动放在销毁之前，可避免关闭流程耗时或异常时新实例没有起来。
 *
 * @param watcher 全局热键监听器
 * @param winList 各屏幕窗口列表
 */
void restartAppWatcher(GlobalKeyWatcher &watcher, QMap<QString, ExternalAppWindow *> &winList)
{
    QObject::connect(&watcher, &GlobalKeyWatcher::restartKeyPressed, [&winList]() {
        qDebug() << "捕获到组合键 Alt+F12，自动重启 MultiDisplayControl";
        const QString appPath = QApplication::applicationFilePath();
        const QString workDir = QApplication::applicationDirPath();
        if (!QProcess::startDetached(appPath, QStringList() << QStringLiteral("--restart"), workDir))
        {
            // 新实例没有起来时保持当前实例运行，避免用户直接失去程序
            qWarning() << "restart MultiDisplayControl failed";
            return;
        }
        // 旧实例正常退出：销毁各屏窗口会按既有流程结束其启动的外部程序，
        // 新实例随后重新启动并嵌入这些应用
        qDeleteAll(winList);
        winList.clear();
        QApplication::quit();
        QApplication::exit(0);
    });
}

/**
 * @brief 注册 Alt+H 快捷键的响应：首次按下弹出快捷键说明浮层，再次按下隐藏
 * @param watcher 全局热键监听器
 * @param helpDialog 快捷键说明浮层，生命周期由 main() 的栈对象管理
 */
void shortcutHelpWatcher(GlobalKeyWatcher &watcher, ShortcutHelpDialog &helpDialog)
{
    QObject::connect(&watcher, &GlobalKeyWatcher::helpKeyPressed, [&helpDialog]() { helpDialog.toggle(); });
}

/**
 * @brief 结束上次运行遗留的、没有可见窗口的 WPS 进程
 *
 * 播放 PPT 依赖 WPS 的 COM 实例：上一次异常退出可能残留没有界面的 wpp/wps 进程，
 * 新放映请求会挂到这些残留实例上导致放映失败。
 * 旧实现在启动时按进程名无条件结束 wpp/wps/wpscloudsvr/wpsupdate，
 * 会把用户正在编辑的 WPS 文档一并杀掉，属于破坏性操作；
 * 这里只结束“没有任何可见顶层窗口”的残留实例，用户已打开的文档不受影响。
 *
 * @param processUtils 进程工具实例，为空时直接返回
 */
void killStaleWpsInstances(ProcessUtils *processUtils)
{
    if (processUtils == nullptr)
    {
        return;
    }
    const QStringList processNames = QStringList() << "wpp.exe" << "wps.exe";
    for (int i = 0; i < processNames.size(); i++)
    {
        const QVector<ProcessInfo> processList = processUtils->getProcessInfo(processNames[i]);
        for (int j = 0; j < processList.size(); j++)
        {
            // hwnd 为 0 表示该进程没有可见的顶层窗口，即上次运行残留的后台实例
            if (processList[j].hwnd != 0 || processList[j].pid == 0)
            {
                continue;
            }
            qDebug() << "kill stale wps process, name = " << processNames[i] << ", pid = " << processList[j].pid;
            processUtils->killProcess(processList[j].pid);
        }
    }
}

int main(int argc, char *argv[])
{
    try
    {
        QApplication a(argc, argv);
        QNetworkProxyFactory::setUseSystemConfiguration(false);

        setAppStyle();

        // 配置统一由 ConfigStore 读取：只使用 config.json，旧版 config.ini 仅在缺失时用于导入
        MultiScreenConfig config;
        QString configError;
        if (!ConfigStore::load(QApplication::applicationDirPath(), &config, &configError))
        {
            qDebug() << "load config failed: " << configError;
            return -1;
        }

        if (config.runAsAdministrator)
        {
            ProcessUtils *processUtils = ProcessFactory::newInstance();
            int ret = processUtils->runAsAdministrators();
            delete processUtils;
            if (ret >= 0)
            {
                return ret;
            }
        }

        // 单实例保护：多开会让全局热键注册冲突、同一外部程序被重复拉起，
        // 且两个实例会互相结束对方启动的进程。必须放在提权重启之后：
        // 提权时父进程会立即退出，若提前占用共享内存，被拉起的提权子进程会误判为“已运行”。
        QSharedMemory singleInstanceGuard("MultiDisplayControl.SingleInstance");
        bool guardCreated = singleInstanceGuard.create(1);
        if (!guardCreated && a.arguments().contains(QStringLiteral("--restart")))
        {
            // 自动重启时新旧实例会短暂并存：旧实例需要先结束外部程序再退出，
            // 这里轮询等待共享内存释放（最多 15 秒），避免新实例误判“程序已在运行”
            for (int i = 0; i < 75 && !guardCreated; i++)
            {
                Sleep(200);
                guardCreated = singleInstanceGuard.create(1);
            }
        }
        if (!guardCreated)
        {
            qDebug() << "another instance is running, error = " << singleInstanceGuard.errorString();
            QMessageBox::warning(nullptr, QString::fromUtf8("多屏控制"),
                                 QString::fromUtf8("程序已在运行，请勿重复启动。"));
            return 0;
        }

        // ==================== CEF(QCefView) 初始化 ====================
        // 约束说明：
        //   1. 全进程只能有一个 QCefContext，且必须在创建任何 QCefView 之前初始化、
        //      在所有 QCefView 销毁之后析构，因此声明在 main 作用域内，生命周期覆盖整个事件循环；
        //   2. CEF 只支持全进程唯一的缓存目录，所有网页共用（不再按应用/按屏隔离缓存与 Cookie）；
        //   3. 为兼容 Windows 7，CEF 版本需选用 Chromium <= 109 的发行版。
        QString cefCachePath = QDir(QApplication::applicationDirPath()).filePath("cefCache");
        QDir().mkpath(cefCachePath);
        QCefConfig cefConfig;
        cefConfig.setLogLevel(QCefConfig::LOGSEVERITY_DEFAULT);
        cefConfig.setCachePath(QDir::toNativeSeparators(cefCachePath));
        // 默认启用 GPU 加速：多屏/4K 分辨率下关闭 GPU 会让 CEF 退化为软件渲染，
        // 多个网页同时展示时页面明显卡顿。个别机型/驱动存在黑屏等渲染异常时，
        // 可在管理界面勾选「禁用 GPU 加速」后重启主程序。
        if (config.disableGpuAcceleration)
        {
            cefConfig.addCommandLineSwitch("disable-gpu");
        }
        // 关闭离屏渲染（OSR），改用窗口模式：
        //   OSR 模式下键盘、鼠标与输入法事件都需要由 Qt 转发给 CEF，实测该链路下网页无法
        //   获得键盘输入，且在输入法组合（ImeSetComposition）时会导致 libcef 触发断点崩溃；
        //   窗口模式下由 Chromium 自己创建原生子窗口并直接处理键盘与输入法，输入更可靠。
        cefConfig.setWindowlessRenderingEnabled(false);
        // 远程调试端口默认关闭，需要调试网页时临时打开即可（注意：开启后本机任意进程可接管页面）
        // cefConfig.setRemoteDebuggingPort(9222);
        QCefContext cefContext(&a, argc, argv, &cefConfig);
        // =============================================================

        qDebug() << "config.count() = " << config.count;
        bool hasPpt = false;
        for (int i = 0; i < qMin(config.count, QGuiApplication::screens().count()); i++)
        {
            for (int j = 0; j < config.screenList[i].appCount; j++)
            {
                if (isPptAppPath(config.screenList[i].appList[j].appPath))
                {
                    hasPpt = true;
                    break;
                }
            }
        }
        if (hasPpt)
        {
            // 只清理上次运行残留的无界面 WPS 实例，不再按进程名无条件结束，
            // 避免误杀用户正在编辑的文档
            ProcessUtils *utils = ProcessFactory::newInstance();
            killStaleWpsInstances(utils);
            delete utils;
            Sleep(200);
        }

        // 由运行时统一创建各屏窗口：先并行预启动全部外部程序，再按配置顺序完成加载与嵌入
        DisplayRuntime runtime;
        runtime.start(config);

        Sleep(200);
        QTimer timer;
        // 快捷键说明浮层：栈对象，生命周期覆盖整个事件循环，避免堆对象的泄漏与悬挂
        ShortcutHelpDialog shortcutHelpDialog;
        GlobalKeyWatcher::watcher.init();
        // 按配置决定是否屏蔽 Win 键：必须在钩子安装之后设置，才能立即影响后续按键事件
        GlobalKeyWatcher::watcher.setWinKeyDisabled(config.disableWinKey);
        closeUndesiredWindowTimer(timer);
        screenSwitchWatcher(GlobalKeyWatcher::watcher, GlobalVariants::winList);
        reloadScreenWatcher(GlobalKeyWatcher::watcher, GlobalVariants::winList);
        reloadCurrentScreenWatcher(GlobalKeyWatcher::watcher, GlobalVariants::winList);
        switchTabWatcher(GlobalKeyWatcher::watcher, GlobalVariants::winList);
        closeAppsWatcher(GlobalKeyWatcher::watcher, GlobalVariants::winList);
        restartAppWatcher(GlobalKeyWatcher::watcher, GlobalVariants::winList);
        shortcutHelpWatcher(GlobalKeyWatcher::watcher, shortcutHelpDialog);

        // 管理界面：Alt+M 显示/隐藏；保存后调用运行时做增量热更新，并把结果回传到状态栏
        ManagementWindow manager;
        manager.setConfig(config);
        QObject::connect(&GlobalKeyWatcher::watcher, &GlobalKeyWatcher::manageKeyPressed, &manager,
                         [&manager]() { manager.toggle(); });
        QObject::connect(&manager, &ManagementWindow::applyRequested, &manager,
                         [&runtime, &manager](const MultiScreenConfig &newConfig) {
                             QStringList warnings;
                             runtime.apply(newConfig, &warnings);
                             // Win 键屏蔽开关保存后立即生效，无需重启主程序
                             GlobalKeyWatcher::watcher.setWinKeyDisabled(newConfig.disableWinKey);
                             manager.notifyApplyResult(warnings);
                         });
        // 管理界面选中某块屏幕时，对应大屏四周显示红色边框，方便多屏定位
        QObject::connect(&manager, &ManagementWindow::screenHighlightChanged, &runtime,
                         [&runtime](int screenIndex) { runtime.highlightScreen(screenIndex); });

        QTimer::singleShot(500, [=]() { reloadScreen(GlobalVariants::winList); });
        return a.exec();
    }
    catch (const std::exception &e)
    {
        qDebug() << "Exception caught: " << e.what();
    }
    catch (...)
    {
        qDebug() << "Unknown exception";
    }
    return -1;
}
