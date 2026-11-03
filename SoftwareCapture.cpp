#include "SoftwareCapture.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QProgressDialog>
#include <QSet>
#include <QStandardPaths>
#include <QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace
{
#ifdef Q_OS_WIN
/** @brief PrintWindow 的 PW_RENDERFULLCONTENT 标志，Windows 8.1+ 生效，Win7 上传入会失败并自动回退 */
const UINT PW_RENDER_FULL_CONTENT = 0x00000002;
/** @brief 等待目标主窗口出现的最长时间（毫秒） */
const int WINDOW_WAIT_TIMEOUT_MS = 20000;
/** @brief 轮询顶层窗口的间隔（毫秒） */
const int WINDOW_POLL_INTERVAL_MS = 120;
/** @brief 找到窗口后等待其完成首屏绘制的时长（毫秒） */
const int WINDOW_SETTLE_MS = 700;
/** @brief 请求关闭本次启动进程后的等待时长（毫秒） */
const int PROCESS_CLOSE_TIMEOUT_MS = 1500;
/** @brief 预览图最大宽度，超出时等比缩小以控制体积 */
const int PREVIEW_MAX_WIDTH = 480;
/** @brief 判定为有效主窗口的最小宽高，用于过滤提示条、输入法等小窗口 */
const int MIN_WINDOW_WIDTH = 200;
const int MIN_WINDOW_HEIGHT = 120;

/**
 * @brief EnumWindows 回调上下文：收集顶层窗口句柄
 */
struct WindowEnumContext
{
    QList<HWND> windows;
};

/**
 * @brief EnumWindows 回调实现，逐个收集顶层窗口
 * @param hwnd   当前窗口句柄
 * @param lParam 指向 WindowEnumContext 的指针
 * @return 始终返回 TRUE 以继续枚举
 */
BOOL CALLBACK collectWindowProc(HWND hwnd, LPARAM lParam)
{
    WindowEnumContext *context = reinterpret_cast<WindowEnumContext *>(lParam);
    if (context != nullptr)
    {
        context->windows.append(hwnd);
    }
    return TRUE;
}

/**
 * @brief 枚举当前所有顶层窗口
 * @return 顶层窗口句柄列表
 */
QList<HWND> enumTopLevelWindows()
{
    WindowEnumContext context;
    EnumWindows(collectWindowProc, reinterpret_cast<LPARAM>(&context));
    return context.windows;
}

/**
 * @brief 收集当前所有顶层窗口句柄，用于识别“启动后新出现的窗口”
 * @return 窗口句柄集合
 */
QSet<qintptr> currentWindowHandles()
{
    QSet<qintptr> handles;
    const QList<HWND> windows = enumTopLevelWindows();
    for (int i = 0; i < windows.size(); i++)
    {
        handles.insert(reinterpret_cast<qintptr>(windows[i]));
    }
    return handles;
}

/**
 * @brief 获取窗口所属进程 id
 * @param hwnd 窗口句柄
 * @return 进程 id，失败返回 0
 */
quint64 windowProcessId(HWND hwnd)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return static_cast<quint64>(pid);
}

/**
 * @brief 获取窗口标题
 * @param hwnd 窗口句柄
 * @return 窗口标题文本
 */
QString windowTitle(HWND hwnd)
{
    wchar_t buffer[512] = {0};
    GetWindowTextW(hwnd, buffer, 512);
    return QString::fromWCharArray(buffer);
}

/**
 * @brief 判断窗口是否可能是需要抓图的“主界面”
 *
 * 过滤条件：可见、无属主（排除对话框/浮动提示）、尺寸不小于阈值、不属于当前进程。
 *
 * @param hwnd 窗口句柄
 * @return true 表示可作为抓取目标
 */
bool isMainWindowCandidate(HWND hwnd)
{
    if (hwnd == nullptr || IsWindowVisible(hwnd) == FALSE || GetWindow(hwnd, GW_OWNER) != nullptr)
    {
        return false;
    }
    if (windowProcessId(hwnd) == static_cast<quint64>(GetCurrentProcessId()))
    {
        return false;
    }
    RECT rect;
    if (GetWindowRect(hwnd, &rect) == FALSE)
    {
        return false;
    }
    return (rect.right - rect.left) >= MIN_WINDOW_WIDTH && (rect.bottom - rect.top) >= MIN_WINDOW_HEIGHT;
}

/**
 * @brief 在启动后新出现的窗口中查找目标窗口
 *
 * 优先匹配启动进程 id，其次匹配窗口标题（兼容启动器拉起子进程的程序）。
 *
 * @param pid        启动进程 id
 * @param existing   启动前已存在的窗口句柄集合
 * @param titleHint  窗口标题关键字
 * @return 命中返回窗口句柄，否则返回 nullptr
 */
HWND findNewWindow(quint64 pid, const QSet<qintptr> &existing, const QString &titleHint)
{
    HWND titleMatch = nullptr;
    const QList<HWND> windows = enumTopLevelWindows();
    for (int i = 0; i < windows.size(); i++)
    {
        HWND hwnd = windows[i];
        if (existing.contains(reinterpret_cast<qintptr>(hwnd)) || !isMainWindowCandidate(hwnd))
        {
            continue;
        }
        if (pid != 0 && windowProcessId(hwnd) == pid)
        {
            return hwnd;
        }
        if (titleMatch == nullptr && !titleHint.isEmpty() &&
            windowTitle(hwnd).contains(titleHint, Qt::CaseInsensitive))
        {
            titleMatch = hwnd;
        }
    }
    return titleMatch;
}

/**
 * @brief 按标题查找已存在的窗口（用于单实例程序已被拉起的场景）
 * @param titleHint 窗口标题关键字
 * @return 命中返回窗口句柄，否则返回 nullptr
 */
HWND findExistingWindowByTitle(const QString &titleHint)
{
    if (titleHint.isEmpty())
    {
        return nullptr;
    }
    const QList<HWND> windows = enumTopLevelWindows();
    for (int i = 0; i < windows.size(); i++)
    {
        HWND hwnd = windows[i];
        if (!isMainWindowCandidate(hwnd))
        {
            continue;
        }
        if (windowTitle(hwnd).contains(titleHint, Qt::CaseInsensitive))
        {
            return hwnd;
        }
    }
    return nullptr;
}

/**
 * @brief 等待目标窗口出现，并在等待期间保持界面响应
 * @param pid       启动进程 id
 * @param existing  启动前已存在的窗口句柄集合
 * @param titleHint 窗口标题关键字
 * @return 命中返回窗口句柄，超时返回 nullptr
 */
HWND waitForNewWindow(quint64 pid, const QSet<qintptr> &existing, const QString &titleHint)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < WINDOW_WAIT_TIMEOUT_MS)
    {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        HWND hwnd = findNewWindow(pid, existing, titleHint);
        if (hwnd != nullptr)
        {
            return hwnd;
        }
        QThread::msleep(WINDOW_POLL_INTERVAL_MS);
    }
    return nullptr;
}

/**
 * @brief 将窗口置前并等待其完成绘制
 * @param hwnd 窗口句柄
 */
void bringWindowToFront(HWND hwnd)
{
    if (IsIconic(hwnd) != FALSE)
    {
        ShowWindow(hwnd, SW_RESTORE);
    }
    else
    {
        ShowWindow(hwnd, SW_SHOW);
    }
    SetForegroundWindow(hwnd);
    QThread::msleep(WINDOW_SETTLE_MS);
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

/**
 * @brief 把内存位图转换为 QImage
 * @param deviceContext 已选中位图的内存设备上下文
 * @param bitmap        位图句柄
 * @param width         位图宽度
 * @param height        位图高度
 * @return 位图数据对应的 QImage，失败返回空图
 */
QImage bitmapToImage(HDC deviceContext, HBITMAP bitmap, int width, int height)
{
    BITMAPINFO info;
    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    // 负数高度表示自上而下的像素顺序，与 QImage 内存布局一致
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    QImage image(width, height, QImage::Format_RGB32);
    if (GetDIBits(deviceContext, bitmap, 0, static_cast<UINT>(height), image.bits(), &info, DIB_RGB_COLORS) == 0)
    {
        return QImage();
    }
    return image;
}

/**
 * @brief 判断图像是否为空白（全黑或纯色）
 *
 * PrintWindow 对部分程序会返回全黑图像，需要识别后回退到其它抓取方式。
 *
 * @param image 待判断图像
 * @return true 表示图像没有有效内容
 */
bool isBlankImage(const QImage &image)
{
    if (image.isNull())
    {
        return true;
    }
    const int stepX = qMax(1, image.width() / 32);
    const int stepY = qMax(1, image.height() / 32);
    int minGray = 255;
    int maxGray = 0;
    for (int y = 0; y < image.height(); y += stepY)
    {
        for (int x = 0; x < image.width(); x += stepX)
        {
            const int gray = qGray(image.pixel(x, y));
            minGray = qMin(minGray, gray);
            maxGray = qMax(maxGray, gray);
        }
    }
    return maxGray < 8 || maxGray == minGray;
}

/**
 * @brief 抓取指定窗口内容
 *
 * 依次尝试：带 PW_RENDERFULLCONTENT 的 PrintWindow → 普通 PrintWindow → 屏幕拷贝，
 * 以兼顾 Windows 8.1+ 高清抓取与 Windows 7 兼容性。
 *
 * @param hwnd 窗口句柄
 * @return 抓取结果，失败返回空图
 */
QImage grabWindowImage(HWND hwnd)
{
    RECT rect;
    if (GetWindowRect(hwnd, &rect) == FALSE)
    {
        return QImage();
    }
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0)
    {
        return QImage();
    }

    HDC windowDc = GetWindowDC(hwnd);
    if (windowDc == nullptr)
    {
        return QImage();
    }
    HDC memoryDc = CreateCompatibleDC(windowDc);
    HBITMAP bitmap = CreateCompatibleBitmap(windowDc, width, height);
    HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);

    QImage image;
    if (PrintWindow(hwnd, memoryDc, PW_RENDER_FULL_CONTENT) != FALSE)
    {
        image = bitmapToImage(memoryDc, bitmap, width, height);
    }
    if (image.isNull() || isBlankImage(image))
    {
        PrintWindow(hwnd, memoryDc, 0);
        image = bitmapToImage(memoryDc, bitmap, width, height);
    }
    if (image.isNull() || isBlankImage(image))
    {
        BitBlt(memoryDc, 0, 0, width, height, windowDc, 0, 0, SRCCOPY);
        image = bitmapToImage(memoryDc, bitmap, width, height);
    }

    SelectObject(memoryDc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memoryDc);
    ReleaseDC(hwnd, windowDc);
    return image;
}

/**
 * @brief 强制结束指定进程（若仍在运行）
 * @param pid 进程 id
 */
void terminateProcess(quint64 pid)
{
    if (pid == 0 || pid == static_cast<quint64>(GetCurrentProcessId()))
    {
        return;
    }
    HANDLE handle = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (handle == nullptr)
    {
        return;
    }
    if (WaitForSingleObject(handle, 0) == WAIT_TIMEOUT)
    {
        TerminateProcess(handle, 0);
    }
    CloseHandle(handle);
}

/**
 * @brief 关闭为抓图而启动的进程
 * @param hwnd        目标窗口句柄
 * @param launchedPid 启动进程 id
 */
void closeLaunchedProcess(HWND hwnd, quint64 launchedPid)
{
    const quint64 windowPid = windowProcessId(hwnd);
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < PROCESS_CLOSE_TIMEOUT_MS)
    {
        if (IsWindow(hwnd) == FALSE)
        {
            break;
        }
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        QThread::msleep(80);
    }
    terminateProcess(windowPid);
    terminateProcess(launchedPid);
}
#endif  // Q_OS_WIN
}  // namespace

QString SoftwareCapture::previewDirectory(const QString &appDir)
{
    return QDir(appDir).filePath(QStringLiteral("previews"));
}

QString SoftwareCapture::relativePreviewPath(const QString &softwareId)
{
    if (softwareId.isEmpty())
    {
        return QString();
    }
    return QStringLiteral("previews/%1.png").arg(softwareId);
}

QString SoftwareCapture::absolutePreviewPath(const QString &appDir, const QString &softwareId)
{
    if (softwareId.isEmpty())
    {
        return QString();
    }
    return QDir(previewDirectory(appDir)).filePath(softwareId + QStringLiteral(".png"));
}

QString SoftwareCapture::resolveExistingPreview(const QString &appDir, const SoftwareItem &software)
{
    QStringList candidates;
    if (!software.previewPath.isEmpty())
    {
        candidates << (QDir::isAbsolutePath(software.previewPath)
                           ? software.previewPath
                           : QDir(appDir).filePath(software.previewPath));
    }
    // 配置中丢失 previewPath 时，仍按约定路径尝试，避免历史预览图不可见
    candidates << absolutePreviewPath(appDir, software.id);
    for (int i = 0; i < candidates.size(); i++)
    {
        if (!candidates[i].isEmpty() && QFileInfo(candidates[i]).isFile())
        {
            return candidates[i];
        }
    }
    return QString();
}

void SoftwareCapture::removePreview(const QString &appDir, const QString &softwareId)
{
    const QString filePath = absolutePreviewPath(appDir, softwareId);
    if (!filePath.isEmpty() && QFile::exists(filePath))
    {
        QFile::remove(filePath);
    }
}

QStringList SoftwareCapture::splitArguments(const QString &args)
{
    QStringList result;
    QString current;
    bool inQuotes = false;
    for (int i = 0; i < args.size(); i++)
    {
        const QChar ch = args.at(i);
        if (ch == QLatin1Char('"'))
        {
            inQuotes = !inQuotes;
            continue;
        }
        if (!inQuotes && ch.isSpace())
        {
            if (!current.isEmpty())
            {
                result.append(current);
                current.clear();
            }
            continue;
        }
        current.append(ch);
    }
    if (!current.isEmpty())
    {
        result.append(current);
    }
    return result;
}

bool SoftwareCapture::buildLaunchCommand(const SoftwareItem &software, QString *program, QStringList *arguments,
                                         QString *workDir)
{
    if (program == nullptr || arguments == nullptr || workDir == nullptr || software.path.isEmpty())
    {
        return false;
    }
    arguments->clear();
    const QString normalizedPath = QDir::fromNativeSeparators(software.path);

    if (software.type == QStringLiteral("web"))
    {
        // 网页需要 CEF 渲染进程配合抓取，当前版本不生成预览图
        return false;
    }
    if (software.type == QStringLiteral("ppt"))
    {
        const QString powerPoint = QStandardPaths::findExecutable(QStringLiteral("POWERPNT.EXE"));
        if (powerPoint.isEmpty() || !QFileInfo::exists(normalizedPath))
        {
            return false;
        }
        *program = powerPoint;
        *arguments << normalizedPath;
        *workDir = QFileInfo(normalizedPath).absolutePath();
        return true;
    }

    if (!QFileInfo::exists(normalizedPath))
    {
        return false;
    }
    *program = normalizedPath;
    *arguments = splitArguments(software.startupArgs);
    *workDir = QFileInfo(normalizedPath).absolutePath();
    return true;
}

bool SoftwareCapture::capturePreview(const SoftwareItem &software, const QString &appDir, QWidget *parent)
{
#ifdef Q_OS_WIN
    if (software.id.isEmpty())
    {
        return false;
    }
    QString program;
    QStringList arguments;
    QString workDir;
    if (!buildLaunchCommand(software, &program, &arguments, &workDir))
    {
        return false;
    }

    // 抓图可能持续数秒，用无取消按钮的进度提示避免界面看起来卡死
    QProgressDialog progress(QStringLiteral("正在启动软件并截取预览图，请稍候…"), QString(), 0, 0, parent);
    progress.setWindowTitle(QStringLiteral("生成预览图"));
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setCancelButton(nullptr);
    progress.setMinimumDuration(0);
    progress.show();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    const QSet<qintptr> existingWindows = currentWindowHandles();
    qint64 launchedPid = 0;
    if (!QProcess::startDetached(program, arguments, workDir, &launchedPid))
    {
        progress.close();
        return false;
    }

    HWND target = waitForNewWindow(static_cast<quint64>(launchedPid), existingWindows, software.windowTitle);
    const bool createdByUs = target != nullptr && !existingWindows.contains(reinterpret_cast<qintptr>(target));
    if (target == nullptr)
    {
        // 单实例程序被已有进程接管时不会产生新窗口，退而抓取标题匹配的既有窗口（且不关闭它）
        target = findExistingWindowByTitle(software.windowTitle);
    }

    QImage image;
    if (target != nullptr)
    {
        // 抓图前隐藏进度窗，避免屏幕拷贝路径把提示窗一起截入
        progress.hide();
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        bringWindowToFront(target);
        image = grabWindowImage(target);
        if (createdByUs)
        {
            closeLaunchedProcess(target, static_cast<quint64>(launchedPid));
        }
    }
    progress.close();

    if (image.isNull())
    {
        return false;
    }
    if (image.width() > PREVIEW_MAX_WIDTH)
    {
        image = image.scaledToWidth(PREVIEW_MAX_WIDTH, Qt::SmoothTransformation);
    }
    if (!QDir().mkpath(previewDirectory(appDir)))
    {
        return false;
    }
    return image.save(absolutePreviewPath(appDir, software.id), "PNG");
#else
    // 非 Windows 平台没有通用窗口抓取手段，直接回退类型图标
    Q_UNUSED(software)
    Q_UNUSED(appDir)
    Q_UNUSED(parent)
    return false;
#endif
}
