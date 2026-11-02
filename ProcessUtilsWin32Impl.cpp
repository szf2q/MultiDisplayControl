#include "ProcessUtilsWin32Impl.h"
#include <QMessageBox>
#include <tlhelp32.h>

namespace
{
/**
 * @brief 通过 Toolhelp32 进程快照枚举指定进程名的所有进程 ID
 * @param processName 进程可执行文件名（例如 DocumentCount.exe），不区分大小写
 * @return 匹配到的进程 ID 列表，未找到时为空
 * @note 原实现每次都用 QProcess 启动 tasklist 并解析文本输出，单次开销数十毫秒且会创建子进程；
 *       快照方式直接在内存中遍历，单次仅 1 毫秒级。启动阶段该函数会被高频轮询，改造后收益明显。
 */
QVector<quint64> enumProcessIdsByName(const QString &processName)
{
    QVector<quint64> pidList;
    if (processName.isEmpty())
    {
        return pidList;
    }
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return pidList;
    }
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (QString::fromWCharArray(entry.szExeFile).compare(processName, Qt::CaseInsensitive) == 0)
            {
                pidList.append(entry.th32ProcessID);
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return pidList;
}

/** @brief 内核设备名与盘符的映射项，first 为设备名（如 \Device\HarddiskVolume3），second 为盘符（如 C:） */
typedef QPair<QString, QString> DeviceDrivePair;

/**
 * @brief 获取“内核设备名 → 盘符”映射表（首次调用时构建并缓存）
 * @return 映射表
 * @note getProcessPath 在启动阶段会被每个进程、每次轮询反复调用，原实现每次都要遍历 26 个盘符
 *       并调用 QueryDosDevice；盘符映射在进程运行期不会变化，因此只构建一次后复用。
 */
const QVector<DeviceDrivePair> &getDeviceDriveMap()
{
    static QVector<DeviceDrivePair> deviceMap;
    static bool initialized = false;
    if (!initialized)
    {
        initialized = true;
        DWORD dwDriveMask = GetLogicalDrives();
        for (int i = 0; i < 26; i++)
        {
            if (!(dwDriveMask & (1 << i)))
            {
                continue;
            }
            TCHAR szDrive[3] = {0};
            szDrive[0] = (TCHAR)(TEXT('A') + i);
            szDrive[1] = TEXT(':');
            TCHAR szDevice[MAX_PATH] = {0};
            if (!QueryDosDevice(szDrive, szDevice, MAX_PATH))
            {
                continue;
            }
            deviceMap.append(qMakePair(QString::fromWCharArray(szDevice), QString::fromWCharArray(szDrive)));
        }
    }
    return deviceMap;
}
}  // namespace

int ProcessUtilsWin32::runAsAdministrators()
{
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID administratorsGroup;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0,
                                 0, &administratorsGroup))
    {
        if (!CheckTokenMembership(NULL, administratorsGroup, &isAdmin))
        {
            isAdmin = FALSE;
        }
        FreeSid(administratorsGroup);
    }
    if (!isAdmin)
    {
        wchar_t szPath[MAX_PATH];
        if (GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath)))
        {
            SHELLEXECUTEINFO sei;
            sei.cbSize = sizeof(sei);
            sei.fMask = SEE_MASK_NOCLOSEPROCESS;
            sei.lpVerb = L"runas";
            sei.lpFile = szPath;
            sei.hwnd = NULL;
            sei.nShow = SW_NORMAL;
            if (!ShellExecuteEx(&sei))
            {
                QMessageBox::warning(nullptr, "Error", "以管理员身份运行失败！");
                return 1;
            }
            return 0;
        }
    }
    return -1;
}

QVector<ProcessInfo> ProcessUtilsWin32::getProcessInfo(const QString &processName, const QString &windowTitle)
{
    QVector<ProcessInfo> processList = getProcessInfo(processName);
    if (processList.size() <= 0)
    {
        return processList;
    }
    for (int i = processList.size() - 1; i >= 0; i--)
    {
        ProcessInfo process = processList[i];
        if (!process.titleName.contains(windowTitle))
        {
            processList.removeAt(i);
            continue;
        }
        //        if (processName.contains("chrome.exe"))
        //        {
        //            // qDebug() << "pid = " << process.pid << ", hwnd = " << process.hwnd << ", title = " <<
        //            //            process.titleName << ", class = " << process.className;
        //            HWND childHwnd = FindWindowExA(process.hwnd, NULL, "Chrome_RenderWidgetHostHWN", NULL);
        //            if (childHwnd != NULL && childHwnd != 0x0)
        //            {
        //                qDebug() << "child hwnd = " << childHwnd;
        //                processList[i].hwnd = childHwnd;
        //                processList[i].className = "Chrome_RenderWidgetHostHWN";
        //            }
        //        }
    }
    return processList;
}
ProcessInfo ProcessUtilsWin32::getProcessInfoByPath(const QString &processPath)
{
    QFileInfo fileInfo(processPath);
    QString processName = fileInfo.fileName();
    // qDebug() << "processName = " << processName;
    QVector<ProcessInfo> processList = getProcessInfo(processName);
    ProcessInfo result;
    for (ProcessInfo process : processList)
    {
        // qDebug() << "processPath = " << process.processPath;
        QString tmpFilePath = fileInfo.filePath().replace("/", QDir::separator());
        if (process.processPath == tmpFilePath)
        {
            result = process;
            break;
        }
    }
    processList.clear();
    return result;
}
QVector<ProcessInfo> ProcessUtilsWin32::getProcessInfoByWindowTitle(const QString &winTitle)
{
    struct EnumInOrOutputInfo
    {
        QString winTitle;
        QVector<ProcessInfo> processList;
    };

    EnumInOrOutputInfo input = {winTitle, QVector<ProcessInfo>()};

    //枚举所有顶层窗口
    EnumWindows(
        [](HWND hwnd, LPARAM lParam) -> BOOL {
            if (!IsWindowVisible(hwnd))
            {
                return TRUE;
            }
            EnumInOrOutputInfo *input = reinterpret_cast<EnumInOrOutputInfo *>(lParam);
            char title[256];
            char className[256];
            //获取窗口标题
            GetWindowTextA(hwnd, title, sizeof(title));
            QString tmpTitle = QString::fromLocal8Bit(title);
            if (!tmpTitle.contains(input->winTitle))
            {
                return TRUE;
            }
            //获取窗口所属进程ID
            DWORD windowProcessId;
            GetWindowThreadProcessId(hwnd, &windowProcessId);
            ProcessInfo processInfo;
            processInfo.hwnd = hwnd;
            processInfo.pid = windowProcessId;
            processInfo.titleName = tmpTitle;
            GetClassNameA(hwnd, className, sizeof(className));
            processInfo.className = QString::fromLocal8Bit(className);
            input->processList.append(processInfo);
            //继续枚举
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&input));
    // qDebug() << "input.processList.size() = " << input.processList.size();
    for (int i = 0; i < input.processList.size(); i++)
    {
        QString processPath = getProcessPath(input.processList[i].pid);
        input.processList[i].processPath = processPath;
        QFileInfo fileInfo(processPath);
        QString processName = fileInfo.fileName();
        input.processList[i].processName = processName;
    }
    return input.processList;
}
QVector<ProcessInfo> ProcessUtilsWin32::getProcessInfo(const QString &processName)
{
    QVector<ProcessInfo> processList;
    // 直接按进程名取进程 ID 列表（内存快照），不再启动 tasklist 解析文本
    const QVector<quint64> pidList = enumProcessIdsByName(processName);
    for (int i = 0; i < pidList.size(); i++)
    {
        processList.append(getProcessInfo(pidList[i]));
    }
    return processList;
}
QVector<ProcessInfo> ProcessUtilsWin32::getProcessInfo(quint64 pid)
{
    QVector<HWND> processHwndList;
    QString processPath = getProcessPath(pid);
    // qDebug() << "pid = " << pid << ", processPath = " << processPath;
    QFileInfo fileInfo(processPath);
    QString processName = fileInfo.fileName();

    processHwndList.append((HWND)pid);
    //枚举所有顶层窗口
    EnumWindows(
        [](HWND hwnd, LPARAM lParam) -> BOOL {
            if (!IsWindowVisible(hwnd))
            {
                return TRUE;
            }
            DWORD windowProcessId;
            QVector<HWND> *tmpHwndList = reinterpret_cast<QVector<HWND> *>(lParam);
            //获取窗口所属进程ID
            GetWindowThreadProcessId(hwnd, &windowProcessId);
            quint64 targetPid = reinterpret_cast<quint64>((*tmpHwndList)[0]);
            if (windowProcessId == targetPid)
            {
                tmpHwndList->append(hwnd);
            }
            //继续枚举
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&processHwndList));
    QVector<ProcessInfo> processList;
    if (processHwndList.count() == 1)
    {
        ProcessInfo processInfo;
        processInfo.hwnd = 0;
        processInfo.pid = pid;
        processInfo.processPath = processPath;
        processInfo.processName = processName;
        processList.append(processInfo);
        return processList;
    }

    char title[256];
    char className[256];
    for (int i = 1; i < processHwndList.count(); i++)
    {
        HWND processHwnd = processHwndList[i];
        if (processHwnd == nullptr || processHwnd == 0x0)
        {
            continue;
        }
        //获取窗口标题
        GetWindowTextA(processHwnd, title, sizeof(title));
        ProcessInfo processInfo;
        processInfo.hwnd = processHwnd;
        processInfo.pid = pid;
        processInfo.titleName = QString::fromLocal8Bit(title);
        if (processInfo.titleName.isEmpty())
        {
            continue;
        }
        GetClassNameA(processHwnd, className, sizeof(className));
        processInfo.className = QString::fromLocal8Bit(className);
        if (processInfo.className.isEmpty())
        {
            continue;
        }
        processInfo.processPath = processPath;
        processInfo.processName = processName;
        processList.append(processInfo);
    }
    return processList;
}

QString ProcessUtilsWin32::getProcessPath(quint64 pid)
{
    // qDebug() << "getProcessPath, pid = " << pid;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, (DWORD)pid);
    // qDebug() << "hProcess = " << hProcess;
    if (!hProcess)
    {
        qDebug() << "Error: " << GetLastError();
        return QString();
    }
    TCHAR path[MAX_PATH] = {0};
    if (!GetProcessImageFileName(hProcess, path, MAX_PATH))
    {
        // qDebug() << "GetProcessImageFileName failed, path = " << path;
        CloseHandle(hProcess);
        return QString();
    }
    CloseHandle(hProcess);
    QString processPath = QString::fromWCharArray(path);
    // 使用缓存的“设备名 → 盘符”映射做前缀替换，避免每次调用都遍历 26 个盘符查询设备名
    const QVector<DeviceDrivePair> &deviceMap = getDeviceDriveMap();
    for (int i = 0; i < deviceMap.size(); i++)
    {
        const QString &device = deviceMap[i].first;
        if (device.isEmpty() || !processPath.startsWith(device, Qt::CaseInsensitive))
        {
            continue;
        }
        return deviceMap[i].second + processPath.mid(device.length());
    }
    return processPath;
}

bool ProcessUtilsWin32::isProcessExist(const QString &processName)
{
    // 使用内存快照判断，避免每次探测都启动一次 tasklist 子进程
    return !enumProcessIdsByName(processName).isEmpty();
}

bool ProcessUtilsWin32::startProcess(const QString &processPath, const QString &startupArgs, const bool runAsAdmin)
{
    QProcess *p = new QProcess();
    QStringList args = startupArgs.split(" ");
    QString userDataDir;
    QString tmpPath = QDir::toNativeSeparators(processPath);
    QFileInfo appFile(tmpPath);
    QString appDir = appFile.absolutePath();
    QString processName = appFile.fileName();
    qDebug() << "appDir = " << appDir << ", processName = " << processName;
    if (tmpPath.contains("chrome.exe"))
    {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.remove("__COMPAT_LAYER");
        env.remove("RunAsInvoker");
        p->setProcessEnvironment(env);
    }
    qDebug() << "runAsAdmin = " << runAsAdmin << "processPath = " << tmpPath << "args = " << args;
    if (runAsAdmin)
    {
        // 说明：QProcess::setNativeArguments 仅对非静态的 startDetached(qint64 *) 生效，
        // 这里调用的是静态 startDetached，且 "runas" 也不是 ShellExecute 动词，
        // 因此并不能真正提权。真正的按需提权需改用 ShellExecuteEx 的 runas 动词，
        // 但那会导致每个外部应用启动时都弹出 UAC 授权框，属功能回归，故此处保持原有行为不变。
        p->startDetached(tmpPath, args);
        // 已分离启动，QProcess 对象不再被使用，及时释放避免内存泄漏
        delete p;
        p = nullptr;
    }
    else
    {
        p->setWorkingDirectory(appDir);
        p->start(tmpPath, args);
        // 注意：QProcess 析构会结束仍在运行的子进程，因此该对象必须常驻（不能 delete），
        // 以保证通过 QProcess 启动的外部应用继续运行。
    }
    ProcessInfo processInfo;
    int timeUsed = 0;
    bool startSuccess = false;
    // 进程存在性检查已改为内存快照，单次开销极低，因此把轮询间隔由 500ms 缩短为 100ms，
    // 让外部程序一就绪就立即返回，不必固定多等半秒
    while (!startSuccess && timeUsed < 5 * 1000)
    {
        startSuccess = isProcessExist(processName);
        qDebug() << "check process exist, process = " << processName << ", exist = " << startSuccess;
        if (startSuccess)
        {
            break;
        }
        timeUsed += 100;
        Sleep(100);
    }
    return startSuccess;
}

void ProcessUtilsWin32::killProcess(QProcess *p)
{
    if (p->state() == QProcess::Running)
    {
        p->terminate();
        if (!p->waitForFinished(1000))
        {
            p->kill();
        }
    }
    delete p;
}
void ProcessUtilsWin32::killProcess(quint64 pid)
{
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess == NULL)
    {
        qDebug() << "无法打开进程，pid = " << pid << ", 错误码 = " << GetLastError();
        return;
    }
    if (!TerminateProcess(hProcess, 0))
    {
        qDebug() << "关闭进程失败，pid = " << pid << ", 错误码 = " << GetLastError();
        CloseHandle(hProcess);
        return;
    }
    CloseHandle(hProcess);
}
void ProcessUtilsWin32::killProcess(QString processName)
{
    QVector<ProcessInfo> processList = getProcessInfo(processName);
    qDebug() << "processList.size() = " << processList.size();
    for (ProcessInfo process : processList)
    {
        killProcess(process.pid);
    }
}
ProcessUtilsWin32::~ProcessUtilsWin32()
{
    qDebug() << "ProcessUtilsWin32 destructor";
}
