#include "ExternalAppWindow.h"

#include "ui_ExternalAppWindow.h"

ExternalAppWindow::ExternalAppWindow(ScreenConfig config, QWidget *parent) :
    QMainWindow(parent), ui(new Ui::ExternalAppWindow)
{
    ui->setupUi(this);
    this->setWindowFlags(Qt::FramelessWindowHint);
    this->setAttribute(Qt::WA_NativeWindow);
    this->setContentsMargins(0, 0, 0, 0);
    this->centralWidget()->setContentsMargins(0, 0, 0, 0);
    this->setMinimumSize(QGuiApplication::primaryScreen()->geometry().width(),
                         QGuiApplication::primaryScreen()->geometry().height());

    this->config = config;
    if (config.screenTitle.isEmpty())
    {
        this->setWindowTitle(QString("%1号屏").arg(config.screenIndex));
    }
    else
    {
        this->setWindowTitle(QString("%1号屏-%2").arg(config.screenIndex).arg(config.screenTitle));
    }

    this->screenList = QGuiApplication::screens();
    qDebug() << "this->screenList.size() = " << this->screenList.size();

    ui->tabWidget->setContentsMargins(0, 0, 0, 0);
    ui->tabWidget->setDocumentMode(true);
    ui->tabWidget->setMinimumSize(QGuiApplication::primaryScreen()->geometry().width(),
                                  QGuiApplication::primaryScreen()->geometry().height());
    ui->tabWidget->tabBar()->setDocumentMode(true);
    if (!config.debugMode)
    {
        ui->tabWidget->tabBar()->hide();
    }
    ui->tabWidget->clear();
    qDebug() << "clear tabwidget completed";

    tabBarWidget = new QWidget(this);
    tabBarWidget->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    tabBarWidget->setAttribute(Qt::WA_TranslucentBackground);
    tabBarWidget->setAttribute(Qt::WA_AlwaysStackOnTop);
    tabBarWidget->setMinimumSize(500, 50);
    tabBar = new QTabBar(tabBarWidget);
    tabBar->setAttribute(Qt::WA_TranslucentBackground);
    tabBar->setAttribute(Qt::WA_AlwaysStackOnTop);
    tabBar->setStyleSheet("border:none;color:red;font-family:微软雅黑;font-weight:bold;");
    tabBar->setElideMode(Qt::ElideNone);
    tabBar->setUsesScrollButtons(false);
    tabBar->setExpanding(false);
    tabBar->setGeometry(tabBarWidget->geometry().x(), tabBarWidget->geometry().y(), 500, 35);
    tabBarWidget->show();

    if (config.showIndex)
    {
        screenIndexWidget = new QWidget(this);
        screenIndexWidget->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        screenIndexWidget->setAttribute(Qt::WA_TranslucentBackground);
        screenIndexWidget->setAttribute(Qt::WA_AlwaysStackOnTop);
        screenIndexWidget->setMinimumSize(90, 90);
        screenIndexLbl = new QLabel("", screenIndexWidget);
        screenIndexLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
        screenIndexLbl->setAttribute(Qt::WA_AlwaysStackOnTop);
        screenIndexLbl->setStyleSheet("color:#11C0FE;"
                                      "font-size:48px;"
                                      "font-family:微软雅黑;"
                                      "font-weight:bold;"
                                      "background-color:rgba(21,191,251,80);"
                                      "border-radius:45px;");
        screenIndexLbl->setAlignment(Qt::AlignCenter);
        screenIndexLbl->setMinimumSize(90, 90);
        screenIndexWidget->show();
    }

    processUtils = ProcessFactory::newInstance();
    this->initWindow();
}

void ExternalAppWindow::initWindow()
{
    if (config.sortScreen)
    {
        std::sort(screenList.begin(), screenList.end(), [](QScreen *a, QScreen *b) {
            bool isPrimaryA = (a == QGuiApplication::primaryScreen());
            bool isPrimaryB = (b == QGuiApplication::primaryScreen());
            if (isPrimaryA)
            {
                return false;
            }
            if (isPrimaryB)
            {
                return true;
            }

            if (abs(a->geometry().x() - b->geometry().x()) < 100)
            {
                return a->geometry().y() < b->geometry().y();
            }
            else
            {
                return a->geometry().x() < b->geometry().x();
            }
        });
    }

    if (this->defaultScreen == nullptr)
    {
        this->defaultScreen = screenList[config.screenIndex - 1];
        this->isPrimaryScreen = (screenList[config.screenIndex - 1] == QGuiApplication::primaryScreen());
    }
    setScreen(config.screenIndex);
    this->displayScreenIndex();
}
void ExternalAppWindow::displayScreenIndex()
{
    if (config.showIndex)
    {
        screenIndexLbl->setText(QString("%1").arg(this->config.screenIndex));
    }
}
void ExternalAppWindow::setScreen(int screenIndex)
{
    if (screenIndex <= 0)
    {
        screenIndex = 1;
    }
    if (screenList.size() < screenIndex)
    {
        qDebug() << "screenList.size() = " << screenList.size() << "screenIndex = " << screenIndex;
        return;
    }
    setScreen(screenList[screenIndex - 1]);
}
void ExternalAppWindow::setScreen(QScreen *screen, bool force)
{
    if (screen == nullptr)
    {
        qDebug() << "setScreen, input screen is null";
        return;
    }
    if (!force && this->currentScreen == screen)
    {
        qDebug() << "setScreen, input screen == current screen and force==false";
        return;
    }

    this->showNormal();
    Sleep(50);
    QRect rect = screen->geometry();
    this->move(rect.topLeft());
    this->currentScreen = screen;
    Sleep(50);
    this->showFullScreen();

    for (int i = 0; i < externalAppList.size(); i++)
    {
        if (externalAppList[i].externalWindow != nullptr)
        {
            if (externalAppList[i].tabName == tabBar->tabText(tabBar->currentIndex()))
            {
                externalAppList[i].externalWindow->setGeometry(0, 0, rect.width(), rect.height());
                break;
            }
        }
    }
    Sleep(50);
    if (this->tabBarWidget != nullptr)
    {
        tabBarWidget->setGeometry(rect.x() + config.tabOffsetX, rect.y() + 10, 500, 50);
    }
    if (config.showIndex && screenIndexWidget != nullptr)
    {
        screenIndexWidget->move(rect.right() - 125, rect.top() + 25);
    }
}
void ExternalAppWindow::setToDefaultScreen(bool force)
{
    setScreen(this->defaultScreen, force);
}
QScreen *ExternalAppWindow::getDefaultScreen()
{
    return this->defaultScreen;
}
QScreen *ExternalAppWindow::getCurrentScreen()
{
    return this->currentScreen;
}
void ExternalAppWindow::loadApps(bool isReload)
{
    qDebug() << "load apps ...";
    if (!config.debugMode)
    {
        for (int i = 1; i <= config.appCount; i++)
        {
            if (config.appList[i - 1].appPath.startsWith("http://") ||
                config.appList[i - 1].appPath.startsWith("https://"))
            {
                loadWebPage(config.appList[i - 1]);
            }
            else if (config.appList[i - 1].appPath.contains(".ppt"))
            {
                startAndPlayPpt(config.appList[i - 1]);
            }
            else
            {
                startExternalProgram(config.appList[i - 1], isReload);
            }
        }
    }
    Sleep(1000);
    qDebug() << "ui->tabWidget->count() = " << ui->tabWidget->count();
    if (ui->tabWidget->count() <= 0 && !config.debugMode)
    {
        this->hide();
    }
    this->initTabWidget();
}
void ExternalAppWindow::initTabWidget()
{
    if (ui->tabWidget->count() <= 0 && config.debugMode)
    {
        qDebug() << "init tabwidget for debug ...";
        qDebug() << "this->currentScreen = " << this->currentScreen;
        QRect rect = this->currentScreen->geometry();
        qDebug() << "rect = " << rect;
        QString title = QString("(%1,%2)").arg(rect.x()).arg(rect.y());
        QWidget *widget = new QWidget();
        QLabel *label = new QLabel();
        label->setText(QString("显示屏-%1 (%2, %3) (%4, %5)")
                           .arg(this->config.screenIndex)
                           .arg(rect.x())
                           .arg(rect.y())
                           .arg(rect.width())
                           .arg(rect.height()));
        label->setStyleSheet("font-size:48px;"
                             "background:gray;");
        label->setGeometry(0, 0, rect.width(), rect.height());
        label->setAlignment(Qt::AlignCenter);
        label->setParent(widget);
        if (config.showIndex)
        {
            QString primaryFlag = this->isPrimaryScreen ? "*" : "";
            QString tabTitle = QString("%1%2-%3").arg(primaryFlag).arg(config.screenIndex).arg(title);
            ui->tabWidget->addTab(widget, tabTitle);
            tabBar->addTab(tabTitle);
        }
        else
        {
            ui->tabWidget->addTab(widget, title);
            tabBar->addTab(title);
        }
    }

    if (ui->tabWidget->count() <= 1 && config.debugMode == false)
    {
        this->tabBarWidget->hide();
        return;
    }

    QObject::connect(tabBar, &QTabBar::currentChanged, [=](int index) {
        if (index < 0 || index >= ui->tabWidget->count())
        {
            return;
        }
        qDebug() << "QTabBar::currentChanged, index = " << index;
        ui->tabWidget->setCurrentIndex(index);
        ui->tabWidget->update();
        ui->tabWidget->adjustSize();

        for (int i = 0; i < externalAppList.size(); i++)
        {
            if (externalAppList[i].externalWindow != nullptr)
            {
                if (externalAppList[i].tabName == tabBar->tabText(tabBar->currentIndex()))
                {
                    externalAppList[i].externalWindow->showFullScreen();
                    break;
                }
                else
                {
                    externalAppList[i].externalWindow->hide();
                }
            }
        }
    });
}
void ExternalAppWindow::readLoadApps()
{
    this->externalAppList.clear();
    ui->tabWidget->clear();
    loadApps(true);
}

void ExternalAppWindow::startExternalProgram(AppConfig &app, bool isReload)
{
    if (!isReload)
    {
        startProcess(app);
    }
    ProcessInfo process = findProcessInfo(app);
    if (!process.hwnd)
    {
        return;
    }
    //获取待嵌入的外部应用窗口
    qDebug() << "process.hwnd = " << process.hwnd << ", wid = " << (WId)process.hwnd;
    QWindow *externanlWindow = QWindow::fromWinId((WId)process.hwnd);
    externanlWindow->setFlags(Qt::FramelessWindowHint | Qt::Tool);
    qDebug() << "externanlWindow = " << externanlWindow << ", wid = " << externanlWindow->winId();

    if (process.processName == "chrome.exe")
    {
        externanlWindow->setParent(this->windowHandle());
        externanlWindow->setGeometry(0, 0, this->geometry().width(), this->geometry().height());
        process.externalWindow = externanlWindow;
        QString tabTitle = app.tabName;
        if (config.showIndex)
        {
            tabTitle =
                QString("%1%2-%3").arg(this->isPrimaryScreen ? "*" : "").arg(config.screenIndex).arg(app.tabName);
        }
        tabBar->addTab(tabTitle);
        ui->tabWidget->addTab(new QTabWidget(), tabTitle);
        ui->tabWidget->setCurrentIndex(0);
        app.tabIndex = tabBar->count() - 1;
        process.tabName = app.tabName;
        externalAppList.append(process);
        connect(qApp, &QApplication::aboutToQuit, [=]() { delete externanlWindow; });
        return;
    }
    //创建嵌入的容器窗口
    QWidget *container = QWidget::createWindowContainer(externanlWindow, this, Qt::FramelessWindowHint | Qt::Tool);
    container->setFocusPolicy(Qt::StrongFocus);
    LONG_PTR style = GetWindowLongPtr(process.hwnd, GWL_STYLE);
    style |= WS_CHILD;
    style &= ~WS_CAPTION;
    style &= ~WS_BORDER;
    style &= ~WS_POPUP;
    SetWindowLongPtr(process.hwnd, GWL_STYLE, style);
    //设置外部应用窗口的尺寸到最大
    QRect rect = currentScreen->geometry();
    externanlWindow->setGeometry(0, 0, rect.width(), rect.height());
    addWindowToTabWidget(container, app);
    connect(qApp, &QApplication::aboutToQuit, [=]() {
        delete externanlWindow;
        delete container;
    });
}

void ExternalAppWindow::loadWebPage(AppConfig &app)
{
    QStringList argList = app.startupArgs.split(" ");
    if (argList.size() <= 0)
    {
        return;
    }
    QString url = app.appPath;
    qDebug() << "url = " << url;
    QWebEngineView *view = new QWebEngineView();
    //使用内存缓存
    QString cachePath = QApplication::applicationDirPath().replace("/", QDir::separator()) + QDir::separator() +
                        "cache" + QDir::separator() + app.windowTitle;
    qDebug() << "cachePath = " << cachePath;
    QDir().mkpath(cachePath);
    QWebEngineProfile *profile = new QWebEngineProfile(QString("%1-%2").arg(this->config.screenIndex).arg(app.tabName));
    profile->setCachePath(cachePath);
    profile->setPersistentStoragePath(cachePath);
    profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
    //禁用持久化Cookie
    profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
    view->setPage(new QWebEnginePage(profile, view));
    view->load(QUrl(url));
    //禁用自动加载图片
    view->settings()->setAttribute(QWebEngineSettings::AutoLoadImages, true);
    //确保JS可用，懒加载依赖JS
    view->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    //禁用滚动条
    view->settings()->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, false);
    view->settings()->setAttribute(QWebEngineSettings::ShowScrollBars, false);
    // 启用2D加速
    // view->settings()->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled, false);
    // 启用WebGL
    // view->settings()->setAttribute(QWebEngineSettings::WebGLEnabled, true);
    // 禁用插件
    // view->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, false);
    view->setStyleSheet("border:none;margin:0px;padding:0px;background:green;");
    QRect rect = currentScreen->geometry();
    view->resize(rect.width(), rect.height());

    QGraphicsScene *scene = new QGraphicsScene(this);
    scene->setSceneRect(0, 0, rect.width(), rect.height());
    scene->addWidget(view);
    QGraphicsView *gpView = new QGraphicsView(scene);
    gpView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    gpView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    gpView->setStyleSheet("border:none;margin:0px;padding:0px;background:red;");
    // 设置容器窗口的尺寸到最大
    gpView->resize(rect.width(), rect.height());
    gpView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    gpView->setToolTip(app.appPath);
    gpView->setStyleSheet("border:none;background-color:blue;background:blue;");
    gpView->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    addWindowToTabWidget(gpView, app);
    gpView->viewport()->update();
    scene->update();
    connect(qApp, &QApplication::aboutToQuit, [view, scene, gpView]() {
        delete gpView;
        delete scene;
        delete view;
    });
}

void ExternalAppWindow::startAndPlayPpt(AppConfig &app)
{
    qDebug("开始播放PPT");
    QAxWidget *ppt = new QAxWidget();
    ppt->setControl("KWPP.Application");
    if (ppt->isNull())
    {
        qDebug() << "未安装WPS";
        ppt->setControl("PowerPoint.Application");
        if (ppt->isNull())
        {
            qDebug() << "未安装PowerPoint";
            return;
        }
    }
    ppt->setProperty("Visible", true);
    ppt->setProperty("DisplayAlerts", false);
    ppt->setFocusPolicy(Qt::StrongFocus);

    QAxObject *presentations = ppt->querySubObject("Presentations");
    QAxObject *presentation =
        presentations->querySubObject("Open(QString)", QVariant(QDir::toNativeSeparators(app.appPath)));
    if (!presentation)
    {
        qDebug() << "打开ppt失败, path = " << app.appPath;
        ppt->dynamicCall("Quit()");
        return;
    }
    QAxObject *slideShowSettings = presentation->querySubObject("SlideShowSettings");
    if (slideShowSettings->isNull())
    {
        qDebug() << "执行SlideShowSettings失败";
        presentation->dynamicCall("Close()");
        ppt->dynamicCall("Quit()");
        return;
    }
    slideShowSettings->setProperty("ShowPresenterView", false);
    slideShowSettings->setProperty("ShowMediaControls", false);
    slideShowSettings->setProperty("LoopUntilStopped", true);
    slideShowSettings->setProperty("AdvanceMode", 1);
    slideShowSettings->setProperty("ShowWithAnimation", true);
    slideShowSettings->setProperty("ShowWithNarration", false);
    slideShowSettings->setProperty("ShowWithSlideShow", true);

    playPpt(ppt, presentation, slideShowSettings, app);

    QTimer *timer = new QTimer();
    connect(timer, &QTimer::timeout, [=]() {
        QAxObject *tmpWins = ppt->querySubObject("SlideShowWindows");
        int winCount = tmpWins->property("Count").toInt();
        if (winCount > 0)
        {
            return;
        }
        AppConfig tmpApp = app;
        playPpt(ppt, presentation, slideShowSettings, tmpApp);
    });
    timer->start(100);
}

void ExternalAppWindow::playPpt(QAxWidget *ppt, QAxObject *presentation, QAxObject *slideShowSettings, AppConfig &app)
{
    QAxObject *slideShowWindow = slideShowSettings->querySubObject("Run");
    if (slideShowWindow->isNull())
    {
        qDebug() << "执行Run失败";
        presentation->dynamicCall("Close()");
        ppt->dynamicCall("Quit()");
        return;
    }
    QAxObject *view = slideShowWindow->querySubObject("View");
    view->setProperty("ViewType", 1);
    Sleep(1000);
    connect(qApp, &QApplication::aboutToQuit, [=]() {
        presentation->dynamicCall("Close()");
        ppt->dynamicCall("Quit()");
    });

    QVector<ProcessInfo> processList;
    int count = 0;
    while (count < 10)
    {
        processList = processUtils->getProcessInfoByWindowTitle(app.windowTitle);
        if (processList.size() == 0)
        {
            Sleep(100);
            count++;
            continue;
        }
        break;
    }
    if (processList.size() == 0)
    {
        qDebug() << "playPpt, processList.size() = " << processList.size();
        presentation->dynamicCall("Close()");
        ppt->dynamicCall("Quit()");
        return;
    }
    ProcessInfo process = processList[0];
    process.tabName = app.tabName;
    externalAppList.append(process);
    processList.clear();
    //获取待嵌入的外部应用窗口
    QWindow *pptWindow = QWindow::fromWinId((WId)process.hwnd);
    pptWindow->setFlags(Qt::FramelessWindowHint | Qt::Tool);
    //创建嵌入的容器窗口
    QWidget *container = QWidget::createWindowContainer(pptWindow);
    LONG_PTR style = GetWindowLongPtr(process.hwnd, GWL_STYLE);
    style |= WS_CHILD;
    style &= ~WS_CAPTION;
    style &= ~WS_BORDER;
    style &= ~WS_POPUP;
    SetWindowLongPtr(process.hwnd, GWL_STYLE, style);
    //设置外部应用窗口的尺寸到最大
    QRect rect = currentScreen->geometry();
    pptWindow->setGeometry(0, 0, rect.width(), rect.height());
    addWindowToTabWidget(container, app);
}

void ExternalAppWindow::addWindowToTabWidget(QWidget *widget, AppConfig &app)
{
    QRect rect = currentScreen->geometry();
    //设置容器窗口的尺寸到最大
    widget->setGeometry(0, 0, rect.width(), rect.height());
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    widget->setToolTip(app.appPath);
    widget->setStyleSheet("border:none;background-color:blue;background:blue;");
    //将容器窗口添加到Tab中
    QString tabTitle = app.tabName;
    if (config.showIndex)
    {
        tabTitle = QString("%1%2-%3").arg(this->isPrimaryScreen ? "*" : "").arg(config.screenIndex).arg(app.tabName);
    }
    qDebug() << "tabTitle = " << tabTitle;
    int findTabIndex = -1;
    for (int i = 0; i < ui->tabWidget->count(); i++)
    {
        if (ui->tabWidget->tabText(i) == tabTitle)
        {
            findTabIndex = i;
            break;
        }
    }
    qDebug() << "findTabIndex = " << findTabIndex;
    if (findTabIndex >= 0)
    {
        ui->tabWidget->removeTab(findTabIndex);
        ui->tabWidget->insertTab(findTabIndex, widget, tabTitle);
    }
    else
    {
        ui->tabWidget->addTab(widget, tabTitle);
        tabBar->addTab(tabTitle);
        ui->tabWidget->setCurrentIndex(0);
        for (int i = 0; i < config.appList.count(); i++)
        {
            if (config.appList[i].appPath == app.appPath)
            {
                qDebug() << "app index = " << i + 1;
                config.appList[i].tabIndex = ui->tabWidget->count() - 1;
                break;
            }
        }
    }
}
bool ExternalAppWindow::startProcess(AppConfig &app)
{
    QVector<ProcessInfo> processList;
    if (app.processName.contains("chrome.exe"))
    {
        processList = processUtils->getProcessInfo(app.processName, app.windowTitle);
    }
    else
    {
        ProcessInfo process = processUtils->getProcessInfoByPath(app.appPath);
        if (!process.processPath.isEmpty())
        {
            processUtils->killProcess(process.pid);
        }
    }
    if (processList.size() == 0)
    {
        if (!processUtils->startProcess(app.appPath, app.startupArgs, true))
        {
            processList.clear();
            return false;
        }
    }
    processList.clear();
    return true;
}

ProcessInfo ExternalAppWindow::findProcessInfo(AppConfig &app)
{
    QVector<ProcessInfo> processList;
    ProcessInfo process;
    int count = 0;
    bool isFind = false;
    while (count < 30 * 100)
    {
        processList.clear();
        processList = processUtils->getProcessInfo(app.processName, app.windowTitle);
        if (processList.size() == 0)
        {
            count++;
            Sleep(100);
            continue;
        }
        process = processList[processList.size() - 1];
        isFind = true;
        break;
    }
    if (!isFind)
    {
        qDebug() << "get process list by process name and window title failed, processName = " << app.processName
                 << ", processTitle = " << app.windowTitle;
        processList.clear();
        return process;
    }

    process.processName = app.processName;
    process.processPath = app.appPath;
    process.tabName = app.tabName;
    externalAppList.append(process);
    qDebug() << "pid = " << process.pid << ", HWND = " << process.hwnd << ", processName = " << process.processName
             << ", titleName = " << process.titleName << ", className = " << process.className
             << ", processPath = " << process.processPath;
    processList.clear();
    return process;
}
ExternalAppWindow::~ExternalAppWindow()
{
    qDebug() << "MainWindow destructor";
    delete ui;
    for (int i = externalAppList.size() - 1; i >= 0; i--)
    {
        ProcessInfo process = externalAppList[i];
        if (process.pid > 0)
        {
            qDebug() << "kill " << process.processName << ", pid= " << process.pid;
            processUtils->killProcess(process.pid);
        }
        if (process.externalWindow != nullptr)
        {
            process.externalWindow->close();
            delete process.externalWindow;
        }
    }
    externalAppList.clear();
    screenList.clear();
    delete tabBar;
    delete tabBarWidget;
    delete screenIndexLbl;
    if (screenIndexWidget != nullptr)
    {
        delete screenIndexWidget;
    }
    delete processUtils;
}
