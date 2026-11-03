#include "GlobalKeyWatcher.h"
GlobalKeyWatcher GlobalKeyWatcher::watcher;

GlobalKeyWatcher::GlobalKeyWatcher(QObject *parent) : QObject(parent)
{
}

GlobalKeyWatcher::~GlobalKeyWatcher()
{
}

void GlobalKeyWatcher::init()
{
    registerHotkey();
    qApp->installNativeEventFilter(this);
}
void GlobalKeyWatcher::registerHotkey()
{
    RegisterHotKey(nullptr, 10, MOD_ALT, 'R');
    RegisterHotKey(nullptr, 11, MOD_ALT, 'H');
    RegisterHotKey(nullptr, 20, MOD_ALT, 'Q');
    RegisterHotKey(nullptr, 21, MOD_ALT, VK_F4);
    RegisterHotKey(nullptr, 22, MOD_ALT, VK_F5);
    RegisterHotKey(nullptr, 31, MOD_ALT, VK_NUMPAD1);
    RegisterHotKey(nullptr, 32, MOD_ALT, VK_NUMPAD2);
    RegisterHotKey(nullptr, 33, MOD_ALT, VK_NUMPAD3);
    RegisterHotKey(nullptr, 34, MOD_ALT, VK_NUMPAD4);
    RegisterHotKey(nullptr, 35, MOD_ALT, VK_NUMPAD5);
    RegisterHotKey(nullptr, 36, MOD_ALT, VK_NUMPAD6);
    RegisterHotKey(nullptr, 37, MOD_ALT, VK_NUMPAD7);
    RegisterHotKey(nullptr, 41, MOD_ALT, '1');
    RegisterHotKey(nullptr, 42, MOD_ALT, '2');
    RegisterHotKey(nullptr, 43, MOD_ALT, '3');
    RegisterHotKey(nullptr, 44, MOD_ALT, '4');
    RegisterHotKey(nullptr, 45, MOD_ALT, '5');
    RegisterHotKey(nullptr, 46, MOD_ALT, '6');
    RegisterHotKey(nullptr, 47, MOD_ALT, '7');
}

bool GlobalKeyWatcher::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
{
    Q_UNUSED(result);
#ifdef Q_OS_WIN
    if (eventType != "windows_generic_MSG")
    {
        return false;
    }
    MSG *msg = static_cast<MSG *>(message);
    if (msg->message == WM_HOTKEY)
    {
        qDebug() << "hotkey pressed, hotkey id = " << msg->wParam;
        if (msg->wParam == 10)
        {
            // ALT+R
            qDebug() << "组合键Alt+R触发";
            emit altAndRPressed();
            return true;
        }
        else if (msg->wParam == 11)
        {
            // ALT+H：弹出快捷键说明界面
            qDebug() << "组合键Alt+H触发";
            emit helpKeyPressed();
            return true;
        }
        else if (msg->wParam == 20 || msg->wParam == 21)
        {
            // ALT+Q或F4
            QString tmpKey = (msg->wParam == 20 ? "Q" : "F4");
            qDebug() << "组合键Alt+" << tmpKey << "触发";
            emit quitKeyPressed("ALT+" + tmpKey);
            return true;
        }
        else if (msg->wParam == 22)
        {
            // ALT+F5：重新加载鼠标所在屏幕的全部应用，用于单个屏幕的故障恢复
            qDebug() << "组合键Alt+F5触发";
            emit altAndF5Pressed();
            return true;
        }
        else if (msg->wParam >= 31 && msg->wParam <= 47)
        {
            // ALT+数字（1-7）
            int keyNum = msg->wParam % 10;
            qDebug() << "组合键Alt+" << keyNum << "触发";
            emit switchScreenKeyPressed(keyNum, QString("Alt+%1").arg(keyNum));
            return true;
        }
    }
#else
    // 非 Windows 平台不处理原生事件，显式忽略入参以避免未使用参数告警
    Q_UNUSED(eventType);
    Q_UNUSED(message);
#endif
    // return 语句必须放在条件编译之外，保证所有平台都有明确返回值，避免非 Windows 平台出现未定义行为
    return false;
}
