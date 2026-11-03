#ifndef GLOBALKEYWATCHER_H
#define GLOBALKEYWATCHER_H
#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QKeyEvent>
#include <QSet>
#ifdef Q_OS_WIN
#    include <windows.h>
#endif

class GlobalKeyWatcher : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
  public:
    static GlobalKeyWatcher watcher;
    GlobalKeyWatcher(QObject *parent = nullptr);
    ~GlobalKeyWatcher(void);
    void init();
    void registerHotkey();
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;

    /**
     * @brief 处理低级键盘钩子转发过来的 Alt+Tab
     *
     * 钩子回调运行在系统消息处理路径上，不能在其中直接操作界面，
     * 因此通过队列调用转到主线程后再发出 altTabPressed 信号。
     *
     * @param reverse true 表示同时按下了 Shift（向前切换），false 表示向后切换
     */
    Q_INVOKABLE void handleAltTab(bool reverse);

    /**
     * @brief 设置是否禁用 Windows 徽标键（Win 键）
     *
     * 由配置项 global.disableWinKey 驱动：开启后低级键盘钩子会吞掉 Win 键的
     * 按下与抬起事件，系统因此收不到 Win 键状态，Win+D、Win+R 等组合键随之失效。
     * 该接口可在运行时反复调用，保存配置后立即生效，无需重启程序。
     *
     * @param disabled true 表示屏蔽 Win 键，false 表示恢复正常
     */
    void setWinKeyDisabled(bool disabled);

    /**
     * @brief 查询当前是否处于禁用 Win 键状态
     * @return true 表示 Win 键已被屏蔽
     */
    bool isWinKeyDisabled() const;

    /**
     * @brief 登记某个屏幕窗口当前是否具备 Alt+Tab 切换目标
     *
     * 低级键盘钩子会无条件吞掉 Alt+Tab，若当前没有任何可切换的标签，
     * 系统自带的窗口切换也会一起失效。这里由各屏幕窗口在标签数量变化时登记，
     * 只有至少一个窗口存在多个标签时钩子才接管该组合键，否则放行给系统。
     *
     * @param source    调用方（屏幕窗口），用作登记表的键
     * @param available true 表示该窗口当前可以切换标签
     * @note 必须在主线程调用；窗口销毁前应传入 false 注销
     */
    void setTabSwitchSource(const QObject *source, bool available);

  signals:
    void switchScreenKeyPressed(const int &screenIndex, const QString &keyMessage);
    void quitKeyPressed(const QString &keyMessage);
    /** @brief Alt+F5 被按下，用于重新加载鼠标所在屏幕的全部应用 */
    void altAndF5Pressed(void);
    /** @brief Alt+F12 被按下，用于自动重启 MultiDisplayControl */
    void restartKeyPressed(void);
    void altAndRPressed(void);
    /** @brief Alt+H 被按下，用于弹出快捷键说明界面 */
    void helpKeyPressed(void);
    /** @brief Alt+M 被按下，用于显示/隐藏多屏控制管理界面 */
    void manageKeyPressed(void);
    /**
     * @brief Alt+Tab 被按下，用于切换鼠标所在屏幕的标签页
     * @param reverse true 表示 Alt+Shift+Tab（向前切换）
     */
    void altTabPressed(bool reverse);

  private:
    /** @brief 当前具备 Alt+Tab 切换目标的屏幕窗口集合（仅主线程访问） */
    QSet<const QObject *> tabSwitchSources;
};

#endif  // GLOBALKEYWATCHER_H
