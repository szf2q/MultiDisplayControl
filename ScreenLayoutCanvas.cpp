#include "ScreenLayoutCanvas.h"

#include <QApplication>
#include <QDebug>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>

#include "ConfigStore.h"

namespace
{
    /** @brief 屏幕数量上限，与 Alt+1~7 热键保持一致 */
    const int kMaxScreenCount = 7;
}  // namespace

PhysicalScreenMap::PhysicalScreenMap(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setCursor(Qt::PointingHandCursor);
}

void PhysicalScreenMap::setConfig(MultiScreenConfig *config)
{
    m_config = config;
    update();
}

void PhysicalScreenMap::setSelectedScreen(const QString &displayName)
{
    m_selected = displayName;
    update();
}

void PhysicalScreenMap::refresh()
{
    update();
}

void PhysicalScreenMap::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(9, 22, 40));

    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty())
    {
        painter.setPen(QColor(150, 180, 210));
        painter.drawText(rect(), Qt::AlignCenter, QString::fromUtf8("未检测到显示器"));
        return;
    }

    // 以所有物理屏的外接矩形为基准等比缩放，最大程度还原真实相对位置
    QRectF bounding;
    for (int i = 0; i < screens.size(); i++)
    {
        bounding = bounding.united(QRectF(screens[i]->geometry()));
    }
    if (bounding.width() <= 0 || bounding.height() <= 0)
    {
        return;
    }
    const double margin = 14;
    const double availableWidth = width() - margin * 2;
    const double availableHeight = height() - margin * 2;
    if (availableWidth <= 0 || availableHeight <= 0)
    {
        return;
    }
    const double scale = qMin(availableWidth / bounding.width(), availableHeight / bounding.height());
    const double offsetX = margin + (availableWidth - bounding.width() * scale) / 2.0;
    const double offsetY = margin + (availableHeight - bounding.height() * scale) / 2.0;

    for (int i = 0; i < screens.size(); i++)
    {
        const QRect geometry = screens[i]->geometry();
        const QRectF targetRect(offsetX + (geometry.x() - bounding.x()) * scale,
                                offsetY + (geometry.y() - bounding.y()) * scale, geometry.width() * scale,
                                geometry.height() * scale);
        const QString &displayName = screens[i]->name();
        const bool selected = (!m_selected.isEmpty() && displayName == m_selected);
        const bool primary = (screens[i] == QGuiApplication::primaryScreen());

        // 选中态：青色边框 + 青色背景（与下方屏幕卡片选中色一致，红色仅用于实际大屏高亮）
        const QColor fillColor = selected ? QColor(18, 82, 130) : QColor(20, 46, 78);
        const QColor borderColor = selected ? QColor(17, 192, 254) : QColor(48, 92, 140);
        const qreal borderWidth = selected ? 3.0 : 1.5;
        painter.setBrush(fillColor);
        painter.setPen(QPen(borderColor, borderWidth));
        painter.drawRoundedRect(targetRect.adjusted(1, 1, -1, -1), 8, 8);

        // 左上角序号徽标：从配置中查找该物理屏对应的屏幕序号
        int screenIndex = 0;
        if (m_config != nullptr)
        {
            for (int j = 0; j < m_config->screenList.size(); j++)
            {
                if (m_config->screenList[j].displayName == displayName)
                {
                    screenIndex = m_config->screenList[j].screenIndex;
                    break;
                }
            }
        }
        if (screenIndex > 0)
        {
            const qreal badgeW = 32;
            const qreal badgeH = 22;
            const QRectF badgeRect(targetRect.left() + 8, targetRect.top() + 8, badgeW, badgeH);
            painter.setBrush(selected ? QColor(17, 192, 254) : QColor(48, 92, 140));
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(badgeRect, 4, 4);
            QFont badgeFont = painter.font();
            badgeFont.setFamily(QString::fromUtf8("微软雅黑"));
            badgeFont.setPixelSize(13);
            badgeFont.setBold(true);
            painter.setFont(badgeFont);
            painter.setPen(QColor(255, 255, 255));
            painter.drawText(badgeRect, Qt::AlignCenter, QString::number(screenIndex));
        }

        QFont font = painter.font();
        font.setFamily(QString::fromUtf8("微软雅黑"));
        font.setPixelSize(13);
        font.setBold(selected || primary);
        painter.setFont(font);
        painter.setPen(selected || primary ? QColor(220, 240, 255) : QColor(170, 195, 220));
        const QString text = QString("%1%2\n%3×%4")
                                 .arg(displayName)
                                 .arg(primary ? QString::fromUtf8("（主屏）") : QString())
                                 .arg(geometry.width())
                                 .arg(geometry.height());
        painter.drawText(targetRect, Qt::AlignCenter, text);
    }
}

void PhysicalScreenMap::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        const int idx = hitTest(event->pos());
        if (idx >= 0)
        {
            const QList<QScreen *> screens = QGuiApplication::screens();
            if (idx < screens.size())
            {
                emit screenClicked(screens[idx]->name());
                return;
            }
        }
    }
    QWidget::mousePressEvent(event);
}

int PhysicalScreenMap::hitTest(const QPoint &pos) const
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty())
    {
        return -1;
    }
    QRectF bounding;
    for (int i = 0; i < screens.size(); i++)
    {
        bounding = bounding.united(QRectF(screens[i]->geometry()));
    }
    if (bounding.width() <= 0 || bounding.height() <= 0)
    {
        return -1;
    }
    const double margin = 14;
    const double availableWidth = width() - margin * 2;
    const double availableHeight = height() - margin * 2;
    if (availableWidth <= 0 || availableHeight <= 0)
    {
        return -1;
    }
    const double scale = qMin(availableWidth / bounding.width(), availableHeight / bounding.height());
    const double offsetX = margin + (availableWidth - bounding.width() * scale) / 2.0;
    const double offsetY = margin + (availableHeight - bounding.height() * scale) / 2.0;

    for (int i = 0; i < screens.size(); i++)
    {
        const QRect geometry = screens[i]->geometry();
        const QRectF targetRect(offsetX + (geometry.x() - bounding.x()) * scale,
                                offsetY + (geometry.y() - bounding.y()) * scale, geometry.width() * scale,
                                geometry.height() * scale);
        if (targetRect.contains(pos))
        {
            return i;
        }
    }
    return -1;
}

AppAssignmentList::AppAssignmentList(QWidget *parent) : QListWidget(parent)
{
    setAcceptDrops(true);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::MoveAction);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setMinimumHeight(120);
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &AppAssignmentList::onContextMenu);
}

QStringList AppAssignmentList::softwareIds() const
{
    QStringList ids;
    for (int i = 0; i < count(); i++)
    {
        ids << item(i)->data(Qt::UserRole).toString();
    }
    return ids;
}

void AppAssignmentList::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(MDC_SOFTWARE_MIME))
    {
        event->acceptProposedAction();
        return;
    }
    QListWidget::dragEnterEvent(event);
}

void AppAssignmentList::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat(MDC_SOFTWARE_MIME))
    {
        event->acceptProposedAction();
        return;
    }
    QListWidget::dragMoveEvent(event);
}

void AppAssignmentList::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasFormat(MDC_SOFTWARE_MIME))
    {
        const QString softwareId = QString::fromUtf8(event->mimeData()->data(MDC_SOFTWARE_MIME));
        QListWidgetItem *target = itemAt(event->pos());
        const int insertRow = (target != nullptr) ? row(target) : count();
        emit softwareDropped(softwareId, insertRow);
        event->acceptProposedAction();
        return;
    }
    // 内部拖动：交给基类完成移动，再通知外部保存新顺序
    QListWidget::dropEvent(event);
    emit orderChanged();
}

void AppAssignmentList::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete && currentRow() >= 0)
    {
        emit removeRequested(currentRow());
        return;
    }
    QListWidget::keyPressEvent(event);
}

void AppAssignmentList::onContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = itemAt(pos);
    if (item == nullptr)
    {
        return;
    }
    setCurrentItem(item);
    QMenu menu(this);
    QAction *removeAction = menu.addAction(QString::fromUtf8("从该屏移除"));
    if (menu.exec(mapToGlobal(pos)) == removeAction)
    {
        emit removeRequested(row(item));
    }
}

ScreenCard::ScreenCard(int row, QWidget *parent) : QFrame(parent), m_row(row)
{
    setObjectName(QStringLiteral("screenCard"));
    setProperty("selected", false);
    setAcceptDrops(true);
    setMinimumWidth(250);
    setMaximumWidth(310);

    indexLabel = new QLabel(this);
    indexLabel->setObjectName(QStringLiteral("screenIndexBadge"));
    indexLabel->setAlignment(Qt::AlignCenter);
    indexLabel->setFixedSize(34, 34);

    nameEdit = new QLineEdit(this);
    nameEdit->setObjectName(QStringLiteral("screenNameEdit"));
    nameEdit->setPlaceholderText(QString::fromUtf8("屏幕名称（可选）"));

    dragHandle = new QLabel(QString::fromUtf8("⣿"), this);
    dragHandle->setObjectName(QStringLiteral("screenDragHandle"));
    dragHandle->setToolTip(QString::fromUtf8("按住此处拖到另一张卡片可调整屏幕序号"));
    dragHandle->setCursor(Qt::SizeAllCursor);
    dragHandle->setAlignment(Qt::AlignCenter);
    dragHandle->setFixedWidth(20);

    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->addWidget(indexLabel);
    headerLayout->addWidget(nameEdit, 1);
    headerLayout->addWidget(dragHandle);

    displayLabel = new QLabel(this);
    displayLabel->setObjectName(QStringLiteral("screenDisplayLabel"));
    displayLabel->setWordWrap(true);

    assignmentList = new AppAssignmentList(this);

    offsetSpin = new QSpinBox(this);
    offsetSpin->setRange(-10000, 10000);
    offsetSpin->setSingleStep(10);
    offsetSpin->setSuffix(QStringLiteral(" px"));
    QLabel *offsetLabel = new QLabel(QString::fromUtf8("标签栏偏移"), this);
    offsetLabel->setObjectName(QStringLiteral("hintLabel"));

    QHBoxLayout *offsetLayout = new QHBoxLayout();
    offsetLayout->setContentsMargins(0, 0, 0, 0);
    offsetLayout->addWidget(offsetLabel);
    offsetLayout->addStretch(1);
    offsetLayout->addWidget(offsetSpin);

    removeButton = new QPushButton(QString::fromUtf8("移除该屏"), this);
    removeButton->setObjectName(QStringLiteral("dangerButton"));

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->addLayout(headerLayout);
    mainLayout->addWidget(displayLabel);
    mainLayout->addWidget(assignmentList, 1);
    mainLayout->addLayout(offsetLayout);
    mainLayout->addWidget(removeButton);

    connect(nameEdit, &QLineEdit::textEdited, this,
            [this](const QString &text) { emit nameChanged(m_row, text); });
    connect(offsetSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
            [this](int value) { emit tabOffsetChanged(m_row, value); });
    connect(removeButton, &QPushButton::clicked, this, [this]() { emit removeRequested(m_row); });
    connect(assignmentList, &AppAssignmentList::softwareDropped, this,
            [this](const QString &softwareId, int insertRow) {
                emit softwareDropped(m_row, softwareId, insertRow);
            });
    connect(assignmentList, &AppAssignmentList::orderChanged, this,
            [this]() { emit assignmentOrderChanged(m_row); });
    connect(assignmentList, &AppAssignmentList::removeRequested, this,
            [this](int listRow) { emit assignmentRemoveRequested(m_row, listRow); });
}

void ScreenCard::setScreen(int row, const ScreenConfig &screen, const MultiScreenConfig *config)
{
    m_row = row;
    indexLabel->setText(QString::number(row + 1));

    nameEdit->blockSignals(true);
    nameEdit->setText(screen.screenTitle);
    nameEdit->blockSignals(false);

    // 主屏固定为 1 号屏：禁用拖拽手柄与移除按钮，并在序号徽标上标注「主」
    const bool isPrimary = screen.primaryScreen;
    if (isPrimary)
    {
        indexLabel->setText(QString::fromUtf8("1(主)"));
        dragHandle->setEnabled(false);
        dragHandle->setToolTip(QString::fromUtf8("主屏固定为 1 号屏，不可调整顺序"));
        dragHandle->setCursor(Qt::ArrowCursor);
        dragHandle->setObjectName(QStringLiteral("screenDragHandleDisabled"));
        removeButton->setEnabled(false);
        removeButton->setToolTip(QString::fromUtf8("主屏不可移除"));
    }
    else
    {
        dragHandle->setEnabled(true);
        dragHandle->setToolTip(QString::fromUtf8("按住此处拖到另一张卡片可调整屏幕序号"));
        dragHandle->setCursor(Qt::SizeAllCursor);
        dragHandle->setObjectName(QStringLiteral("screenDragHandle"));
        removeButton->setEnabled(true);
        removeButton->setToolTip(QString());
    }
    // 强制刷新样式（objectName 改变后 QSS 选择器需要重新匹配）
    dragHandle->style()->unpolish(dragHandle);
    dragHandle->style()->polish(dragHandle);

    if (screen.displayName.isEmpty())
    {
        displayLabel->setText(QString::fromUtf8("未绑定物理屏，保存后按序号回退匹配"));
    }
    else
    {
        const QString primaryFlag = isPrimary ? QString::fromUtf8(" · 主屏") : QString();
        displayLabel->setText(QString("%1 · %2×%3%4")
                                  .arg(screen.displayName)
                                  .arg(screen.geometry.width())
                                  .arg(screen.geometry.height())
                                  .arg(primaryFlag));
    }

    offsetSpin->blockSignals(true);
    offsetSpin->setValue(screen.tabOffsetX);
    offsetSpin->blockSignals(false);

    assignmentList->clear();
    for (int i = 0; i < screen.assignments.size(); i++)
    {
        const ScreenAssignment &assignment = screen.assignments[i];
        QString name = assignment.softwareId;
        QString path;
        if (config != nullptr)
        {
            for (int j = 0; j < config->softwareList.size(); j++)
            {
                if (config->softwareList[j].id == assignment.softwareId)
                {
                    name = assignment.tabName.isEmpty() ? config->softwareList[j].name : assignment.tabName;
                    path = config->softwareList[j].path;
                    break;
                }
            }
        }
        QListWidgetItem *item = new QListWidgetItem(assignmentList);
        item->setText(QString("%1. %2").arg(i + 1).arg(name));
        item->setToolTip(path);
        item->setData(Qt::UserRole, assignment.softwareId);
        item->setSizeHint(QSize(0, 34));
    }
    setSelected(property("selected").toBool());
}

void ScreenCard::setRow(int row)
{
    m_row = row;
    indexLabel->setText(QString::number(row + 1));
}

void ScreenCard::setSelected(bool selected)
{
    setProperty("selected", selected);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

QStringList ScreenCard::assignmentIds() const
{
    return assignmentList->softwareIds();
}

void ScreenCard::mousePressEvent(QMouseEvent *event)
{
    emit selected(m_row);
    // 主屏禁用拖拽，只有启用状态下的手柄才允许发起屏幕卡片拖拽
    if (event->button() == Qt::LeftButton && dragHandle != nullptr && dragHandle->isEnabled() &&
        dragHandle->geometry().contains(event->pos()))
    {
        // 从拖拽手柄发起屏幕卡片拖拽，数据为当前行号，落在其它卡片上即调整序号
        QMimeData *mimeData = new QMimeData();
        mimeData->setData(MDC_SCREEN_MIME, QByteArray::number(m_row));
        QDrag *drag = new QDrag(this);
        drag->setMimeData(mimeData);
        drag->exec(Qt::MoveAction);
        return;
    }
    QFrame::mousePressEvent(event);
}

void ScreenCard::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(MDC_SCREEN_MIME))
    {
        // 主屏卡片不接受其它屏幕的拖拽（主屏固定为 1 号屏）
        if (dragHandle != nullptr && !dragHandle->isEnabled())
        {
            event->ignore();
            return;
        }
        event->acceptProposedAction();
        return;
    }
    QFrame::dragEnterEvent(event);
}

void ScreenCard::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasFormat(MDC_SCREEN_MIME))
    {
        bool ok = false;
        const int fromRow = QString::fromUtf8(event->mimeData()->data(MDC_SCREEN_MIME)).toInt(&ok);
        if (ok)
        {
            emit screenMoved(fromRow, m_row);
        }
        event->acceptProposedAction();
        return;
    }
    QFrame::dropEvent(event);
}

ScreenLayoutCanvas::ScreenLayoutCanvas(QWidget *parent) : QWidget(parent)
{
    screenMap = new PhysicalScreenMap(this);
    // 点击物理屏示意图上的屏幕，同步选中下方对应的屏幕卡片
    connect(screenMap, &PhysicalScreenMap::screenClicked, this,
            &ScreenLayoutCanvas::onScreenMapClicked);

    cardContainer = new QWidget();
    QHBoxLayout *cardLayout = new QHBoxLayout(cardContainer);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(12);
    cardLayout->addStretch(1);

    cardScroll = new QScrollArea(this);
    cardScroll->setWidgetResizable(true);
    cardScroll->setFrameShape(QFrame::NoFrame);
    cardScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    cardScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    cardScroll->setWidget(cardContainer);

    // 添加/删除入口统一由外部（管理界面顶部操作区）与屏幕卡片提供，此处不再放置按钮
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(screenMap);
    mainLayout->addWidget(cardScroll, 1);

    if (qApp != nullptr)
    {
        connect(qApp, &QGuiApplication::screenAdded, this, [this](QScreen *) {
            if (screenMap != nullptr)
            {
                screenMap->refresh();
            }
        });
        connect(qApp, &QGuiApplication::screenRemoved, this, [this](QScreen *) {
            if (screenMap != nullptr)
            {
                screenMap->refresh();
            }
        });
    }
}

void ScreenLayoutCanvas::setConfig(MultiScreenConfig *config)
{
    m_config = config;
    m_selectedRow = -1;
    if (screenMap != nullptr)
    {
        screenMap->setConfig(m_config);
    }
    // 加载配置后先把主屏移到第一位，保证界面展示与 Alt+1 一致
    if (m_config != nullptr)
    {
        ensurePrimaryFirst();
        renumberScreens();
    }
    refresh();
}

void ScreenLayoutCanvas::refresh()
{
    // 整块卡片区重建：屏幕增删与顺序调整后统一按配置重新生成，避免局部状态不一致
    QWidget *oldContainer = cardScroll->takeWidget();
    if (oldContainer != nullptr)
    {
        oldContainer->deleteLater();
    }
    m_cards.clear();

    cardContainer = new QWidget();
    QHBoxLayout *cardLayout = new QHBoxLayout(cardContainer);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(12);

    if (m_config != nullptr)
    {
        for (int i = 0; i < m_config->screenList.size(); i++)
        {
            ScreenCard *card = new ScreenCard(i, cardContainer);
            card->setScreen(i, m_config->screenList[i], m_config);
            cardLayout->addWidget(card);
            m_cards.append(card);

            connect(card, &ScreenCard::nameChanged, this, &ScreenLayoutCanvas::onNameChanged);
            connect(card, &ScreenCard::tabOffsetChanged, this, &ScreenLayoutCanvas::onTabOffsetChanged);
            connect(card, &ScreenCard::selected, this, &ScreenLayoutCanvas::onSelectScreen);
            connect(card, &ScreenCard::screenMoved, this, &ScreenLayoutCanvas::onScreenMoved);
            connect(card, &ScreenCard::removeRequested, this, [this](int row) {
                onSelectScreen(row);
                onRemoveScreen();
            });
            connect(card, &ScreenCard::softwareDropped, this,
                    [this](int screenRow, const QString &softwareId, int insertRow) {
                        emit softwareDropped(screenRow, softwareId, insertRow);
                    });
            connect(card, &ScreenCard::assignmentOrderChanged, this, [this](int row) {
                if (m_config == nullptr || row < 0 || row >= m_config->screenList.size() ||
                    row >= m_cards.size())
                {
                    return;
                }
                const QStringList orderedIds = m_cards[row]->assignmentIds();
                ScreenConfig &screen = m_config->screenList[row];
                QList<ScreenAssignment> ordered;
                for (int i = 0; i < orderedIds.size(); i++)
                {
                    for (int j = 0; j < screen.assignments.size(); j++)
                    {
                        if (screen.assignments[j].softwareId != orderedIds[i])
                        {
                            continue;
                        }
                        bool duplicated = false;
                        for (int k = 0; k < ordered.size(); k++)
                        {
                            if (ordered[k].softwareId == orderedIds[i])
                            {
                                duplicated = true;
                                break;
                            }
                        }
                        if (!duplicated)
                        {
                            ordered << screen.assignments[j];
                        }
                    }
                }
                screen.assignments = ordered;
                screen.appCount = ordered.size();
                emit configChanged();
            });
            connect(card, &ScreenCard::assignmentRemoveRequested, this, [this](int row, int listRow) {
                if (m_config == nullptr || row < 0 || row >= m_config->screenList.size())
                {
                    return;
                }
                ScreenConfig &screen = m_config->screenList[row];
                if (listRow < 0 || listRow >= screen.assignments.size())
                {
                    return;
                }
                screen.assignments.removeAt(listRow);
                screen.appCount = screen.assignments.size();
                refresh();
                onSelectScreen(row);
                emit configChanged();
            });
        }
    }
    cardLayout->addStretch(1);
    cardScroll->setWidget(cardContainer);

    if (m_config != nullptr && m_selectedRow >= 0 && m_selectedRow < m_cards.size())
    {
        onSelectScreen(m_selectedRow);
    }
    else
    {
        m_selectedRow = -1;
        if (screenMap != nullptr)
        {
            screenMap->setSelectedScreen(QString());
        }
    }
    if (screenMap != nullptr)
    {
        screenMap->refresh();
    }
}

int ScreenLayoutCanvas::selectedRow() const
{
    return m_selectedRow;
}

void ScreenLayoutCanvas::onAddScreen()
{
    if (m_config == nullptr)
    {
        return;
    }
    if (m_config->screenList.size() >= kMaxScreenCount)
    {
        QMessageBox::information(this, QString::fromUtf8("添加屏幕"),
                                 QString::fromUtf8("最多支持 %1 块屏幕（与 Alt+1~7 热键对应）。").arg(kMaxScreenCount));
        return;
    }
    const QString displayName = findFreeScreenName();
    if (displayName.isEmpty())
    {
        QMessageBox::information(this, QString::fromUtf8("添加屏幕"),
                                 QString::fromUtf8("当前没有未绑定的显示器，请先接入更多显示器。"));
        return;
    }

    ScreenConfig screen;
    screen.id = ConfigStore::makeScreenId();
    screen.displayName = displayName;
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); i++)
    {
        if (screens[i]->name() != displayName)
        {
            continue;
        }
        screen.geometry = screens[i]->geometry();
        screen.primaryScreen = (screens[i] == QGuiApplication::primaryScreen());
        break;
    }
    screen.showIndex = m_config->showIndex;
    screen.sortScreen = m_config->sortScreen;
    screen.debugMode = m_config->debugMode;
    screen.tabOffsetX = 0;
    // 主屏加到第一位（固定为 1 号屏），其它屏追加到末尾
    if (screen.primaryScreen)
    {
        m_config->screenList.prepend(screen);
    }
    else
    {
        m_config->screenList.append(screen);
    }
    renumberScreens();
    refresh();
    // 选中刚添加的那块屏
    const int newRow = screen.primaryScreen ? 0 : m_config->screenList.size() - 1;
    onSelectScreen(newRow);
    emit configChanged();
}

void ScreenLayoutCanvas::addScreen()
{
    onAddScreen();
}

void ScreenLayoutCanvas::onRemoveScreen()
{
    if (m_config == nullptr || m_selectedRow < 0 || m_selectedRow >= m_config->screenList.size())
    {
        return;
    }
    if (m_config->screenList.size() <= 1)
    {
        QMessageBox::information(this, QString::fromUtf8("删除屏幕"), QString::fromUtf8("至少需要保留一块屏幕。"));
        return;
    }
    // 主屏固定不可移除
    if (m_config->screenList[m_selectedRow].primaryScreen)
    {
        QMessageBox::information(this, QString::fromUtf8("删除屏幕"),
                                 QString::fromUtf8("主屏固定为 1 号屏，不可移除。"));
        return;
    }
    const int row = m_selectedRow;
    const QString name = m_config->screenList[row].screenTitle.isEmpty()
                             ? QString::fromUtf8("%1号屏").arg(row + 1)
                             : m_config->screenList[row].screenTitle;
    if (QMessageBox::question(this, QString::fromUtf8("删除屏幕"),
                              QString::fromUtf8("确定删除“%1”？该屏上分配的软件会一并移除。").arg(name),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }
    m_config->screenList.removeAt(row);
    m_selectedRow = -1;
    renumberScreens();
    refresh();
    emit configChanged();
}

void ScreenLayoutCanvas::onSelectScreen(int row)
{
    if (m_config == nullptr || row < 0 || row >= m_config->screenList.size())
    {
        return;
    }
    m_selectedRow = row;
    for (int i = 0; i < m_cards.size(); i++)
    {
        m_cards[i]->setSelected(i == row);
    }
    if (screenMap != nullptr)
    {
        screenMap->setSelectedScreen(m_config->screenList[row].displayName);
    }
    emit screenSelected(row);
}

void ScreenLayoutCanvas::onScreenMapClicked(const QString &displayName)
{
    if (m_config == nullptr || displayName.isEmpty())
    {
        return;
    }
    // 找到与该物理屏绑定的屏幕配置，选中对应的卡片
    for (int i = 0; i < m_config->screenList.size(); i++)
    {
        if (m_config->screenList[i].displayName == displayName)
        {
            onSelectScreen(i);
            return;
        }
    }
}

void ScreenLayoutCanvas::onScreenMoved(int fromRow, int toRow)
{
    if (m_config == nullptr || fromRow == toRow || fromRow < 0 || toRow < 0 ||
        fromRow >= m_config->screenList.size() || toRow >= m_config->screenList.size())
    {
        return;
    }
    // 主屏固定为 1 号屏：不允许被拖走，也不允许其它屏幕插到它前面
    if (m_config->screenList[fromRow].primaryScreen)
    {
        return;
    }
    if (m_config->screenList[toRow].primaryScreen)
    {
        return;
    }
    m_config->screenList.move(fromRow, toRow);
    renumberScreens();
    refresh();
    onSelectScreen(toRow);
    emit configChanged();
}

void ScreenLayoutCanvas::onNameChanged(int row, const QString &name)
{
    if (m_config == nullptr || row < 0 || row >= m_config->screenList.size())
    {
        return;
    }
    m_config->screenList[row].screenTitle = name;
    emit configChanged();
}

void ScreenLayoutCanvas::onTabOffsetChanged(int row, int value)
{
    if (m_config == nullptr || row < 0 || row >= m_config->screenList.size())
    {
        return;
    }
    m_config->screenList[row].tabOffsetX = value;
    emit configChanged();
}

void ScreenLayoutCanvas::renumberScreens()
{
    if (m_config == nullptr)
    {
        return;
    }
    // 序号始终等于卡片顺序：Alt+N 的映射也随之变化
    for (int i = 0; i < m_config->screenList.size(); i++)
    {
        m_config->screenList[i].screenIndex = i + 1;
    }
    m_config->count = m_config->screenList.size();
}

bool ScreenLayoutCanvas::ensurePrimaryFirst()
{
    if (m_config == nullptr || m_config->screenList.isEmpty())
    {
        return false;
    }
    // 已经在第一位就不用动
    if (m_config->screenList.first().primaryScreen)
    {
        return false;
    }
    // 找到主屏的位置并移到最前面
    for (int i = 1; i < m_config->screenList.size(); i++)
    {
        if (m_config->screenList[i].primaryScreen)
        {
            m_config->screenList.move(i, 0);
            return true;
        }
    }
    return false;
}

QString ScreenLayoutCanvas::findFreeScreenName() const
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); i++)
    {
        bool used = false;
        if (m_config != nullptr)
        {
            for (int j = 0; j < m_config->screenList.size(); j++)
            {
                if (m_config->screenList[j].displayName == screens[i]->name())
                {
                    used = true;
                    break;
                }
            }
        }
        if (!used)
        {
            return screens[i]->name();
        }
    }
    return QString();
}

QString ScreenLayoutCanvas::softwareName(const QString &softwareId) const
{
    if (m_config != nullptr)
    {
        for (int i = 0; i < m_config->softwareList.size(); i++)
        {
            if (m_config->softwareList[i].id == softwareId)
            {
                return m_config->softwareList[i].name;
            }
        }
    }
    return softwareId;
}
