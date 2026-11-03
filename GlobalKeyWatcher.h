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
};

#endif  // GLOBALKEYWATCHER_H
