#include "SoftwareLibraryPanel.h"

#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QDrag>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QStringList>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QVBoxLayout>

#include "ConfigStore.h"
#include "SoftwareCapture.h"

namespace
{
    /**
     * @brief 软件清单列表控件
     *
     * 仅用于为拖拽提供自定义 MIME 数据（软件 id），不做其它扩展。
     */
    class SoftwareListWidget : public QListWidget
    {
      public:
        explicit SoftwareListWidget(QWidget *parent = nullptr) : QListWidget(parent)
        {
        }

      protected:
        /**
         * @brief 构造拖拽数据：写入软件 id
         * @param items 被拖拽的条目
         * @return 拖拽数据对象
         */
        QMimeData *mimeData(const QList<QListWidgetItem *> items) const override
        {
            QMimeData *data = QListWidget::mimeData(items);
            if (data == nullptr)
            {
                data = new QMimeData();
            }
            if (!items.isEmpty())
            {
                data->setData(MDC_SOFTWARE_MIME, items.first()->data(Qt::UserRole).toString().toUtf8());
            }
            return data;
        }
    };

    /** @brief 列表项数据角色：软件名称（第一行文本） */
    const int kSoftwareNameRole = Qt::UserRole + 1;
    /** @brief 列表项数据角色：类型展示文本（第二行左侧） */
    const int kSoftwareTypeRole = Qt::UserRole + 2;
    /** @brief 列表项数据角色：已绑定屏幕标记文本（第二行右侧，未绑定为空） */
    const int kSoftwareBoundRole = Qt::UserRole + 3;

    /**
     * @brief 把预览图裁剪缩放为固定尺寸
     *
     * 先等比放大到完全覆盖目标尺寸，再居中裁剪多余部分：
     * 既保证输出宽高完全一致（软件名称左边界对齐），又不会拉伸变形。
     *
     * @param source     原始预览图
     * @param targetSize 目标尺寸
     * @return 固定尺寸的预览图；输入无效时原样返回
     */
    QPixmap makeFixedPreview(const QPixmap &source, const QSize &targetSize)
    {
        if (source.isNull() || targetSize.isEmpty())
        {
            return source;
        }
        const QPixmap covered = source.scaled(targetSize, Qt::KeepAspectRatioByExpanding,
                                              Qt::SmoothTransformation);
        const QRect cropRect((covered.width() - targetSize.width()) / 2,
                             (covered.height() - targetSize.height()) / 2, targetSize.width(),
                             targetSize.height());
        return covered.copy(cropRect);
    }

    /**
     * @brief 软件清单条目绘制代理
     *
     * 列表条目需要“名称 + 类型 + 已绑定屏幕标记”三段样式不同的文本：
     * 名称与类型左对齐，已绑定标记右对齐并使用醒目色。
     * QListWidgetItem 只能设置单一颜色与对齐方式，因此这里自绘文本，
     * 背景、选中态与缩略图仍交给基类绘制，保证与样式表外观一致。
     */
    class SoftwareItemDelegate : public QStyledItemDelegate
    {
      public:
        /**
         * @brief 构造函数
         * @param parent 父对象
         */
        explicit SoftwareItemDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent)
        {
        }

        /**
         * @brief 绘制条目：基类画背景/选中态/图标，本类叠加两行文本与右对齐标记
         * @param painter 绘制设备
         * @param option  条目样式选项
         * @param index   模型索引
         */
        void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
        {
            QStyleOptionViewItem opt = option;
            initStyleOption(&opt, index);
            const QString name = index.data(kSoftwareNameRole).toString();
            const QString typeText = index.data(kSoftwareTypeRole).toString();
            const QString boundText = index.data(kSoftwareBoundRole).toString();

            // 直接调用样式绘制背景、选中/悬停效果与缩略图。
            // 不能调用 QStyledItemDelegate::paint()：它内部会重新 initStyleOption，
            // 把我们清空的文本又填回去，导致与自绘文字叠加出现重影。
            opt.text.clear();
            const QWidget *widget = option.widget;
            QStyle *style = (widget != nullptr) ? widget->style() : QApplication::style();
            style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

            // 文本区域由样式计算，自动避开样式表里的 padding、margin 与左侧图标
            QRect textRect = option.rect;
            if (widget != nullptr)
            {
                textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, widget);
            }
            if (textRect.isEmpty() || name.isEmpty())
            {
                return;
            }

            const bool selected = (opt.state & QStyle::State_Selected) != 0;
            const int lineHeight = 22;
            const int totalHeight = lineHeight * 2 + 2;
            const int top = textRect.top() + qMax(0, (textRect.height() - totalHeight) / 2);

            painter->save();
            // 第一行：软件名称，选中时用白色保证对比度
            QFont nameFont = opt.font;
            nameFont.setPixelSize(15);
            nameFont.setBold(true);
            painter->setFont(nameFont);
            painter->setPen(selected ? QColor(Qt::white) : QColor(0xd7, 0xe9, 0xff));
            const QRect nameRect(textRect.left(), top, textRect.width(), lineHeight);
            painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                              QFontMetrics(nameFont).elidedText(name, Qt::ElideRight, nameRect.width()));

            // 第二行：左侧类型文本，右侧已绑定标记（右对齐、醒目色）
            QFont infoFont = opt.font;
            infoFont.setPixelSize(13);
            painter->setFont(infoFont);
            const QFontMetrics infoMetrics(infoFont);
            const QRect infoRect(textRect.left(), nameRect.bottom() + 2, textRect.width(), lineHeight);
            int reservedWidth = 0;
            if (!boundText.isEmpty())
            {
                const int boundWidth = infoMetrics.horizontalAdvance(boundText);
                const QRect boundRect(infoRect.right() - boundWidth, infoRect.top(), boundWidth, infoRect.height());
                painter->setPen(QColor(0xff, 0xb6, 0x48));
                painter->drawText(boundRect, Qt::AlignRight | Qt::AlignVCenter, boundText);
                // 为标记与左侧类型文本之间留出间距，避免文字相贴
                reservedWidth = boundWidth + 12;
            }
            painter->setPen(QColor(0x8a, 0xa6, 0xc4));
            const int typeWidth = qMax(0, infoRect.width() - reservedWidth);
            painter->drawText(QRect(infoRect.left(), infoRect.top(), typeWidth, infoRect.height()),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              infoMetrics.elidedText(typeText, Qt::ElideRight, typeWidth));
            painter->restore();
        }
    };

    /**
     * @brief 类型显示名称
     * @param type 类型标识
     * @return 中文名称
     */
    QString typeDisplayName(const QString &type)
    {
        if (type == "web")
        {
            return QString::fromUtf8("网页");
        }
        if (type == "ppt")
        {
            return QString::fromUtf8("演示文稿");
        }
        return QString::fromUtf8("桌面应用");
    }
}  // namespace

SoftwareEditDialog::SoftwareEditDialog(const SoftwareItem &item, const QList<SoftwareItem> &all, QWidget *parent)
    : QDialog(parent), m_item(item), m_all(all), m_isNewItem(item.id.isEmpty())
{
    setWindowTitle(item.id.isEmpty() ? QString::fromUtf8("新增软件") : QString::fromUtf8("编辑软件"));
    setMinimumWidth(760);

    typeCombo = new QComboBox(this);
    typeCombo->addItem(QString::fromUtf8("桌面应用"), QStringLiteral("desktop"));
    typeCombo->addItem(QString::fromUtf8("网页"), QStringLiteral("web"));
    typeCombo->addItem(QString::fromUtf8("演示文稿"), QStringLiteral("ppt"));

    nameEdit = new QLineEdit(this);
    nameEdit->setPlaceholderText(QString::fromUtf8("例如：OA办公系统"));

    pathEdit = new QLineEdit(this);
    browseButton = new QPushButton(QString::fromUtf8("浏览..."), this);

    QWidget *pathRow = new QWidget(this);
    QHBoxLayout *pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    pathLayout->addWidget(pathEdit, 1);
    pathLayout->addWidget(browseButton);

    windowTitleEdit = new QLineEdit(this);
    windowTitleEdit->setPlaceholderText(QString::fromUtf8("桌面应用必填，用于匹配要嵌入的窗口"));
    startupArgsEdit = new QLineEdit(this);
    startupArgsEdit->setPlaceholderText(QString::fromUtf8("可选，例如：--kiosk"));
    remarkEdit = new QLineEdit(this);
    remarkEdit->setPlaceholderText(QString::fromUtf8("可选，仅用于备注说明"));

    QFormLayout *form = new QFormLayout();
    form->addRow(QString::fromUtf8("类型"), typeCombo);
    form->addRow(QString::fromUtf8("名称"), nameEdit);
    form->addRow(QString::fromUtf8("路径/地址"), pathRow);
    form->addRow(QString::fromUtf8("窗口标题"), windowTitleEdit);
    form->addRow(QString::fromUtf8("启动参数"), startupArgsEdit);
    form->addRow(QString::fromUtf8("备注"), remarkEdit);

    // ==================== 预览区域 ====================
    previewLabel = new QLabel(this);
    previewLabel->setObjectName(QStringLiteral("previewImage"));
    previewLabel->setFixedSize(360, 200);
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setText(QString::fromUtf8("暂无预览"));
    captureButton = new QPushButton(QString::fromUtf8("获取预览与窗口标题"), this);
    QLabel *previewHint = new QLabel(
        QString::fromUtf8("演示文稿会导出第一页作为预览；桌面应用会启动一次并抓取第一个界面；网页会加载地址后截取"
                          "页面首屏。窗口标题自动识别后仍可手工修改。"), this);
    previewHint->setObjectName(QStringLiteral("hintLabel"));
    previewHint->setWordWrap(true);

    QVBoxLayout *previewSideLayout = new QVBoxLayout();
    previewSideLayout->addWidget(captureButton);
    previewSideLayout->addWidget(previewHint);
    previewSideLayout->addStretch(1);

    QHBoxLayout *previewLayout = new QHBoxLayout();
    previewLayout->addWidget(previewLabel);
    previewLayout->addLayout(previewSideLayout, 1);

    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addStretch(1);
    QPushButton *cancelButton = new QPushButton(QString::fromUtf8("取消"), this);
    QPushButton *okButton = new QPushButton(QString::fromUtf8("确定"), this);
    okButton->setDefault(true);
    buttons->addWidget(cancelButton);
    buttons->addWidget(okButton);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(form);
    mainLayout->addSpacing(6);
    mainLayout->addLayout(previewLayout);
    mainLayout->addSpacing(6);
    mainLayout->addLayout(buttons);

    // 回填已有数据
    const int typeIndex = typeCombo->findData(item.type.isEmpty() ? QStringLiteral("desktop") : item.type);
    typeCombo->setCurrentIndex(typeIndex >= 0 ? typeIndex : 0);
    nameEdit->setText(item.name);
    pathEdit->setText(item.path);
    windowTitleEdit->setText(item.windowTitle);
    startupArgsEdit->setText(item.startupArgs);
    remarkEdit->setText(item.remark);

    connect(typeCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this](int) { onTypeChanged(); });
    connect(browseButton, &QPushButton::clicked, this, [this]() { onBrowse(); });
    connect(captureButton, &QPushButton::clicked, this, [this]() { onCapture(); });
    connect(cancelButton, &QPushButton::clicked, this, [this]() { reject(); });
    connect(okButton, &QPushButton::clicked, this, [this]() { onAccept(); });
    onTypeChanged();
    loadPreviewImage();
}

SoftwareItem SoftwareEditDialog::result() const
{
    return m_item;
}

bool SoftwareEditDialog::previewAttempted() const
{
    return m_previewAttempted;
}

void SoftwareEditDialog::reject()
{
    // 新增流程取消时，本次抓取的预览图没有配置引用，直接清理，避免留下孤儿文件
    if (m_isNewItem && m_previewGenerated && !m_item.id.isEmpty())
    {
        SoftwareCapture::removePreview(QApplication::applicationDirPath(), m_item.id);
    }
    QDialog::reject();
}

void SoftwareEditDialog::onTypeChanged()
{
    const QString type = typeCombo->currentData().toString();
    const bool isWeb = (type == "web");
    browseButton->setEnabled(!isWeb);
    windowTitleEdit->setEnabled(!isWeb);
    if (isWeb)
    {
        pathEdit->setPlaceholderText(QStringLiteral("https://example.com"));
        windowTitleEdit->clear();
    }
    else if (type == "ppt")
    {
        pathEdit->setPlaceholderText(QString::fromUtf8(".ppt / .pptx 文件完整路径"));
    }
    else
    {
        pathEdit->setPlaceholderText(QString::fromUtf8(".exe 文件完整路径"));
    }
}

void SoftwareEditDialog::onBrowse()
{
    const QString type = typeCombo->currentData().toString();
    QString filter = QString::fromUtf8("所有文件 (*.*)");
    if (type == "desktop")
    {
        filter = QString::fromUtf8("可执行文件 (*.exe)");
    }
    else if (type == "ppt")
    {
        filter = QString::fromUtf8("演示文稿 (*.ppt *.pptx)");
    }
    const QString filePath = QFileDialog::getOpenFileName(this, QString::fromUtf8("选择文件"), pathEdit->text(), filter);
    if (filePath.isEmpty())
    {
        return;
    }
    pathEdit->setText(filePath);
    if (nameEdit->text().isEmpty())
    {
        nameEdit->setText(QFileInfo(filePath).completeBaseName());
    }
    // 选好路径后立即运行一次程序，自动获取窗口标题并生成预览图
    onCapture();
}

void SoftwareEditDialog::onCapture()
{
    const QString type = typeCombo->currentData().toString();
    SoftwareItem probe;
    probe.id = m_item.id;
    probe.type = type;
    probe.path = pathEdit->text().trimmed();
    probe.startupArgs = startupArgsEdit->text().trimmed();
    probe.windowTitle = windowTitleEdit->text().trimmed();
    if (type == "web")
    {
        // 网页类型不需要检查本地文件，只校验地址格式
        if (!ConfigStore::isWebPath(probe.path))
        {
            QMessageBox::warning(this, QString::fromUtf8("生成预览"),
                                 QString::fromUtf8("请先填写以 http:// 或 https:// 开头的网页地址。"));
            return;
        }
    }
    else if (probe.path.isEmpty() || !QFileInfo::exists(probe.path))
    {
        QMessageBox::warning(this, QString::fromUtf8("生成预览"), QString::fromUtf8("请先选择有效的软件路径。"));
        return;
    }
    if (probe.id.isEmpty())
    {
        probe.id = ConfigStore::makeSoftwareId();
        m_item.id = probe.id;
    }

    m_previewAttempted = true;
    QString detectedTitle;
    const bool ok = SoftwareCapture::capturePreview(probe, QApplication::applicationDirPath(), this, &detectedTitle);
    if (!ok)
    {
        QMessageBox::warning(
            this, QString::fromUtf8("生成预览"),
            QString::fromUtf8("未能抓取到预览画面。桌面应用请确认程序可正常启动（需要启动参数才能显示窗口的，"
                              "可先填写启动参数）；网页请确认地址可正常访问。"));
        return;
    }

    m_previewGenerated = true;
    if (!detectedTitle.isEmpty())
    {
        // 自动填充窗口标题（演示文稿用文档名），填充后仍可在输入框中手工修改
        windowTitleEdit->setText(detectedTitle);
    }
    loadPreviewImage();
}

void SoftwareEditDialog::loadPreviewImage()
{
    SoftwareItem probe = m_item;
    probe.previewPath =
        m_previewGenerated ? SoftwareCapture::relativePreviewPath(m_item.id) : m_item.previewPath;
    const QString previewFile = SoftwareCapture::resolveExistingPreview(QApplication::applicationDirPath(), probe);
    QPixmap pixmap(previewFile);
    if (pixmap.isNull())
    {
        previewLabel->setPixmap(QPixmap());
        previewLabel->setText(QString::fromUtf8("暂无预览"));
        return;
    }
    previewLabel->setText(QString());
    // 预览区域固定 360x200，等比缩放并留出少量内边距
    previewLabel->setPixmap(pixmap.scaled(previewLabel->size() - QSize(10, 10), Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation));
}

void SoftwareEditDialog::onAccept()
{
    const QString type = typeCombo->currentData().toString();
    const QString name = nameEdit->text().trimmed();
    const QString path = pathEdit->text().trimmed();
    const QString windowTitle = windowTitleEdit->text().trimmed();
    // 目标程序是否发生变化：变化后旧预览图不可再用，必须本次抓取成功才能保留
    const bool targetChanged = (type != m_item.type) || (path != m_item.path);

    if (name.isEmpty())
    {
        QMessageBox::warning(this, QString::fromUtf8("校验失败"), QString::fromUtf8("名称不能为空。"));
        return;
    }
    if (path.isEmpty())
    {
        QMessageBox::warning(this, QString::fromUtf8("校验失败"), QString::fromUtf8("路径/地址不能为空。"));
        return;
    }
    if (type == "web")
    {
        if (!ConfigStore::isWebPath(path))
        {
            QMessageBox::warning(this, QString::fromUtf8("校验失败"),
                                 QString::fromUtf8("网页地址必须以 http:// 或 https:// 开头。"));
            return;
        }
    }
    else if (type == "ppt")
    {
        if (!ConfigStore::isPptPath(path))
        {
            QMessageBox::warning(this, QString::fromUtf8("校验失败"), QString::fromUtf8("请选择 .ppt 或 .pptx 文件。"));
            return;
        }
    }
    else if (windowTitle.isEmpty())
    {
        QMessageBox::warning(this, QString::fromUtf8("校验失败"),
                             QString::fromUtf8("桌面应用必须填写窗口标题，运行时依赖它匹配要嵌入的窗口。"));
        return;
    }

    // 桌面应用路径不允许重复，避免同一程序被两个条目同时拉起
    if (type != "web")
    {
        for (int i = 0; i < m_all.size(); i++)
        {
            if (m_all[i].id == m_item.id || m_all[i].path != path)
            {
                continue;
            }
            QMessageBox::warning(this, QString::fromUtf8("校验失败"),
                                 QString::fromUtf8("该路径已存在于软件清单：%1").arg(m_all[i].name));
            return;
        }
    }

    m_item.type = type;
    m_item.name = name;
    m_item.path = path;
    m_item.windowTitle = windowTitle;
    m_item.startupArgs = startupArgsEdit->text().trimmed();
    m_item.remark = remarkEdit->text().trimmed();
    // 记录当前可用的预览图：未换目标程序或本次抓取成功时写相对路径，否则留空回退类型图标
    if (!targetChanged || m_previewGenerated)
    {
        const QString previewFile =
            SoftwareCapture::resolveExistingPreview(QApplication::applicationDirPath(), m_item);
        m_item.previewPath = previewFile.isEmpty() ? QString() : SoftwareCapture::relativePreviewPath(m_item.id);
    }
    else
    {
        m_item.previewPath.clear();
    }
    if (m_item.type == "web" && m_item.windowTitle.isEmpty())
    {
        // 网页窗口标题仅用于展示，缺省时用名称补齐，便于运行时日志识别
        m_item.windowTitle = m_item.name;
    }
    accept();
}

SoftwareLibraryPanel::SoftwareLibraryPanel(QWidget *parent) : QWidget(parent)
{
    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText(QString::fromUtf8("搜索名称或路径"));

    typeCombo = new QComboBox(this);
    typeCombo->addItem(QString::fromUtf8("全部类型"), QString());
    typeCombo->addItem(QString::fromUtf8("桌面应用"), QStringLiteral("desktop"));
    typeCombo->addItem(QString::fromUtf8("网页"), QStringLiteral("web"));
    typeCombo->addItem(QString::fromUtf8("演示文稿"), QStringLiteral("ppt"));

    QHBoxLayout *filterLayout = new QHBoxLayout();
    filterLayout->addWidget(searchEdit, 1);
    filterLayout->addWidget(typeCombo);

    listWidget = new SoftwareListWidget(this);
    // 列表项左侧直接展示软件预览缩略图，尺寸按 16:9 预留
    listWidget->setIconSize(QSize(128, 72));
    listWidget->setDragEnabled(true);
    listWidget->setDragDropMode(QAbstractItemView::DragOnly);
    listWidget->setDefaultDropAction(Qt::CopyAction);
    listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    listWidget->setUniformItemSizes(false);
    listWidget->setWordWrap(false);
    // 自绘代理：名称/类型左对齐，已绑定屏幕标记右对齐并使用醒目色
    listWidget->setItemDelegate(new SoftwareItemDelegate(listWidget));

    addButton = new QPushButton(QString::fromUtf8("新增"), this);
    editButton = new QPushButton(QString::fromUtf8("编辑"), this);
    removeButton = new QPushButton(QString::fromUtf8("删除"), this);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(editButton);
    buttonLayout->addWidget(removeButton);
    buttonLayout->addStretch(1);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(filterLayout);
    mainLayout->addWidget(listWidget, 1);
    mainLayout->addLayout(buttonLayout);

    connect(searchEdit, &QLineEdit::textChanged, this, [this](const QString &) { onFilterChanged(); });
    connect(typeCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this](int) { onFilterChanged(); });
    connect(listWidget, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *, QListWidgetItem *) { onCurrentItemChanged(); });
    connect(listWidget, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *item) { onItemDoubleClicked(item); });
    connect(addButton, &QPushButton::clicked, this, [this]() { onAdd(); });
    connect(editButton, &QPushButton::clicked, this, [this]() { onEdit(); });
    connect(removeButton, &QPushButton::clicked, this, [this]() { onRemove(); });
}

void SoftwareLibraryPanel::setConfig(MultiScreenConfig *config)
{
    m_config = config;
    refresh();
}

void SoftwareLibraryPanel::refresh()
{
    const QString keepId = currentSoftwareId();
    listWidget->blockSignals(true);
    listWidget->clear();
    if (m_config != nullptr)
    {
        const QString keyword = searchEdit->text().trimmed();
        const QString filterType = typeCombo->currentData().toString();
        for (int i = 0; i < m_config->softwareList.size(); i++)
        {
            const SoftwareItem &software = m_config->softwareList[i];
            if (!filterType.isEmpty() && software.type != filterType)
            {
                continue;
            }
            if (!keyword.isEmpty() && !software.name.contains(keyword, Qt::CaseInsensitive) &&
                !software.path.contains(keyword, Qt::CaseInsensitive))
            {
                continue;
            }
            // 已绑定到屏幕的软件在列表第二行右侧显示标记，便于一眼识别不可再绑定的条目
            const QString boundText = boundScreenText(software.id);
            const QString typeText = typeDisplayName(software.type);
            QListWidgetItem *item = new QListWidgetItem(listWidget);
            item->setIcon(iconForSoftware(software));
            // 文本保留完整信息用于无障碍读取，实际显示由 SoftwareItemDelegate 分层绘制
            item->setText(QString("%1\n%2").arg(software.name).arg(typeText));
            item->setToolTip(boundText.isEmpty() ? software.path
                                                 : software.path + QStringLiteral("\n") + boundText);
            item->setData(Qt::UserRole, software.id);
            item->setData(kSoftwareNameRole, software.name);
            item->setData(kSoftwareTypeRole, typeText);
            item->setData(kSoftwareBoundRole, boundText);
            if (!boundText.isEmpty())
            {
                // 已绑定的软件不允许再次绑定：去掉拖拽标志，从拖拽源头阻止拖出，
                // 避免用户拖到屏幕后才收到“已绑定”的失败提示
                item->setFlags(item->flags() & ~Qt::ItemIsDragEnabled);
            }
            // 与 128x72 缩略图匹配的列表项高度，留出上下内边距
            item->setSizeHint(QSize(0, 84));
        }
    }
    listWidget->blockSignals(false);
    if (!keepId.isEmpty())
    {
        selectSoftware(keepId);
    }
    onCurrentItemChanged();
}

void SoftwareLibraryPanel::refreshBindingMarks()
{
    for (int i = 0; i < listWidget->count(); i++)
    {
        QListWidgetItem *item = listWidget->item(i);
        if (item == nullptr)
        {
            continue;
        }
        const QString softwareId = item->data(Qt::UserRole).toString();
        const SoftwareItem *software = findSoftware(softwareId);
        if (software == nullptr)
        {
            continue;
        }
        const QString boundText = boundScreenText(softwareId);
        const QString typeText = typeDisplayName(software->type);
        item->setText(QString("%1\n%2").arg(software->name).arg(typeText));
        item->setData(kSoftwareNameRole, software->name);
        item->setData(kSoftwareTypeRole, typeText);
        item->setData(kSoftwareBoundRole, boundText);
        item->setToolTip(boundText.isEmpty() ? software->path
                                             : software->path + QStringLiteral("\n") + boundText);
        // 绑定状态变化后同步拖拽能力：已绑定不可再拖出，解除绑定后恢复可拖拽
        Qt::ItemFlags flags = item->flags();
        if (boundText.isEmpty())
        {
            flags |= Qt::ItemIsDragEnabled;
        }
        else
        {
            flags &= ~Qt::ItemIsDragEnabled;
        }
        item->setFlags(flags);
    }
}

QString SoftwareLibraryPanel::currentSoftwareId() const
{
    QListWidgetItem *item = listWidget->currentItem();
    if (item == nullptr)
    {
        return QString();
    }
    return item->data(Qt::UserRole).toString();
}

void SoftwareLibraryPanel::selectSoftware(const QString &softwareId)
{
    for (int i = 0; i < listWidget->count(); i++)
    {
        QListWidgetItem *item = listWidget->item(i);
        if (item->data(Qt::UserRole).toString() == softwareId)
        {
            listWidget->setCurrentItem(item);
            return;
        }
    }
}

void SoftwareLibraryPanel::onFilterChanged()
{
    refresh();
}

void SoftwareLibraryPanel::onCurrentItemChanged()
{
    const QString softwareId = currentSoftwareId();
    editButton->setEnabled(!softwareId.isEmpty());
    removeButton->setEnabled(!softwareId.isEmpty());
    emit selectionChanged(softwareId);
}

void SoftwareLibraryPanel::onAdd()
{
    if (m_config == nullptr)
    {
        return;
    }
    // 预先生成 id：对话框内抓取的预览图按 id 命名，取消时可据 id 清理
    SoftwareItem software;
    software.id = ConfigStore::makeSoftwareId();
    SoftwareEditDialog dialog(software, m_config->softwareList, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }
    software = dialog.result();
    if (!dialog.previewAttempted())
    {
        // 对话框中未抓取过预览图（例如手动输入路径后直接确定），这里补抓一次
        generatePreview(&software);
    }
    else if (software.previewPath.isEmpty())
    {
        // 抓取失败时不再重复启动程序，保持无预览并回退类型图标
        software.previewPath.clear();
    }
    m_config->softwareList.append(software);
    refresh();
    selectSoftware(software.id);
    emit configChanged();
}

void SoftwareLibraryPanel::onEdit()
{
    if (m_config == nullptr)
    {
        return;
    }
    SoftwareItem *software = findSoftware(currentSoftwareId());
    if (software == nullptr)
    {
        return;
    }
    const QString softwareId = software->id;
    const QString oldPath = software->path;
    const QString oldType = software->type;
    SoftwareEditDialog dialog(*software, m_config->softwareList, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }
    *software = dialog.result();
    software->id = softwareId;
    if (software->previewPath.isEmpty())
    {
        if (software->path != oldPath || software->type != oldType)
        {
            // 目标程序变化且对话框内没有重新抓到图，旧预览已失效
            SoftwareCapture::removePreview(QApplication::applicationDirPath(), softwareId);
        }
        if (!dialog.previewAttempted())
        {
            generatePreview(software);
        }
    }
    refresh();
    selectSoftware(softwareId);
    emit configChanged();
}

void SoftwareLibraryPanel::onRemove()
{
    if (m_config == nullptr)
    {
        return;
    }
    SoftwareItem *software = findSoftware(currentSoftwareId());
    if (software == nullptr)
    {
        return;
    }
    const QString softwareId = software->id;
    QString message = QString::fromUtf8("确定删除软件“%1”？").arg(software->name);
    if (isReferenced(softwareId))
    {
        message += QString::fromUtf8("\n该软件已被屏幕引用，删除后会同时从这些屏幕中移除。");
    }
    if (QMessageBox::question(this, QString::fromUtf8("删除软件"), message,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }
    // 级联移除屏幕中的引用，避免保存时引用不存在的软件
    for (int i = 0; i < m_config->screenList.size(); i++)
    {
        QList<ScreenAssignment> &assignments = m_config->screenList[i].assignments;
        for (int j = assignments.size() - 1; j >= 0; j--)
        {
            if (assignments[j].softwareId == softwareId)
            {
                assignments.removeAt(j);
            }
        }
        m_config->screenList[i].appCount = assignments.size();
    }
    for (int i = m_config->softwareList.size() - 1; i >= 0; i--)
    {
        if (m_config->softwareList[i].id == softwareId)
        {
            m_config->softwareList.removeAt(i);
        }
    }
    // 软件删除后其预览图不再被引用，一并清理
    SoftwareCapture::removePreview(QApplication::applicationDirPath(), softwareId);
    refresh();
    emit configChanged();
}

void SoftwareLibraryPanel::onItemDoubleClicked(QListWidgetItem *item)
{
    if (item != nullptr)
    {
        listWidget->setCurrentItem(item);
        onEdit();
    }
}

QIcon SoftwareLibraryPanel::iconForSoftware(const SoftwareItem &software) const
{
    // 预览图统一输出为 128x72：宽度、高度都固定，避免不同宽高比的图片
    // 让列表项左侧图标的实际宽度不同，进而导致软件名称左边界参差不齐
    const QSize iconSize(128, 72);
    const QString previewFile = SoftwareCapture::resolveExistingPreview(QApplication::applicationDirPath(), software);
    if (!previewFile.isEmpty())
    {
        QPixmap preview(previewFile);
        if (!preview.isNull())
        {
            return QIcon(makeFixedPreview(preview, iconSize));
        }
    }

    // 无预览图时回退类型图标：绘制到同样尺寸的透明画布上居中显示，
    // 保证所有条目图标区域宽度一致、名称左对齐
    QIcon typeIcon;
    if (software.type == "web")
    {
        typeIcon = style()->standardIcon(QStyle::SP_DriveNetIcon);
    }
    else if (software.type == "ppt")
    {
        typeIcon = style()->standardIcon(QStyle::SP_FileIcon);
    }
    else
    {
        QFileInfo fileInfo(software.path);
        if (fileInfo.exists())
        {
            QFileIconProvider provider;
            typeIcon = provider.icon(fileInfo);
        }
        else
        {
            typeIcon = style()->standardIcon(QStyle::SP_ComputerIcon);
        }
    }
    QPixmap canvas(iconSize);
    canvas.fill(Qt::transparent);
    const QPixmap typePixmap = typeIcon.pixmap(48, 48);
    QPainter painter(&canvas);
    painter.drawPixmap((iconSize.width() - typePixmap.width()) / 2,
                       (iconSize.height() - typePixmap.height()) / 2, typePixmap);
    painter.end();
    return QIcon(canvas);
}

void SoftwareLibraryPanel::generatePreview(SoftwareItem *software)
{
    if (software == nullptr || software->id.isEmpty())
    {
        return;
    }
    const QString appDir = QApplication::applicationDirPath();
    if (SoftwareCapture::capturePreview(*software, appDir, this))
    {
        software->previewPath = SoftwareCapture::relativePreviewPath(software->id);
    }
    else
    {
        software->previewPath.clear();
    }
}

bool SoftwareLibraryPanel::isReferenced(const QString &softwareId) const
{
    if (m_config == nullptr)
    {
        return false;
    }
    for (int i = 0; i < m_config->screenList.size(); i++)
    {
        const QList<ScreenAssignment> &assignments = m_config->screenList[i].assignments;
        for (int j = 0; j < assignments.size(); j++)
        {
            if (assignments[j].softwareId == softwareId)
            {
                return true;
            }
        }
    }
    return false;
}

QString SoftwareLibraryPanel::boundScreenText(const QString &softwareId) const
{
    if (m_config == nullptr || softwareId.isEmpty())
    {
        return QString();
    }
    QStringList boundRows;
    for (int i = 0; i < m_config->screenList.size(); i++)
    {
        const QList<ScreenAssignment> &assignments = m_config->screenList[i].assignments;
        for (int j = 0; j < assignments.size(); j++)
        {
            if (assignments[j].softwareId == softwareId)
            {
                // 同一屏幕若因历史配置重复引用，只记录一次屏幕序号
                boundRows << QString::number(i + 1);
                break;
            }
        }
    }
    if (boundRows.isEmpty())
    {
        return QString();
    }
    return QString::fromUtf8("已绑定 %1号屏").arg(boundRows.join(QString::fromUtf8("、")));
}

SoftwareItem *SoftwareLibraryPanel::findSoftware(const QString &softwareId)
{
    if (m_config == nullptr || softwareId.isEmpty())
    {
        return nullptr;
    }
    for (int i = 0; i < m_config->softwareList.size(); i++)
    {
        if (m_config->softwareList[i].id == softwareId)
        {
            return &m_config->softwareList[i];
        }
    }
    return nullptr;
}
