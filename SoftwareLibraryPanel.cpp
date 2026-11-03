#include "SoftwareLibraryPanel.h"

#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QDrag>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QStyle>
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

    /**
     * @brief 类型显示名称
     * @param type 类型标识
     * @return 中文名称
     */
    QString typeDisplayName(const QString &type)
    {
        if (type == "web")
        {
            return QStringLiteral("网页");
        }
        if (type == "ppt")
        {
            return QStringLiteral("演示文稿");
        }
        return QStringLiteral("桌面应用");
    }
}  // namespace

SoftwareEditDialog::SoftwareEditDialog(const SoftwareItem &item, const QList<SoftwareItem> &all, QWidget *parent)
    : QDialog(parent), m_item(item), m_all(all)
{
    setWindowTitle(item.id.isEmpty() ? QStringLiteral("新增软件") : QStringLiteral("编辑软件"));
    setMinimumWidth(560);

    typeCombo = new QComboBox(this);
    typeCombo->addItem(QStringLiteral("桌面应用"), QStringLiteral("desktop"));
    typeCombo->addItem(QStringLiteral("网页"), QStringLiteral("web"));
    typeCombo->addItem(QStringLiteral("演示文稿"), QStringLiteral("ppt"));

    nameEdit = new QLineEdit(this);
    nameEdit->setPlaceholderText(QStringLiteral("例如：OA办公系统"));

    pathEdit = new QLineEdit(this);
    browseButton = new QPushButton(QStringLiteral("浏览..."), this);

    QWidget *pathRow = new QWidget(this);
    QHBoxLayout *pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    pathLayout->addWidget(pathEdit, 1);
    pathLayout->addWidget(browseButton);

    windowTitleEdit = new QLineEdit(this);
    windowTitleEdit->setPlaceholderText(QStringLiteral("桌面应用必填，用于匹配要嵌入的窗口"));
    startupArgsEdit = new QLineEdit(this);
    startupArgsEdit->setPlaceholderText(QStringLiteral("可选，例如：--kiosk"));
    remarkEdit = new QLineEdit(this);
    remarkEdit->setPlaceholderText(QStringLiteral("可选，仅用于备注说明"));

    QFormLayout *form = new QFormLayout(this);
    form->addRow(QStringLiteral("类型"), typeCombo);
    form->addRow(QStringLiteral("名称"), nameEdit);
    form->addRow(QStringLiteral("路径/地址"), pathRow);
    form->addRow(QStringLiteral("窗口标题"), windowTitleEdit);
    form->addRow(QStringLiteral("启动参数"), startupArgsEdit);
    form->addRow(QStringLiteral("备注"), remarkEdit);

    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addStretch(1);
    QPushButton *cancelButton = new QPushButton(QStringLiteral("取消"), this);
    QPushButton *okButton = new QPushButton(QStringLiteral("确定"), this);
    okButton->setDefault(true);
    buttons->addWidget(cancelButton);
    buttons->addWidget(okButton);
    form->addRow(buttons);

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
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(okButton, &QPushButton::clicked, this, [this]() { onAccept(); });
    onTypeChanged();
}

SoftwareItem SoftwareEditDialog::result() const
{
    return m_item;
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
        pathEdit->setPlaceholderText(QStringLiteral(".ppt / .pptx 文件完整路径"));
    }
    else
    {
        pathEdit->setPlaceholderText(QStringLiteral(".exe 文件完整路径"));
    }
}

void SoftwareEditDialog::onBrowse()
{
    const QString type = typeCombo->currentData().toString();
    QString filter = QStringLiteral("所有文件 (*.*)");
    if (type == "desktop")
    {
        filter = QStringLiteral("可执行文件 (*.exe)");
    }
    else if (type == "ppt")
    {
        filter = QStringLiteral("演示文稿 (*.ppt *.pptx)");
    }
    const QString filePath = QFileDialog::getOpenFileName(this, QStringLiteral("选择文件"), pathEdit->text(), filter);
    if (!filePath.isEmpty())
    {
        pathEdit->setText(filePath);
        if (nameEdit->text().isEmpty())
        {
            nameEdit->setText(QFileInfo(filePath).completeBaseName());
        }
    }
}

void SoftwareEditDialog::onAccept()
{
    const QString type = typeCombo->currentData().toString();
    const QString name = nameEdit->text().trimmed();
    const QString path = pathEdit->text().trimmed();
    const QString windowTitle = windowTitleEdit->text().trimmed();

    if (name.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("校验失败"), QStringLiteral("名称不能为空。"));
        return;
    }
    if (path.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("校验失败"), QStringLiteral("路径/地址不能为空。"));
        return;
    }
    if (type == "web")
    {
        if (!path.startsWith("http://") && !path.startsWith("https://"))
        {
            QMessageBox::warning(this, QStringLiteral("校验失败"),
                                 QStringLiteral("网页地址必须以 http:// 或 https:// 开头。"));
            return;
        }
    }
    else if (type == "ppt")
    {
        if (!ConfigStore::isPptPath(path))
        {
            QMessageBox::warning(this, QStringLiteral("校验失败"), QStringLiteral("请选择 .ppt 或 .pptx 文件。"));
            return;
        }
    }
    else if (windowTitle.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("校验失败"),
                             QStringLiteral("桌面应用必须填写窗口标题，运行时依赖它匹配要嵌入的窗口。"));
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
            QMessageBox::warning(this, QStringLiteral("校验失败"),
                                 QStringLiteral("该路径已存在于软件清单：%1").arg(m_all[i].name));
            return;
        }
    }

    m_item.type = type;
    m_item.name = name;
    m_item.path = path;
    m_item.windowTitle = windowTitle;
    m_item.startupArgs = startupArgsEdit->text().trimmed();
    m_item.remark = remarkEdit->text().trimmed();
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
    searchEdit->setPlaceholderText(QStringLiteral("搜索名称或路径"));

    typeCombo = new QComboBox(this);
    typeCombo->addItem(QStringLiteral("全部类型"), QString());
    typeCombo->addItem(QStringLiteral("桌面应用"), QStringLiteral("desktop"));
    typeCombo->addItem(QStringLiteral("网页"), QStringLiteral("web"));
    typeCombo->addItem(QStringLiteral("演示文稿"), QStringLiteral("ppt"));

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

    addButton = new QPushButton(QStringLiteral("新增"), this);
    editButton = new QPushButton(QStringLiteral("编辑"), this);
    removeButton = new QPushButton(QStringLiteral("删除"), this);

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
            QListWidgetItem *item = new QListWidgetItem(listWidget);
            item->setIcon(iconForSoftware(software));
            item->setText(QString("%1\n%2").arg(software.name).arg(typeDisplayName(software.type)));
            item->setToolTip(software.path);
            item->setData(Qt::UserRole, software.id);
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
    SoftwareItem software;
    SoftwareEditDialog dialog(software, m_config->softwareList, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }
    software = dialog.result();
    software.id = ConfigStore::makeSoftwareId();
    // 新增软件时自动运行一次并抓取第一个界面作为预览图，失败则回退类型图标
    generatePreview(&software);
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
    if (software->path != oldPath || software->type != oldType)
    {
        // 目标程序变化后旧预览图已失效，先清理再重新抓取
        SoftwareCapture::removePreview(QApplication::applicationDirPath(), softwareId);
        software->previewPath.clear();
        generatePreview(software);
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
    QString message = QStringLiteral("确定删除软件“%1”？").arg(software->name);
    if (isReferenced(softwareId))
    {
        message += QStringLiteral("\n该软件已被屏幕引用，删除后会同时从这些屏幕中移除。");
    }
    if (QMessageBox::question(this, QStringLiteral("删除软件"), message,
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
    const QString previewFile = SoftwareCapture::resolveExistingPreview(QApplication::applicationDirPath(), software);
    if (!previewFile.isEmpty())
    {
        QPixmap preview(previewFile);
        if (!preview.isNull())
        {
            // 预览图按列表图标区域等比缩放，避免拉伸变形
            preview = preview.scaled(QSize(128, 72), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            return QIcon(preview);
        }
    }
    if (software.type == "web")
    {
        return style()->standardIcon(QStyle::SP_DriveNetIcon);
    }
    if (software.type == "ppt")
    {
        return style()->standardIcon(QStyle::SP_FileIcon);
    }
    QFileInfo fileInfo(software.path);
    if (fileInfo.exists())
    {
        QFileIconProvider provider;
        return provider.icon(fileInfo);
    }
    return style()->standardIcon(QStyle::SP_ComputerIcon);
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
