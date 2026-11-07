#include "ScreenLayoutCanvas.h"

#include <algorithm>
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
#include <QPainterPath>
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
    // 开启鼠标跟踪，确保拖拽过程中光标形态正确
    setMouseTracking(true);
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
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty())
    {
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
    const QPointF baseOffset(offsetX, offsetY);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    for (int i = 0; i < screens.size(); i++)
    {
        const QRectF targetRect = screenDisplayRect(i, bounding, scale, baseOffset);
        const QString &displayName = screens[i]->name();
        const bool selected = (!m_selected.isEmpty() && displayName == m_selected);

        // 只画屏幕框：选中态青色边框，普通态灰蓝色边框，内部深色填充
        const QColor fillColor = selected ? QColor(18, 82, 130) : QColor(20, 46, 78);
        const QColor borderColor = selected ? QColor(17, 192, 254) : QColor(48, 92, 140);
        const qreal borderWidth = selected ? 3.0 : 1.5;
        painter.setBrush(fillColor);
        painter.setPen(QPen(borderColor, borderWidth));
        painter.drawRoundedRect(targetRect.adjusted(1, 1, -1, -1), 6, 6);

        // 左上角序号徽标
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
            const qreal badgeW = 30;
            const qreal badgeH = 20;
            const QRectF badgeRect(targetRect.left() + 8, targetRect.top() + 8, badgeW, badgeH);
            painter.setBrush(selected ? QColor(17, 192, 254) : QColor(48, 92, 140));
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(badgeRect, 4, 4);
            QFont badgeFont = painter.font();
            badgeFont.setFamily(QString::fromUtf8("微软雅黑"));
            badgeFont.setPixelSize(12);
            badgeFont.setBold(true);
            painter.setFont(badgeFont);
            painter.setPen(QColor(255, 255, 255));
            painter.drawText(badgeRect, Qt::AlignCenter, QString::number(screenIndex));
        }

        // 屏幕名称居中显示
        QFont font = painter.font();
        font.setFamily(QString::fromUtf8("微软雅黑"));
        font.setPixelSize(12);
        painter.setFont(font);
        painter.setPen(selected ? QColor(220, 240, 255) : QColor(170, 195, 220));
        painter.drawText(targetRect, Qt::AlignCenter, displayName);

        // 右下角调整大小手柄（仅选中态显示）
        if (selected)
        {
            const int handleSize = 12;
            const QRectF handleRect(targetRect.right() - handleSize - 3, targetRect.bottom() - handleSize - 3,
                                    handleSize, handleSize);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(17, 192, 254));
            QPainterPath path;
            path.moveTo(handleRect.bottomRight());
            path.lineTo(handleRect.bottomLeft());
            path.lineTo(handleRect.topRight());
            path.closeSubpath();
            painter.drawPath(path);
        }
    }
}

void PhysicalScreenMap::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        int resizeIdx = -1;
        if (hitResizeHandle(event->pos(), &resizeIdx))
        {
            // 命中右下角调整手柄，进入调整大小模式
            const QList<QScreen *> screens = QGuiApplication::screens();
            if (resizeIdx >= 0 && resizeIdx < screens.size())
            {
                m_dragMode = DragResize;
                m_dragIndex = resizeIdx;
                const QString &name = screens[resizeIdx]->name();
                m_dragStartPos = event->pos();
                QRectF bounding;
                for (int i = 0; i < screens.size(); i++)
                {
                    bounding = bounding.united(QRectF(screens[i]->geometry()));
                }
                const double margin = 14;
                const double scale = qMin((width() - margin * 2) / bounding.width(),
                                          (height() - margin * 2) / bounding.height());
                const double offsetX = margin + ((width() - margin * 2) - bounding.width() * scale) / 2.0;
                const double offsetY = margin + ((height() - margin * 2) - bounding.height() * scale) / 2.0;
                const QPointF baseOffset(offsetX, offsetY);
                const QRectF rect = screenDisplayRect(resizeIdx, bounding, scale, baseOffset);
                m_dragStartSize = rect.size().toSize();
                setCursor(Qt::SizeFDiagCursor);
                emit screenClicked(name);
                return;
            }
        }
        const int idx = hitTest(event->pos());
        if (idx >= 0)
        {
            const QList<QScreen *> screens = QGuiApplication::screens();
            if (idx < screens.size())
            {
                // 命中屏幕内部，进入拖动位置模式
                m_dragMode = DragMove;
                m_dragIndex = idx;
                const QString &name = screens[idx]->name();
                QRectF bounding;
                for (int i = 0; i < screens.size(); i++)
                {
                    bounding = bounding.united(QRectF(screens[i]->geometry()));
                }
                const double margin = 14;
                const double scale = qMin((width() - margin * 2) / bounding.width(),
                                          (height() - margin * 2) / bounding.height());
                const double offsetX = margin + ((width() - margin * 2) - bounding.width() * scale) / 2.0;
                const double offsetY = margin + ((height() - margin * 2) - bounding.height() * scale) / 2.0;
                const QPointF baseOffset(offsetX, offsetY);
                const QRectF rect = screenDisplayRect(idx, bounding, scale, baseOffset);
                QPoint customOffset = m_customOffsets.value(name, QPoint(0, 0));
                // 拖拽偏移 = 鼠标位置 - 屏幕矩形左上角（含自定义偏移）
                m_dragOffset = event->pos() - rect.topLeft().toPoint();
                setCursor(Qt::ClosedHandCursor);
                emit screenClicked(name);
                return;
            }
        }
    }
    QWidget::mousePressEvent(event);
}

void PhysicalScreenMap::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragMode == DragMove && m_dragIndex >= 0)
    {
        // 拖动位置：更新自定义偏移
        const QList<QScreen *> screens = QGuiApplication::screens();
        if (m_dragIndex < screens.size())
        {
            const QString &name = screens[m_dragIndex]->name();
            QRectF bounding;
            for (int i = 0; i < screens.size(); i++)
            {
                bounding = bounding.united(QRectF(screens[i]->geometry()));
            }
            const double margin = 14;
            const double scale = qMin((width() - margin * 2) / bounding.width(),
                                      (height() - margin * 2) / bounding.height());
            const double offsetX = margin + ((width() - margin * 2) - bounding.width() * scale) / 2.0;
            const double offsetY = margin + ((height() - margin * 2) - bounding.height() * scale) / 2.0;
            const QPointF baseOffset(offsetX, offsetY);
            const QRectF rect = screenDisplayRect(m_dragIndex, bounding, scale, baseOffset);
            // 计算新的自定义偏移
            QPoint defaultTopLeft = rect.topLeft().toPoint() - m_customOffsets.value(name, QPoint(0, 0));
            QPoint newTopLeft = event->pos() - m_dragOffset;
            QPoint newOffset = newTopLeft - defaultTopLeft;
            m_customOffsets[name] = newOffset;
            update();
        }
    }
    else if (m_dragMode == DragResize && m_dragIndex >= 0)
    {
        // 调整大小：右下角拖拽，固定 16:9 比例，最小宽度 160
        const QList<QScreen *> screens = QGuiApplication::screens();
        if (m_dragIndex < screens.size())
        {
            const QString &name = screens[m_dragIndex]->name();
            const int dx = event->pos().x() - m_dragStartPos.x();
            const int dy = event->pos().y() - m_dragStartPos.y();
            // 以水平拖拽为主，宽度变化决定高度变化（16:9）
            // 同时考虑垂直方向的输入，取两者换算后较大的那个
            int newW = qMax(160, m_dragStartSize.width() + dx);
            int newHfromW = qMax(90, qRound(newW * 9.0 / 16.0));
            int newHfromDy = qMax(90, m_dragStartSize.height() + dy);
            int newWfromH = qMax(160, qRound(newHfromDy * 16.0 / 9.0));
            // 取两种换算中尺寸较大的一组（用户拖拽方向决定）
            if (newW >= newWfromH)
            {
                m_customSizes[name] = QSize(newW, newHfromW);
            }
            else
            {
                m_customSizes[name] = QSize(newWfromH, newHfromDy);
            }
            update();
        }
    }
    else
    {
        // 未拖拽时，根据鼠标位置更新光标形状
        int resizeIdx = -1;
        if (hitResizeHandle(event->pos(), &resizeIdx))
        {
            setCursor(Qt::SizeFDiagCursor);
        }
        else if (hitTest(event->pos()) >= 0)
        {
            setCursor(Qt::OpenHandCursor);
        }
        else
        {
            setCursor(Qt::ArrowCursor);
        }
        QWidget::mouseMoveEvent(event);
    }
}

void PhysicalScreenMap::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_dragMode != DragNone)
    {
        m_dragMode = DragNone;
        m_dragIndex = -1;
        setCursor(Qt::PointingHandCursor);
    }
    QWidget::mouseReleaseEvent(event);
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
    const QPointF baseOffset(offsetX, offsetY);

    // 从后往前遍历，后绘制的（上层）优先命中
    for (int i = screens.size() - 1; i >= 0; i--)
    {
        const QRectF targetRect = screenDisplayRect(i, bounding, scale, baseOffset);
        if (targetRect.contains(pos))
        {
            return i;
        }
    }
    return -1;
}

QRectF PhysicalScreenMap::screenDisplayRect(int screenIndex, const QRectF &bounding, double scale,
                                            const QPointF &baseOffset) const
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screenIndex < 0 || screenIndex >= screens.size())
    {
        return QRectF();
    }
    const QRect geometry = screens[screenIndex]->geometry();
    const QString &name = screens[screenIndex]->name();
    QRectF baseRect(baseOffset.x() + (geometry.x() - bounding.x()) * scale,
                    baseOffset.y() + (geometry.y() - bounding.y()) * scale, geometry.width() * scale,
                    geometry.height() * scale);
    // 加上该物理屏的自定义拖拽偏移（仅显示用，不影响真实配置）
    const QPoint &customOffset = m_customOffsets.value(name, QPoint(0, 0));
    baseRect.translate(customOffset);
    // 如果有自定义大小，按自定义大小调整（保持左上角不变，只改宽高）
    if (m_customSizes.contains(name))
    {
        const QSize &customSize = m_customSizes[name];
        baseRect.setWidth(customSize.width());
        baseRect.setHeight(customSize.height());
    }
    return baseRect;
}

bool PhysicalScreenMap::hitResizeHandle(const QPoint &pos, int *index) const
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty())
    {
        return false;
    }
    QRectF bounding;
    for (int i = 0; i < screens.size(); i++)
    {
        bounding = bounding.united(QRectF(screens[i]->geometry()));
    }
    if (bounding.width() <= 0 || bounding.height() <= 0)
    {
        return false;
    }
    const double margin = 14;
    const double availableWidth = width() - margin * 2;
    const double availableHeight = height() - margin * 2;
    if (availableWidth <= 0 || availableHeight <= 0)
    {
        return false;
    }
    const double scale = qMin(availableWidth / bounding.width(), availableHeight / bounding.height());
    const double offsetX = margin + (availableWidth - bounding.width() * scale) / 2.0;
    const double offsetY = margin + (availableHeight - bounding.height() * scale) / 2.0;
    const QPointF baseOffset(offsetX, offsetY);

    // 调整手柄大小：右下角 16×16 区域
    const int handleSize = 16;
    // 从后往前遍历，后绘制的（上层）优先命中
    for (int i = screens.size() - 1; i >= 0; i--)
    {
        const QRectF rect = screenDisplayRect(i, bounding, scale, baseOffset);
        // 右下角区域
        const QRectF handleRect(rect.right() - handleSize, rect.bottom() - handleSize, handleSize, handleSize);
        if (handleRect.contains(pos))
        {
            if (index != nullptr)
            {
                *index = i;
            }
            return true;
        }
    }
    return false;
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

    // 序号输入框：可手动设置屏幕序号，范围 1~99
    indexSpin = new QSpinBox(this);
    indexSpin->setObjectName(QStringLiteral("screenIndexSpin"));
    indexSpin->setRange(1, 99);
    indexSpin->setAlignment(Qt::AlignCenter);
    indexSpin->setFixedSize(54, 34);
    QLabel *indexLabel = new QLabel(QString::fromUtf8("序号"), this);
    indexLabel->setObjectName(QStringLiteral("hintLabel"));

    nameEdit = new QLineEdit(this);
    nameEdit->setObjectName(QStringLiteral("screenNameEdit"));
    nameEdit->setPlaceholderText(QString::fromUtf8("屏幕名称（可选）"));

    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->addWidget(indexLabel);
    headerLayout->addWidget(indexSpin);
    headerLayout->addWidget(nameEdit, 1);

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

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->addLayout(headerLayout);
    mainLayout->addWidget(displayLabel);
    mainLayout->addWidget(assignmentList, 1);
    mainLayout->addLayout(offsetLayout);

    connect(indexSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
            [this](int value) { emit indexChanged(m_row, value); });
    connect(nameEdit, &QLineEdit::textEdited, this,
            [this](const QString &text) { emit nameChanged(m_row, text); });
    connect(offsetSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
            [this](int value) { emit tabOffsetChanged(m_row, value); });
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

    // 序号输入框：从配置中读取 screenIndex，用户可手动修改
    indexSpin->blockSignals(true);
    indexSpin->setValue(screen.screenIndex > 0 ? screen.screenIndex : row + 1);
    indexSpin->blockSignals(false);

    nameEdit->blockSignals(true);
    nameEdit->setText(screen.screenTitle);
    nameEdit->blockSignals(false);

    const bool isPrimary = screen.primaryScreen;

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
    QFrame::mousePressEvent(event);
}

void ScreenCard::dragEnterEvent(QDragEnterEvent *event)
{
    QFrame::dragEnterEvent(event);
}

void ScreenCard::dropEvent(QDropEvent *event)
{
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
    // 加载配置后先同步物理屏，再按序号排序
    if (m_config != nullptr)
    {
        syncPhysicalScreens();
        sortScreensByIndex();
    }
    refresh();
}

void ScreenLayoutCanvas::refresh()
{
    // 整块卡片区重建：屏幕配置变化后统一按配置重新生成，避免局部状态不一致
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

            connect(card, &ScreenCard::indexChanged, this, &ScreenLayoutCanvas::onIndexChanged);
            connect(card, &ScreenCard::nameChanged, this, &ScreenLayoutCanvas::onNameChanged);
            connect(card, &ScreenCard::tabOffsetChanged, this, &ScreenLayoutCanvas::onTabOffsetChanged);
            connect(card, &ScreenCard::selected, this, &ScreenLayoutCanvas::onSelectScreen);
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

void ScreenLayoutCanvas::onIndexChanged(int row, int newIndex)
{
    if (m_config == nullptr || row < 0 || row >= m_config->screenList.size())
    {
        return;
    }
    // 更新该屏幕的序号
    m_config->screenList[row].screenIndex = newIndex;
    // 按新序号重新排序
    sortScreensByIndex();
    // 记住当前选中的物理屏，排序后重新定位选中行
    QString selectedDisplayName;
    if (m_selectedRow >= 0 && m_selectedRow < m_config->screenList.size())
    {
        selectedDisplayName = m_config->screenList[m_selectedRow].displayName;
    }
    refresh();
    // 重新定位选中行
    if (!selectedDisplayName.isEmpty())
    {
        for (int i = 0; i < m_config->screenList.size(); i++)
        {
            if (m_config->screenList[i].displayName == selectedDisplayName)
            {
                onSelectScreen(i);
                break;
            }
        }
    }
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

void ScreenLayoutCanvas::syncPhysicalScreens()
{
    if (m_config == nullptr)
    {
        return;
    }
    const QList<QScreen *> screens = QGuiApplication::screens();
    QList<ScreenConfig> newList;

    // 遍历所有物理屏，已有配置的保留，没有的新建
    for (int i = 0; i < screens.size(); i++)
    {
        const QString &name = screens[i]->name();
        bool found = false;
        for (int j = 0; j < m_config->screenList.size(); j++)
        {
            if (m_config->screenList[j].displayName == name)
            {
                // 更新几何信息（分辨率变化时同步）
                ScreenConfig cfg = m_config->screenList[j];
                cfg.geometry = screens[i]->geometry();
                cfg.primaryScreen = (screens[i] == QGuiApplication::primaryScreen());
                newList.append(cfg);
                found = true;
                break;
            }
        }
        if (!found)
        {
            // 新接入的物理屏，创建默认配置
            ScreenConfig cfg;
            cfg.id = ConfigStore::makeScreenId();
            cfg.displayName = name;
            cfg.geometry = screens[i]->geometry();
            cfg.primaryScreen = (screens[i] == QGuiApplication::primaryScreen());
            cfg.screenIndex = i + 1;
            cfg.showIndex = m_config->showIndex;
            cfg.sortScreen = m_config->sortScreen;
            cfg.debugMode = m_config->debugMode;
            cfg.stripAppBorder = m_config->stripAppBorder;
            cfg.tabOffsetX = 0;
            newList.append(cfg);
        }
    }
    m_config->screenList = newList;
    m_config->count = newList.size();
}

void ScreenLayoutCanvas::sortScreensByIndex()
{
    if (m_config == nullptr || m_config->screenList.size() <= 1)
    {
        return;
    }
    // 按 screenIndex 从小到大排序；序号相同时按 displayName 稳定排序
    std::sort(m_config->screenList.begin(), m_config->screenList.end(),
              [](const ScreenConfig &a, const ScreenConfig &b) {
                  if (a.screenIndex != b.screenIndex)
                  {
                      return a.screenIndex < b.screenIndex;
                  }
                  return a.displayName < b.displayName;
              });
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
