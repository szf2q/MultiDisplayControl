#ifndef SHORTCUTHELPDIALOG_H
#define SHORTCUTHELPDIALOG_H

#include <QWidget>

class QFrame;
class QGridLayout;
class QTimer;

/**
 * @brief 快捷键说明浮层（深蓝色大屏风格）
 *
 * @note 该浮层用于替代系统样式的 QMessageBox，原因如下：
 *       1. 大屏场景下需要与整体深色风格一致，系统消息框的浅色外观过于突兀；
 *       2. 需求要求“无边框、无关闭按钮、点击外部自动隐藏”，系统消息框无法满足。
 *
 * @note 窗口本身为无边框置顶窗口（不使用原生标题栏），因此不存在系统边框与关闭按钮；
 *       圆角与渐变背景由内部卡片绘制，四周留出边距用于承载外发光阴影。
 */
class ShortcutHelpDialog : public QWidget
{
    Q_OBJECT

  public:
    /**
     * @brief 构造函数，完成窗口标志设置与界面构建
     * @param parent 父对象，默认空（本类为顶级浮层，由调用方负责生命周期）
     */
    explicit ShortcutHelpDialog(QWidget *parent = nullptr);

    /**
     * @brief 析构函数，解除全局事件过滤器
     */
    ~ShortcutHelpDialog() override;

    /**
     * @brief 切换浮层显隐：已显示则隐藏，未显示则在鼠标所在屏幕居中弹出
     */
    void toggle();

  protected:
    /**
     * @brief 全局事件过滤器，用于实现“点击浮层外部区域自动隐藏”
     * @param watched 事件接收者
     * @param event 事件对象
     * @return 始终返回 false，不拦截任何事件，避免影响其他控件的正常交互
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

    /**
     * @brief 隐藏事件处理，停止外部点击检测定时器
     * @param event 隐藏事件
     */
    void hideEvent(QHideEvent *event) override;

    /**
     * @brief 键盘事件处理，支持 Esc 关闭浮层
     * @param event 键盘事件
     */
    void keyPressEvent(QKeyEvent *event) override;

  private:
    /**
     * @brief 构建界面：标题、分隔线、快捷键列表与底部提示
     */
    void buildUi();

    /**
     * @brief 向快捷键列表追加一行
     * @param grid 列表使用的网格布局
     * @param row 行号
     * @param keys 快捷键文本，可包含换行
     * @param desc 功能说明文本
     */
    void addShortcutRow(QGridLayout *grid, int row, const QString &keys, const QString &desc);

    /**
     * @brief 在鼠标所在屏幕的可用区域居中显示
     */
    void showAtCursorScreen();

    /**
     * @brief 定时检测全局鼠标左键是否落在浮层外部，用于实现“点击外部自动隐藏”
     * @note 点击可能落在本程序之外的窗口（如嵌入的外部程序、CEF 原生窗口），
     *       这类点击不会产生 Qt 鼠标事件，因此必须借助系统按键状态判断。
     */
    void checkGlobalClickOutside();

    /** 承载深蓝渐变背景与圆角的卡片容器，四周边距用于显示外发光阴影 */
    QFrame *card = nullptr;

    /** 外部点击检测定时器，仅在浮层显示期间运行 */
    QTimer *outsideClickTimer = nullptr;

    /** 上一次检测到的鼠标左键按下状态，用于只响应“按下”瞬间，避免长按重复触发 */
    bool leftButtonPressed = false;
};

#endif  // SHORTCUTHELPDIALOG_H
