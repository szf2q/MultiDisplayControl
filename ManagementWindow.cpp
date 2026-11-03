#include "ManagementWindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QFile>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
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
#include "SoftwarePreviewPanel.h"

ManagementWindow::ManagementWindow(QWidget *parent) : QWidget(parent)
{
    setWindowTitle(QStringLiteral("多屏控制管理"));
    setObjectName(QStringLiteral("managementWindow"));
    // 无边框窗口：去掉系统标题栏与阴影边框，由窗口内部的标题区承担拖动与关闭职责
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    resize(1500, 950);
    setMinimumSize(1200, 800);

    // 只对管理窗口自身应用样式，避免影响全屏展示窗口
    QFile styleFile(QStringLiteral(":/qss/manager.css"));
    if (styleFile.open(QIODevice::ReadOnly))
    {
        setStyleSheet(QString::fromUtf8(styleFile.readAll()));
        styleFile.close();
    }

    // ==================== 顶部标题栏 ====================
    titleLabel = new QLabel(QStringLiteral("多屏控制管理"), this);
    titleLabel->setObjectName(QStringLiteral("managerTitle"));
    QLabel *subtitleLabel = new QLabel(
        QStringLiteral("Alt+M 显示/隐藏 · 保存后立即热应用（调试模式、管理员权限需重启主程序）"), this);
    subtitleLabel->setObjectName(QStringLiteral("hintLabel"));
    dirtyLabel = new QLabel(QStringLiteral("未保存"), this);
    dirtyLabel->setObjectName(QStringLiteral("dirtyLabel"));
    dirtyLabel->setVisible(false);

    saveButton = new QPushButton(QStringLiteral("保存并应用"), this);
    saveButton->setObjectName(QStringLiteral("primaryButton"));
    reloadButton = new QPushButton(QStringLiteral("重新加载"), this);
    closeButton = new QPushButton(QStringLiteral("关闭"), this);

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
    showIndexCheck = new QCheckBox(QStringLiteral("显示屏幕序号"), this);
    sortScreenCheck = new QCheckBox(QStringLiteral("按位置排序屏幕"), this);
    debugModeCheck = new QCheckBox(QStringLiteral("调试模式（需重启）"), this);
    runAsAdminCheck = new QCheckBox(QStringLiteral("以管理员运行（需重启）"), this);

    QHBoxLayout *settingLayout = new QHBoxLayout();
    settingLayout->addWidget(showIndexCheck);
    settingLayout->addWidget(sortScreenCheck);
    settingLayout->addWidget(debugModeCheck);
    settingLayout->addWidget(runAsAdminCheck);
    settingLayout->addStretch(1);

    // ==================== 三栏内容 ====================
    libraryPanel = new SoftwareLibraryPanel(this);
    QWidget *libraryGroup = new QWidget(this);
    libraryGroup->setObjectName(QStringLiteral("panelGroup"));
    QVBoxLayout *libraryLayout = new QVBoxLayout(libraryGroup);
    libraryLayout->setContentsMargins(10, 10, 10, 10);
    QLabel *libraryTitle = new QLabel(QStringLiteral("软件清单"), libraryGroup);
    libraryTitle->setObjectName(QStringLiteral("groupTitle"));
    libraryLayout->addWidget(libraryTitle);
    libraryLayout->addWidget(libraryPanel, 1);

    QWidget *canvasGroup = new QWidget(this);
    canvasGroup->setObjectName(QStringLiteral("panelGroup"));
    QVBoxLayout *canvasLayout = new QVBoxLayout(canvasGroup);
    canvasLayout->setContentsMargins(10, 10, 10, 10);
    QLabel *canvasTitle = new QLabel(QStringLiteral("屏幕布局（拖拽软件到卡片，拖动手柄调整屏序）"), canvasGroup);
    canvasTitle->setObjectName(QStringLiteral("groupTitle"));
    layoutCanvas = new ScreenLayoutCanvas(canvasGroup);
    canvasLayout->addWidget(canvasTitle);
    canvasLayout->addWidget(layoutCanvas, 1);

    previewPanel = new SoftwarePreviewPanel(this);
    QWidget *previewGroup = new QWidget(this);
    previewGroup->setObjectName(QStringLiteral("panelGroup"));
    QVBoxLayout *previewLayout = new QVBoxLayout(previewGroup);
    previewLayout->setContentsMargins(10, 10, 10, 10);
    QLabel *previewTitle = new QLabel(QStringLiteral("预览"), previewGroup);
    previewTitle->setObjectName(QStringLiteral("groupTitle"));
    previewLayout->addWidget(previewTitle);
    previewLayout->addWidget(previewPanel, 1);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(libraryGroup);
    splitter->addWidget(canvasGroup);
    splitter->addWidget(previewGroup);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes(QList<int>() << 340 << 760 << 380);
    splitter->setChildrenCollapsible(false);

    // ==================== 状态栏 ====================
    statusLabel = new QLabel(this);
    statusLabel->setObjectName(QStringLiteral("statusLabel"));
    statusLabel->setWordWrap(true);
    setStatusMessage(QStringLiteral("配置文件：%1").arg(ConfigStore::jsonPath(QApplication::applicationDirPath())));

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 14, 16, 12);
    mainLayout->addLayout(headerLayout);
    mainLayout->addLayout(settingLayout);
    mainLayout->addWidget(splitter, 1);
    mainLayout->addWidget(statusLabel);

    connect(saveButton, &QPushButton::clicked, this, [this]() { onSave(); });
    connect(reloadButton, &QPushButton::clicked, this, [this]() { onReload(); });
    connect(closeButton, &QPushButton::clicked, this, [this]() { hide(); });
    connect(showIndexCheck, &QCheckBox::toggled, this, [this](bool) { onGlobalSettingChanged(); });
    connect(sortScreenCheck, &QCheckBox::toggled, this, [this](bool) { onGlobalSettingChanged(); });
    connect(debugModeCheck, &QCheckBox::toggled, this, [this](bool) { onGlobalSettingChanged(); });
    connect(runAsAdminCheck, &QCheckBox::toggled, this, [this](bool) { onGlobalSettingChanged(); });

    connect(libraryPanel, &SoftwareLibraryPanel::selectionChanged, this,
            [this](const QString &softwareId) { onSoftwareSelected(softwareId); });
    connect(libraryPanel, &SoftwareLibraryPanel::configChanged, this, [this]() {
        if (layoutCanvas != nullptr)
        {
            layoutCanvas->refresh();
        }
        // 软件被改名或删除后，预览面板需要跟随刷新
        refreshPreview();
        markDirty();
    });
    connect(layoutCanvas, &ScreenLayoutCanvas::screenSelected, this,
            [this](int row) { onScreenSelected(row); });
    connect(layoutCanvas, &ScreenLayoutCanvas::configChanged, this, [this]() { markDirty(); });
    connect(layoutCanvas, &ScreenLayoutCanvas::softwareDropped, this,
            [this](int screenRow, const QString &softwareId, int insertRow) {
                onSoftwareDropped(screenRow, softwareId, insertRow);
            });
}

void ManagementWindow::setConfig(const MultiScreenConfig &config)
{
    m_config = config;
    m_config.count = m_config.screenList.size();
    for (int i = 0; i < m_config.screenList.size(); i++)
    {
        m_config.screenList[i].screenIndex = i + 1;
    }

    // 回填全局开关，阻塞信号避免触发“未保存”标记
    const QList<QCheckBox *> checks = QList<QCheckBox *>() << showIndexCheck << sortScreenCheck << debugModeCheck
                                                           << runAsAdminCheck;
    for (int i = 0; i < checks.size(); i++)
    {
        checks[i]->blockSignals(true);
    }
    showIndexCheck->setChecked(m_config.showIndex);
    sortScreenCheck->setChecked(m_config.sortScreen);
    debugModeCheck->setChecked(m_config.debugMode);
    runAsAdminCheck->setChecked(m_config.runAsAdministrator);
    for (int i = 0; i < checks.size(); i++)
    {
        checks[i]->blockSignals(false);
    }

    libraryPanel->setConfig(&m_config);
    layoutCanvas->setConfig(&m_config);
    if (m_config.screenList.isEmpty())
    {
        previewPanel->showScreen(nullptr, nullptr);
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
    if (isMinimized())
    {
        showNormal();
    }
    show();
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
        setStatusMessage(QStringLiteral("已保存并热应用，配置：%1")
                             .arg(ConfigStore::jsonPath(QApplication::applicationDirPath())));
        return;
    }
    setStatusMessage(QStringLiteral("已保存并应用，但存在以下提示：\n%1").arg(warnings.join(QStringLiteral("\n"))),
                     true);
}

void ManagementWindow::closeEvent(QCloseEvent *event)
{
    // 关闭仅隐藏，保持管理界面状态与运行时展示不受影响
    hide();
    event->ignore();
}

void ManagementWindow::mousePressEvent(QMouseEvent *event)
{
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
        QMessageBox::warning(this, QStringLiteral("无法保存"), error);
        return;
    }
    const MultiScreenConfig submitted = buildSubmitConfig();
    const QString directory = QApplication::applicationDirPath();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    setStatusMessage(QStringLiteral("正在保存并应用，请稍候..."));
    QApplication::processEvents();
    const bool saved = ConfigStore::save(directory, submitted, &error);
    QApplication::restoreOverrideCursor();
    if (!saved)
    {
        QMessageBox::critical(this, QStringLiteral("保存失败"), error);
        setStatusMessage(QStringLiteral("保存失败：%1").arg(error), true);
        return;
    }

    m_config = submitted;
    clearDirty();
    setStatusMessage(QStringLiteral("已写入 %1，正在应用...").arg(ConfigStore::jsonPath(directory)));
    // 同步应用：主程序会在此处调用 DisplayRuntime::apply()，完成后回调 notifyApplyResult()
    emit applyRequested(submitted);
}

void ManagementWindow::onReload()
{
    if (m_dirty)
    {
        if (QMessageBox::question(this, QStringLiteral("重新加载"),
                                  QStringLiteral("存在未保存的修改，确定放弃并重新从磁盘加载？"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        {
            return;
        }
    }
    MultiScreenConfig loaded;
    QString error;
    if (!ConfigStore::load(QApplication::applicationDirPath(), &loaded, &error))
    {
        QMessageBox::critical(this, QStringLiteral("加载失败"), error);
        return;
    }
    setConfig(loaded);
    setStatusMessage(QStringLiteral("已重新加载配置"));
}

void ManagementWindow::onSoftwareSelected(const QString &softwareId)
{
    previewPanel->showSoftware(findSoftware(softwareId));
    const int row = layoutCanvas->selectedRow();
    if (row >= 0 && row < m_config.screenList.size())
    {
        previewPanel->showScreen(&m_config.screenList[row], &m_config.softwareList);
    }
}

void ManagementWindow::onScreenSelected(int row)
{
    if (row < 0 || row >= m_config.screenList.size())
    {
        previewPanel->showScreen(nullptr, nullptr);
        return;
    }
    previewPanel->showScreen(&m_config.screenList[row], &m_config.softwareList);
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
    ScreenConfig &target = m_config.screenList[screenRow];
    for (int i = 0; i < target.assignments.size(); i++)
    {
        if (target.assignments[i].softwareId == softwareId)
        {
            setStatusMessage(QStringLiteral("“%1”已在该屏幕上，不能重复添加。").arg(software->name), true);
            return;
        }
    }

    // 分配规则：桌面应用独占，网页可复用；桌面应用拖到新屏时自动从原屏移除
    int movedFromRow = -1;
    if (software->type != "web")
    {
        for (int i = 0; i < m_config.screenList.size(); i++)
        {
            if (i == screenRow)
            {
                continue;
            }
            QList<ScreenAssignment> &assignments = m_config.screenList[i].assignments;
            for (int j = assignments.size() - 1; j >= 0; j--)
            {
                if (assignments[j].softwareId != softwareId)
                {
                    continue;
                }
                assignments.removeAt(j);
                m_config.screenList[i].appCount = assignments.size();
                movedFromRow = i;
            }
        }
    }

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
    markDirty();
    onScreenSelected(screenRow);

    if (movedFromRow >= 0)
    {
        setStatusMessage(QStringLiteral("“%1”已从 %2 号屏移入 %3 号屏（桌面应用独占）。")
                             .arg(software->name)
                             .arg(movedFromRow + 1)
                             .arg(screenRow + 1));
    }
    else
    {
        setStatusMessage(QStringLiteral("已将“%1”放入 %2 号屏。").arg(software->name).arg(screenRow + 1));
    }
}

void ManagementWindow::onGlobalSettingChanged()
{
    m_config.showIndex = showIndexCheck->isChecked();
    m_config.sortScreen = sortScreenCheck->isChecked();
    m_config.debugMode = debugModeCheck->isChecked();
    m_config.runAsAdministrator = runAsAdminCheck->isChecked();
    for (int i = 0; i < m_config.screenList.size(); i++)
    {
        m_config.screenList[i].showIndex = m_config.showIndex;
        m_config.screenList[i].sortScreen = m_config.sortScreen;
        m_config.screenList[i].debugMode = m_config.debugMode;
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

void ManagementWindow::refreshPreview()
{
    onSoftwareSelected(libraryPanel->currentSoftwareId());
}

bool ManagementWindow::validate(QString *error) const
{
    if (m_config.screenList.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("至少需要保留一块屏幕。");
        }
        return false;
    }
    // 引用完整性 + 同屏去重 + 桌面应用独占
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
                    *error = QStringLiteral("%1号屏引用了不存在的软件，请重新分配后再保存。").arg(i + 1);
                }
                return false;
            }
            for (int k = 0; k < j; k++)
            {
                if (screen.assignments[k].softwareId == softwareId)
                {
                    if (error != nullptr)
                    {
                        *error = QStringLiteral("%1号屏重复分配了“%2”。").arg(i + 1).arg(software->name);
                    }
                    return false;
                }
            }
            if (software->type == "web")
            {
                continue;
            }
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
                            *error = QStringLiteral("桌面应用“%1”同时分配在 %2 号屏与 %3 号屏，只能保留一处。")
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
