#ifndef SOFTWARELIBRARYPANEL_H
#define SOFTWARELIBRARYPANEL_H

#include <QDialog>
#include <QIcon>
#include <QWidget>

#include "ScreenConfig.h"

class QComboBox;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;

/**
 * @brief 软件清单编辑对话框
 *
 * 新增与编辑共用同一套界面：
 *  - 桌面应用：路径为可执行文件，必须填写窗口标题用于运行时匹配窗口；
 *  - 网页：路径为 http(s) 地址，可选择性填写窗口标题；
 *  - PPT：路径为 .ppt/.pptx 文件，运行时通过 COM 放映。
 */
class SoftwareEditDialog : public QDialog
{
    Q_OBJECT

  public:
    /**
     * @brief 构造函数
     * @param item   待编辑的软件条目，新增时传入空条目
     * @param all    现有软件清单，用于路径重复校验
     * @param parent 父窗口
     */
    SoftwareEditDialog(const SoftwareItem &item, const QList<SoftwareItem> &all, QWidget *parent = nullptr);

    /**
     * @brief 获取编辑后的软件条目
     * @return 软件条目（id 与传入条目一致，新增时由调用方补全）
     */
    SoftwareItem result() const;

  private slots:
    /**
     * @brief 类型下拉框变化时切换“浏览”按钮状态与路径提示
     */
    void onTypeChanged();

    /**
     * @brief 浏览本地文件
     */
    void onBrowse();

    /**
     * @brief 校验并接受对话框
     */
    void onAccept();

  private:
    /** 待编辑条目（保存 id 等调用方字段） */
    SoftwareItem m_item;
    /** 现有软件清单，用于重复校验 */
    QList<SoftwareItem> m_all;

    QComboBox *typeCombo = nullptr;
    QLineEdit *nameEdit = nullptr;
    QLineEdit *pathEdit = nullptr;
    QLineEdit *windowTitleEdit = nullptr;
    QLineEdit *startupArgsEdit = nullptr;
    QLineEdit *remarkEdit = nullptr;
    QPushButton *browseButton = nullptr;
};

/**
 * @brief 软件清单面板
 *
 * 维护全局软件条目：搜索、按类型筛选、新增、编辑、删除，
 * 并提供拖拽源能力（MIME 类型 application/x-mdc-software-id）。
 */
class SoftwareLibraryPanel : public QWidget
{
    Q_OBJECT

  public:
    /**
     * @brief 构造函数
     * @param parent 父窗口
     */
    explicit SoftwareLibraryPanel(QWidget *parent = nullptr);

    /**
     * @brief 绑定配置对象（面板直接读写其中的 softwareList）
     * @param config 全量配置
     */
    void setConfig(MultiScreenConfig *config);

    /**
     * @brief 按当前配置重建列表
     */
    void refresh();

    /**
     * @brief 获取当前选中的软件 id
     * @return 软件 id，未选中返回空字符串
     */
    QString currentSoftwareId() const;

    /**
     * @brief 选中指定软件
     * @param softwareId 软件 id
     */
    void selectSoftware(const QString &softwareId);

  signals:
    /**
     * @brief 当前选中的软件发生变化
     * @param softwareId 软件 id，未选中为空字符串
     */
    void selectionChanged(const QString &softwareId);

    /**
     * @brief 软件清单发生增删改
     */
    void configChanged();

  private slots:
    /**
     * @brief 搜索关键字或类型筛选变化
     */
    void onFilterChanged();

    /**
     * @brief 列表选中项变化
     */
    void onCurrentItemChanged();

    /**
     * @brief 新增软件
     */
    void onAdd();

    /**
     * @brief 编辑当前软件
     */
    void onEdit();

    /**
     * @brief 删除当前软件（级联移除各屏幕引用）
     */
    void onRemove();

    /**
     * @brief 双击条目编辑
     * @param item 被双击的条目
     */
    void onItemDoubleClicked(QListWidgetItem *item);

  private:
    /**
     * @brief 为软件条目生成列表图标
     * @param software 软件条目
     * @return 预览缩略图；无预览图时回退类型图标
     */
    QIcon iconForSoftware(const SoftwareItem &software) const;

    /**
     * @brief 启动软件一次并抓取预览图，成功时写入 software->previewPath
     * @param software 软件条目（id 必须已生成）
     */
    void generatePreview(SoftwareItem *software);

    /**
     * @brief 判断软件是否已被任意屏幕引用
     * @param softwareId 软件 id
     * @return true 表示已被引用
     */
    bool isReferenced(const QString &softwareId) const;

    /**
     * @brief 从软件清单中查找条目
     * @param softwareId 软件 id
     * @return 命中返回条目指针，否则返回 nullptr
     */
    SoftwareItem *findSoftware(const QString &softwareId);

    MultiScreenConfig *m_config = nullptr;

    QLineEdit *searchEdit = nullptr;
    QComboBox *typeCombo = nullptr;
    QListWidget *listWidget = nullptr;
    QPushButton *addButton = nullptr;
    QPushButton *editButton = nullptr;
    QPushButton *removeButton = nullptr;
};

#endif  // SOFTWARELIBRARYPANEL_H
