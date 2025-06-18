#include "ProcessUtilsWin32Impl.h"
#include <QMessageBox>

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
    QVector<ProcessInfo> processList = QVector<ProcessInfo>();
    QProcess process;
    process.start("tasklist /FI \"IMAGENAME eq " + processName + "\"");
    process.waitForFinished();
    QString output = process.readAllStandardOutput();
    QStringList lines = output.split('\n');
    for (const QString &line : lines)
    {
        if (!line.contains(processName, Qt::CaseInsensitive))
        {
            continue;
        }
        QStringList parts = line.split(" ", Qt::SkipEmptyParts);
        if (parts.size() < 2)
        {
            continue;
        }
        bool ok;
        quint64 pid = parts[1].toULongLong(&ok);
        if (ok)
        {
            // qDebug() << "pid = " << pid;
            QVector<ProcessInfo> tmpList = getProcessInfo(pid);
            processList.append(tmpList);
        }
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
    QString processPath;
    TCHAR szDrive[3] = TEXT(" :");
    TCHAR szDevice[MAX_PATH];
    TCHAR szDrivePath[MAX_PATH];
    DWORD dwDriveMask = GetLogicalDrives();
    for (int i = 0; i < 26; i++)
    {
        if (!(dwDriveMask & (1 << i)))
        {
            continue;
        }
        szDrive[0] = (TCHAR)(TEXT('A') + i);
        if (!QueryDosDevice(szDrive, szDevice, MAX_PATH))
        {
            continue;
        }
        size_t len = _tcslen(szDevice);
        if (_tcsnicmp(path, szDevice, len) == 0)
        {
            _stprintf_s(szDrivePath, MAX_PATH, TEXT("%s%s"), szDrive, path + len);
            return QString::fromWCharArray(szDrivePath);
        }
    }
    return QString::fromWCharArray(path);
}

bool ProcessUtilsWin32::isProcessExist(const QString &processName)
{
    QVector<ProcessInfo> processList = QVector<ProcessInfo>();
    QProcess process;
    process.start("tasklist /FI \"IMAGENAME eq " + processName + "\"");
    process.waitForFinished();
    QString output = process.readAllStandardOutput();
    QStringList lines = output.split('\n');
    for (const QString &line : lines)
    {
        if (line.contains(processName, Qt::CaseInsensitive))
        {
            return true;
        }
    }
    return false;
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
        p->setNativeArguments("runas");
        p->startDetached(tmpPath, args);
    }
    else
    {
        p->setWorkingDirectory(appDir);
        p->start(tmpPath, args);
    }
    ProcessInfo processInfo;
    int timeUsed = 0;
    bool startSuccess = false;
    while (!startSuccess && timeUsed < 5 * 1000)
    {
        startSuccess = isProcessExist(processName);
        timeUsed += 500;
        Sleep(500);
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
