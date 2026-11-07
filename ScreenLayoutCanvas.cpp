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
    // 开启鼠标跟踪，未按下时也能按位置切换拖动/缩放光标
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    // 固定高度撑开绘图区，避免被下方卡片区挤成 0 高度导致屏幕框不可见
    setMinimumHeight(180);
}

QList<QRectF> PhysicalScreenMap::screenRects() const
{
    QList<QRectF> rects;
    const QList<QScreen *> screens = QGuiApplication::screens();
    const int count = screens.size();
    if (count <= 0)
    {
        return rects;
    }

    const double margin = 20.0;
    const double gap = 16.0;
    const double availableWidth = width() - margin * 2;
    const double availableHeight = height() - margin * 2;
    if (availableWidth <= 0 || availableHeight <= 0)
    {
        return rects;
    }

    // 每块屏按实际分辨率的长宽比换算宽度，统一高度；分辨率异常时退回 16:9
    QList<double> ratios;
    double ratioSum = 0.0;
    for (int i = 0; i < count; i++)
    {
        const QSize geoSize = screens[i]->geometry().size();
        const double ratio = (geoSize.width() > 0 && geoSize.height() > 0)
                                 ? static_cast<double>(geoSize.width()) / static_cast<double>(geoSize.height())
                                 : 16.0 / 9.0;
        ratios.append(ratio);
        ratioSum += ratio;
    }
    if (ratioSum <= 0)
    {
        return rects;
    }

    // 统一高度受控件高度与总宽度两个约束，取较小值保证不溢出
    const double heightByWidth = (availableWidth - gap * (count - 1)) / ratioSum;
    const double rectH = qMin(availableHeight, heightByWidth);
    if (rectH <= 0)
    {
        return rects;
    }

    double totalW = gap * (count - 1);
    for (int i = 0; i < count; i++)
    {
        totalW += ratios[i] * rectH;
    }
    const double startX = margin + (availableWidth - totalW) / 2.0;
    const double startY = margin + (availableHeight - rectH) / 2.0;

    double x = startX;
    for (int i = 0; i < count; i++)
    {
        const double rectW = ratios[i] * rectH;
        QRectF rect(x, startY, rectW, rectH);
        // 叠加用户拖拽产生的自定义偏移与尺寸，仅影响示意图显示，不改动配置
        const QString &displayName = screens[i]->name();
        rect.translate(m_customOffsets.value(displayName, QPoint(0, 0)));
        if (m_customSizes.contains(displayName))
        {
            const QSizeF customSize = m_customSizes.value(displayName);
            if (customSize.width() > 0 && customSize.height() > 0)
            {
                rect.setSize(customSize);
            }
        }
        rects.append(rect);
        x += rectW + gap;
    }
    return rects;
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
    const int count = screens.size();
    if (count <= 0)
    {
        return;
    }

    // 各屏矩形由 screenRects 统一计算，保证绘制与命中判定使用同一套几何
    const QList<QRectF> rects = screenRects();
    if (rects.size() != count)
    {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (int i = 0; i < count; i++)
    {
        const QRectF targetRect = rects[i];
        const qreal rectW = targetRect.width();
        const QString &displayName = screens[i]->name();
        const bool selected = (!m_selected.isEmpty() && displayName == m_selected);
        const QSize geoSize = screens[i]->geometry().size();
        const bool isPrimary = (screens[i] == QGuiApplication::primaryScreen());

        // 屏幕框：选中态青色边框，普通态灰蓝色边框，内部深色填充
        const QColor fillColor = selected ? QColor(18, 82, 130) : QColor(20, 46, 78);
        const QColor borderColor = selected ? QColor(17, 192, 254) : QColor(48, 92, 140);
        const qreal borderWidth = selected ? 3.0 : 1.5;
        painter.setBrush(fillColor);
        painter.setPen(QPen(borderColor, borderWidth));
        painter.drawRoundedRect(targetRect, 6, 6);

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
            const qreal badgeW = 32;
            const qreal badgeH = 22;
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

        // 主屏标记（右上角）
        if (isPrimary)
        {
            const QString primaryText = QString::fromUtf8("主屏");
            QFont primaryFont = painter.font();
            primaryFont.setFamily(QString::fromUtf8("微软雅黑"));
            primaryFont.setPixelSize(11);
            const QFontMetrics primaryFm(primaryFont);
            const qreal textW = primaryFm.horizontalAdvance(primaryText) + 12;
            const qreal textH = 20;
            const QRectF primaryRect(targetRect.right() - textW - 8, targetRect.top() + 8, textW, textH);
            painter.setBrush(QColor(255, 182, 72));
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(primaryRect, 4, 4);
            painter.setFont(primaryFont);
            painter.setPen(QColor(255, 255, 255));
            painter.drawText(primaryRect, Qt::AlignCenter, primaryText);
        }

        // 屏幕名称 + 分辨率作为一组在框内水平垂直居中
        QFont nameFont = painter.font();
        nameFont.setFamily(QString::fromUtf8("微软雅黑"));
        nameFont.setPixelSize(13);
        nameFont.setBold(true);
        painter.setFont(nameFont);

        QFont resFont = painter.font();
        resFont.setFamily(QString::fromUtf8("微软雅黑"));
        resFont.setPixelSize(11);
        resFont.setBold(false);

        const QFontMetrics nameFm(nameFont);
        const QFontMetrics resFm(resFont);
        const qreal lineGap = 2;
        const qreal blockH = nameFm.height() + lineGap + resFm.height();
        const qreal blockTop = targetRect.center().y() - blockH / 2.0;
        const QString resText = QString("%1 × %2").arg(geoSize.width()).arg(geoSize.height());

        painter.setFont(nameFont);
        painter.setPen(selected ? QColor(220, 240, 255) : QColor(190, 210, 230));
        const QString elidedName = nameFm.elidedText(displayName, Qt::ElideMiddle,
                                                     static_cast<int>(rectW - 16));
        painter.drawText(QRectF(targetRect.left() + 8, blockTop, rectW - 16, nameFm.height()),
                         Qt::AlignCenter, elidedName);

        painter.setFont(resFont);
        painter.setPen(QColor(130, 160, 190));
        painter.drawText(QRectF(targetRect.left() + 8, blockTop + nameFm.height() + lineGap, rectW - 16,
                                resFm.height()),
                         Qt::AlignCenter, resText);

        // 右下角调整大小手柄：斜纹三角，选中态高亮，提示此处可拖动缩放
        const qreal handleSize = 12;
        if (targetRect.width() > handleSize * 2 && targetRect.height() > handleSize * 2)
        {
            const QRectF handleRect(targetRect.right() - handleSize - 3,
                                    targetRect.bottom() - handleSize - 3, handleSize, handleSize);
            QPainterPath handlePath;
            handlePath.moveTo(handleRect.bottomRight());
            handlePath.lineTo(handleRect.bottomLeft());
            handlePath.lineTo(handleRect.topRight());
            handlePath.closeSubpath();
            painter.setPen(Qt::NoPen);
            painter.setBrush(selected ? QColor(17, 192, 254) : QColor(90, 130, 170));
            painter.drawPath(handlePath);
        }
    }
}

void PhysicalScreenMap::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        const QList<QScreen *> screens = QGuiApplication::screens();
        const QList<QRectF> rects = screenRects();

        // 优先判定右下角手柄：命中则进入保持长宽比的缩放
        int resizeIdx = -1;
        if (hitResizeHandle(event->pos(), &resizeIdx) && resizeIdx >= 0 &&
            resizeIdx < screens.size() && resizeIdx < rects.size())
        {
            m_dragMode = DragResize;
            m_dragIndex = resizeIdx;
            m_dragStartPos = event->pos();
            m_dragStartRect = rects[resizeIdx];
            setCursor(Qt::SizeFDiagCursor);
            // 缩放前先选中该屏，手柄与卡片内容随之切换
            emit screenClicked(screens[resizeIdx]->name());
            return;
        }

        // 命中屏幕矩形内部：进入拖动位置
        const int idx = hitTest(event->pos());
        if (idx >= 0 && idx < screens.size() && idx < rects.size())
        {
            const QString &name = screens[idx]->name();
            m_dragMode = DragMove;
            m_dragIndex = idx;
            m_dragStartPos = event->pos();
            m_dragStartRect = rects[idx];
            m_dragStartCustomOffset = m_customOffsets.value(name, QPoint(0, 0));
            setCursor(Qt::ClosedHandCursor);
            emit screenClicked(name);
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void PhysicalScreenMap::mouseMoveEvent(QMouseEvent *event)
{
    const QList<QScreen *> screens = QGuiApplication::screens();

    if (m_dragMode == DragMove && m_dragIndex >= 0 && m_dragIndex < screens.size())
    {
        // 拖动位置：以起始矩形左上角为基准累加鼠标位移，并限制在控件范围内
        const QString &name = screens[m_dragIndex]->name();
        const QPoint delta = event->pos() - m_dragStartPos;
        QPointF topLeft = m_dragStartRect.topLeft() + QPointF(delta.x(), delta.y());
        const double maxX = qMax(0.0, width() - m_dragStartRect.width());
        const double maxY = qMax(0.0, height() - m_dragStartRect.height());
        topLeft.setX(qBound(0.0, topLeft.x(), maxX));
        topLeft.setY(qBound(0.0, topLeft.y(), maxY));
        // 默认布局位置 = 起始矩形位置 - 起始自定义偏移，据此反算新的自定义偏移
        const QPointF defaultTopLeft = m_dragStartRect.topLeft() - QPointF(m_dragStartCustomOffset);
        m_customOffsets[name] = (topLeft - defaultTopLeft).toPoint();
        update();
        return;
    }

    if (m_dragMode == DragResize && m_dragIndex >= 0 && m_dragIndex < screens.size())
    {
        // 调整大小：以起始矩形的宽高比为准，拖拽幅度更大的方向决定尺寸
        const QString &name = screens[m_dragIndex]->name();
        const double aspect = (m_dragStartRect.height() > 0)
                                  ? m_dragStartRect.width() / m_dragStartRect.height()
                                  : 16.0 / 9.0;
        const QPoint delta = event->pos() - m_dragStartPos;
        double newWidth = (qAbs(delta.x()) >= qAbs(delta.y()))
                              ? m_dragStartRect.width() + delta.x()
                              : (m_dragStartRect.height() + delta.y()) * aspect;
        const double minWidth = 100.0;
        // 上限保证缩放后矩形右下角不越出控件，宽度与高度两个方向取更严格者
        const double maxWidthByWidth = qMax(minWidth, width() - m_dragStartRect.left());
        const double maxWidthByHeight = qMax(minWidth, (height() - m_dragStartRect.top()) * aspect);
        newWidth = qBound(minWidth, newWidth, qMin(maxWidthByWidth, maxWidthByHeight));
        m_customSizes[name] = QSizeF(newWidth, newWidth / aspect);
        update();
        return;
    }

    // 未拖拽时按鼠标所在位置切换光标：手柄为缩放光标，矩形内部为张手的光标
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
        setCursor(Qt::PointingHandCursor);
    }
    QWidget::mouseMoveEvent(event);
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
    const QList<QRectF> rects = screenRects();
    // 从后往前遍历，后绘制（位于上层）的矩形优先命中
    for (int i = rects.size() - 1; i >= 0; i--)
    {
        if (rects[i].contains(pos))
        {
            return i;
        }
    }
    return -1;
}

bool PhysicalScreenMap::hitResizeHandle(const QPoint &pos, int *index) const
{
    const QList<QRectF> rects = screenRects();
    // 命中区域略大于绘制出的三角，便于鼠标抓取
    const qreal handleSize = 18;
    for (int i = rects.size() - 1; i >= 0; i--)
    {
        const QRectF &rect = rects[i];
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
    setAcceptDrops(true);
    // 卡片由中间栏承载，宽度随中间栏伸缩，下限保证「序号 + 名称」一行仍可编辑
    setMinimumWidth(180);
    setMaximumWidth(340);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

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
    // 已分配软件清单不在屏幕布局区展示（改由中间的「选中屏幕的软件」清单呈现），
    // 控件本身继续保留并同步数据，作为保存顺序的数据源
    assignmentList->hide();
    mainLayout->addLayout(offsetLayout);
    mainLayout->addStretch(1);

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
}

void ScreenCard::setRow(int row)
{
    m_row = row;
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
    // 点击物理屏示意图上的屏幕，同步选中对应的屏幕配置卡片
    connect(screenMap, &PhysicalScreenMap::screenClicked, this,
            &ScreenLayoutCanvas::onScreenMapClicked);

    // 卡片容器由管理界面中间栏承载（列表上方），画布自身只保留物理屏示意图
    cardContainer = new QWidget();
    m_cardLayout = new QHBoxLayout(cardContainer);
    m_cardLayout->setContentsMargins(0, 0, 0, 0);
    m_cardLayout->setSpacing(12);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(screenMap, 1);

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
    // 卡片容器由外部界面持有，这里只清空并重建容器内的卡片，避免外部布局失效
    m_cards.clear();
    while (QLayoutItem *item = m_cardLayout->takeAt(0))
    {
        if (item->widget() != nullptr)
        {
            // 先从布局摘下并隐藏，再延迟销毁，防止旧卡片在新卡片生成前残留在界面上
            item->widget()->hide();
            item->widget()->deleteLater();
        }
        delete item;
    }

    if (m_config != nullptr)
    {
        for (int i = 0; i < m_config->screenList.size(); i++)
        {
            ScreenCard *card = new ScreenCard(i, cardContainer);
            card->setScreen(i, m_config->screenList[i], m_config);
            // 默认隐藏，由 onSelectScreen 只显示当前选中屏幕的卡片
            card->setVisible(false);
            m_cardLayout->addWidget(card, 1);
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

    if (m_config != nullptr && m_selectedRow >= 0 && m_selectedRow < m_cards.size())
    {
        onSelectScreen(m_selectedRow);
    }
    else
    {
        // 无有效选中项时默认选中主屏；配置中没有主屏标记则退回第一块屏
        int defaultRow = -1;
        if (m_config != nullptr)
        {
            for (int i = 0; i < m_config->screenList.size(); i++)
            {
                if (m_config->screenList[i].primaryScreen)
                {
                    defaultRow = i;
                    break;
                }
            }
            if (defaultRow < 0 && !m_config->screenList.isEmpty())
            {
                defaultRow = 0;
            }
        }
        if (defaultRow >= 0 && defaultRow < m_cards.size())
        {
            onSelectScreen(defaultRow);
        }
        else
        {
            m_selectedRow = -1;
            if (screenMap != nullptr)
            {
                screenMap->setSelectedScreen(QString());
            }
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

QWidget *ScreenLayoutCanvas::cardsWidget() const
{
    return cardContainer;
}

void ScreenLayoutCanvas::onSelectScreen(int row)
{
    if (m_config == nullptr || row < 0 || row >= m_config->screenList.size())
    {
        return;
    }
    m_selectedRow = row;
    // 中间栏只展示当前屏幕的配置卡片：切换屏幕即换卡片内容，卡片本身不做选中态高亮
    for (int i = 0; i < m_cards.size(); i++)
    {
        m_cards[i]->setVisible(i == row);
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
