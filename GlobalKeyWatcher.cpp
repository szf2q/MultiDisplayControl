#include "GlobalKeyWatcher.h"

GlobalKeyWatcher::GlobalKeyWatcher(QObject *parent) : QObject(parent)
{
    qApp->installNativeEventFilter(this);
    //    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, keyProcessedProc, GetModuleHandle(NULL), 0);
    m_scrollLockCount = 0;
}
GlobalKeyWatcher::~GlobalKeyWatcher()
{
    //    UnhookWindowsHookEx(GlobalKeyWatcher::keyboardHook);
}

// LRESULT CALLBACK GlobalKeyWatcher::keyProcessedProc(int nCode, WPARAM wParam, LPARAM lParam)
//{
//    if (nCode >= 0)
//    {
//        KBDLLHOOKSTRUCT *pKeyInfo = (KBDLLHOOKSTRUCT *)lParam;
//        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
//        {
//            if (!processKeyPress(pKeyInfo->vkCode))
//            {
//                return -1;
//            }
//        }
//    }
//    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
//}

bool GlobalKeyWatcher::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
{
    Q_UNUSED(result);
#ifdef Q_OS_WIN
    if (eventType != "windows_generic_MSG")
    {
        return false;
    }
    MSG *msg = static_cast<MSG *>(message);
    if (msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN)
    {
        int key = msg->wParam;
        return processKeyPress(key);
    }
    return false;
#endif
}

bool GlobalKeyWatcher::processKeyPress(int key)
{
    bool isScrollLock = (key == VK_SCROLL);
    bool isNumKey = ((key >= '0' && key <= '9') || (key >= VK_NUMPAD0 && key <= VK_NUMPAD9));
    bool isAltPressed = GetKeyState(VK_MENU) & 0x8000;
    bool isCtrlPressed = GetKeyState(VK_CONTROL) & 0x8000;
    bool isF4 = (key == VK_F4);
    //        bool isF5 = (key == VK_F5);
    bool isR = (key == 'r' || key == 'R');
    bool isQ = (key == 'q' || key == 'Q');
    int keyNum;
    if (isNumKey)
    {
        keyNum = static_cast<char>(key);
        if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9)
        {
            keyNum = key - VK_NUMPAD0;
        }
    }
    if (isScrollLock)
    {
        m_scrollLockCount++;
        m_lastScrollLockTime = QDateTime::currentDateTime();
        return false;
    }
    else if (!isNumKey)
    {
        m_scrollLockCount = 0;
    }
    if (m_scrollLockCount >= 2 && m_lastScrollLockTime.msecsTo(QDateTime::currentDateTime()) < 2000 && isNumKey)
    {
        qDebug() << "组合键ScrollLock+ScrollLock+" << keyNum << "触发";
        m_scrollLockCount = 0;
        emit switchScreenKeyPressed(keyNum, QString("ScrollLock+ScrollLock+%1").arg(keyNum));
        return true;
    }
    if (isAltPressed && isNumKey)
    {
        qDebug() << "组合键Alt+" << keyNum << "触发";
        emit switchScreenKeyPressed(keyNum, QString("Alt+%1").arg(keyNum));
        return true;
    }
    if (isCtrlPressed && isNumKey)
    {
        qDebug() << "组合键Ctrl+" << keyNum << "触发";
        emit switchScreenKeyPressed(keyNum, QString("Ctrl+%1").arg(keyNum));
        return true;
    }
    if (isAltPressed && isF4)
    {
        qDebug() << "组合键ALT+F4触发";
        emit quitKeyPressed("ALT+F4");
        return true;
    }
    if (isAltPressed && isQ)
    {
        qDebug() << "组合键ALT+Q触发";
        emit quitKeyPressed("ALT+Q");
        return true;
    }
    if (isAltPressed && isR)
    {
        qDebug() << "组合键ALT+R触发";
        emit altAndRPressed();
        return true;
    }
    //        if (isAltPressed && isF5)
    //        {
    //            qDebug() << "组合键ALT+F5触发";
    //            emit altAndF5Pressed();
    //            return true;
    //        }
    return false;
}
