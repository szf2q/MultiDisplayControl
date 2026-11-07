#include "ManagementWindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCursor>
#include <QFile>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QSplitter>
#include <QStyle>
#include <QVBoxLayout>

#include "ConfigStore.h"
#include "ScreenLayoutCanvas.h"
#include "SoftwareLibraryPanel.h"

ManagementWindow::ManagementWindow(QWidget *parent) : QWidget(parent)
{
    setWindowTitle(QString::fromUtf8("多屏控制管理"));
    setObjectName(QStringLiteral("managementWindow"));
    // 无边框窗口：去掉系统标题栏与阴影边框，由窗口内部的标题区承担拖动与关闭职责
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    // 管理界面默认尺寸：按主屏 90% 计算（在 showManager 中按当前屏幕动态调整）
    resize(1200, 760);
    // 最小尺寸：保证在低分辨率屏幕上也能正常使用
    setMinimumSize(820, 560);

    // 只对管理窗口自身应用样式，避免影响全屏展示窗口
    QFile styleFile(QStringLiteral(":/qss/manager.css"));
    if (styleFile.open(QIODevice::ReadOnly))
    {
        setStyleSheet(QString::fromUtf8(styleFile.readAll()));
        styleFile.close();
    }

    // ==================== 顶部标题栏 ====================
    titleLabel = new QLabel(QString::fromUtf8("多屏控制管理"), this);
    titleLabel->setObjectName(QStringLiteral("managerTitle"));
    QLabel *subtitleLabel = new QLabel(
        QString::fromUtf8("Alt+M 显示/隐藏 · 保存后立即热应用（调试模式、管理员权限需重启主程序）"), this);
    subtitleLabel->setObjectName(QStringLiteral("hintLabel"));
    dirtyLabel = new QLabel(QString::fromUtf8("未保存"), this);
    dirtyLabel->setObjectName(QStringLiteral("dirtyLabel"));
    dirtyLabel->setVisible(false);

    saveButton = new QPushButton(QString::fromUtf8("保存并应用"), this);
    saveButton->setObjectName(QStringLiteral("primaryButton"));
    // 「重新加载」仅表示从磁盘重新读取配置文件刷新管理界面，不重启已运行的应用
    reloadButton = new QPushButton(QString::fromUtf8("重新加载配置"), this);
    closeButton = new QPushButton(QString::fromUtf8("关闭"), this);

    // 顶部按钮按文字长度设置最小宽度：样式表里的 15px 字体不会参与 sizeHint 计算，
    // 否则「重新加载配置」这类较长文案会被按钮边界裁掉最后一个字
    saveButton->setMinimumWidth(130);
    reloadButton->setMinimumWidth(150);
    closeButton->setMinimumWidth(90);

    QVBoxLayout *titleLayout = new QVBoxLayout();
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(2);
    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(subtitleLabel);

    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->addLayout(titleLayout);
    headerLayout->addWidget(dirtyLabel);
    headerLayout->addStretch(1);
    headerLayout->addWidget(saveButton);
    headerLayout->addWidget(reloadButton);
    headerLayout->addWidget(closeButton);

    // ==================== 全局设置 ====================
    showIndexCheck = new QCheckBox(QString::fromUtf8("显示屏幕序号"), this);
    sortScreenCheck = new QCheckBox(QString::fromUtf8("按位置排序屏幕"), this);
    debugModeCheck = new QCheckBox(QString::fromUtf8("调试模式（需重启）"), this);
    runAsAdminCheck = new QCheckBox(QString::fromUtf8("以管理员运行（需重启）"), this);
    // 禁用 Win 键：勾选后由全局键盘钩子吞掉 Win 键，避免误触 Win+D/Win+R 打断全屏展示
    disableWinKeyCheck = new QCheckBox(QString::fromUtf8("禁用 Win 键"), this);
    // 去掉嵌入应用边框：勾选后桌面应用不显示自身标题栏与边框，客户区完整铺满屏幕
    stripAppBorderCheck = new QCheckBox(QString::fromUtf8("嵌入应用去掉边框（需重启）"), this);
    // 禁用 GPU 加速：默认关闭以保留硬件加速，仅在个别机型/驱动出现网页渲染异常时开启
    disableGpuAccelerationCheck = new QCheckBox(QString::fromUtf8("禁用 GPU 加速（需重启）"), this);

    QHBoxLayout *settingLayout = new QHBoxLayout();
    settingLayout->addWidget(showIndexCheck);
    settingLayout->addWidget(sortScreenCheck);
    settingLayout->addWidget(debugModeCheck);
    settingLayout->addWidget(runAsAdminCheck);
    settingLayout->addWidget(disableWinKeyCheck);
    settingLayout->addWidget(stripAppBorderCheck);
    settingLayout->addWidget(disableGpuAccelerationCheck);
    settingLayout->addStretch(1);

    // ==================== 三栏内容（软件清单 | 屏幕布局 | 选中屏软件） + 底部预览 ====================
    // 左栏：软件清单
    libraryPanel = new SoftwareLibraryPanel(this);
    QWidget *libraryGroup = new QWidget(this);
    libraryGroup->setObjectName(QStringLiteral("panelGroup"));
    QVBoxLayout *libraryLayout = new QVBoxLayout(libraryGroup);
    libraryLayout->setContentsMargins(10, 10, 10, 10);
    QLabel *libraryTitle = new QLabel(QString::fromUtf8("软件清单"), libraryGroup);
    libraryTitle->setObjectName(QStringLiteral("groupTitle"));
    libraryLayout->addWidget(libraryTitle);
    libraryLayout->addWidget(libraryPanel, 1);

    // 中栏：屏幕布局示意图
    QWidget *canvasGroup = new QWidget(this);
    canvasGroup->setObjectName(QStringLiteral("panelGroup"));
    QVBoxLayout *canvasLayout = new QVBoxLayout(canvasGroup);
    canvasLayout->setContentsMargins(10, 10, 10, 10);
    QLabel *canvasTitle = new QLabel(
        QString::fromUtf8("屏幕布局（可拖动调整位置和大小，仅显示用；点击选中屏幕）"), canvasGroup);
    canvasTitle->setObjectName(QStringLiteral("groupTitle"));
    layoutCanvas = new ScreenLayoutCanvas(canvasGroup);
    canvasLayout->addWidget(canvasTitle);
    canvasLayout->addWidget(layoutCanvas, 1);

    // 右栏：选中屏幕的软件清单
    QWidget *screenListGroup = new QWidget(this);
    screenListGroup->setObjectName(QStringLiteral("panelGroup"));
    QVBoxLayout *screenListLayout = new QVBoxLayout(screenListGroup);
    screenListLayout->setContentsMargins(10, 10, 10, 10);
    screenListTitle = new QLabel(QString::fromUtf8("选中屏幕的软件"), screenListGroup);
    screenListTitle->setObjectName(QStringLiteral("groupTitle"));
    screenSoftwareList = new QListWidget(screenListGroup);
    screenSoftwareList->setSelectionMode(QAbstractItemView::SingleSelection);
    screenSoftwareList->setIconSize(QSize(64, 36));
    screenListLayout->addWidget(screenListTitle);
    screenListLayout->addWidget(screenSoftwareList, 1);

    // 顶部三栏 Splitter（横向）
    QSplitter *topSplitter = new QSplitter(Qt::Horizontal, this);
    topSplitter->addWidget(libraryGroup);
    topSplitter->addWidget(canvasGroup);
    topSplitter->addWidget(screenListGroup);
    topSplitter->setStretchFactor(0, 0);
    topSplitter->setStretchFactor(1, 1);
    topSplitter->setStretchFactor(2, 0);
    topSplitter->setSizes(QList<int>() << 280 << 600 << 240);
    topSplitter->setChildrenCollapsible(false);

    // 底部：软件预览大区域（横跨三栏）
    QWidget *previewGroup = new QWidget(this);
    previewGroup->setObjectName(QStringLiteral("panelGroup"));
    QVBoxLayout *previewLayout = new QVBoxLayout(previewGroup);
    previewLayout->setContentsMargins(10, 10, 10, 10);
    previewTitleLabel = new QLabel(QString::fromUtf8("软件预览"), previewGroup);
    previewTitleLabel->setObjectName(QStringLiteral("groupTitle"));

    // 预览内容：左图 + 右文字信息
    QHBoxLayout *previewContent = new QHBoxLayout();
    previewContent->setSpacing(16);

    previewImageLabel = new QLabel(previewGroup);
    previewImageLabel->setAlignment(Qt::AlignCenter);
    previewImageLabel->setMinimumSize(QSize(360, 220));
    previewImageLabel->setStyleSheet(QStringLiteral(
        "QLabel { background-color: #0f1f36; border: 1px solid #28507d; "
        "border-radius: 6px; color: #6d8cb0; }"));
    previewImageLabel->setText(QString::fromUtf8("暂无预览图"));

    // 右侧文字信息区
    QWidget *infoWidget = new QWidget(previewGroup);
    QVBoxLayout *infoLayout = new QVBoxLayout(infoWidget);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(6);

    previewNameLabel = new QLabel(infoWidget);
    previewNameLabel->setStyleSheet(
        QStringLiteral("QLabel { font-size: 18px; font-weight: bold; color: #e0eefc; }"));
    previewNameLabel->setWordWrap(true);

    previewTypeLabel = new QLabel(infoWidget);
    previewTypeLabel->setStyleSheet(QStringLiteral("color: #88aacc;"));

    QFrame *divider = new QFrame(infoWidget);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(QStringLiteral("color: #28507d;"));

    QLabel *pathTitle = new QLabel(QString::fromUtf8("路径"), infoWidget);
    pathTitle->setStyleSheet(QStringLiteral("color: #88aacc; font-size: 12px;"));
    previewPathLabel = new QLabel(infoWidget);
    previewPathLabel->setWordWrap(true);
    previewPathLabel->setStyleSheet(QStringLiteral("color: #c5d8ec; font-size: 12px;"));

    QLabel *wtTitle = new QLabel(QString::fromUtf8("窗口标题"), infoWidget);
    wtTitle->setStyleSheet(QStringLiteral("color: #88aacc; font-size: 12px;"));
    previewWindowTitleLabel = new QLabel(infoWidget);
    previewWindowTitleLabel->setWordWrap(true);
    previewWindowTitleLabel->setStyleSheet(QStringLiteral("color: #c5d8ec; font-size: 12px;"));

    QLabel *remarkTitle = new QLabel(QString::fromUtf8("备注"), infoWidget);
    remarkTitle->setStyleSheet(QStringLiteral("color: #88aacc; font-size: 12px;"));
    previewRemarkLabel = new QLabel(infoWidget);
    previewRemarkLabel->setWordWrap(true);
    previewRemarkLabel->setStyleSheet(QStringLiteral("color: #c5d8ec; font-size: 12px;"));

    infoLayout->addWidget(previewNameLabel);
    infoLayout->addWidget(previewTypeLabel);
    infoLayout->addWidget(divider);
    infoLayout->addWidget(pathTitle);
    infoLayout->addWidget(previewPathLabel);
    infoLayout->addWidget(wtTitle);
    infoLayout->addWidget(previewWindowTitleLabel);
    infoLayout->addWidget(remarkTitle);
    infoLayout->addWidget(previewRemarkLabel);
    infoLayout->addStretch(1);

    previewContent->addWidget(previewImageLabel, 0);
    previewContent->addWidget(infoWidget, 1);

    previewLayout->addWidget(previewTitleLabel);
    previewLayout->addLayout(previewContent);

    // 整体纵向 Splitter：上三栏 + 下预览
    QSplitter *mainSplitter = new QSplitter(Qt::Vertical, this);
    mainSplitter->addWidget(topSplitter);
    mainSplitter->addWidget(previewGroup);
    mainSplitter->setStretchFactor(0, 3);
    mainSplitter->setStretchFactor(1, 2);
    mainSplitter->setChildrenCollapsible(false);

    // ==================== 状态栏 ====================
    statusLabel = new QLabel(this);
    statusLabel->setObjectName(QStringLiteral("statusLabel"));
    statusLabel->setWordWrap(true);
    setStatusMessage(QString::fromUtf8("配置文件：%1").arg(ConfigStore::jsonPath(QApplication::applicationDirPath())));

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 14, 16, 12);
    mainLayout->addLayout(headerLayout);
    mainLayout->addLayout(settingLayout);
    mainLayout->addWidget(mainSplitter, 1);
    mainLayout->addWidget(statusLabel);

    connect(saveButton, &QPushButton::clicked, this, [this]() { onSave(); });
    connect(reloadButton, &QPushButton::clicked, this, [this]() { onReload(); });
    connect(closeButton, &QPushButton::clicked, this, [this]() { hide(); });
    connect(showIndexCheck, &QCheckBox::toggled, this, [this](bool) { onGlobalSettingChanged(); });
    connect(sortScreenCheck, &QCheckBox::toggled, this, [this](bool) { onGlobalSettingChanged(); });
    connect(debugModeCheck, &QCheckBox::toggled, this, [this](bool) { onGlobalSettingChanged(); });
    connect(runAsAdminCheck, &QCheckBox::toggled, this, [this](bool) { onGlobalSettingChanged(); });
    connect(disableWinKeyCheck, &QCheckBox::toggled, this, [this](bool) { onGlobalSettingChanged(); });
    connect(stripAppBorderCheck, &QCheckBox::toggled, this, [this](bool) { onGlobalSettingChanged(); });
    connect(disableGpuAccelerationCheck, &QCheckBox::toggled, this, [this](bool) { onGlobalSettingChanged(); });

    connect(libraryPanel, &SoftwareLibraryPanel::configChanged, this, [this]() {
        if (layoutCanvas != nullptr)
        {
            layoutCanvas->refresh();
        }
        markDirty();
    });
    connect(layoutCanvas, &ScreenLayoutCanvas::configChanged, this, [this]() {
        // 屏幕布局变化可能带来绑定关系增删（如从屏幕移除软件），
        // 只需同步"已绑定"标记与拖拽能力，不重建清单以免选中项和滚动位置跳动
        if (libraryPanel != nullptr)
        {
            libraryPanel->refreshBindingMarks();
        }
        // 同时刷新右侧选中屏幕的软件清单
        if (layoutCanvas != nullptr)
        {
            refreshScreenSoftwareList(layoutCanvas->selectedRow());
        }
        markDirty();
    });
    connect(layoutCanvas, &ScreenLayoutCanvas::softwareDropped, this,
            [this](int screenRow, const QString &softwareId, int insertRow) {
                onSoftwareDropped(screenRow, softwareId, insertRow);
            });
    // 选中屏幕时同步高亮大屏（红色边框），并刷新右侧软件清单
    connect(layoutCanvas, &ScreenLayoutCanvas::screenSelected, this, &ManagementWindow::onScreenSelected);
    // 软件清单选中变化，更新底部预览
    connect(libraryPanel, &SoftwareLibraryPanel::selectionChanged, this,
            &ManagementWindow::onLibrarySelectionChanged);
    // 右侧屏幕软件清单点击，更新底部预览
    connect(screenSoftwareList, &QListWidget::itemClicked, this,
            &ManagementWindow::onScreenSoftwareClicked);
}

void ManagementWindow::setConfig(const MultiScreenConfig &config)
{
    m_config = config;
    m_config.count = m_config.screenList.size();
    // 注：屏幕序号 screenIndex 保留配置中的原值，由用户在界面上手动调整，不再按列表顺序重置

    // 回填全局开关，阻塞信号避免触发"未保存"标记
    const QList<QCheckBox *> checks = QList<QCheckBox *>() << showIndexCheck << sortScreenCheck << debugModeCheck
                                                           << runAsAdminCheck << disableWinKeyCheck
                                                           << stripAppBorderCheck << disableGpuAccelerationCheck;
    for (int i = 0; i < checks.size(); i++)
    {
        checks[i]->blockSignals(true);
    }
    showIndexCheck->setChecked(m_config.showIndex);
    sortScreenCheck->setChecked(m_config.sortScreen);
    debugModeCheck->setChecked(m_config.debugMode);
    runAsAdminCheck->setChecked(m_config.runAsAdministrator);
    disableWinKeyCheck->setChecked(m_config.disableWinKey);
    stripAppBorderCheck->setChecked(m_config.stripAppBorder);
    disableGpuAccelerationCheck->setChecked(m_config.disableGpuAcceleration);
    for (int i = 0; i < checks.size(); i++)
    {
        checks[i]->blockSignals(false);
    }

    libraryPanel->setConfig(&m_config);
    layoutCanvas->setConfig(&m_config);
    // 默认选中第一块屏，同步右侧软件清单和大屏高亮
    if (m_config.screenList.size() > 0)
    {
        refreshScreenSoftwareList(0);
        emit screenHighlightChanged(m_config.screenList[0].screenIndex);
    }
    else
    {
        refreshScreenSoftwareList(-1);
    }
    // 底部预览默认显示软件清单中第一个软件
    if (libraryPanel != nullptr && !m_config.softwareList.isEmpty())
    {
        updateSoftwarePreview(m_config.softwareList.first().id);
    }
    clearDirty();
}

void ManagementWindow::toggle()
{
    if (isVisible())
    {
        hide();
        return;
    }
    showManager();
}

void ManagementWindow::showManager()
{
    // 管理界面以所在屏幕 90% 的尺寸显示，并在鼠标所在屏幕居中，便于多屏环境下就地管理
    QScreen *targetScreen = QGuiApplication::screenAt(QCursor::pos());
    if (targetScreen == nullptr)
    {
        targetScreen = QGuiApplication::primaryScreen();
    }
    if (targetScreen != nullptr)
    {
        const QRect screenGeo = targetScreen->geometry();
        // 窗口尺寸为屏幕的 90%
        const int windowWidth = qMax(minimumWidth(), static_cast<int>(screenGeo.width() * 0.9));
        const int windowHeight = qMax(minimumHeight(), static_cast<int>(screenGeo.height() * 0.9));
        resize(windowWidth, windowHeight);
        // 水平及垂直居中：屏幕中心减去窗口尺寸的一半
        const int x = screenGeo.center().x() - windowWidth / 2;
        const int y = screenGeo.center().y() - windowHeight / 2;
        move(x, y);
    }
    if (isMinimized())
    {
        showNormal();
    }
    showNormal();
    raise();
    activateWindow();
}

void ManagementWindow::setStatusMessage(const QString &message, bool warning)
{
    if (statusLabel == nullptr)
    {
        return;
    }
    statusLabel->setProperty("warning", warning);
    statusLabel->style()->unpolish(statusLabel);
    statusLabel->style()->polish(statusLabel);
    statusLabel->setText(message);
}

void ManagementWindow::notifyApplyResult(const QStringList &warnings)
{
    if (warnings.isEmpty())
    {
        setStatusMessage(QString::fromUtf8("已保存并热应用，配置：%1")
                             .arg(ConfigStore::jsonPath(QApplication::applicationDirPath())));
        return;
    }
    setStatusMessage(QString::fromUtf8("已保存并应用，但存在以下提示：\n%1").arg(warnings.join(QStringLiteral("\n"))),
                     true);
}

void ManagementWindow::closeEvent(QCloseEvent *event)
{
    // 隐藏管理界面时清除所有大屏的红色高亮边框
    emit screenHighlightChanged(0);
    // 关闭仅隐藏，保持管理界面状态与运行时展示不受影响
    hide();
    event->ignore();
}

void ManagementWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // 显示管理界面时，如果已选中某块屏幕，立即在大屏上显示红色边框
    if (layoutCanvas != nullptr && m_config.screenList.size() > 0)
    {
        const int row = layoutCanvas->selectedRow();
        if (row >= 0 && row < m_config.screenList.size())
        {
            emit screenHighlightChanged(m_config.screenList[row].screenIndex);
        }
    }
}

void ManagementWindow::mousePressEvent(QMouseEvent *event)
{
    // 全屏状态下窗口位置由系统接管，不再响应拖动
    if (isFullScreen())
    {
        QWidget::mousePressEvent(event);
        return;
    }
    // 无边框窗口没有系统标题栏，按住窗口空白处（标题、说明文字等）即可拖动
    if (event->button() == Qt::LeftButton)
    {
        m_dragging = true;
        m_dragOffset = event->globalPos() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ManagementWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton))
    {
        move(event->globalPos() - m_dragOffset);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void ManagementWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_dragging = false;
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void ManagementWindow::onSave()
{
    QString error;
    if (!validate(&error))
    {
        QMessageBox::warning(this, QString::fromUtf8("无法保存"), error);
        return;
    }
    const MultiScreenConfig submitted = buildSubmitConfig();
    const QString directory = QApplication::applicationDirPath();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    setStatusMessage(QString::fromUtf8("正在保存并应用，请稍候..."));
    QApplication::processEvents();
    const bool saved = ConfigStore::save(directory, submitted, &error);
    QApplication::restoreOverrideCursor();
    if (!saved)
    {
        QMessageBox::critical(this, QString::fromUtf8("保存失败"), error);
        setStatusMessage(QString::fromUtf8("保存失败：%1").arg(error), true);
        return;
    }

    m_config = submitted;
    clearDirty();
    setStatusMessage(QString::fromUtf8("已写入 %1，正在应用...").arg(ConfigStore::jsonPath(directory)));
    // 同步应用：主程序会在此处调用 DisplayRuntime::apply()，完成后回调 notifyApplyResult()
    emit applyRequested(submitted);
}

void ManagementWindow::onReload()
{
    if (m_dirty)
    {
        if (QMessageBox::question(this, QString::fromUtf8("重新加载"),
                                  QString::fromUtf8("存在未保存的修改，确定放弃并重新从磁盘加载？"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        {
            return;
        }
    }
    MultiScreenConfig loaded;
    QString error;
    if (!ConfigStore::load(QApplication::applicationDirPath(), &loaded, &error))
    {
        QMessageBox::critical(this, QString::fromUtf8("加载失败"), error);
        return;
    }
    setConfig(loaded);
    setStatusMessage(QString::fromUtf8("已重新加载配置"));
}

void ManagementWindow::onSoftwareDropped(int screenRow, const QString &softwareId, int insertRow)
{
    if (screenRow < 0 || screenRow >= m_config.screenList.size())
    {
        return;
    }
    const SoftwareItem *software = findSoftware(softwareId);
    if (software == nullptr)
    {
        return;
    }
    // 同一软件只允许绑定到一块屏幕：已绑定到任意屏幕时拒绝再次绑定。
    // 软件清单侧已通过去掉拖拽标志阻止拖出，这里做二次防御，兼容历史配置或其它入口
    const int boundRow = boundScreenRow(softwareId);
    if (boundRow >= 0)
    {
        setStatusMessage(QString::fromUtf8("“%1”已绑定到 %2 号屏，请先移除后再绑定。")
                             .arg(software->name)
                             .arg(boundRow + 1),
                         true);
        return;
    }

    ScreenConfig &target = m_config.screenList[screenRow];
    int insertPosition = insertRow;
    if (insertPosition < 0 || insertPosition > target.assignments.size())
    {
        insertPosition = target.assignments.size();
    }
    ScreenAssignment assignment;
    assignment.softwareId = softwareId;
    assignment.tabName = software->name;
    target.assignments.insert(insertPosition, assignment);
    target.appCount = target.assignments.size();

    layoutCanvas->refresh();
    // 绑定关系变化后立即刷新软件清单，让“已绑定 N号屏”标记与禁止拖拽状态同步生效；
    // 这里只更新标记而不重建列表，避免拖拽完成后选中项与滚动位置跳回顶部
    libraryPanel->refreshBindingMarks();
    markDirty();
    setStatusMessage(QString::fromUtf8("已将“%1”绑定到 %2 号屏。").arg(software->name).arg(screenRow + 1));
}

void ManagementWindow::onGlobalSettingChanged()
{
    m_config.showIndex = showIndexCheck->isChecked();
    m_config.sortScreen = sortScreenCheck->isChecked();
    m_config.debugMode = debugModeCheck->isChecked();
    m_config.runAsAdministrator = runAsAdminCheck->isChecked();
    m_config.disableWinKey = disableWinKeyCheck->isChecked();
    m_config.stripAppBorder = stripAppBorderCheck->isChecked();
    m_config.disableGpuAcceleration = disableGpuAccelerationCheck->isChecked();
    for (int i = 0; i < m_config.screenList.size(); i++)
    {
        m_config.screenList[i].showIndex = m_config.showIndex;
        m_config.screenList[i].sortScreen = m_config.sortScreen;
        m_config.screenList[i].debugMode = m_config.debugMode;
        m_config.screenList[i].stripAppBorder = m_config.stripAppBorder;
    }
    markDirty();
}

void ManagementWindow::markDirty()
{
    m_dirty = true;
    if (dirtyLabel != nullptr)
    {
        dirtyLabel->setVisible(true);
    }
}

void ManagementWindow::clearDirty()
{
    m_dirty = false;
    if (dirtyLabel != nullptr)
    {
        dirtyLabel->setVisible(false);
    }
}

bool ManagementWindow::validate(QString *error) const
{
    if (m_config.screenList.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QString::fromUtf8("至少需要保留一块屏幕。");
        }
        return false;
    }
    // 引用完整性 + 同屏去重 + 跨屏唯一（同一软件只能绑定一块屏幕）
    for (int i = 0; i < m_config.screenList.size(); i++)
    {
        const ScreenConfig &screen = m_config.screenList[i];
        for (int j = 0; j < screen.assignments.size(); j++)
        {
            const QString &softwareId = screen.assignments[j].softwareId;
            const SoftwareItem *software = findSoftware(softwareId);
            if (software == nullptr)
            {
                if (error != nullptr)
                {
                    *error = QString::fromUtf8("%1号屏引用了不存在的软件，请重新分配后再保存。").arg(i + 1);
                }
                return false;
            }
            for (int k = 0; k < j; k++)
            {
                if (screen.assignments[k].softwareId == softwareId)
                {
                    if (error != nullptr)
                    {
                        *error = QString::fromUtf8("%1号屏重复分配了“%2”。").arg(i + 1).arg(software->name);
                    }
                    return false;
                }
            }
            // 同一软件只允许绑定到一块屏幕（网页也不例外），
            // 用于拦截历史配置或其它入口产生的跨屏重复绑定
            for (int m = 0; m < m_config.screenList.size(); m++)
            {
                if (m == i)
                {
                    continue;
                }
                for (int n = 0; n < m_config.screenList[m].assignments.size(); n++)
                {
                    if (m_config.screenList[m].assignments[n].softwareId == softwareId)
                    {
                        if (error != nullptr)
                        {
                            *error = QString::fromUtf8("“%1”同时分配在 %2 号屏与 %3 号屏，同一软件只能绑定一块屏幕。")
                                         .arg(software->name)
                                         .arg(i + 1)
                                         .arg(m + 1);
                        }
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

MultiScreenConfig ManagementWindow::buildSubmitConfig() const
{
    MultiScreenConfig result = m_config;
    result.count = result.screenList.size();
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (int i = 0; i < result.screenList.size(); i++)
    {
        ScreenConfig &screen = result.screenList[i];
        // 序号始终等于卡片顺序，保证 Alt+N 与界面一致
        screen.screenIndex = i + 1;
        screen.showIndex = result.showIndex;
        screen.sortScreen = result.sortScreen;
        screen.debugMode = result.debugMode;
        screen.stripAppBorder = result.stripAppBorder;
        // 旧配置没有物理屏标识：首次保存时按当前连接顺序补齐，后续插拔也能稳定匹配
        if (screen.displayName.isEmpty() && i < screens.size())
        {
            screen.displayName = screens[i]->name();
            screen.geometry = screens[i]->geometry();
            screen.primaryScreen = (screens[i] == QGuiApplication::primaryScreen());
        }
    }
    ConfigStore::rebuildRuntimeApps(&result);
    return result;
}

const SoftwareItem *ManagementWindow::findSoftware(const QString &softwareId) const
{
    if (softwareId.isEmpty())
    {
        return nullptr;
    }
    for (int i = 0; i < m_config.softwareList.size(); i++)
    {
        if (m_config.softwareList[i].id == softwareId)
        {
            return &m_config.softwareList[i];
        }
    }
    return nullptr;
}

int ManagementWindow::boundScreenRow(const QString &softwareId) const
{
    if (softwareId.isEmpty())
    {
        return -1;
    }
    for (int i = 0; i < m_config.screenList.size(); i++)
    {
        const QList<ScreenAssignment> &assignments = m_config.screenList[i].assignments;
        for (int j = 0; j < assignments.size(); j++)
        {
            if (assignments[j].softwareId == softwareId)
            {
                return i;
            }
        }
    }
    return -1;
}

void ManagementWindow::onScreenSelected(int row)
{
    // 选中屏幕时更新大屏高亮
    if (m_config.screenList.size() > 0 && row >= 0 && row < m_config.screenList.size())
    {
        emit screenHighlightChanged(m_config.screenList[row].screenIndex);
    }
    else
    {
        emit screenHighlightChanged(0);
    }
    // 刷新右侧选中屏幕的软件清单
    refreshScreenSoftwareList(row);
}

void ManagementWindow::onLibrarySelectionChanged(const QString &softwareId)
{
    // 左侧软件清单选中变化，更新底部预览
    updateSoftwarePreview(softwareId);
}

void ManagementWindow::onScreenSoftwareClicked(QListWidgetItem *item)
{
    if (item == nullptr)
    {
        return;
    }
    const QString softwareId = item->data(Qt::UserRole).toString();
    updateSoftwarePreview(softwareId);
}

void ManagementWindow::refreshScreenSoftwareList(int screenRow)
{
    if (screenSoftwareList == nullptr)
    {
        return;
    }
    screenSoftwareList->clear();
    if (screenRow < 0 || screenRow >= m_config.screenList.size())
    {
        screenListTitle->setText(QString::fromUtf8("选中屏幕的软件"));
        return;
    }
    const ScreenConfig &screen = m_config.screenList[screenRow];
    screenListTitle->setText(QString::fromUtf8("%1号屏的软件").arg(screen.screenIndex));

    for (int i = 0; i < screen.assignments.size(); i++)
    {
        const ScreenAssignment &assignment = screen.assignments[i];
        const SoftwareItem *item = findSoftware(assignment.softwareId);
        QListWidgetItem *listItem = new QListWidgetItem(screenSoftwareList);
        listItem->setData(Qt::UserRole, assignment.softwareId);
        const QString name = item != nullptr ? item->name : assignment.softwareId;
        listItem->setText(QString("%1. %2").arg(i + 1).arg(name));
        // 预览图标
        if (item != nullptr && !item->previewPath.isEmpty())
        {
            QPixmap pix(item->previewPath);
            if (!pix.isNull())
            {
                listItem->setIcon(QIcon(pix.scaled(64, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            }
        }
        const QString tabName = assignment.tabName.isEmpty() ? name : assignment.tabName;
        listItem->setToolTip(QString::fromUtf8("标签名称：%1\n软件：%2").arg(tabName).arg(name));
        listItem->setSizeHint(QSize(0, 42));
        screenSoftwareList->addItem(listItem);
    }
}

void ManagementWindow::updateSoftwarePreview(const QString &softwareId)
{
    if (softwareId.isEmpty())
    {
        m_currentPreviewId.clear();
        previewImageLabel->clear();
        previewImageLabel->setText(QString::fromUtf8("暂无预览图"));
        previewNameLabel->setText(QString());
        previewTypeLabel->setText(QString());
        previewPathLabel->setText(QString());
        previewWindowTitleLabel->setText(QString());
        previewRemarkLabel->setText(QString());
        return;
    }
    const SoftwareItem *item = findSoftware(softwareId);
    if (item == nullptr)
    {
        return;
    }
    m_currentPreviewId = softwareId;

    // 预览图
    if (!item->previewPath.isEmpty())
    {
        QPixmap pix(item->previewPath);
        if (!pix.isNull())
        {
            previewImageLabel->setPixmap(pix.scaled(previewImageLabel->size(), Qt::KeepAspectRatio,
                                                    Qt::SmoothTransformation));
        }
        else
        {
            previewImageLabel->clear();
            previewImageLabel->setText(QString::fromUtf8("预览图加载失败"));
        }
    }
    else
    {
        previewImageLabel->clear();
        previewImageLabel->setText(QString::fromUtf8("暂无预览图"));
    }

    // 名称与类型
    previewNameLabel->setText(item->name);
    QString typeText;
    if (item->type == QLatin1String("desktop"))
    {
        typeText = QString::fromUtf8("桌面应用");
    }
    else if (item->type == QLatin1String("web"))
    {
        typeText = QString::fromUtf8("网页");
    }
    else if (item->type == QLatin1String("ppt"))
    {
        typeText = QString::fromUtf8("演示文稿");
    }
    else
    {
        typeText = item->type;
    }
    // 已绑定屏幕信息
    const int bindRow = boundScreenRow(softwareId);
    if (bindRow >= 0 && bindRow < m_config.screenList.size())
    {
        typeText += QString::fromUtf8(" · 已绑定 %1 号屏").arg(m_config.screenList[bindRow].screenIndex);
    }
    previewTypeLabel->setText(typeText);

    // 路径 / URL
    previewPathLabel->setText(item->path.isEmpty() ? QString::fromUtf8("（未设置）") : item->path);

    // 窗口标题
    previewWindowTitleLabel->setText(
        item->windowTitle.isEmpty() ? QString::fromUtf8("（未设置）") : item->windowTitle);

    // 备注
    previewRemarkLabel->setText(item->remark.isEmpty() ? QString::fromUtf8("（无）") : item->remark);
}
