#ifndef SOFTWAREPREVIEWPANEL_H
#define SOFTWAREPREVIEWPANEL_H

#include <QStringList>
#include <QWidget>

#include "ScreenConfig.h"

class CefWebView;
class QLabel;
class QPushButton;
class QVBoxLayout;

/**
 * @brief 分层预览面板
 *
 * 包含两部分：
 *  - 软件详情：桌面应用显示系统图标与路径，网页提供“加载实时预览”按钮，使用 CEF 内嵌预览；
 *  - 屏幕摘要：显示选中屏幕的序号、名称、标签顺序与软件数量，便于确认拖拽结果。
 */
class SoftwarePreviewPanel : public QWidget
{
    Q_OBJECT

  public:
    /**
     * @brief 构造函数
     * @param parent 父窗口
     */
    explicit SoftwarePreviewPanel(QWidget *parent = nullptr);

    /**
     * @brief 预览指定的软件条目
     * @param software 软件条目，nullptr 表示清空
     * @param screenNames 未使用，保留接口扩展
     */
    void showSoftware(const SoftwareItem *software);

    /**
     * @brief 显示屏幕摘要
     * @param screen   屏幕配置，nullptr 表示清空
     * @param software 全量软件清单，用于解析名称
     */
    void showScreen(const ScreenConfig *screen, const QList<SoftwareItem> *software);

    /**
     * @brief 关闭并销毁实时预览视图
     */
    void closePreview();

  private slots:
    /**
     * @brief 切换网页实时预览的打开/关闭
     */
    void onTogglePreview();

  private:
    /**
     * @brief 构造标签顺序示意文本
     * @param screen   屏幕配置
     * @param software 软件清单
     * @return 富文本
     */
    QString buildTagStrip(const ScreenConfig &screen, const QList<SoftwareItem> *software) const;

    /** 当前预览的软件条目副本 */
    SoftwareItem m_software;
    /** 当前是否有软件被选中 */
    bool m_hasSoftware = false;

    QLabel *softwareTitle = nullptr;
    QLabel *softwareType = nullptr;
    QLabel *softwareIcon = nullptr;
    QLabel *pathLabel = nullptr;
    QLabel *detailLabel = nullptr;
    QPushButton *previewButton = nullptr;
    QWidget *previewHost = nullptr;
    QVBoxLayout *previewLayout = nullptr;
    CefWebView *cefView = nullptr;

    QLabel *screenTitle = nullptr;
    QLabel *screenTagStrip = nullptr;
    QLabel *screenHint = nullptr;
};

#endif  // SOFTWAREPREVIEWPANEL_H
