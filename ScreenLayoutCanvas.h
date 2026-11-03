#ifndef SCREENLAYOUTCANVAS_H
#define SCREENLAYOUTCANVAS_H

#include <QFrame>
#include <QListWidget>
#include <QWidget>

#include "ScreenConfig.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QSpinBox;

/**
 * @brief 物理屏布局示意图
 *
 * 按所有物理屏的真实几何等比缩放绘制矩形，并标注名称、分辨率与主屏标记；
 * 选中某张屏序卡片时高亮其绑定的物理屏，帮助用户确认映射关系。
 */
class PhysicalScreenMap : public QWidget
{
    Q_OBJECT

  public:
    /**
     * @brief 构造函数
     * @param parent 父窗口
     */
    explicit PhysicalScreenMap(QWidget *parent = nullptr);

    /**
     * @brief 设置要高亮的物理屏名称
     * @param displayName QScreen::name()
     */
    void setHighlightedScreen(const QString &displayName);

    /**
     * @brief 按当前连接的物理屏刷新示意图
     */
    void refresh();

  protected:
    /**
     * @brief 自绘物理屏矩形
     * @param event 绘制事件
     */
    void paintEvent(QPaintEvent *event) override;

  private:
    /** 要高亮的物理屏名称 */
    QString m_highlighted;
};

/**
 * @brief 屏幕上的软件分配列表
 *
 * 支持：
 *  - 接收软件清单拖入（MIME application/x-mdc-software-id）；
 *  - 内部拖拽调整标签顺序；
 *  - Delete 键或右键菜单移除分配。
 */
class AppAssignmentList : public QListWidget
{
    Q_OBJECT

  public:
    /**
     * @brief 构造函数
     * @param parent 父窗口
     */
    explicit AppAssignmentList(QWidget *parent = nullptr);

    /**
     * @brief 按顺序返回列表中的软件 id
     * @return 软件 id 列表
     */
    QStringList softwareIds() const;

  signals:
    /**
     * @brief 从软件清单拖入了某个软件
     * @param softwareId 软件 id
     * @param insertRow  插入行，-1 表示追加到末尾
     */
    void softwareDropped(const QString &softwareId, int insertRow);

    /**
     * @brief 列表内部顺序发生变化
     */
    void orderChanged();

    /**
     * @brief 请求移除指定行的分配
     * @param row 行号
     */
    void removeRequested(int row);

  protected:
    /**
     * @brief 拖入时只接受软件清单 MIME
     * @param event 拖拽事件
     */
    void dragEnterEvent(QDragEnterEvent *event) override;

    /**
     * @brief 拖动过程中给出可放置提示
     * @param event 拖拽事件
     */
    void dragMoveEvent(QDragMoveEvent *event) override;

    /**
     * @brief 放置：外部软件走插入逻辑，内部拖动交给基类完成重排
     * @param event 放置事件
     */
    void dropEvent(QDropEvent *event) override;

    /**
     * @brief Delete 键移除当前分配
     * @param event 按键事件
     */
    void keyPressEvent(QKeyEvent *event) override;

  private slots:
    /**
     * @brief 弹出右键菜单
     * @param pos 鼠标位置
     */
    void onContextMenu(const QPoint &pos);
};

/**
 * @brief 单块屏幕的配置卡片
 *
 * 展示序号、屏幕名称、绑定的物理屏信息、已分配软件列表与标签栏偏移，
 * 并支持拖拽卡片头部调整屏幕序号。
 */
class ScreenCard : public QFrame
{
    Q_OBJECT

  public:
    /**
     * @brief 构造函数
     * @param row    在配置屏幕列表中的下标
     * @param parent 父窗口
     */
    explicit ScreenCard(int row, QWidget *parent = nullptr);

    /**
     * @brief 按配置填充卡片内容
     * @param row    下标
     * @param screen 屏幕配置
     * @param config 全量配置（用于解析软件名称）
     */
    void setScreen(int row, const ScreenConfig &screen, const MultiScreenConfig *config);

    /**
     * @brief 刷新序号徽标（屏幕顺序调整后调用）
     * @param row 新下标
     */
    void setRow(int row);

    /**
     * @brief 设置卡片选中态样式
     * @param selected 是否选中
     */
    void setSelected(bool selected);

    /**
     * @brief 获取当前分配列表中的软件 id 顺序
     * @return 软件 id 列表
     */
    QStringList assignmentIds() const;

  signals:
    /**
     * @brief 屏幕名称被修改
     * @param row  行号
     * @param name 新名称
     */
    void nameChanged(int row, const QString &name);

    /**
     * @brief 标签栏水平偏移被修改
     * @param row   行号
     * @param value 新偏移量
     */
    void tabOffsetChanged(int row, int value);

    /**
     * @brief 卡片被点击
     * @param row 行号
     */
    void selected(int row);

    /**
     * @brief 请求删除该屏
     * @param row 行号
     */
    void removeRequested(int row);

    /**
     * @brief 从软件清单拖入软件
     * @param row        屏幕行号
     * @param softwareId 软件 id
     * @param insertRow  插入行
     */
    void softwareDropped(int row, const QString &softwareId, int insertRow);

    /**
     * @brief 分配顺序变化
     * @param row 屏幕行号
     */
    void assignmentOrderChanged(int row);

    /**
     * @brief 请求移除某个分配
     * @param row      屏幕行号
     * @param listRow  分配列表行号
     */
    void assignmentRemoveRequested(int row, int listRow);

    /**
     * @brief 卡片被拖拽到另一张卡片上，请求交换/移动序号
     * @param fromRow 源卡片行号
     * @param toRow   目标卡片行号
     */
    void screenMoved(int fromRow, int toRow);

  protected:
    /**
     * @brief 点击卡片时通知画布选中
     * @param event 鼠标事件
     */
    void mousePressEvent(QMouseEvent *event) override;

    /**
     * @brief 接受屏幕卡片拖拽
     * @param event 拖拽事件
     */
    void dragEnterEvent(QDragEnterEvent *event) override;

    /**
     * @brief 处理屏幕卡片拖拽放置
     * @param event 放置事件
     */
    void dropEvent(QDropEvent *event) override;

  private:
    /** 当前行号 */
    int m_row = 0;
    /** 序号徽标 */
    QLabel *indexLabel = nullptr;
    /** 屏幕名称输入框 */
    QLineEdit *nameEdit = nullptr;
    /** 物理屏信息标签 */
    QLabel *displayLabel = nullptr;
    /** 拖拽手柄 */
    QLabel *dragHandle = nullptr;
    /** 软件分配列表 */
    AppAssignmentList *assignmentList = nullptr;
    /** 标签栏偏移 */
    QSpinBox *offsetSpin = nullptr;
    /** 删除按钮 */
    QPushButton *removeButton = nullptr;
};

/**
 * @brief 屏幕布局画布
 *
 * 组合物理屏示意图、屏序卡片区与添加/删除操作，负责维护配置中的屏幕顺序、
 * 绑定关系、屏幕名称与软件分配。
 */
class ScreenLayoutCanvas : public QWidget
{
    Q_OBJECT

  public:
    /**
     * @brief 构造函数
     * @param parent 父窗口
     */
    explicit ScreenLayoutCanvas(QWidget *parent = nullptr);

    /**
     * @brief 绑定配置对象（画布直接读写其中的 screenList）
     * @param config 全量配置
     */
    void setConfig(MultiScreenConfig *config);

    /**
     * @brief 重建全部卡片
     */
    void refresh();

    /**
     * @brief 当前选中的屏幕行号
     * @return 行号，未选中返回 -1
     */
    int selectedRow() const;

    /**
     * @brief 添加一块屏幕（供管理界面顶部按钮调用）
     */
    void addScreen();

  signals:
    /**
     * @brief 屏幕配置发生变化（名称、顺序、分配、偏移、增删）
     */
    void configChanged();

    /**
     * @brief 选中的屏幕变化
     * @param row 行号
     */
    void screenSelected(int row);

    /**
     * @brief 从软件清单拖入软件
     * @param screenRow  目标屏幕行号
     * @param softwareId 软件 id
     * @param insertRow  插入行
     */
    void softwareDropped(int screenRow, const QString &softwareId, int insertRow);

  private slots:
    /**
     * @brief 添加一块屏幕
     */
    void onAddScreen();

    /**
     * @brief 删除当前选中屏幕
     */
    void onRemoveScreen();

    /**
     * @brief 选中屏幕
     * @param row 行号
     */
    void onSelectScreen(int row);

    /**
     * @brief 交换/移动屏幕顺序
     * @param fromRow 源行号
     * @param toRow   目标行号
     */
    void onScreenMoved(int fromRow, int toRow);

    /**
     * @brief 更新屏幕名称
     * @param row  行号
     * @param name 名称
     */
    void onNameChanged(int row, const QString &name);

    /**
     * @brief 更新标签栏偏移
     * @param row   行号
     * @param value 偏移量
     */
    void onTabOffsetChanged(int row, int value);

  private:
    /**
     * @brief 重新编号：序号始终等于列表顺序
     */
    void renumberScreens();

    /**
     * @brief 找到尚未被任何屏幕配置占用的物理屏
     * @return 物理屏名称，找不到返回空字符串
     */
    QString findFreeScreenName() const;

    /**
     * @brief 按软件 id 查找软件名称
     * @param softwareId 软件 id
     * @return 名称，未找到返回 id 本身
     */
    QString softwareName(const QString &softwareId) const;

    MultiScreenConfig *m_config = nullptr;
    PhysicalScreenMap *screenMap = nullptr;
    QScrollArea *cardScroll = nullptr;
    QWidget *cardContainer = nullptr;
    QList<ScreenCard *> m_cards;
    int m_selectedRow = -1;
};

#endif  // SCREENLAYOUTCANVAS_H
