#include <QApplication>

#include <windows.h>
#include <QDebug>
#include <QDir>
#include <QMap>
#include <QMessageBox>
#include <QAbstractButton>
#include <QNetworkProxyFactory>
#include <QSharedPointer>
#include <QTimer>
#include <QVector>
#include <QCefConfig.h>
#include <QCefContext.h>
#include <QtConcurrent/QtConcurrent>
#include "ExternalAppWindow.h"
#include "GlobalKeyWatcher.h"
#include "GlobalVariants.h"
#include "IniConfiguration.h"
#include "ScreenConfig.h"
MultiScreenConfig loadScreenConfig(QString path)
{
    MultiScreenConfig config;
    IniConfiguration ini("config.ini", path);
    QVariant screenCount = ini.getValue("screen", "count", 0);
    QVariant showIndex = ini.getValue("screen", "show_index", true);
    QVariant sortScreen = ini.getValue("screen", "sort_screen", true);
    QVariant debugMode = ini.getValue("screen", "debug_mode", false);
    QVariant runasAdmin = ini.getValue("screen", "runas_administrator", false);
    config.count = screenCount.toInt();
    config.showIndex = showIndex.toBool();
    config.sortScreen = sortScreen.toBool();
    config.debugMode = debugMode.toBool();
    config.runAsAdministrator = runasAdmin.toBool();

    for (int screenIndex = 1; screenIndex <= screenCount.toInt(); screenIndex++)
    {
        ScreenConfig screen;
        QString section = QString("screen-%1").arg(screenIndex);
        QVariant appCount = ini.getValue(section, "app_count", 0);
        QVariant screenTitle = ini.getValue(section, "screen_title", "");
        QVariant tabOffsetX = ini.getValue(section, "tab_offset_x", 0);
        screen.appCount = appCount.toInt();
        screen.screenTitle = screenTitle.toString();
        screen.screenIndex = screenIndex;
        screen.sortScreen = config.sortScreen;
        screen.showIndex = config.showIndex;
        screen.debugMode = debugMode.toBool();
        screen.tabOffsetX = tabOffsetX.toInt();
        if (screen.tabOffsetX < 0)
        {
            screen.tabOffsetX = 0;
        }

        for (int i = 1; i <= appCount.toInt(); i++)
        {
            QString keyPrefix = QString("app_%1").arg(i);
            QVariant appPath = ini.getValue(section, keyPrefix + "_path", "");
            QVariant appTitle = ini.getValue(section, keyPrefix + "_title", "");
            QVariant appTabName = ini.getValue(section, keyPrefix + "_tab_name", "");
            QVariant appStartupArgs = ini.getValue(section, keyPrefix + "_startup_args", "");
            if (appPath.toString().isEmpty() || appTitle.toString().isEmpty())
            {
                continue;
            }
            AppConfig app;
            app.appPath = appPath.toString();
            if (!app.appPath.startsWith("https://") && !app.appPath.startsWith("http://"))
            {
                QFileInfo fileInfo(app.appPath);
                app.processName = fileInfo.fileName();
                app.appPath = app.appPath.replace("/", QDir::separator());
            }
            app.windowTitle = appTitle.toString();
            app.tabName = appTabName.toString();
            if (app.tabName.isEmpty())
            {
                app.tabName = app.windowTitle;
            }
            app.startupArgs = appStartupArgs.toString();
            screen.appList.append(app);
        }
        screen.appCount = screen.appList.size();
        if (screen.appCount > 0)
        {
            config.screenList.append(screen);
        }
    }
    config.count = config.screenList.size();
    return config;
}
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
            char text[256];
            GetWindowTextA(process.hwnd, text, sizeof(text));
            char className[256];
            GetClassNameA(process.hwnd, className, sizeof(className));
            QString tmpTitle = QString::fromLocal8Bit(text);
            QString tmpClass = QString::fromLocal8Bit(className);
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
    timer.start(500);
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
 * @brief 弹出快捷键说明界面
 * @note 大屏窗体是无边框全屏窗口，普通对话框容易被压在后面，因此这里显式置顶（Qt::WindowStaysOnTopHint）；
 *       另用静态标志防止长按热键或连发时叠加弹出多个说明窗口（对话框 exec 期间仍会收到 WM_HOTKEY）。
 */
void showShortcutHelpDialog()
{
    static bool isDialogShowing = false;
    if (isDialogShowing)
    {
        return;
    }
    isDialogShowing = true;

    QMessageBox box;
    box.setWindowTitle(QStringLiteral("快捷键说明"));
    box.setIcon(QMessageBox::Information);
    box.setWindowFlag(Qt::WindowStaysOnTopHint, true);
    box.setTextFormat(Qt::RichText);
    box.setText(QStringLiteral(
        "<p style='font-size:14px;'>快捷键在程序运行期间全局生效，无需先激活窗口。</p>"
        "<table cellspacing='0' cellpadding='6' style='font-size:14px;'>"
        "<tr><th align='left'>快捷键</th><th align='left'>功能</th></tr>"
        "<tr><td><b>Alt + H</b></td><td>显示本快捷键说明（回车 / Esc 关闭）</td></tr>"
        "<tr><td><b>Alt + R</b></td><td>重新设置各窗体显示位置（全部复位到各自的默认屏幕）</td></tr>"
        "<tr><td><b>Alt + 1 ~ 7</b><br/><b>Alt + 小键盘 1 ~ 7</b></td>"
        "<td>切换屏幕：目标屏的显示内容与主屏对调，其余屏幕复位</td></tr>"
        "<tr><td><b>Alt + Q</b><br/><b>Alt + F4</b></td><td>关闭所有窗体并退出程序</td></tr>"
        "</table>"));
    // 限定最小宽度，避免在大屏上对话框过窄导致内容换行拥挤
    box.setStyleSheet(QStringLiteral("QLabel{min-width:560px;}"));
    box.setStandardButtons(QMessageBox::Ok);
    if (QAbstractButton *okButton = box.button(QMessageBox::Ok))
    {
        okButton->setText(QStringLiteral("知道了"));
    }
    box.exec();

    isDialogShowing = false;
}

/**
 * @brief 注册 Alt+H 快捷键的响应：按下时弹出快捷键说明界面
 * @param watcher 全局热键监听器
 */
void shortcutHelpWatcher(GlobalKeyWatcher &watcher)
{
    QObject::connect(&watcher, &GlobalKeyWatcher::helpKeyPressed, []() { showShortcutHelpDialog(); });
}

QMutex globalMutex;
int main(int argc, char *argv[])
{
    try
    {
        QApplication a(argc, argv);
        QNetworkProxyFactory::setUseSystemConfiguration(false);

        setAppStyle();

        MultiScreenConfig config = loadScreenConfig(QApplication::applicationDirPath());

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
        // 关闭 GPU 加速，规避部分机型/驱动下的渲染异常；如现场需要硬件加速请删除该开关
        cefConfig.addCommandLineSwitch("disable-gpu");
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
                QString tmpPath = config.screenList[i].appList[j].appPath;
                if (tmpPath.contains(".ppt"))
                {
                    hasPpt = true;
                    break;
                }
            }
        }
        if (hasPpt)
        {
            ProcessUtils *utils = ProcessFactory::newInstance();
            utils->killProcess("wpp.exe");
            utils->killProcess("wps.exe");
            Sleep(200);
            utils->killProcess("wpscloudsvr.exe");
            utils->killProcess("wpsupdate.exe");
            delete utils;
        }

        // 第一轮：创建各屏窗体，并先把每屏的外部程序全部并行拉起（不等待窗口出现），
        // 使多屏、多程序之间的启动耗时相互重叠，而不是逐屏逐个串行等待
        QVector<ExternalAppWindow *> createdWindows;
        for (int i = 0; i < qMin(config.count, QGuiApplication::screens().count()); i++)
        {
            qDebug() << "start screen: " << i + 1 << ", app count = " << config.screenList[i].appCount;
            ExternalAppWindow *win = new ExternalAppWindow(config.screenList[i]);
            win->prestartApps();
            // 使用 QString::number 显式构造十进制字符串键，避免 QString(int) 命中 QChar(int) 重载生成不可见控制字符键
            QString key = QString::number(config.screenList[i].screenIndex);
            GlobalVariants::winList.insert(key, win);
            createdWindows.append(win);
        }

        // 第二轮：按屏依次完成网页加载与外部窗口嵌入（外部程序此时大多已就绪，等待时间大幅缩短）
        for (int i = 0; i < createdWindows.size(); i++)
        {
            createdWindows[i]->loadApps(false);
        }
        createdWindows.clear();

        Sleep(200);
        QTimer timer;
        GlobalKeyWatcher::watcher.init();
        closeUndesiredWindowTimer(timer);
        screenSwitchWatcher(GlobalKeyWatcher::watcher, GlobalVariants::winList);
        reloadScreenWatcher(GlobalKeyWatcher::watcher, GlobalVariants::winList);
        closeAppsWatcher(GlobalKeyWatcher::watcher, GlobalVariants::winList);
        shortcutHelpWatcher(GlobalKeyWatcher::watcher);

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
