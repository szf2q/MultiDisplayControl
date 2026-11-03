#include "SoftwarePreviewPanel.h"

#include <QFileIconProvider>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>

#include "CefWebView.h"

SoftwarePreviewPanel::SoftwarePreviewPanel(QWidget *parent) : QWidget(parent)
{
    // ==================== 软件详情 ====================
    QGroupBox *softwareGroup = new QGroupBox(QStringLiteral("软件预览"), this);

    softwareIcon = new QLabel(softwareGroup);
    softwareIcon->setFixedSize(72, 72);
    softwareIcon->setAlignment(Qt::AlignCenter);
    softwareIcon->setObjectName(QStringLiteral("previewIcon"));

    softwareTitle = new QLabel(softwareGroup);
    softwareTitle->setObjectName(QStringLiteral("previewTitle"));
    softwareTitle->setWordWrap(true);
    softwareType = new QLabel(softwareGroup);
    softwareType->setObjectName(QStringLiteral("hintLabel"));

    QVBoxLayout *titleLayout = new QVBoxLayout();
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->addWidget(softwareTitle);
    titleLayout->addWidget(softwareType);
    titleLayout->addStretch(1);

    QHBoxLayout *headLayout = new QHBoxLayout();
    headLayout->addWidget(softwareIcon);
    headLayout->addLayout(titleLayout, 1);

    pathLabel = new QLabel(softwareGroup);
    pathLabel->setObjectName(QStringLiteral("hintLabel"));
    pathLabel->setWordWrap(true);
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    detailLabel = new QLabel(softwareGroup);
    detailLabel->setObjectName(QStringLiteral("hintLabel"));
    detailLabel->setWordWrap(true);

    previewButton = new QPushButton(QStringLiteral("加载实时预览"), softwareGroup);
    previewButton->setEnabled(false);

    previewHost = new QWidget(softwareGroup);
    previewLayout = new QVBoxLayout(previewHost);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewHost->setMinimumHeight(260);

    QVBoxLayout *softwareLayout = new QVBoxLayout(softwareGroup);
    softwareLayout->addLayout(headLayout);
    softwareLayout->addWidget(pathLabel);
    softwareLayout->addWidget(detailLabel);
    softwareLayout->addWidget(previewButton);
    softwareLayout->addWidget(previewHost, 1);

    // ==================== 屏幕摘要 ====================
    QGroupBox *screenGroup = new QGroupBox(QStringLiteral("屏幕摘要"), this);
    screenTitle = new QLabel(screenGroup);
    screenTitle->setObjectName(QStringLiteral("previewTitle"));
    screenTagStrip = new QLabel(screenGroup);
    screenTagStrip->setWordWrap(true);
    screenTagStrip->setTextFormat(Qt::RichText);
    screenHint = new QLabel(screenGroup);
    screenHint->setObjectName(QStringLiteral("hintLabel"));
    screenHint->setWordWrap(true);

    QVBoxLayout *screenLayout = new QVBoxLayout(screenGroup);
    screenLayout->addWidget(screenTitle);
    screenLayout->addWidget(screenTagStrip);
    screenLayout->addWidget(screenHint);
    screenLayout->addStretch(1);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(softwareGroup, 1);
    mainLayout->addWidget(screenGroup);

    connect(previewButton, &QPushButton::clicked, this, [this]() { onTogglePreview(); });

    showSoftware(nullptr);
    showScreen(nullptr, nullptr);
}

void SoftwarePreviewPanel::showSoftware(const SoftwareItem *software)
{
    closePreview();
    m_hasSoftware = (software != nullptr);
    m_software = software != nullptr ? *software : SoftwareItem();

    if (software == nullptr)
    {
        softwareTitle->setText(QStringLiteral("未选择软件"));
        softwareType->setText(QStringLiteral("在左侧软件清单中选择条目查看详情"));
        softwareIcon->clear();
        pathLabel->clear();
        detailLabel->clear();
        previewButton->setEnabled(false);
        previewButton->setText(QStringLiteral("加载实时预览"));
        return;
    }

    softwareTitle->setText(m_software.name);
    pathLabel->setText(m_software.path);
    previewButton->setEnabled(m_software.type == "web");
    previewButton->setText(QStringLiteral("加载实时预览"));

    if (m_software.type == "web")
    {
        softwareType->setText(QStringLiteral("网页"));
        const QString host = QUrl(m_software.path).host();
        detailLabel->setText(QStringLiteral("站点：%1\n窗口标题：%2")
                                 .arg(host.isEmpty() ? QStringLiteral("-") : host)
                                 .arg(m_software.windowTitle.isEmpty() ? QStringLiteral("-")
                                                                       : m_software.windowTitle));
        softwareIcon->setPixmap(style()->standardIcon(QStyle::SP_DriveNetIcon).pixmap(64, 64));
    }
    else if (m_software.type == "ppt")
    {
        softwareType->setText(QStringLiteral("演示文稿"));
        detailLabel->setText(QStringLiteral("运行时通过 COM 放映并嵌入窗口"));
        softwareIcon->setPixmap(style()->standardIcon(QStyle::SP_FileIcon).pixmap(64, 64));
    }
    else
    {
        softwareType->setText(QStringLiteral("桌面应用"));
        detailLabel->setText(QStringLiteral("窗口标题：%1\n启动参数：%2")
                                 .arg(m_software.windowTitle.isEmpty() ? QStringLiteral("-") : m_software.windowTitle)
                                 .arg(m_software.startupArgs.isEmpty() ? QStringLiteral("-")
                                                                      : m_software.startupArgs));
        QFileInfo fileInfo(m_software.path);
        if (fileInfo.exists())
        {
            QFileIconProvider provider;
            softwareIcon->setPixmap(provider.icon(fileInfo).pixmap(64, 64));
        }
        else
        {
            softwareIcon->setPixmap(style()->standardIcon(QStyle::SP_ComputerIcon).pixmap(64, 64));
        }
    }
}

void SoftwarePreviewPanel::showScreen(const ScreenConfig *screen, const QList<SoftwareItem> *software)
{
    if (screen == nullptr)
    {
        screenTitle->setText(QStringLiteral("未选择屏幕"));
        screenTagStrip->clear();
        screenHint->setText(QStringLiteral("在中间屏序卡片中选择一块屏幕查看摘要"));
        return;
    }
    const QString name = screen->screenTitle.isEmpty() ? QStringLiteral("未命名") : screen->screenTitle;
    screenTitle->setText(QString("%1号屏 · %2").arg(screen->screenIndex).arg(name));
    screenTagStrip->setText(buildTagStrip(*screen, software));
    screenHint->setText(QStringLiteral("已分配 %1 个软件，标签顺序即上方芯片从左到右的顺序。")
                            .arg(screen->assignments.size()));
}

void SoftwarePreviewPanel::closePreview()
{
    if (cefView != nullptr)
    {
        previewLayout->removeWidget(cefView);
        cefView->deleteLater();
        cefView = nullptr;
    }
}

void SoftwarePreviewPanel::onTogglePreview()
{
    if (cefView != nullptr)
    {
        closePreview();
        previewButton->setText(QStringLiteral("加载实时预览"));
        return;
    }
    if (!m_hasSoftware || m_software.type != "web")
    {
        return;
    }
    // 实时预览复用主程序已初始化的 CEF 上下文，只在需要时创建，关闭或切换软件时立即销毁
    cefView = new CefWebView(QUrl(m_software.path), previewHost);
    previewLayout->addWidget(cefView);
    cefView->show();
    previewButton->setText(QStringLiteral("关闭实时预览"));
}

QString SoftwarePreviewPanel::buildTagStrip(const ScreenConfig &screen, const QList<SoftwareItem> *software) const
{
    if (screen.assignments.isEmpty())
    {
        return QStringLiteral("<span style='color:#8aa6c4'>该屏尚未分配软件，可从左侧拖入</span>");
    }
    QString html;
    for (int i = 0; i < screen.assignments.size(); i++)
    {
        const ScreenAssignment &assignment = screen.assignments[i];
        QString name = assignment.softwareId;
        if (software != nullptr)
        {
            for (int j = 0; j < software->size(); j++)
            {
                if ((*software)[j].id == assignment.softwareId)
                {
                    name = assignment.tabName.isEmpty() ? (*software)[j].name : assignment.tabName;
                    break;
                }
            }
        }
        html += QString("<span style='background-color:#16395f;color:#d7e9ff;padding:4px 8px;"
                        "border-radius:6px;margin-right:6px;'>%1. %2</span>")
                    .arg(i + 1)
                    .arg(name.toHtmlEscaped());
    }
    return html;
}
