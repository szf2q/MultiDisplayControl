#include "ExternalAppWindow.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QSignalBlocker>
#include <QTimer>
#include <QUrl>

#include "ui_ExternalAppWindow.h"

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
    // 宽度不再固定：最小宽度置 0，由 layoutFloatingTabBar() 按标签总宽度重新计算
    tabBarWidget->setMinimumSize(0, 50);
    tabBar = new QTabBar(tabBarWidget);
    tabBar->setAttribute(Qt::WA_TranslucentBackground);
    tabBar->setAttribute(Qt::WA_AlwaysStackOnTop);
    tabBar->setStyleSheet("border:none;color:red;font-family:微软雅黑;font-weight:bold;");
    tabBar->setElideMode(Qt::ElideNone);
    tabBar->setUsesScrollButtons(false);
    tabBar->setExpanding(false);
    tabBar->setGeometry(0, 0, 500, 35);
    tabBarWidget->show();
    // 悬浮标签栏与内容标签页只在这里连接一次，避免 initTabWidget 多次调用导致重复响应
    QObject::connect(tabBar, &QTabBar::currentChanged, this, [this](int index) { tabIndexChanged(index); });

    // 序号浮层始终创建，是否显示由 config.showIndex 控制：
    // 这样运行时热更新打开/关闭“显示屏幕序号”时无需重建控件
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
    screenIndexWidget->setVisible(config.showIndex);

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
    // 序号浮层始终存在，文字始终同步，是否显示由 showIndex 单独控制
    if (screenIndexLbl != nullptr)
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

    const QString activeAppId = currentAppId();
    for (int i = 0; i < externalAppList.size(); i++)
    {
        if (externalAppList[i].externalWindow != nullptr)
        {
            // 优先按软件 id 匹配当前标签，避免重排或重名标签导致尺寸同步到错误窗口
            if (!activeAppId.isEmpty() && externalAppList[i].appId == activeAppId)
            {
                externalAppList[i].externalWindow->setGeometry(0, 0, rect.width(), rect.height());
                break;
            }
        }
    }
    Sleep(50);
    if (this->tabBarWidget != nullptr)
    {
        // 由自适应布局统一设置位置与宽度，避免标签较多时最后一个标签被裁掉
        layoutFloatingTabBar();
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
        // 首次批量加载期间抑制标签自动切页：标签按就绪顺序插入，
        // 先建好的标签会自动成为当前页（表现为启动后闪出第二个应用），
        // 这里统一在加载结束、顺序全部就位后再切回配置里的第一个应用
        appsLoading = !isReload;
        // 第一阶段：先把本屏所有外部程序并行拉起（不等待窗口出现），
        // 使多个程序的启动耗时相互重叠，而不是逐个“启动 + 等待 30 秒兜底”串行执行
        if (!isReload)
        {
            prestartApps();
        }
        // 立即让全屏窗口先绘制一帧：后续加载是同步阻塞的，不先处理一次事件循环的话，
        // 窗口要等所有应用加载完才会第一次绘制，用户会长时间看到桌面或黑屏
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

        // 第二阶段：按配置顺序创建标签，保证第一个标签就是配置里的第一个应用。
        // 普通外部程序若此刻窗口尚未出现，先登记下来，留到下一阶段统一并行等待，
        // 避免像旧实现那样在单个应用上阻塞，导致后面所有标签都要排队
        QVector<int> pendingExternalIndexes;
        QVector<int> pptAppIndexes;
        for (int i = 0; i < config.appCount; i++)
        {
            AppConfig &app = config.appList[i];
            if (app.appPath.startsWith("http://") || app.appPath.startsWith("https://"))
            {
                loadWebPage(app, i);
                // 每建好一个网页标签就处理一次事件循环，让标签栏与页面尽早刷新出来
                QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                continue;
            }
            if (app.appPath.contains(".ppt"))
            {
                pptAppIndexes.append(i);
                continue;
            }
            // 预启动阶段已明确失败的应用不再进入等待，否则会白等满超时时长
            if (!isReload && failedAppPaths.contains(app.appPath))
            {
                qDebug() << "prestart process failed, skip embedding, app path = " << app.appPath;
                continue;
            }
            // 未经 prestartApps() 拉起过的应用在这里保底启动，
            // 已预启动的应用不再重复启动，避免误杀掉刚拉起的进程
            if (!isReload && !prestartedAppPaths.contains(app.appPath) && !startProcess(app))
            {
                qDebug() << "start process failed, app path = " << app.appPath;
                continue;
            }
            // 快速通道：窗口已经就绪时立即嵌入，不进入等待队列
            ProcessInfo process = probeProcessWindow(app);
            if (process.hwnd != 0)
            {
                registerExternalProcess(app, process);
                embedProcessWindow(app, process, i);
                QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                continue;
            }
            pendingExternalIndexes.append(i);
        }

        // 第三阶段：对第一轮未就绪的外部程序并行等待窗口，
        // 总等待时长约等于“最慢的那个应用”，而不是各应用等待时间之和
        if (!pendingExternalIndexes.isEmpty())
        {
            QHash<int, ProcessInfo> readyWindows = waitForProcessWindows(pendingExternalIndexes);
            for (int i = 0; i < pendingExternalIndexes.size(); i++)
            {
                const int appIndex = pendingExternalIndexes[i];
                AppConfig &app = config.appList[appIndex];
                if (!readyWindows.contains(appIndex))
                {
                    qDebug() << "get process window failed, processName = " << app.processName
                             << ", processTitle = " << app.windowTitle << ", appPath = " << app.appPath;
                    continue;
                }
                ProcessInfo process = readyWindows.value(appIndex);
                embedProcessWindow(app, process, appIndex);
                // 嵌入完成后立即让宿主窗口绘制一次，避免所有外部程序都嵌完才一起显示
                QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            }
        }

        // 第四阶段：PPT 通过 ActiveQt 调用 WPS/PowerPoint，必须在主线程串行执行，
        // 放到最后处理，避免放映窗口的启动耗时阻塞前面所有标签的创建
        for (int i = 0; i < pptAppIndexes.size(); i++)
        {
            startAndPlayPpt(config.appList[pptAppIndexes[i]], pptAppIndexes[i]);
        }
    }
    // 所有嵌入均已同步完成，这里只保留极短间隔让 Qt 处理一次布局，不再固定等待 1 秒
    Sleep(200);
    // 首次加载时显式切回配置中的第一个应用：网页标签先建、外部程序与 PPT 后建，
    // 按插入位置建标签会让当前页停在后建的那个标签上，这里归位到配置首项
    appsLoading = false;
    if (!isReload && config.appCount > 0 && ui->tabWidget->count() > 0)
    {
        const int firstTabIndex = findTabIndexByAppId(config.appList[0].id);
        if (firstTabIndex >= 0)
        {
            ui->tabWidget->setCurrentIndex(firstTabIndex);
        }
    }
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

    // 统一由 syncFloatingTabBar 维护悬浮标签栏的可见性与内容：
    // 普通模式只有多个标签时才需要悬浮标签栏，调试模式始终保留
    syncFloatingTabBar();
    tabIndexChanged(ui->tabWidget->currentIndex());
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

    // 当前标签对应的软件 id 是显隐外部窗口的唯一依据，避免标签名重复或重排后匹配错位
    QWidget *page = ui->tabWidget->widget(index);
    const QString activeAppId = page != nullptr ? page->property("mdcAppId").toString() : QString();

    for (int i = 0; i < externalAppList.size(); i++)
    {
        if (externalAppList[i].externalWindow == nullptr)
        {
            continue;
        }
        bool isActive = false;
        if (!activeAppId.isEmpty())
        {
            isActive = (externalAppList[i].appId == activeAppId);
        }
        else
        {
            // 兼容早期没有 appId 的登记项
            isActive = (externalAppList[i].tabName == ui->tabWidget->tabText(index));
        }

        // TODO: chrome窗口强制隐藏后，无法重新显示，暂未解决
        if (isActive)
        {
            if (externalAppList[i].processName.contains("chrome.exe"))
            {
                ShowWindow(externalAppList[i].hwnd, true);
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

void ExternalAppWindow::reloadCurrentTab()
{
    if (config.debugMode)
    {
        qDebug() << "reload current tab, debug mode, skip";
        return;
    }
    // 以内容区当前页为准：悬浮标签栏只是视图，重载重建标签期间两者的索引可能短暂不同步
    int tabIndex = ui->tabWidget->currentIndex();
    if (tabIndex < 0)
    {
        tabIndex = tabBar->currentIndex();
    }
    if (tabIndex < 0 || tabIndex >= ui->tabWidget->count())
    {
        qDebug() << "reload current tab failed, invalid tab index = " << tabIndex;
        return;
    }
    // 通过标签文本反查配置项：标签顺序与配置顺序一致，但应用启动失败时不会创建标签，
    // 因此不能用下标直接对应 config.appList
    const QString tabTitle = ui->tabWidget->tabText(tabIndex);
    qDebug() << "reload current tab of screen " << config.screenIndex << ", tab = " << tabTitle;
    for (int i = 0; i < config.appList.size(); i++)
    {
        QString expectTitle = config.appList[i].tabName;
        if (config.showIndex)
        {
            expectTitle = QString("%1%2-%3")
                              .arg(this->isPrimaryScreen ? "*" : "")
                              .arg(config.screenIndex)
                              .arg(config.appList[i].tabName);
        }
        if (expectTitle != tabTitle)
        {
            continue;
        }
        // 重载期间标签会被原位重建，重建逻辑会按软件 id 恢复当前选中项，
        // 这里按内容区当前页重新同步一次内容区显隐即可，不再使用标签栏索引
        reloadApp(config.appList[i]);
        tabIndexChanged(ui->tabWidget->currentIndex());
        return;
    }
    qDebug() << "reload current tab failed, no app matched tab = " << tabTitle;
}

void ExternalAppWindow::switchToNextTab(int step)
{
    const int tabCount = ui->tabWidget->count();
    if (tabCount <= 1 || step == 0)
    {
        return;
    }
    // 批量加载期间当前页可能为 -1，此时从第一个标签开始循环切换
    int currentIndex = ui->tabWidget->currentIndex();
    if (currentIndex < 0)
    {
        currentIndex = 0;
    }
    // 支持任意步长并按标签总数取模，负值表示向前切换（Alt+Shift+Tab）
    const int nextIndex = ((currentIndex + step) % tabCount + tabCount) % tabCount;
    if (nextIndex == ui->tabWidget->currentIndex())
    {
        return;
    }
    qDebug() << "switch tab by shortcut, from " << currentIndex << " to " << nextIndex;
    ui->tabWidget->setCurrentIndex(nextIndex);
    // 内容区与悬浮标签栏保持一致，并按新标签重新同步外部窗口显隐
    syncFloatingTabBar();
    tabIndexChanged(nextIndex);
}

void ExternalAppWindow::reloadApp(AppConfig &app)
{
    // 网页：新建 CefWebView，由 addWindowToTabWidget 原地替换同名标签（旧视图随之销毁）
    if (app.appPath.startsWith("http://") || app.appPath.startsWith("https://"))
    {
        loadWebPage(app);
        return;
    }
    // PPT：放映窗口由 COM 实例承载，重新加载会与现有放映实例冲突并可能产生重复放映，故保持不变
    if (app.appPath.contains(".ppt"))
    {
        qDebug() << "reload skip ppt app, path = " << app.appPath;
        return;
    }
    // 外部程序：先结束旧进程并清理映射，再走常规“启动 → 查找窗口 → 嵌入”流程
    stopExternalApp(app);
    // 清理预启动/失败标记，避免上一次的失败记录导致本次重载被直接跳过
    prestartedAppPaths.remove(app.appPath);
    failedAppPaths.remove(app.appPath);
    startExternalProgram(app, false);
}

void ExternalAppWindow::stopExternalApp(AppConfig &app)
{
    // 倒序遍历，便于安全移除元素
    for (int i = externalAppList.size() - 1; i >= 0; i--)
    {
        ProcessInfo info = externalAppList[i];
        // 优先按软件 id 精确匹配；早期没有 appId 的登记项回退按路径匹配
        bool matched = !app.id.isEmpty() ? (info.appId == app.id) : (info.processPath == app.appPath);
        if (!matched)
        {
            continue;
        }
        // 先移除宿主控件到窗口句柄的映射：宿主控件会在标签替换时被销毁，
        // 若保留映射，尺寸校正定时器会访问已释放的控件而崩溃
        for (QHash<QWidget *, WId>::iterator it = nativeHostWindows.begin(); it != nativeHostWindows.end();)
        {
            if (it.value() == (WId)info.hwnd)
            {
                it = nativeHostWindows.erase(it);
            }
            else
            {
                ++it;
            }
        }
        if (info.pid > 0)
        {
            qDebug() << "reload, kill " << info.processName << ", pid = " << info.pid;
            processUtils->killProcess(info.pid);
        }
        // chrome 使用 QWindow::fromWinId 方式嵌入，需要显式释放该包装对象
        if (info.externalWindow != nullptr)
        {
            info.externalWindow->close();
            delete info.externalWindow;
        }
        externalAppList.remove(i);
    }
}

void ExternalAppWindow::startExternalProgram(AppConfig &app, bool isReload, int insertIndex)
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
    embedProcessWindow(app, process, insertIndex);
}

QHash<int, ProcessInfo> ExternalAppWindow::waitForProcessWindows(const QVector<int> &appIndexes)
{
    // 单轮探测间隔与总超时沿用 findProcessInfo 的取值：
    // 窗口通常在外程序启动后数秒内出现，8 秒仍未出现则认为该程序没有可嵌入窗口
    const int intervalMs = 100;
    const int maxLoopCount = 8000 / intervalMs;
    // 连续命中次数，用于确认窗口已稳定创建，避免抓到启动过程中的过渡态句柄
    const int stableHitCount = 3;
    QHash<int, ProcessInfo> result;
    QHash<int, int> hitCounts;
    for (int loop = 0; loop < maxLoopCount; loop++)
    {
        bool allReady = true;
        for (int i = 0; i < appIndexes.size(); i++)
        {
            const int appIndex = appIndexes[i];
            if (result.contains(appIndex))
            {
                continue;
            }
            AppConfig &app = config.appList[appIndex];
            ProcessInfo candidate = probeProcessWindow(app);
            if (candidate.hwnd == 0)
            {
                // 本轮未命中：清零该应用的连续命中次数，避免把不同轮次的偶然命中当成稳定窗口
                hitCounts[appIndex] = 0;
                allReady = false;
                continue;
            }
            hitCounts[appIndex] = hitCounts.value(appIndex) + 1;
            if (hitCounts.value(appIndex) < stableHitCount)
            {
                allReady = false;
                continue;
            }
            // 连续命中次数达标，认为窗口已稳定：登记进程信息后交给调用方嵌入
            registerExternalProcess(app, candidate);
            result.insert(appIndex, candidate);
        }
        if (allReady)
        {
            break;
        }
        // 等待窗口期间继续处理界面事件（排除用户输入，避免加载中途触发交互导致重入），
        // 让已创建的标签页保持可绘制，而不是整段加载期间界面完全冻结
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        Sleep(intervalMs);
    }
    return result;
}

void ExternalAppWindow::registerExternalProcess(AppConfig &app, ProcessInfo &process)
{
    // 补齐配置信息并登记到 externalAppList，供切页显隐、尺寸同步与热更新按 appId 精确匹配
    process.processName = app.processName;
    process.processPath = app.appPath;
    process.tabName = app.tabName;
    process.appId = app.id;
    externalAppList.append(process);
}

void ExternalAppWindow::embedProcessWindow(AppConfig &app, ProcessInfo &process, int insertIndex)
{
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
        // 等待阶段已把本应用的进程信息按 appId 追加到 externalAppList，
        // 这里只补充 Qt 窗口包装，标签创建交给统一的 addWindowToTabWidget 处理
        for (int i = 0; i < externalAppList.size(); i++)
        {
            if (!app.id.isEmpty() && externalAppList[i].appId == app.id)
            {
                externalAppList[i].externalWindow = externanlWindow;
                break;
            }
        }
        addWindowToTabWidget(new QTabWidget(), app, insertIndex);
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
    // 按要求保留目标窗口自身的边框与标题栏（WS_CAPTION / WS_BORDER），嵌入后外观与独立运行一致；
    // WS_CHILD 与 WS_POPUP 互斥，必须去掉弹出窗口标记
    style &= ~WS_POPUP;
    // 注意：不要清除目标窗口的 WS_THICKFRAME（可缩放边框）来禁止拖拽调整尺寸。
    // 实测跨进程修改外部 Qt 程序的窗口样式后，该程序会在 Qt5Gui.dll 中崩溃（0xc0000005）。
    // 因此这里保持原有样式不变，改由周期定时器把被拖动过的窗口重新校正为铺满宿主控件。
    SetWindowLongPtr(process.hwnd, GWL_STYLE, style);
    SetParent(process.hwnd, (HWND)host->winId());

    // 先隐藏窗口，再按宿主尺寸改大小，最后延迟显示：
    // 外部程序启动阶段 UI 线程往往正忙于加载文档/组件，非客户区（标题栏）的重绘会滞后。
    // 如果改完尺寸立刻显示，它会先按旧的窗口宽度把标题栏画出来（左上角一小条标题栏残影），
    // 直到处理完尺寸变更才铺满。这里保持隐藏状态完成尺寸变更，等它处理过尺寸消息后再显示，
    // 显示时整块区域重新绘制，就不会出现小标题栏残影。
    ShowWindow(process.hwnd, SW_HIDE);
    QRect rect = currentScreen->geometry();
    MoveWindow(process.hwnd, 0, 0, rect.width(), rect.height(), TRUE);
    // 隐藏状态下完成尺寸变更后，先让出主线程一小段时间并驱动事件循环，
    // 给外部程序机会处理 WM_SIZE / WM_NCCALCSIZE，随后再显示；
    // 显示时它会按新尺寸重绘整块区域，不会留下按旧宽度绘制的小标题栏残影。
    for (int i = 0; i < 8; i++)
    {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        Sleep(20);
    }
    ShowWindow(process.hwnd, SW_SHOWNA);
    // 启动尺寸校正定时器，抵消用户用鼠标拖动窗口边框造成的尺寸变化
    nativeHostWindows.insert(host, (WId)process.hwnd);
    host->installEventFilter(this);
    startNativeHostFitTimer();
    // 该外部窗口不交给 Qt 管理（externalWindow 保持为空）：
    // 这样 tabIndexChanged/setScreen 中的显隐与几何同步逻辑会跳过它，
    // 显隐完全由 Windows 依据宿主控件的可见性自动处理。
    addWindowToTabWidget(host, app, insertIndex);
}

void ExternalAppWindow::loadWebPage(AppConfig &app, int insertIndex)
{
    QString url = app.appPath;
    qDebug() << "load web page by QCefView, url = " << url;
    // CEF 只支持全进程唯一的缓存目录（已在 main() 中通过 QCefConfig::setCachePath 统一设置），
    // 因此这里不再按应用维度创建独立 profile 做缓存/Cookie 隔离。
    // 直接把真实地址交给 CefWebView 构造，避免“about:blank → 真实地址”的二次导航
    CefWebView *cefWebView = new CefWebView(QUrl(url));
    addWindowToTabWidget(cefWebView, app, insertIndex);
}

void ExternalAppWindow::startAndPlayPpt(AppConfig &app, int insertIndex)
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
            delete ppt;
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
        delete ppt;
        return;
    }
    QAxObject *slideShowSettings = presentation->querySubObject("SlideShowSettings");
    // querySubObject 失败时会返回空指针（例如文档尚未加载完成），
    // 必须先判空再调用 isNull，否则会对空指针解引用直接崩溃
    if (slideShowSettings == nullptr || slideShowSettings->isNull())
    {
        qDebug() << "执行SlideShowSettings失败";
        presentation->dynamicCall("Close()");
        ppt->dynamicCall("Quit()");
        delete ppt;
        return;
    }
    // MsoTriState 类型的属性统一传整型 msoTrue(-1)/msoFalse(0)：
    // 实测传 bool 时 WPS 会忽略写入（放映过程中读回 LoopUntilStopped 仍为 0），
    // 导致放完最后一页直接结束放映、出现"结束放映"黑屏，再被守护定时器重新拉起。
    slideShowSettings->setProperty("ShowPresenterView", QVariant(0));
    slideShowSettings->setProperty("ShowMediaControls", QVariant(0));
    slideShowSettings->setProperty("LoopUntilStopped", QVariant(-1));
    slideShowSettings->setProperty("AdvanceMode", QVariant(1));
    slideShowSettings->setProperty("ShowWithAnimation", QVariant(-1));
    slideShowSettings->setProperty("ShowWithNarration", QVariant(0));
    slideShowSettings->setProperty("ShowWithSlideShow", QVariant(-1));
    qDebug() << "ppt LoopUntilStopped = " << slideShowSettings->property("LoopUntilStopped")
             << ", AdvanceMode = " << slideShowSettings->property("AdvanceMode");

    // 记录放映会话：供热更新移除 PPT 时主动结束放映并释放 COM 对象
    PptSession session;
    session.control = ppt;
    session.presentation = presentation;
    playPpt(ppt, presentation, slideShowSettings, app, insertIndex, &session);

    // 定时器以本对象为父对象，随窗体销毁而停止并释放，避免无父对象造成的内存泄漏
    QTimer *timer = new QTimer(this);
    session.timer = timer;
    // 捕获配置副本而不是引用：配置可能在热更新过程中被修改或移除
    const AppConfig appCopy = app;
    connect(timer, &QTimer::timeout, this, [=]() { guardPptLoop(ppt, presentation, slideShowSettings, appCopy, insertIndex); });
    // 轮询间隔取 50ms：黑屏停留时间最长为一次轮询周期，肉眼基本不可见
    timer->start(50);

    if (!app.id.isEmpty())
    {
        pptSessions.insert(app.id, session);
    }
}

void ExternalAppWindow::playPpt(QAxWidget *ppt, QAxObject *presentation, QAxObject *slideShowSettings,
                                AppConfig &app, int insertIndex, PptSession *session)
{
    QAxObject *slideShowWindow = slideShowSettings->querySubObject("Run");
    // Run 失败时 querySubObject 会返回空指针，必须先判空再解引用，否则会直接崩溃
    if (slideShowWindow == nullptr || slideShowWindow->isNull())
    {
        qDebug() << "执行Run失败";
        delete slideShowWindow;
        presentation->dynamicCall("Close()");
        ppt->dynamicCall("Quit()");
        return;
    }
    // 个别版本在启动放映时会用文档里的设置覆盖之前写入的值，
    // 这里再写一次循环放映，保证放映到最后一页后自动回到第一页而不是结束放映
    slideShowSettings->setProperty("LoopUntilStopped", QVariant(-1));
    QAxObject *view = slideShowWindow->querySubObject("View");
    if (view == nullptr || view->isNull())
    {
        qDebug() << "获取放映视图失败";
        delete view;
        return;
    }
    view->setProperty("ViewType", 1);
    Sleep(1000);
    if (session != nullptr)
    {
        // 保存清理连接，热移除该 PPT 时可断开，避免程序退出时访问已释放的 COM 对象
        session->quitConnection =
            connect(qApp, &QApplication::aboutToQuit, qApp, [presentation, ppt]() {
                presentation->dynamicCall("Close()");
                ppt->dynamicCall("Quit()");
            });
    }

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
    // 记录软件 id，供热更新时按 id 结束放映窗口
    process.appId = app.id;
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
    addWindowToTabWidget(container, app, insertIndex);
}

void ExternalAppWindow::guardPptLoop(QAxWidget *ppt, QAxObject *presentation, QAxObject *slideShowSettings,
                                     const AppConfig &app, int insertIndex)
{
    // 放映视图状态（对应 PowerPoint/WPS 的 PpSlideShowState 枚举）
    const int pptStateRunning = 1;      // 正在放映
    const int pptStateBlackScreen = 3;  // 黑屏（按 B 或放映结束时的黑屏）
    const int pptStateWhiteScreen = 4;  // 白屏（按 W）

    // 注意：querySubObject 返回的对象会以“调用者”为父对象，
    // 删除父对象会连带销毁子对象。因此必须先读完子对象数据，再按“子→父”的顺序释放，
    // 否则会出现父对象已删除、子对象指针悬空导致的访问违例崩溃。
    QAxObject *showWindows = ppt->querySubObject("SlideShowWindows");
    if (showWindows == nullptr || showWindows->isNull())
    {
        delete showWindows;
        return;
    }
    const int windowCount = showWindows->property("Count").toInt();
    if (windowCount <= 0)
    {
        // 放映窗口已被关闭（例如误按 Esc），重新开始一次放映；定时器本身继续复用，避免重复创建
        delete showWindows;
        AppConfig appCopy = app;
        playPpt(ppt, presentation, slideShowSettings, appCopy, insertIndex, nullptr);
        return;
    }

    QAxObject *showWindow = showWindows->querySubObject("Item(int)", 1);
    if (showWindow == nullptr || showWindow->isNull())
    {
        delete showWindow;
        delete showWindows;
        return;
    }
    QAxObject *view = showWindow->querySubObject("View");
    if (view == nullptr || view->isNull())
    {
        delete view;
        delete showWindow;
        delete showWindows;
        return;
    }

    // 放映到最后一页后若仍出现黑屏/白屏，说明循环放映没有被放映程序本身接管，
    // 这里立刻恢复放映并跳回第一页，最多只闪一次轮询周期（50ms），肉眼基本不可见。
    // 说明：该守护用于无人值守的大屏循环播放，若有人按 B/W 主动切黑屏也会被还原。
    const int state = view->property("State").toInt();
    if (state == pptStateBlackScreen || state == pptStateWhiteScreen)
    {
        // 先退出黑屏/白屏状态恢复放映，再跳回第一页；顺序颠倒时视图仍处于黑屏，跳页会被忽略
        view->setProperty("State", pptStateRunning);
        view->dynamicCall("First()");
    }
    // 状态 5（放映已结束）不在这里重新拉起：放映结束后放映窗口随即被销毁，
    // 由下一次轮询命中 Count<=0 分支统一重新发起放映，避免对已失效的视图发指令导致崩溃
    delete view;
    delete showWindow;
    delete showWindows;
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

void ExternalAppWindow::addWindowToTabWidget(QWidget *widget, AppConfig &app, int insertIndex)
{
    QRect rect = currentScreen->geometry();
    //设置容器窗口的尺寸到最大
    widget->setGeometry(0, 0, rect.width(), rect.height());
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    widget->setToolTip(app.appPath);
    // 占位底色使用与深色主题一致的深蓝黑：外部窗口/网页渲染完成前不会闪出纯蓝或纯白，
    // 重载或切换标签时视觉过渡更平滑
    widget->setStyleSheet("border:none;background:#010A1B;");
    // 在页面控件上记录软件 id：切页显隐、重排标签、热更新都依赖该属性和 appId 稳定匹配，
    // 不再使用可能重复或被修改的标签文本
    if (!app.id.isEmpty())
    {
        widget->setProperty("mdcAppId", app.id);
    }
    //将容器窗口添加到Tab中
    const QString tabTitle = buildTabTitle(app);
    qDebug() << "tabTitle = " << tabTitle;
    int findTabIndex = findTabIndexByAppId(app.id);
    if (findTabIndex < 0 && app.id.isEmpty())
    {
        // 兼容早期没有软件 id 的配置：退化为按标签文本匹配
        for (int i = 0; i < ui->tabWidget->count(); i++)
        {
            if (ui->tabWidget->tabText(i) == tabTitle)
            {
                findTabIndex = i;
                break;
            }
        }
    }
    qDebug() << "findTabIndex = " << findTabIndex;
    // 将已登记进程项的 tabName 同步为该 app 实际使用的标签文本（含 show_index 时的序号前缀），
    // 保证兼容旧匹配逻辑时判断成立
    for (int i = 0; i < externalAppList.size(); i++)
    {
        bool matched = !app.id.isEmpty() ? (externalAppList[i].appId == app.id)
                                         : (externalAppList[i].tabName == app.tabName);
        if (matched)
        {
            externalAppList[i].tabName = tabTitle;
        }
    }
    if (findTabIndex >= 0)
    {
        // 记录替换前的当前软件：移除旧标签时 Qt 可能把当前页切到相邻标签，
        // 这里在替换完成后按软件 id 复位，保证 Alt+F5 重载当前标签时不会跳页
        const QString activeAppIdBefore = currentAppId();
        // 先用“插入新标签 → 移除旧标签”的方式原位替换，而不是“移除 → 插入”：
        // 移除当前选中项时 Qt 会自动改选相邻标签并触发切页，导致重载后标签跳变；
        // 先插入时当前项只是索引后移一位，选中状态保持不变
        QWidget *oldWidget = ui->tabWidget->widget(findTabIndex);
        ui->tabWidget->insertTab(findTabIndex, widget, tabTitle);
        ui->tabWidget->removeTab(findTabIndex + 1);
        if (oldWidget != nullptr)
        {
            // QTabWidget::removeTab 只是解除管理、不会销毁控件，
            // 重载时若不显式释放，旧的网页视图或宿主控件会一直残留在内存中
            oldWidget->deleteLater();
        }
        if (!app.id.isEmpty())
        {
            appTabWidgets.insert(app.id, widget);
        }
        if (!activeAppIdBefore.isEmpty())
        {
            const int restoreIndex = findTabIndexByAppId(activeAppIdBefore);
            if (restoreIndex >= 0 && restoreIndex != ui->tabWidget->currentIndex())
            {
                ui->tabWidget->setCurrentIndex(restoreIndex);
            }
        }
    }
    else
    {
        int pos = insertIndex;
        if (pos < 0 || pos > ui->tabWidget->count())
        {
            pos = ui->tabWidget->count();
        }
        ui->tabWidget->insertTab(pos, widget, tabTitle);
        if (!app.id.isEmpty())
        {
            appTabWidgets.insert(app.id, widget);
        }
        if (appsLoading)
        {
            // 批量加载期间不自动切页：insertTab 会自动选中新建标签，
            // 先建好的标签会短暂成为当前页（启动后闪出第二个应用），这里立即取消选中
            if (ui->tabWidget->currentIndex() >= 0)
            {
                ui->tabWidget->setCurrentIndex(-1);
            }
        }
        else if (ui->tabWidget->count() == 1)
        {
            // 只在第一个标签时切换到它；热更新追加标签时保持现场当前页面不变
            ui->tabWidget->setCurrentIndex(0);
        }
        for (int i = 0; i < config.appList.count(); i++)
        {
            bool matched = !app.id.isEmpty() ? (config.appList[i].id == app.id)
                                             : (config.appList[i].appPath == app.appPath);
            if (matched)
            {
                qDebug() << "app index = " << i + 1;
                config.appList[i].tabIndex = ui->tabWidget->indexOf(widget);
                break;
            }
        }
    }
    // 悬浮标签栏与内容标签页保持完全一致，并按当前页重新同步外部窗口显隐
    syncFloatingTabBar();
    tabIndexChanged(ui->tabWidget->currentIndex());
}

QString ExternalAppWindow::buildTabTitle(const AppConfig &app) const
{
    if (config.showIndex)
    {
        return QString("%1%2-%3").arg(this->isPrimaryScreen ? "*" : "").arg(config.screenIndex).arg(app.tabName);
    }
    return app.tabName;
}

int ExternalAppWindow::findTabIndexByAppId(const QString &appId) const
{
    if (appId.isEmpty())
    {
        return -1;
    }
    for (int i = 0; i < ui->tabWidget->count(); i++)
    {
        QWidget *page = ui->tabWidget->widget(i);
        if (page != nullptr && page->property("mdcAppId").toString() == appId)
        {
            return i;
        }
    }
    return -1;
}

QString ExternalAppWindow::currentAppId() const
{
    QWidget *page = ui->tabWidget->currentWidget();
    if (page == nullptr)
    {
        return QString();
    }
    return page->property("mdcAppId").toString();
}

QStringList ExternalAppWindow::appIdsInTabOrder() const
{
    QStringList ids;
    for (int i = 0; i < ui->tabWidget->count(); i++)
    {
        QWidget *page = ui->tabWidget->widget(i);
        if (page == nullptr)
        {
            continue;
        }
        const QString appId = page->property("mdcAppId").toString();
        if (!appId.isEmpty())
        {
            ids << appId;
        }
    }
    return ids;
}

bool ExternalAppWindow::hasApp(const QString &appId) const
{
    return findTabIndexByAppId(appId) >= 0;
}

void ExternalAppWindow::syncFloatingTabBar()
{
    if (tabBar == nullptr || tabBarWidget == nullptr)
    {
        return;
    }
    const int currentIndex = ui->tabWidget->currentIndex();
    {
        // 重建期间屏蔽信号：否则逐项增删会触发 currentChanged，
        // 导致外部窗口被反复隐藏/显示，出现闪烁甚至切错标签
        const QSignalBlocker blocker(tabBar);
        while (tabBar->count() > 0)
        {
            tabBar->removeTab(0);
        }
        for (int i = 0; i < ui->tabWidget->count(); i++)
        {
            tabBar->addTab(ui->tabWidget->tabText(i));
        }
        if (currentIndex >= 0 && currentIndex < tabBar->count())
        {
            tabBar->setCurrentIndex(currentIndex);
        }
    }
    // 标签数量或名称变化后，需要重新计算标签栏宽度，保证最后一个标签完整可见
    layoutFloatingTabBar();
    // 普通模式下只有多个标签才需要悬浮标签栏，调试模式始终显示
    tabBarWidget->setVisible(config.debugMode || ui->tabWidget->count() > 1);
}

void ExternalAppWindow::layoutFloatingTabBar()
{
    if (tabBar == nullptr || tabBarWidget == nullptr)
    {
        return;
    }
    QScreen *screen = (currentScreen != nullptr) ? currentScreen : QGuiApplication::primaryScreen();
    if (screen == nullptr)
    {
        return;
    }
    const QRect screenRect = screen->geometry();
    // 默认布局参数：宽度 500、整体高度 50、标签高度 35（保持原有外观）
    const int defaultWidth = 500;
    const int barHeight = 50;
    const int tabHeight = 35;
    // 逐个累加每个标签的实际宽度：标签数量较多或名称较长时总宽度会超过 500，
    // 旧实现把标签栏宽度写死为 500，超出部分会被直接裁掉（表现为最后一个标签显示不全）。
    // tabSizeHint 是 protected 接口，这里先把标签栏撑到屏幕最大可用宽度让 Qt 完成布局，
    // 再读取最后一个标签的右边界，得到标签实际需要的总宽度。
    tabBar->setGeometry(0, 0, screenRect.width(), tabHeight);
    int tabsWidth = 0;
    if (tabBar->count() > 0)
    {
        tabsWidth = tabBar->tabRect(tabBar->count() - 1).right() + 1;
    }
    int width = qMax(defaultWidth, tabsWidth + 10);
    // 右边缘保持默认布局位置不变，宽度增加时向左扩展，
    // 避免向右扩展超出屏幕边界导致末尾标签再次被裁掉
    const int right = screenRect.x() + config.tabOffsetX + defaultWidth;
    int left = right - width;
    if (left < screenRect.x())
    {
        // 标签总宽度超过整块屏幕可用宽度时只能占满屏幕，此时无法再完整显示所有标签
        left = screenRect.x();
        width = right - left;
    }
    tabBarWidget->setGeometry(left, screenRect.y() + 10, width, barHeight);
    tabBar->setGeometry(0, 0, width, tabHeight);
}

void ExternalAppWindow::applyScreenConfig(const ScreenConfig &cfg, QScreen *targetScreen)
{
    const bool showIndexChanged = (this->config.showIndex != cfg.showIndex);
    this->config.screenIndex = cfg.screenIndex;
    this->config.screenTitle = cfg.screenTitle;
    this->config.showIndex = cfg.showIndex;
    this->config.debugMode = cfg.debugMode;
    this->config.tabOffsetX = cfg.tabOffsetX;

    if (this->config.screenTitle.isEmpty())
    {
        this->setWindowTitle(QString("%1号屏").arg(this->config.screenIndex));
    }
    else
    {
        this->setWindowTitle(QString("%1号屏-%2").arg(this->config.screenIndex).arg(this->config.screenTitle));
    }
    this->displayScreenIndex();

    // 只有当本窗口最初就处于调试模式时标签栏才隐藏，热更新切换调试模式时同步可见性
    if (config.debugMode)
    {
        ui->tabWidget->tabBar()->show();
    }

    if (screenIndexWidget != nullptr)
    {
        screenIndexWidget->setVisible(this->config.showIndex);
        // 显示序号时立即按当前屏幕位置重新摆放，避免浮层停在旧坐标
        if (this->config.showIndex && this->currentScreen != nullptr)
        {
            QRect rect = this->currentScreen->geometry();
            screenIndexWidget->move(rect.right() - 125, rect.top() + 25);
        }
    }

    if (targetScreen != nullptr)
    {
        this->defaultScreen = targetScreen;
        this->isPrimaryScreen = (targetScreen == QGuiApplication::primaryScreen());
        // force 为 true：即使目标物理屏没变也要重新铺满，保证分辨率/位置变化后几何正确
        this->setToDefaultScreen(true);
    }

    // 屏幕序号与“显示序号”开关都会影响标签前缀，统一刷新一次
    if (showIndexChanged)
    {
        refreshTabTitles();
    }
    syncFloatingTabBar();
}

void ExternalAppWindow::addApp(const AppConfig &app, int insertIndex)
{
    // 复制一份配置：启动流程内部会修改配置对象，不能直接使用调用方的引用
    AppConfig cfg = app;
    if (cfg.appPath.startsWith("http://") || cfg.appPath.startsWith("https://"))
    {
        loadWebPage(cfg, insertIndex);
        return;
    }
    if (cfg.appPath.contains(".ppt"))
    {
        startAndPlayPpt(cfg, insertIndex);
        return;
    }
    startExternalProgram(cfg, false, insertIndex);
}

void ExternalAppWindow::removeApp(const QString &appId)
{
    if (appId.isEmpty())
    {
        return;
    }
    // PPT 的放映窗口由 COM 实例承载，不是普通进程，必须先结束放映会话
    stopPptSession(appId);
    // 结束外部进程并清理宿主控件映射，避免尺寸校正定时器访问已销毁控件
    for (int i = externalAppList.size() - 1; i >= 0; i--)
    {
        ProcessInfo info = externalAppList[i];
        if (info.appId != appId)
        {
            continue;
        }
        for (QHash<QWidget *, WId>::iterator it = nativeHostWindows.begin(); it != nativeHostWindows.end();)
        {
            if (it.value() == (WId)info.hwnd)
            {
                it = nativeHostWindows.erase(it);
            }
            else
            {
                ++it;
            }
        }
        if (info.pid > 0)
        {
            qDebug() << "remove app, kill " << info.processName << ", pid = " << info.pid;
            processUtils->killProcess(info.pid);
        }
        if (info.externalWindow != nullptr)
        {
            info.externalWindow->close();
            delete info.externalWindow;
        }
        externalAppList.remove(i);
    }
    // 移除标签并显式释放内容控件（removeTab 不会销毁控件）
    const int index = findTabIndexByAppId(appId);
    if (index >= 0)
    {
        QWidget *page = ui->tabWidget->widget(index);
        ui->tabWidget->removeTab(index);
        if (page != nullptr)
        {
            page->deleteLater();
        }
    }
    appTabWidgets.remove(appId);
    // 同步本窗口的配置副本，保证后续重排与标题刷新使用最新的应用列表
    for (int i = this->config.appList.size() - 1; i >= 0; i--)
    {
        if (this->config.appList[i].id == appId)
        {
            this->config.appList.removeAt(i);
        }
    }
    this->config.appCount = this->config.appList.size();
    syncFloatingTabBar();
    tabIndexChanged(ui->tabWidget->currentIndex());
}

void ExternalAppWindow::reorderTabs(const QStringList &orderedAppIds)
{
    if (ui->tabWidget->count() < 2 || orderedAppIds.isEmpty())
    {
        return;
    }
    QWidget *currentPage = ui->tabWidget->currentWidget();
    QList<QWidget *> pages;
    QStringList titles;
    QStringList ids;
    for (int i = 0; i < ui->tabWidget->count(); i++)
    {
        QWidget *page = ui->tabWidget->widget(i);
        pages << page;
        titles << ui->tabWidget->tabText(i);
        ids << (page != nullptr ? page->property("mdcAppId").toString() : QString());
    }
    // 先按目标顺序排列已存在的标签，未出现在目标列表中的标签保持原有相对顺序追加在后
    QList<int> newOrder;
    for (int i = 0; i < orderedAppIds.size(); i++)
    {
        for (int j = 0; j < ids.size(); j++)
        {
            if (ids[j] == orderedAppIds[i] && !newOrder.contains(j))
            {
                newOrder << j;
            }
        }
    }
    for (int i = 0; i < ids.size(); i++)
    {
        if (!newOrder.contains(i))
        {
            newOrder << i;
        }
    }
    bool orderChanged = false;
    for (int i = 0; i < newOrder.size(); i++)
    {
        if (newOrder[i] != i)
        {
            orderChanged = true;
            break;
        }
    }
    if (!orderChanged)
    {
        return;
    }
    {
        // 重排期间屏蔽信号，避免摘除标签时触发切页把外部窗口反复隐藏/显示
        const QSignalBlocker blocker(ui->tabWidget);
        // 逐个摘下标签（不销毁控件），再按新顺序插回，避免重建内容导致网页重载或窗口解绑
        while (ui->tabWidget->count() > 0)
        {
            ui->tabWidget->removeTab(0);
        }
        for (int i = 0; i < newOrder.size(); i++)
        {
            ui->tabWidget->addTab(pages[newOrder[i]], titles[newOrder[i]]);
        }
        if (currentPage != nullptr)
        {
            ui->tabWidget->setCurrentWidget(currentPage);
        }
    }
    syncFloatingTabBar();
    tabIndexChanged(ui->tabWidget->currentIndex());
}

void ExternalAppWindow::updateAppTabName(const QString &appId, const QString &tabName)
{
    const int index = findTabIndexByAppId(appId);
    if (index < 0)
    {
        return;
    }
    // 同步本窗口配置副本，保证后续重排/前缀刷新使用最新名称
    for (int i = 0; i < this->config.appList.size(); i++)
    {
        if (this->config.appList[i].id == appId)
        {
            this->config.appList[i].tabName = tabName;
            break;
        }
    }
    AppConfig tmp;
    tmp.id = appId;
    tmp.tabName = tabName;
    const QString tabTitle = buildTabTitle(tmp);
    ui->tabWidget->setTabText(index, tabTitle);
    for (int i = 0; i < externalAppList.size(); i++)
    {
        if (externalAppList[i].appId == appId)
        {
            externalAppList[i].tabName = tabTitle;
        }
    }
    syncFloatingTabBar();
}

void ExternalAppWindow::refreshTabTitles()
{
    for (int i = 0; i < ui->tabWidget->count(); i++)
    {
        QWidget *page = ui->tabWidget->widget(i);
        if (page == nullptr)
        {
            continue;
        }
        const QString appId = page->property("mdcAppId").toString();
        if (appId.isEmpty())
        {
            continue;
        }
        bool found = false;
        AppConfig tmp;
        for (int j = 0; j < this->config.appList.size(); j++)
        {
            if (this->config.appList[j].id == appId)
            {
                tmp = this->config.appList[j];
                found = true;
                break;
            }
        }
        if (!found)
        {
            continue;
        }
        const QString tabTitle = buildTabTitle(tmp);
        ui->tabWidget->setTabText(i, tabTitle);
        for (int k = 0; k < externalAppList.size(); k++)
        {
            if (externalAppList[k].appId == appId)
            {
                externalAppList[k].tabName = tabTitle;
            }
        }
    }
    syncFloatingTabBar();
}

void ExternalAppWindow::stopPptSession(const QString &appId)
{
    if (!pptSessions.contains(appId))
    {
        return;
    }
    PptSession session = pptSessions.take(appId);
    if (session.timer != nullptr)
    {
        session.timer->stop();
        session.timer->deleteLater();
    }
    // 断开退出清理连接，避免本窗口销毁后 aboutToQuit 仍访问已释放的 COM 对象
    if (session.quitConnection)
    {
        QObject::disconnect(session.quitConnection);
    }
    if (session.presentation != nullptr)
    {
        session.presentation->dynamicCall("Close()");
        session.presentation->deleteLater();
    }
    if (session.control != nullptr)
    {
        session.control->dynamicCall("Quit()");
        session.control->deleteLater();
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

ProcessInfo ExternalAppWindow::probeProcessWindow(AppConfig &app)
{
    // chrome 继续沿用“进程名 + 标题”的匹配方式：chrome 的主窗口由浏览器进程承载，
    // 但同路径下还有渲染、GPU 等子进程，按 exe 路径取到的可能不是可嵌入的主窗口
    if (app.processName.contains("chrome.exe"))
    {
        QVector<ProcessInfo> processList = processUtils->getProcessInfo(app.processName, app.windowTitle);
        if (processList.isEmpty())
        {
            return ProcessInfo();
        }
        return processList.last();
    }
    // 桌面应用优先按可执行文件路径定位窗口：
    // 启动阶段已按路径结束同路径旧进程，命中的一定是本次拉起的实例。
    // 旧实现只按“进程名 + 窗口标题”子串匹配，一旦程序运行中标题发生变化
    // （例如编辑器把文档名、修改状态拼进标题），就会一直匹配不到窗口而嵌入失败。
    ProcessInfo byPath = processUtils->getProcessInfoByPath(app.appPath);
    if (byPath.hwnd != 0)
    {
        return byPath;
    }
    // 兜底：路径暂时取不到窗口时（例如窗口实际由启动器派生的子进程承载），
    // 再退回按“进程名 + 窗口标题”匹配，兼容原有配置
    if (app.windowTitle.isEmpty())
    {
        return ProcessInfo();
    }
    QVector<ProcessInfo> processList = processUtils->getProcessInfo(app.processName, app.windowTitle);
    if (processList.isEmpty())
    {
        return ProcessInfo();
    }
    return processList.last();
}

ProcessInfo ExternalAppWindow::findProcessInfo(AppConfig &app)
{
    ProcessInfo process;
    // 等待窗口出现的最长时间与探测间隔。
    // 外部程序已在 prestartApps() 阶段并行拉起，正常情况下窗口在数秒内就会出现；
    // 若超过该时限仍未找到，说明该程序没有可嵌入的可见窗口，直接放弃嵌入。
    // 旧实现最长阻塞主线程 30 秒，期间整个界面显示“未响应”，体验极差。
    const int intervalMs = 200;
    const int maxWaitCount = 8000 / intervalMs;
    // 连续命中次数用于确认窗口已稳定创建，避免抓到程序启动过程中的过渡态句柄
    const int stableHitCount = 3;
    int count = 0;
    int findCount = 0;
    qDebug() << "starting find process info, app = " << app.appPath;
    while (count < maxWaitCount)
    {
        ProcessInfo candidate = probeProcessWindow(app);
        if (candidate.hwnd == 0)
        {
            count++;
            Sleep(intervalMs);
            continue;
        }
        process = candidate;
        findCount++;
        Sleep(intervalMs);
        if (findCount >= stableHitCount)
        {
            break;
        }
    }
    if (process.hwnd == 0)
    {
        qDebug() << "get process window failed, processName = " << app.processName
                 << ", processTitle = " << app.windowTitle << ", appPath = " << app.appPath;
        return process;
    }

    process.processName = app.processName;
    process.processPath = app.appPath;
    process.tabName = app.tabName;
    // 记录软件 id，供切页显隐与热更新精确匹配
    process.appId = app.id;
    externalAppList.append(process);
    qDebug() << "pid = " << process.pid << ", HWND = " << process.hwnd << ", processName = " << process.processName
             << ", titleName = " << process.titleName << ", className = " << process.className
             << ", processPath = " << process.processPath;
    return process;
}
ExternalAppWindow::~ExternalAppWindow()
{
    qDebug() << "MainWindow destructor";
    // 先结束所有 PPT 放映会话：断开 aboutToQuit 连接并释放 COM 对象，
    // 避免本窗口销毁后播放守护定时器或退出回调继续访问已释放对象
    const QStringList pptAppIds = pptSessions.keys();
    for (int i = 0; i < pptAppIds.size(); i++)
    {
        stopPptSession(pptAppIds[i]);
    }
    pptSessions.clear();
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
