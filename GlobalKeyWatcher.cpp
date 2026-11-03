#include "GlobalKeyWatcher.h"

#include <atomic>

namespace
{
/**
 * @brief 是否屏蔽 Win 键
 *
 * 由主线程通过 setWinKeyDisabled() 写入，低级键盘钩子在安装它的线程（主线程）
 * 回调中读取，理论上不存在并发；这里仍使用原子量，避免极端情况下读到撕裂值。
 * 声明在平台条件编译之外，保证非 Windows 平台也能编译通过（只是不生效）。
 */
std::atomic<bool> winKeyBlocked(false);

/**
 * @brief 当前是否存在可切换的标签目标
 *
 * 由 setTabSwitchSource() 维护：只有至少一块屏存在多个标签时才为 true。
 * 为 false 时钩子放行 Alt+Tab，保证没有任何标签可切时系统窗口切换仍然可用。
 */
std::atomic<bool> altTabAvailable(false);

#ifdef Q_OS_WIN
/** @brief 低级键盘钩子句柄，全局唯一，随进程存活 */
HHOOK keyboardHook = nullptr;

/**
 * @brief 注册单个全局热键，并在失败时输出日志
 *
 * RegisterHotKey 可能因组合键被其它程序占用而失败，旧实现忽略了返回值，
 * 现场遇到“快捷键无响应”时无从排查。
 *
 * @param id        热键标识
 * @param modifiers 修饰键（MOD_ALT 等）
 * @param key       虚拟键码
 * @param name      可读名称，仅用于日志
 */
void registerOneHotkey(int id, UINT modifiers, UINT key, const char *name)
{
    if (RegisterHotKey(nullptr, id, modifiers, key) == FALSE)
    {
        qWarning() << "register hotkey failed, name = " << name << ", id = " << id << ", error = " << GetLastError();
    }
}

/**
 * @brief 判断虚拟键码是否为左右 Win 键
 * @param vkCode 虚拟键码
 * @return true 表示是 Win 键
 */
bool isWinKey(UINT vkCode)
{
    return vkCode == VK_LWIN || vkCode == VK_RWIN;
}

/**
 * @brief 低级键盘钩子回调：拦截系统 Alt+Tab / Alt+Shift+Tab，并按开关屏蔽 Win 键
 *
 * Alt+Tab 是 Windows 保留的组合键，RegisterHotKey 无法注册，
 * 因此改用 WH_KEYBOARD_LL 钩子在按键派发到系统之前截获：
 * 命中后转入主线程切换本程序标签，并返回 1 阻止系统弹出窗口切换界面。
 *
 * @param nCode  钩子状态码，小于 0 时必须直接透传
 * @param wParam 消息类型（WM_KEYDOWN / WM_SYSKEYDOWN 等）
 * @param lParam 指向 KBDLLHOOKSTRUCT 的指针
 * @return 1 表示已吞掉该按键，其他情况交给下一个钩子处理
 */
LRESULT CALLBACK lowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        const KBDLLHOOKSTRUCT *keyInfo = reinterpret_cast<const KBDLLHOOKSTRUCT *>(lParam);
        if (keyInfo != nullptr)
        {
            // 禁用 Win 键：按下与抬起一并吞掉。系统收不到 Win 键状态，
            // Win+D、Win+R 等组合键不会触发；松开时一并拦截，避免出现“只收到抬起”的异常状态
            if (winKeyBlocked.load() && isWinKey(keyInfo->vkCode))
            {
                return 1;
            }
            if ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && keyInfo->vkCode == VK_TAB &&
                (GetAsyncKeyState(VK_MENU) & 0x8000) != 0 && altTabAvailable.load())
            {
                const bool reverse = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                // 钩子回调处于系统消息路径，界面操作统一排队到主线程执行，避免重入与死锁
                QMetaObject::invokeMethod(&GlobalKeyWatcher::watcher, "handleAltTab", Qt::QueuedConnection,
                                          Q_ARG(bool, reverse));
                return 1;
            }
        }
    }
    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}
#endif
}  // namespace

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
#ifdef Q_OS_WIN
    // Alt+Tab 无法通过 RegisterHotKey 注册，改用低级键盘钩子全局截获；
    // 钩子随进程存活，进程退出时由系统自动回收，无需显式卸载
    if (keyboardHook == nullptr)
    {
        keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, lowLevelKeyboardProc, GetModuleHandleW(nullptr), 0);
        if (keyboardHook == nullptr)
        {
            qWarning() << "install low level keyboard hook failed, error = " << GetLastError();
        }
    }
#endif
    qApp->installNativeEventFilter(this);
}

void GlobalKeyWatcher::handleAltTab(bool reverse)
{
    qDebug() << "组合键Alt+Tab触发, reverse = " << reverse;
    emit altTabPressed(reverse);
}

void GlobalKeyWatcher::setWinKeyDisabled(bool disabled)
{
    if (winKeyBlocked.load() == disabled)
    {
        return;
    }
    winKeyBlocked.store(disabled);
    qDebug() << "禁用Win键状态变更, disabled = " << disabled;
}

bool GlobalKeyWatcher::isWinKeyDisabled() const
{
    return winKeyBlocked.load();
}

void GlobalKeyWatcher::setTabSwitchSource(const QObject *source, bool available)
{
    if (source == nullptr)
    {
        return;
    }
    if (available)
    {
        tabSwitchSources.insert(source);
    }
    else
    {
        tabSwitchSources.remove(source);
    }
    // 钩子回调只读取该原子量，此处写入后立即对后续按键生效
    altTabAvailable.store(!tabSwitchSources.isEmpty());
}

void GlobalKeyWatcher::registerHotkey()
{
    // 统一通过 registerOneHotkey 注册：任一热键注册失败只记录日志，不影响其它热键
    registerOneHotkey(10, MOD_ALT, 'R', "Alt+R");
    registerOneHotkey(11, MOD_ALT, 'H', "Alt+H");
    registerOneHotkey(12, MOD_ALT, 'M', "Alt+M");
    registerOneHotkey(20, MOD_ALT, 'Q', "Alt+Q");
    registerOneHotkey(21, MOD_ALT, VK_F4, "Alt+F4");
    registerOneHotkey(22, MOD_ALT, VK_F5, "Alt+F5");
    registerOneHotkey(31, MOD_ALT, VK_NUMPAD1, "Alt+Num1");
    registerOneHotkey(32, MOD_ALT, VK_NUMPAD2, "Alt+Num2");
    registerOneHotkey(33, MOD_ALT, VK_NUMPAD3, "Alt+Num3");
    registerOneHotkey(34, MOD_ALT, VK_NUMPAD4, "Alt+Num4");
    registerOneHotkey(35, MOD_ALT, VK_NUMPAD5, "Alt+Num5");
    registerOneHotkey(36, MOD_ALT, VK_NUMPAD6, "Alt+Num6");
    registerOneHotkey(37, MOD_ALT, VK_NUMPAD7, "Alt+Num7");
    registerOneHotkey(41, MOD_ALT, '1', "Alt+1");
    registerOneHotkey(42, MOD_ALT, '2', "Alt+2");
    registerOneHotkey(43, MOD_ALT, '3', "Alt+3");
    registerOneHotkey(44, MOD_ALT, '4', "Alt+4");
    registerOneHotkey(45, MOD_ALT, '5', "Alt+5");
    registerOneHotkey(46, MOD_ALT, '6', "Alt+6");
    registerOneHotkey(47, MOD_ALT, '7', "Alt+7");
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
        else if (msg->wParam == 12)
        {
            // ALT+M：显示/隐藏多屏控制管理界面
            qDebug() << "组合键Alt+M触发";
            emit manageKeyPressed();
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
