#ifndef MANAGEMENTWINDOW_H
#define MANAGEMENTWINDOW_H

#include <QStringList>
#include <QWidget>

#include "ScreenConfig.h"

class QCheckBox;
class QLabel;
class QPushButton;
class ScreenLayoutCanvas;
class SoftwareLibraryPanel;
class SoftwarePreviewPanel;

/**
 * @brief 多屏控制管理主窗口
 *
 * 通过 Alt+M 显示/隐藏，负责：
 *  - 汇总软件清单、屏幕布局、预览三个面板的编辑结果；
 *  - 校验并保存配置（config.json + config.ini）；
 *  - 向主程序请求增量热应用，并展示应用结果与警告。
 */
class ManagementWindow : public QWidget
{
    Q_OBJECT

  public:
    /**
     * @brief 构造函数
     * @param parent 父窗口
     */
    explicit ManagementWindow(QWidget *parent = nullptr);

    /**
     * @brief 载入配置并刷新全部面板
     * @param config 从磁盘加载的配置
     */
    void setConfig(const MultiScreenConfig &config);

    /**
     * @brief 显示/隐藏管理窗口（Alt+M）
     */
    void toggle();

    /**
     * @brief 显示管理窗口并置顶
     */
    void showManager();

    /**
     * @brief 在状态栏显示提示
     * @param message 提示内容
     * @param warning 是否为警告样式
     */
    void setStatusMessage(const QString &message, bool warning = false);

    /**
     * @brief 展示运行时热应用结果
     * @param warnings 未能热应用的警告列表
     */
    void notifyApplyResult(const QStringList &warnings);

  signals:
    /**
     * @brief 请求把配置应用到运行时
     * @param config 已完成归一化的全量配置
     */
    void applyRequested(const MultiScreenConfig &config);

  protected:
    /**
     * @brief 关闭按钮只隐藏窗口，不退出程序
     * @param event 关闭事件
     */
    void closeEvent(QCloseEvent *event) override;

  private slots:
    /**
     * @brief 保存并应用
     */
    void onSave();

    /**
     * @brief 放弃未保存修改并从磁盘重新加载
     */
    void onReload();

    /**
     * @brief 软件清单选中项变化
     * @param softwareId 软件 id
     */
    void onSoftwareSelected(const QString &softwareId);

    /**
     * @brief 屏幕选中项变化
     * @param row 屏幕行号
     */
    void onScreenSelected(int row);

    /**
     * @brief 从软件清单拖入软件到指定屏幕
     * @param screenRow  屏幕行号
     * @param softwareId 软件 id
     * @param insertRow  插入行
     */
    void onSoftwareDropped(int screenRow, const QString &softwareId, int insertRow);

    /**
     * @brief 全局开关变化
     */
    void onGlobalSettingChanged();

  private:
    /**
     * @brief 标记存在未保存修改
     */
    void markDirty();

    /**
     * @brief 清除未保存标记
     */
    void clearDirty();

    /**
     * @brief 刷新预览面板的选中内容
     */
    void refreshPreview();

    /**
     * @brief 校验配置是否可以保存
     * @param error 输出参数，校验失败原因
     * @return true 表示校验通过
     */
    bool validate(QString *error) const;

    /**
     * @brief 归一化配置：补齐序号、全局开关与物理屏绑定
     * @return 归一化后的配置副本
     */
    MultiScreenConfig buildSubmitConfig() const;

    /**
     * @brief 查找软件条目
     * @param softwareId 软件 id
     * @return 命中返回条目指针，否则返回 nullptr
     */
    const SoftwareItem *findSoftware(const QString &softwareId) const;

    /** 工作配置（所有面板直接读写） */
    MultiScreenConfig m_config;
    /** 是否存在未保存修改 */
    bool m_dirty = false;

    QLabel *titleLabel = nullptr;
    QLabel *dirtyLabel = nullptr;
    QLabel *statusLabel = nullptr;
    QCheckBox *showIndexCheck = nullptr;
    QCheckBox *sortScreenCheck = nullptr;
    QCheckBox *debugModeCheck = nullptr;
    QCheckBox *runAsAdminCheck = nullptr;
    QPushButton *saveButton = nullptr;
    QPushButton *reloadButton = nullptr;
    QPushButton *closeButton = nullptr;

    SoftwareLibraryPanel *libraryPanel = nullptr;
    ScreenLayoutCanvas *layoutCanvas = nullptr;
    SoftwarePreviewPanel *previewPanel = nullptr;
};

#endif  // MANAGEMENTWINDOW_H
