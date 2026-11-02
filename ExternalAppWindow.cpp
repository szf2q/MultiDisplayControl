#include "ExternalAppWindow.h"

#include <QTimer>
#include <QUrl>

#include "ui_ExternalAppWindow.h"

#ifndef USE_QCEFVIEW
#    include <QGraphicsScene>
#    include <QGraphicsView>
#    include <QWebEnginePage>
#    include <QWebEngineProfile>
#    include <QWebEngineSettings>
#    include <QWebEngineView>
#endif

ExternalAppWindow::ExternalAppWindow(ScreenConfig config, QWidget *parent) :
    QMainWindow(parent), ui(new Ui::ExternalAppWindow)
{
    ui->setupUi(this);
    this->setWindowFlags(Qt::FramelessWindowHint);
    this->setAttribute(Qt::WA_NativeWindow);
    this->setContentsMargins(0, 0, 0, 0);
    this->centralWidget()->setContentsMargins(0, 0, 0, 0);
    // 注意：这里不再按主屏分辨率设置最小尺寸。多屏场景下副屏分辨率可能小于主屏，
    // 若固定按主屏尺寸设置最小尺寸会导致副屏上的窗体无法完整铺满/被撑破。

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
        // 屏幕索引可能大于实际可用屏幕数量（例如配置了多屏但实际只接入一屏），
        // 这里先钳制到合法区间，避免数组越界访问导致崩溃
        int validScreenIndex = config.screenIndex;
        if (validScreenIndex < 1)
        {
            validScreenIndex = 1;
        }
        if (validScreenIndex > screenList.size())
        {
            qDebug() << "initWindow, screenIndex out of range, screenIndex = " << config.screenIndex
                     << ", screenList.size() = " << screenList.size();
            validScreenIndex = screenList.size();
        }
        this->defaultScreen = screenList[validScreenIndex - 1];
        this->isPrimaryScreen = (this->defaultScreen == QGuiApplication::primaryScreen());
        // 使用钳制后的合法索引设置屏幕，保证与 defaultScreen 保持一致
        setScreen(validScreenIndex);
    }
    else
    {
        setScreen(config.screenIndex);
    }
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
        // 第一阶段：先把本屏所有外部程序并行拉起（不等待窗口出现），
        // 使多个程序的启动耗时相互重叠，而不是逐个“启动 + 等待 30 秒兜底”串行执行
        if (!isReload)
        {
            prestartApps();
        }
        // 第二阶段：仍按配置顺序创建标签，保证标签顺序与配置一致；
        // 此时外部程序大多已就绪，窗口查找通常一次即可命中
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
    // 所有嵌入均已同步完成，这里只保留极短间隔让 Qt 处理一次布局，不再固定等待 1 秒
    Sleep(200);
    qDebug() << "ui->tabWidget->count() = " << ui->tabWidget->count();
    if (ui->tabWidget->count() <= 0 && !config.debugMode)
    {
        this->hide();
    }
    this->initTabWidget();
}

void ExternalAppWindow::prestartApps()
{
    if (config.debugMode)
    {
        return;
    }
    for (int i = 0; i < config.appList.size(); i++)
    {
        AppConfig &app = config.appList[i];
        // 网页与 PPT 由后续流程各自处理，这里只负责普通外部程序
        if (app.appPath.startsWith("http://") || app.appPath.startsWith("https://") || app.appPath.contains(".ppt"))
        {
            continue;
        }
        if (prestartedAppPaths.contains(app.appPath))
        {
            continue;
        }
        prestartedAppPaths.insert(app.appPath);
        if (!startProcess(app))
        {
            qDebug() << "prestart process failed, app path = " << app.appPath;
            failedAppPaths.insert(app.appPath);
        }
    }
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

    QObject::connect(tabBar, &QTabBar::currentChanged, [=](int index) { tabIndexChanged(index); });
    tabIndexChanged(0);
}
void ExternalAppWindow::tabIndexChanged(int index)
{
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
        if (externalAppList[i].externalWindow == nullptr)
        {
            continue;
        }
        //        HWND childHwnd;
        // TODO: chrome窗口强制隐藏后，无法重新显示，暂未解决
        if (externalAppList[i].tabName == tabBar->tabText(tabBar->currentIndex()))
        {
            if (externalAppList[i].processName.contains("chrome.exe"))
            {
                ShowWindow(externalAppList[i].hwnd, true);
                //                childHwnd = FindWindowExA(externalAppList[i].hwnd, NULL, "Intermediate D3D Window",
                //                NULL); if (childHwnd != NULL && childHwnd != 0x0)
                //                {
                //                    ShowWindow(childHwnd, true);
                //                }
                //                childHwnd = FindWindowExA(externalAppList[i].hwnd, NULL,
                //                "Chrome_RenderWidgetHostHWND", NULL); if (childHwnd != NULL && childHwnd != 0x0)
                //                {
                //                    ShowWindow(childHwnd, true);
                //                }
                //                SetForegroundWindow(externalAppList[i].hwnd);
                //                externalAppList[i].externalWindow->setFlags(Qt::FramelessWindowHint | Qt::Tool);
                externalAppList[i].externalWindow->showFullScreen();
            }
            else
            {
                externalAppList[i].externalWindow->showFullScreen();
            }
        }
        else
        {
            qDebug() << "hide win form, titleName = " << externalAppList[i].titleName;
            externalAppList[i].externalWindow->hide();
            if (externalAppList[i].processName.contains("chrome.exe"))
            {
                ShowWindow(externalAppList[i].hwnd, false);
                //                childHwnd = FindWindowExA(externalAppList[i].hwnd, NULL, "Intermediate D3D Window",
                //                NULL); if (childHwnd != NULL && childHwnd != 0x0)
                //                {
                //                    ShowWindow(childHwnd, false);
                //                }
                //                childHwnd = FindWindowExA(externalAppList[i].hwnd, NULL,
                //                "Chrome_RenderWidgetHostHWND", NULL); if (childHwnd != NULL && childHwnd != 0x0)
                //                {
                //                    ShowWindow(childHwnd, false);
                //                }
            }
        }
    }
}
void ExternalAppWindow::readLoadApps()
{
    this->externalAppList.clear();
    // 重新加载会清空标签页并销毁宿主控件，这里同步清空宿主与外部窗口的映射，避免残留失效指针
    nativeHostWindows.clear();
    // 重新加载不走“启动进程”流程，清空预启动记录，避免沿用上一次的失败标记导致应用被跳过
    prestartedAppPaths.clear();
    failedAppPaths.clear();
    ui->tabWidget->clear();
    loadApps(true);
}

void ExternalAppWindow::startExternalProgram(AppConfig &app, bool isReload)
{
    if (!isReload)
    {
        // 预启动阶段已明确失败的应用直接跳过，避免再白等 30 秒
        if (failedAppPaths.contains(app.appPath))
        {
            qDebug() << "prestart process failed, skip embedding, app path = " << app.appPath;
            return;
        }
        // 未经过 prestartApps() 的应用（例如直接调用 loadApps）在这里保底启动；
        // 已预启动的应用不再重复启动，避免误杀掉刚拉起的进程
        if (!prestartedAppPaths.contains(app.appPath) && !startProcess(app))
        {
            qDebug() << "start process failed, app path = " << app.appPath;
            return;
        }
    }
    ProcessInfo process = findProcessInfo(app);
    if (!process.hwnd)
    {
        return;
    }
    //获取待嵌入的外部应用窗口
    qDebug() << "process.hwnd = " << process.hwnd << ", wid = " << (WId)process.hwnd;

    if (process.processName == "chrome.exe")
    {
        // chrome 的窗口结构特殊，继续沿用 Qt 的 foreign window 方式挂到本窗口下；
        // 非 chrome 的外部程序不再经过任何 Qt 窗口包装（见下方 Win32 嵌入实现）。
        QWindow *externanlWindow = QWindow::fromWinId((WId)process.hwnd);
        externanlWindow->setFlags(Qt::FramelessWindowHint | Qt::Tool);
        qDebug() << "externanlWindow = " << externanlWindow << ", wid = " << externanlWindow->winId();
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
        // findProcessInfo 已将本 app 的进程信息追加到 externalAppList，此处不再重复追加；
        // 只需把已登记项的 tabName 同步为与 tabBar 文本一致的带序号前缀标题，
        // 否则 show_index=true 时按标签名匹配会恒不成立，导致切页时该窗口被误隐藏（标签空白）。
        process.tabName = tabTitle;
        bool isRegistered = false;
        for (int i = 0; i < externalAppList.size(); i++)
        {
            if (externalAppList[i].tabName == app.tabName)
            {
                externalAppList[i].tabName = tabTitle;
                externalAppList[i].externalWindow = externanlWindow;
                isRegistered = true;
                break;
            }
        }
        if (!isRegistered)
        {
            externalAppList.append(process);
        }
        // 窗体所有权已由本对象析构函数统一释放，这里不再注册 aboutToQuit 回收，避免重复释放
        return;
    }
    // 不使用 QWindow::fromWinId + QWidget::createWindowContainer 的方式嵌入：
    // 实测该方式下被嵌入的 Qt 外部程序（例如 DocumentCount.exe）会在嵌入后数秒内
    // 于 Qt5Gui.dll 中发生访问违例崩溃（0xc0000005），现象就是该标签页内容消失、
    // 只剩黑屏，而后面的网页标签页仍可正常显示。
    // 这里改为纯 Win32 方式嵌入：把外部窗口直接设为标签页内一个原生 QWidget 的子窗口，
    // 父子关系由 Windows 维护，宿主控件隐藏/移动/销毁时外部窗口自动跟随。
    QWidget *host = new QWidget(ui->tabWidget);
    host->setAttribute(Qt::WA_NativeWindow);
    // 宿主客户区完全被子窗口覆盖，无需再擦除背景，避免父子窗口重复绘制产生闪烁
    host->setAttribute(Qt::WA_OpaquePaintEvent);

    // 先取消目标窗口的最大化/最小化状态并改为子窗口样式，再建立父子关系，
    // 避免目标程序仍按“最大化”重新计算几何
    ShowWindow(process.hwnd, SW_RESTORE);
    LONG_PTR style = GetWindowLongPtr(process.hwnd, GWL_STYLE);
    style &= ~WS_MAXIMIZE;
    style &= ~WS_MINIMIZE;
    style |= WS_CHILD;
    style &= ~WS_CAPTION;
    style &= ~WS_BORDER;
    style &= ~WS_POPUP;
    // 注意：不要清除目标窗口的 WS_THICKFRAME（可缩放边框）来禁止拖拽调整尺寸。
    // 实测跨进程修改外部 Qt 程序的窗口样式后，该程序会在 Qt5Gui.dll 中崩溃（0xc0000005）。
    // 因此这里保持原有样式不变，改由周期定时器把被拖动过的窗口重新校正为铺满宿主控件。
    SetWindowLongPtr(process.hwnd, GWL_STYLE, style);
    SetParent(process.hwnd, (HWND)host->winId());

    // 外部窗口先铺满宿主控件，后续尺寸变化由 eventFilter 统一同步
    QRect rect = currentScreen->geometry();
    MoveWindow(process.hwnd, 0, 0, rect.width(), rect.height(), TRUE);
    // 启动尺寸校正定时器，抵消用户用鼠标拖动窗口边框造成的尺寸变化
    nativeHostWindows.insert(host, (WId)process.hwnd);
    host->installEventFilter(this);
    startNativeHostFitTimer();
    // 该外部窗口不交给 Qt 管理（externalWindow 保持为空）：
    // 这样 tabIndexChanged/setScreen 中的显隐与几何同步逻辑会跳过它，
    // 显隐完全由 Windows 依据宿主控件的可见性自动处理。
    addWindowToTabWidget(host, app);
}

void ExternalAppWindow::loadWebPage(AppConfig &app)
{
#ifdef USE_QCEFVIEW
    loadWebPageByCef(app);
#else
    loadWebPageByWebEngine(app);
#endif
}

#ifdef USE_QCEFVIEW
void ExternalAppWindow::loadWebPageByCef(AppConfig &app)
{
    QString url = app.appPath;
    qDebug() << "load web page by QCefView, url = " << url;
    // CEF 只支持全进程唯一的缓存目录（已在 main() 中通过 QCefConfig::setCachePath 统一设置），
    // 因此这里不再像 QWebEngineView 那样按应用维度创建独立 profile 做缓存/Cookie 隔离。
    // 直接把真实地址交给 CefWebView 构造，避免“about:blank → 真实地址”的二次导航
    CefWebView *cefWebView = new CefWebView(QUrl(url));
    addWindowToTabWidget(cefWebView, app);
}

#else

void ExternalAppWindow::loadWebPageByWebEngine(AppConfig &app)
{
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
    //禁用自动加载图片
    view->settings()->setAttribute(QWebEngineSettings::AutoLoadImages, true);
    //确保JS可用，懒加载依赖JS
    view->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    //禁用滚动条
    view->settings()->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, false);
    view->settings()->setAttribute(QWebEngineSettings::ShowScrollBars, false);
    view->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    QWebEnginePage *page = new QWebEnginePage(profile, view);
    view->setPage(page);
    view->load(QUrl(url));
    // 启用2D加速
    // view->settings()->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled, false);
    // 启用WebGL
    // view->settings()->setAttribute(QWebEngineSettings::WebGLEnabled, true);
    // 禁用插件
    // view->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, false);
    view->setStyleSheet("border:none;margin:0px;padding:0px;background:red;");
    QRect rect = currentScreen->geometry();
    view->resize(rect.width(), rect.height());
    //    view->showMaximized();
    connect(view, &QWebEngineView::loadFinished,
            [=](bool ok) { qDebug() << "QWebEngineView::loadFinished: " << ok << ", url = " << url; });
    connect(view->page(), &QWebEnginePage::loadFinished,
            [=](bool ok) { qDebug() << "QWebEnginePage::loadFinished: " << ok << ", url = " << url; });

    // 场景不指定父对象，登记到 webSceneList 中由析构函数统一释放。
    // QGraphicsScene 会接管通过 addWidget 加入的 QWebEngineView 的所有权（级联释放 view/page），
    // 因此不能在此处再注册 aboutToQuit 回收 view/page，否则会重复释放。
    QGraphicsScene *scene = new QGraphicsScene();
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
    //    gpView->showMaximized();
    addWindowToTabWidget(gpView, app);
    gpView->viewport()->update();
    scene->update();
    // 登记场景与 profile，交由析构函数按“先场景（级联释放 view/page）、后 profile”的顺序释放
    webSceneList.append(scene);
    webProfileList.append(profile);
}

#endif  // USE_QCEFVIEW

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

    // 定时器以本对象为父对象，随窗体销毁而停止并释放，避免无父对象造成的内存泄漏
    QTimer *timer = new QTimer(this);
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

bool ExternalAppWindow::eventFilter(QObject *watched, QEvent *event)
{
    QWidget *host = qobject_cast<QWidget *>(watched);
    // 仅在宿主控件（承载外部程序窗口的原生控件）尺寸变化或重新显示时同步外部窗口大小
    if (host != nullptr && nativeHostWindows.contains(host) &&
        (event->type() == QEvent::Resize || event->type() == QEvent::Show))
    {
        HWND childHwnd = (HWND)nativeHostWindows.value(host);
        // IsWindow 用于防止外部程序在嵌入后自行退出导致句柄失效
        if (childHwnd != nullptr && IsWindow(childHwnd) != FALSE)
        {
            MoveWindow(childHwnd, 0, 0, host->width(), host->height(), TRUE);
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void ExternalAppWindow::startNativeHostFitTimer()
{
    if (nativeHostFitTimer != nullptr)
    {
        return;
    }
    nativeHostFitTimer = new QTimer(this);
    nativeHostFitTimer->setInterval(200);
    connect(nativeHostFitTimer, &QTimer::timeout, this, [this]() {
        for (QHash<QWidget *, WId>::const_iterator it = nativeHostWindows.constBegin();
             it != nativeHostWindows.constEnd(); ++it)
        {
            QWidget *host = it.key();
            HWND childHwnd = (HWND)it.value();
            // 窗口句柄可能因外部程序退出而失效，句柄失效后跳过
            if (host == nullptr || childHwnd == nullptr || IsWindow(childHwnd) == FALSE || !host->isVisible())
            {
                continue;
            }
            RECT rect;
            if (GetWindowRect(childHwnd, &rect) == FALSE)
            {
                continue;
            }
            // 外部窗口被用户拖动边框改变了尺寸，立即校正回铺满宿主控件
            if (rect.right - rect.left != host->width() || rect.bottom - rect.top != host->height())
            {
                MoveWindow(childHwnd, 0, 0, host->width(), host->height(), TRUE);
            }
        }
    });
    nativeHostFitTimer->start();
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
    // 将已登记进程项的 tabName 同步为该 app 实际使用的标签文本（含 show_index 时的序号前缀），
    // 保证切页时按标签名匹配显示/隐藏的判断成立
    for (int i = 0; i < externalAppList.size(); i++)
    {
        if (externalAppList[i].tabName == app.tabName)
        {
            externalAppList[i].tabName = tabTitle;
        }
    }
    if (findTabIndex >= 0)
    {
        ui->tabWidget->removeTab(findTabIndex);
        ui->tabWidget->insertTab(findTabIndex, widget, tabTitle);
        // 同步维护悬浮标签栏，避免 ui->tabWidget 与 tabBar 的标签数量/顺序不一致导致切页索引错位
        if (findTabIndex < tabBar->count())
        {
            tabBar->removeTab(findTabIndex);
            tabBar->insertTab(findTabIndex, tabTitle);
        }
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
            qDebug() << "start process failed, app path = " << app.appPath;
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
    // 等待窗口出现的最长时间与探测间隔。原实现上限为 30*100 次、每次 Sleep(200ms)，
    // 最坏情况下会阻塞主线程约 600 秒，这里将其收敛为约 30 秒。
    // 由于外部程序已在 prestartApps() 阶段并行拉起，窗口通常已存在，
    // 连续命中次数由 5 次降为 3 次（约 600ms），用于确认窗口已稳定创建，避免抓到过渡态句柄。
    const int intervalMs = 200;
    const int maxWaitCount = 30000 / intervalMs;
    const int stableHitCount = 3;
    int count = 0;
    bool isFind = false;
    int findCount = 0;
    qDebug() << "starting find process info, app = " << app.appPath;
    while (count < maxWaitCount)
    {
        processList.clear();
        processList = processUtils->getProcessInfo(app.processName, app.windowTitle);
        if (processList.size() == 0)
        {
            count++;
            Sleep(intervalMs);
            continue;
        }
        process = processList[processList.size() - 1];
        isFind = true;
        findCount++;
        Sleep(intervalMs);
        if (findCount >= stableHitCount)
        {
            break;
        }
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
#ifndef USE_QCEFVIEW
    // 先释放网页相关的场景与 profile：QGraphicsScene 会级联释放其内部的 QWebEngineView（进而释放 page），
    // 因此必须先删除场景，再删除 QWebEngineProfile，避免 profile 被提前销毁导致仍在使用的 page/view 崩溃。
    for (int i = webSceneList.size() - 1; i >= 0; i--)
    {
        delete webSceneList[i];
    }
    webSceneList.clear();
    for (int i = webProfileList.size() - 1; i >= 0; i--)
    {
        delete webProfileList[i];
    }
    webProfileList.clear();
#endif
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
