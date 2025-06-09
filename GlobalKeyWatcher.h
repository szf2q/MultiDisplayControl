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
    HHOOK keyboardHook;
    int m_scrollLockCount;
    QDateTime m_lastScrollLockTime;
    GlobalKeyWatcher(QObject *parent = nullptr);
    ~GlobalKeyWatcher(void);
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;
    LRESULT CALLBACK keyProcessedProc(int nCode, WPARAM wParam, LPARAM lParam);
    bool processKeyPress(int key);

  signals:
    void switchScreenKeyPressed(const int &screenIndex, const QString &keyMessage);
    void quitKeyPressed(const QString &keyMessage);
    void altAndF5Pressed(void);
    void altAndRPressed(void);
};

#endif  // GLOBALKEYWATCHER_H
