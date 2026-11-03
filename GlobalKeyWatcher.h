#ifndef GLOBALKEYWATCHER_H
#define GLOBALKEYWATCHER_H
#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QKeyEvent>
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

  signals:
    void switchScreenKeyPressed(const int &screenIndex, const QString &keyMessage);
    void quitKeyPressed(const QString &keyMessage);
    /** @brief Alt+F5 被按下，用于重新加载鼠标所在屏幕的全部应用 */
    void altAndF5Pressed(void);
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
};

#endif  // GLOBALKEYWATCHER_H
