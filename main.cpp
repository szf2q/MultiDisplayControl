#include <QApplication>

#include <windows.h>
#include <QDebug>
#include <QMap>
#include <QTimer>
#include <QVector>
#include <QWebEngineProfile>
#include <QWebEngineView>
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
    bool isTimerRunning = false;
    timer.setTimerType(Qt::PreciseTimer);

    ProcessUtils *processUtils = ProcessFactory::newInstance();
    QObject::connect(&timer, &QTimer::timeout, [processUtils, &isTimerRunning]() {
        if (isTimerRunning)
        {
            return;
        }
        isTimerRunning = true;
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
        isTimerRunning = false;
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
        ExternalAppWindow *tmpWin = winList.value(QString(i), nullptr);
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
    if (winList.value(QString(screenIndex))->isHidden() || primaryWin->isHidden())
    {
        qDebug() << "primaryWin isHidden";
        return;
    }
    if (primaryWinIndex != screenIndex)
    {
        //            qDebug() << "primaryWinIndex != num， num = " << num;
        ExternalAppWindow *tmpWin = winList.value(QString(screenIndex));
        tmpWin->setScreen(primaryWin->getDefaultScreen());
        primaryWin->setScreen(tmpWin->getDefaultScreen());
    }
    else
    {
        //            qDebug() << "set primary to default, num = " << num;
        primaryWin->setScreen(primaryWin->getDefaultScreen());
    }
    for (int i = 1; i <= screenCount; i++)
    {
        //            qDebug() << "set default screen, i = " << i;
        ExternalAppWindow *tmpWin = winList.value(QString(i), nullptr);
        if (tmpWin->isHidden())
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

void reloadScreenWatcher(GlobalKeyWatcher &watcher, QMap<QString, ExternalAppWindow *> &winList)
{
    QObject::connect(&watcher, &GlobalKeyWatcher::altAndRPressed, [&winList]() {
        qDebug() << "捕获到组合键: Alt + R，重新设置各窗口的显示位置";
        int screenCount = QGuiApplication::screens().size();
        for (int i = 1; i <= screenCount; i++)
        {
            qDebug() << "set default screen, i = " << i;
            ExternalAppWindow *tmpWin = winList.value(QString(i), nullptr);
            if (tmpWin->isHidden())
            {
                qDebug() << "app window is hidden, window title = " << tmpWin->windowTitle();
                continue;
            }
            tmpWin->setToDefaultScreen(true);
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

QMutex globalMutex;
int main(int argc, char *argv[])
{
    try
    {
        QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
        QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
        QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
        //        QLoggingCategory::setFilterRules("qt.webenginecontext.debug=true");
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--enable-logging --v=0");
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
        QMap<QString, ExternalAppWindow *> *tmpWinList = &GlobalVariants::winList;
        for (int i = 0; i < qMin(config.count, QGuiApplication::screens().count()); i++)
        {
            QTimer::singleShot(100, [=]() {
                ExternalAppWindow *win = new ExternalAppWindow(config.screenList[i]);
                win->loadApps(false);
                QString key = QString(config.screenList[i].screenIndex);
                QMutex &mutex = globalMutex;
                mutex.lock();
                tmpWinList->insert(key, win);
                mutex.unlock();
            });
        }

        Sleep(500);
        QTimer timer;
        GlobalKeyWatcher watcher;
        closeUndesiredWindowTimer(timer);
        screenSwitchWatcher(watcher, GlobalVariants::winList);
        reloadScreenWatcher(watcher, GlobalVariants::winList);
        closeAppsWatcher(watcher, GlobalVariants::winList);
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
