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
 * 按所有物理屏的真实几何等比缩放绘制矩形，并标注序号、名称、分辨率与主屏标记；
 * 点击某块物理屏可选中对应屏幕配置，选中态用青色边框标识，帮助用户快速定位；
 * 支持拖动物理屏调整示意图中的布局位置（仅影响管理界面显示，不改变真实物理屏位置与配置）。
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
     * @brief 绑定配置对象，用于读取每块物理屏对应的序号、主屏标记等
     * @param config 全量配置，可为 nullptr（仅显示物理屏，不显示序号）
     */
    void setConfig(MultiScreenConfig *config);

    /**
     * @brief 设置选中的物理屏名称（青色边框）
     * @param displayName QScreen::name()，空字符串表示无选中
     */
    void setSelectedScreen(const QString &displayName);

    /**
     * @brief 按当前连接的物理屏刷新示意图
     */
    void refresh();

  signals:
    /**
     * @brief 点击了某块物理屏
     * @param displayName 物理屏名称（QScreen::name()）
     */
    void screenClicked(const QString &displayName);

  protected:
    /**
     * @brief 自绘物理屏矩形
     * @param event 绘制事件
     */
    void paintEvent(QPaintEvent *event) override;

    /**
     * @brief 鼠标按下：命中物理屏矩形时记录拖拽起点
     * @param event 鼠标事件
     */
    void mousePressEvent(QMouseEvent *event) override;

    /**
     * @brief 鼠标移动：拖拽状态下更新屏幕位置
     * @param event 鼠标事件
     */
    void mouseMoveEvent(QMouseEvent *event) override;

    /**
     * @brief 鼠标释放：结束拖拽
     * @param event 鼠标事件
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

  private:
    /**
     * @brief 拖拽操作类型
     */
    enum DragMode
    {
        DragNone,    /**< 无拖拽 */
        DragMove,    /**< 拖动位置 */
        DragResize   /**< 调整大小 */
    };

    /**
     * @brief 根据鼠标位置计算命中的物理屏下标
     * @param pos 鼠标坐标（相对本控件）
     * @return 物理屏在 QGuiApplication::screens() 中的下标，未命中返回 -1
     */
    int hitTest(const QPoint &pos) const;

    /**
     * @brief 检测鼠标位置是否在调整大小手柄区域
     * @param pos    鼠标坐标
     * @param index  输出参数：命中的屏索引
     * @return true 表示命中调整手柄
     */
    bool hitResizeHandle(const QPoint &pos, int *index) const;

    /**
     * @brief 获取物理屏在示意图中的显示矩形（含自定义拖拽偏移和自定义大小）
     * @param screenIndex 物理屏下标
     * @param bounding    所有屏的外接矩形
     * @param scale       缩放比例
     * @param baseOffset  基础偏移（居中用）
     * @return 显示矩形
     */
    QRectF screenDisplayRect(int screenIndex, const QRectF &bounding, double scale,
                             const QPointF &baseOffset) const;

    /** 配置引用，用于读取屏幕序号与主屏标记，可为 nullptr */
    MultiScreenConfig *m_config = nullptr;
    /** 选中的物理屏名称（青色边框） */
    QString m_selected;
    /** 当前拖拽模式 */
    DragMode m_dragMode = DragNone;
    /** 正在拖拽的物理屏下标 */
    int m_dragIndex = -1;
    /** 拖拽起点相对屏幕矩形左上角的偏移（像素，显示坐标） */
    QPoint m_dragOffset;
    /** 调整大小时记录的起始尺寸 */
    QSize m_dragStartSize;
    /** 调整大小时记录的起始鼠标位置 */
    QPoint m_dragStartPos;
    /**
     * @brief 物理屏在示意图中的自定义偏移（像素，已缩放后的显示坐标）
     *
     * key 为物理屏名称（QScreen::name()），value 为相对默认位置的偏移量。
     * 仅用于管理界面显示，不影响真实物理屏位置与配置文件。
     */
    QHash<QString, QPoint> m_customOffsets;
    /**
     * @brief 物理屏在示意图中的自定义大小（像素，已缩放后的显示尺寸）
     *
     * key 为物理屏名称（QScreen::name()），value 为自定义宽高。
     * 为空时使用默认缩放尺寸。仅用于管理界面显示，不影响真实配置。
     */
    QHash<QString, QSize> m_customSizes;
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
     * @brief 屏幕序号被修改
     * @param row      行号
     * @param newIndex 新序号
     */
    void indexChanged(int row, int newIndex);

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
    /** 序号输入框（可手动设置屏幕序号） */
    QSpinBox *indexSpin = nullptr;
    /** 屏幕名称输入框 */
    QLineEdit *nameEdit = nullptr;
    /** 物理屏信息标签 */
    QLabel *displayLabel = nullptr;
    /** 软件分配列表 */
    AppAssignmentList *assignmentList = nullptr;
    /** 标签栏偏移 */
    QSpinBox *offsetSpin = nullptr;
};

/**
 * @brief 屏幕布局画布
 *
 * 组合物理屏示意图与屏幕卡片列表，负责维护配置中的屏幕序号、
 * 绑定关系、屏幕名称与软件分配。屏幕由物理屏自动获取，不可手动增删；
 * 屏幕序号可手动设置，卡片按序号排序显示。
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

  signals:
    /**
     * @brief 屏幕配置发生变化（序号、名称、分配、偏移）
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
     * @brief 选中屏幕
     * @param row 行号
     */
    void onSelectScreen(int row);

    /**
     * @brief 物理屏示意图上点击了某块物理屏，同步选中对应屏幕卡片
     * @param displayName 物理屏名称
     */
    void onScreenMapClicked(const QString &displayName);

    /**
     * @brief 更新屏幕序号
     * @param row      行号
     * @param newIndex 新序号
     */
    void onIndexChanged(int row, int newIndex);

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
     * @brief 同步物理屏与配置：为每块物理屏创建对应配置，移除不存在的屏
     *
     * 屏幕数量由当前连接的物理屏决定，用户不能手动增删。
     * 已存在的配置项按 displayName 匹配并保留；新增物理屏自动创建默认配置。
     */
    void syncPhysicalScreens();

    /**
     * @brief 按 screenIndex 从小到大对屏幕列表排序
     */
    void sortScreensByIndex();

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
