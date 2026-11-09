#ifndef MANAGEMENTWINDOW_H
#define MANAGEMENTWINDOW_H

#include <QPixmap>
#include <QPoint>
#include <QStringList>
#include <QWidget>

#include "ScreenConfig.h"

class QCheckBox;
class QFrame;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QSplitter;
class ScreenLayoutCanvas;
class SoftwareLibraryPanel;

/**
 * @brief 多屏控制管理主窗口
 *
 * 通过 Alt+M 显示/隐藏，负责：
 *  - 汇总软件清单与屏幕布局两个面板的编辑结果；
 *  - 校验并保存配置（仅写入 config.json）；
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
     * @brief 以所在屏幕 90% 的尺寸居中显示管理窗口并置顶
     *
     * 窗口尺寸为鼠标所在屏幕的 90%（不小于最小尺寸 820×560），
     * 显示在屏幕中心位置，便于多屏环境下就地管理。
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

    /**
     * @brief 刷新各 splitter 的尺寸比例
     *
     * 在窗口显示后调用，强制按设计比例分配三栏宽度和左侧上下高度，
     * 避免子控件 sizeHint 导致初始比例偏离。
     */
    void refreshSplitterSizes();

  signals:
    /**
     * @brief 请求把配置应用到运行时
     * @param config 已完成归一化的全量配置
     */
    void applyRequested(const MultiScreenConfig &config);

    /**
     * @brief 当前选中的屏幕序号变化（大屏红色边框反馈用）
     *
     * 在管理界面点击不同屏幕卡片或物理屏示意图时发出，
     * 运行时收到后在对应大屏上显示红色边框，方便用户定位正在编辑的屏幕。
     * 未选中任何屏幕时参数为 0。
     *
     * @param screenIndex 屏幕序号（1-based），0 表示取消高亮
     */
    void screenHighlightChanged(int screenIndex);

  protected:
    /**
     * @brief 关闭按钮只隐藏窗口，不退出程序
     * @param event 关闭事件
     */
    void closeEvent(QCloseEvent *event) override;

    /**
     * @brief 显示时恢复当前选中屏幕的大屏高亮
     * @param event 显示事件
     */
    void showEvent(QShowEvent *event) override;

    /**
     * @brief 无边框窗口拖动：按下空白区域开始拖动
     * @param event 鼠标事件
     */
    void mousePressEvent(QMouseEvent *event) override;

    /**
     * @brief 无边框窗口拖动：按住左键移动窗口
     * @param event 鼠标事件
     */
    void mouseMoveEvent(QMouseEvent *event) override;

    /**
     * @brief 无边框窗口拖动：松开左键结束拖动
     * @param event 鼠标事件
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

    /**
     * @brief 事件过滤器：监听预览图标签的尺寸变化并重新缩放图片
     *
     * 首次打开窗口时预览标签尚未完成布局，直接按当时的 size() 缩放会得到很小的图；
     * 因此保存原始 pixmap，在标签真实尺寸确定后（Resize 事件）再按实际大小重绘。
     *
     * @param watched 事件来源对象
     * @param event   事件
     * @return true 表示已处理并拦截该事件，false 表示继续正常分发
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

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

    /**
     * @brief 选中屏幕变化，更新右侧软件清单
     * @param row 屏幕行号
     */
    void onScreenSelected(int row);

    /**
     * @brief 软件清单选中变化，更新选中状态
     * @param softwareId 软件 id
     */
    void onLibrarySelectionChanged(const QString &softwareId);

    /**
     * @brief 右侧屏幕软件清单选中变化
     * @param item 列表项
     */
    void onScreenSoftwareClicked(QListWidgetItem *item);

  private:
    /**
     * @brief 解除指定软件与当前选中屏幕的绑定
     *
     * 由中间清单的“拖出条目”与“点击右侧 × 图标”触发；当前无选中屏幕或软件未绑定时不做处理。
     *
     * @param softwareId 要解除绑定的软件 id
     */
    void unbindScreenSoftware(const QString &softwareId);

    /**
     * @brief 标记存在未保存修改
     */
    void markDirty();

    /**
     * @brief 清除未保存标记
     */
    void clearDirty();

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

    /**
     * @brief 查找软件当前绑定的屏幕行号
     *
     * 用于禁止同一软件重复绑定：绑定关系按软件 id 在全部屏幕的 assignments 中查找。
     *
     * @param softwareId 软件 id
     * @return 屏幕行号（从 0 开始），未绑定返回 -1
     */
    int boundScreenRow(const QString &softwareId) const;

    /**
     * @brief 更新左下软件预览区
     * @param softwareId 软件 id，为空时清空预览
     */
    void updateSoftwarePreview(const QString &softwareId);

    /**
     * @brief 按预览图标签的当前尺寸重绘原始 pixmap
     *
     * 内部保留原始图片，只在标签已有有效尺寸时缩放显示，避免首次打开时因
     * 标签尚未完成布局而被缩成小图。
     */
    void applyPreviewPixmap();

    /**
     * @brief 刷新中间选中屏幕的软件清单
     * @param screenRow 屏幕行号
     */
    void refreshScreenSoftwareList(int screenRow);

    /** 工作配置（所有面板直接读写） */
    MultiScreenConfig m_config;
    /** 是否存在未保存修改 */
    bool m_dirty = false;
    /** 无边框窗口拖动：按下点相对窗口左上角的偏移 */
    QPoint m_dragOffset;
    /** 是否正在拖动窗口 */
    bool m_dragging = false;

    QLabel *titleLabel = nullptr;
    QLabel *dirtyLabel = nullptr;
    QLabel *statusLabel = nullptr;
    QCheckBox *showIndexCheck = nullptr;
    QCheckBox *sortScreenCheck = nullptr;
    QCheckBox *debugModeCheck = nullptr;
    QCheckBox *runAsAdminCheck = nullptr;
    /** 禁用 Win 键开关：保存后立即生效，用于避免误触系统组合键打断全屏展示 */
    QCheckBox *disableWinKeyCheck = nullptr;
    /** 嵌入应用去掉边框开关：开启后桌面应用客户区铺满整块屏幕，需重启后生效 */
    QCheckBox *stripAppBorderCheck = nullptr;
    /** 禁用 GPU 加速开关：仅在机型/驱动出现网页渲染异常时开启，需重启后生效 */
    QCheckBox *disableGpuAccelerationCheck = nullptr;
    QPushButton *saveButton = nullptr;
    QPushButton *reloadButton = nullptr;
    QPushButton *closeButton = nullptr;

    // 布局：左(上屏幕布局 + 下软件预览) / 中(选中屏软件) / 右(总软件清单)
    class QSplitter *contentSplitter = nullptr;
    class QSplitter *leftSplitter = nullptr;
    ScreenLayoutCanvas *layoutCanvas = nullptr;
    /** 中间「x号屏的软件」面板，宽度上限决定其可拖动的最大宽度 */
    QWidget *screenListGroup = nullptr;
    /** 右侧「软件清单」面板，宽度上限决定其可拖动的最大宽度 */
    QWidget *libraryGroup = nullptr;

    // 左下软件预览
    QLabel *previewTitleLabel = nullptr;
    QLabel *previewImageLabel = nullptr;
    /** 当前预览图的原始像素图，用于在标签尺寸变化后按实际大小重新缩放 */
    QPixmap m_previewSourcePixmap;
    QLabel *previewNameLabel = nullptr;
    QLabel *previewTypeLabel = nullptr;
    QLabel *previewPathLabel = nullptr;
    QLabel *previewWindowTitleLabel = nullptr;
    QLabel *previewRemarkLabel = nullptr;

    // 中间：选中屏幕的软件清单
    QLabel *screenListTitle = nullptr;
    QListWidget *screenSoftwareList = nullptr;

    // 右侧：总软件清单
    SoftwareLibraryPanel *libraryPanel = nullptr;
};

#endif  // MANAGEMENTWINDOW_H
