#include "SoftwareCapture.h"

#include "ProcessFactory.h"

#include <QCefView.h>
#include <QAxObject>
#include <QCursor>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QPixmap>
#include <QProcess>
#include <QProgressDialog>
#include <QScreen>
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
/** @brief 网页预览窗口尺寸：宽高足够渲染出完整首屏，最终会按预览图最大宽度等比缩小 */
const int WEB_PREVIEW_WIDTH = 1280;
const int WEB_PREVIEW_HEIGHT = 800;
/** @brief 网页加载的最长等待时间（毫秒） */
const int WEB_LOAD_TIMEOUT_MS = 15000;
/** @brief 网页加载结束后的渲染稳定等待时间（毫秒） */
const int WEB_RENDER_SETTLE_MS = 1200;
/** @brief 演示文稿第一页导出图片的宽度（像素），高度按页面比例计算 */
const int PPT_EXPORT_WIDTH = 1280;

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
 * @brief 从屏幕上截取指定矩形区域
 *
 * CEF 使用 GPU 合成时 PrintWindow 可能返回空白，此时回退到屏幕拷贝。
 *
 * @param globalRect 待截取区域的屏幕坐标
 * @return 截取结果，失败返回空图
 */
QImage grabScreenRegion(const QRect &globalRect)
{
    QScreen *screen = QGuiApplication::screenAt(globalRect.center());
    if (screen == nullptr)
    {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen == nullptr || globalRect.isEmpty())
    {
        return QImage();
    }
    const QPixmap screenPixmap = screen->grabWindow(0);
    if (screenPixmap.isNull())
    {
        return QImage();
    }
    // 高 DPI 下窗口逻辑坐标与屏幕像素坐标存在缩放关系，需要按 devicePixelRatio 换算
    const qreal ratio = screen->devicePixelRatio();
    const QRect screenGeometry = screen->geometry();
    QRect sourceRect(qRound((globalRect.x() - screenGeometry.x()) * ratio),
                     qRound((globalRect.y() - screenGeometry.y()) * ratio), qRound(globalRect.width() * ratio),
                     qRound(globalRect.height() * ratio));
    sourceRect = sourceRect.intersected(screenPixmap.rect());
    if (sourceRect.isEmpty())
    {
        return QImage();
    }
    return screenPixmap.copy(sourceRect).toImage();
}

/** @brief 获取当前正在运行的演示文稿宿主进程 id 集合（前置声明） */
QSet<quint64> presentationProcessIds(ProcessUtils *processUtils);

/** @brief 关闭本次打开的演示文稿并清理本次预览拉起的进程（前置声明） */
void closePresentationApp(QAxObject &powerpoint, QAxObject *presentations, ProcessUtils *processUtils,
                          const QSet<quint64> &processesBefore);

/**
 * @brief 通过 Office/WPS 的 COM 接口把演示文稿第一页导出为图片
 *
 * 相比“启动 PowerPoint 再抓窗口”，COM 导出得到的一定是第一页，
 * 不受编辑窗口当前停在哪一页、窗口是否可见、是否已进入放映的影响。
 *
 * @param filePath 演示文稿完整路径
 * @return 第一页画面，失败返回空图（由调用方回退到窗口抓取）
 */
QImage exportFirstSlideByCom(const QString &filePath)
{
    // 记录调用前已存在的演示进程，用于结束时判断哪些进程是本次预览新拉起的
    std::unique_ptr<ProcessUtils> processUtils(ProcessFactory::newInstance());
    const QSet<quint64> processesBefore = presentationProcessIds(processUtils.get());

    QAxObject powerpoint(QStringLiteral("PowerPoint.Application"));
    if (powerpoint.isNull())
    {
        return QImage();
    }
    // 不显示编辑窗口，避免导出过程中弹出整套 PowerPoint/WPS 界面
    powerpoint.setProperty("Visible", false);
    QAxObject *presentations = powerpoint.querySubObject("Presentations");
    if (presentations == nullptr)
    {
        closePresentationApp(powerpoint, nullptr, processUtils.get(), processesBefore);
        return QImage();
    }
    // Open(FileName, ReadOnly, Untitled, WithWindow)：只读打开且不创建编辑窗口
    QAxObject *presentation = presentations->querySubObject("Open(const QString&, int, int, int)",
                                                            QDir::toNativeSeparators(filePath), true, false, false);
    if (presentation == nullptr)
    {
        closePresentationApp(powerpoint, presentations, processUtils.get(), processesBefore);
        return QImage();
    }

    QImage image;
    QAxObject *slides = presentation->querySubObject("Slides");
    QAxObject *firstSlide = (slides != nullptr) ? slides->querySubObject("Item(int)", 1) : nullptr;
    if (firstSlide != nullptr)
    {
        // 按页面实际宽高比计算导出高度，保证第一页不被拉伸
        int exportHeight = PPT_EXPORT_WIDTH * 3 / 4;
        QAxObject *pageSetup = presentation->querySubObject("PageSetup");
        if (pageSetup != nullptr)
        {
            const double slideWidth = pageSetup->property("SlideWidth").toDouble();
            const double slideHeight = pageSetup->property("SlideHeight").toDouble();
            if (slideWidth > 0.0)
            {
                exportHeight = qRound(PPT_EXPORT_WIDTH * slideHeight / slideWidth);
            }
        }
        const QString tempFile = QDir(QDir::tempPath())
                                     .filePath(QStringLiteral("mdc_ppt_preview_%1.png")
                                                   .arg(QCoreApplication::applicationPid()));
        firstSlide->dynamicCall("Export(const QString&, const QString&, int, int)",
                                QDir::toNativeSeparators(tempFile), QStringLiteral("PNG"), PPT_EXPORT_WIDTH,
                                exportHeight);
        // Export 是同步调用，返回后即可读取；读不到说明当前 Office/WPS 不支持该导出接口
        image = QImage(tempFile);
        QFile::remove(tempFile);
    }

    // 关闭本次打开的文档且不保存，避免留下未关闭的演示文稿
    presentation->dynamicCall("Close()");
    closePresentationApp(powerpoint, presentations, processUtils.get(), processesBefore);
    return image;
}

/**
 * @brief 获取当前正在运行的演示文稿宿主进程 id 集合
 *
 * PowerPoint 与 WPS 演示都可能承载 PowerPoint.Application 这个 COM 接口，
 * 因此两者都要统计，用于区分“用户本来就在运行的进程”和“本次预览新拉起的进程”。
 *
 * @param processUtils 进程工具，可空
 * @return 进程 id 集合
 */
QSet<quint64> presentationProcessIds(ProcessUtils *processUtils)
{
    QSet<quint64> ids;
    if (processUtils == nullptr)
    {
        return ids;
    }
    const QStringList processNames = {QStringLiteral("POWERPNT.EXE"), QStringLiteral("wpp.exe")};
    for (int i = 0; i < processNames.size(); i++)
    {
        const QVector<ProcessInfo> processList = processUtils->getProcessInfo(processNames[i]);
        for (int j = 0; j < processList.size(); j++)
        {
            ids.insert(processList[j].pid);
        }
    }
    return ids;
}

/**
 * @brief 关闭本次打开的演示文稿，并清理本次预览新拉起的演示进程
 *
 * WPS/PowerPoint 的 Quit 调用不一定会真正退出进程，若不清理，反复生成演示文稿预览
 * 会残留不可见的后台进程。这里只结束“本次预览新拉起”的进程，绝不影响用户原有的文稿。
 *
 * @param powerpoint     PowerPoint.Application COM 对象
 * @param presentations  Presentations 集合对象
 * @param processUtils   进程工具，可空
 * @param processesBefore 导出前已存在的演示进程 id 集合
 */
void closePresentationApp(QAxObject &powerpoint, QAxObject *presentations, ProcessUtils *processUtils,
                          const QSet<quint64> &processesBefore)
{
    if (presentations != nullptr && processesBefore.isEmpty() && presentations->property("Count").toInt() == 0)
    {
        // 导出前用户没有打开任何文稿，才可以退出整个应用
        powerpoint.dynamicCall("Quit()");
    }
    if (processUtils == nullptr || !processesBefore.isEmpty())
    {
        // 用户原本就开着演示软件时不做任何强杀，避免影响到用户正在编辑的文稿
        return;
    }
    // 给 Quit 一点生效时间，仍残留的进程再强制结束
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < PROCESS_CLOSE_TIMEOUT_MS)
    {
        QThread::msleep(100);
        if (presentationProcessIds(processUtils).isEmpty())
        {
            return;
        }
    }
    const QSet<quint64> processIds = presentationProcessIds(processUtils);
    for (QSet<quint64>::const_iterator it = processIds.constBegin(); it != processIds.constEnd(); ++it)
    {
        processUtils->killProcess(*it);
    }
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

bool SoftwareCapture::capturePreview(const SoftwareItem &software, const QString &appDir, QWidget *parent,
                                     QString *detectedTitle)
{
#ifdef Q_OS_WIN
    if (detectedTitle != nullptr)
    {
        detectedTitle->clear();
    }
    if (software.id.isEmpty())
    {
        return false;
    }

    // 网页走内置 CEF 渲染抓取，演示文稿优先导出第一页，桌面应用走“启动进程 + 抓窗口”
    QImage image;
    if (software.type == QStringLiteral("web"))
    {
        image = captureWebPage(software, detectedTitle);
    }
    else if (software.type == QStringLiteral("ppt"))
    {
        image = capturePptFirstSlide(software, parent, detectedTitle);
        if (image.isNull())
        {
            // 当前 Office/WPS 不支持 COM 导出时，退回“打开文件后抓窗口”的方式
            image = captureLaunchedWindow(software, parent, detectedTitle);
        }
    }
    else
    {
        image = captureLaunchedWindow(software, parent, detectedTitle);
    }

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
    Q_UNUSED(detectedTitle)
    return false;
#endif
}

QImage SoftwareCapture::captureLaunchedWindow(const SoftwareItem &software, QWidget *parent, QString *detectedTitle)
{
#ifdef Q_OS_WIN
    QString program;
    QStringList arguments;
    QString workDir;
    if (!buildLaunchCommand(software, &program, &arguments, &workDir))
    {
        return QImage();
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
        return QImage();
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
        if (detectedTitle != nullptr && !image.isNull())
        {
            // 抓图成功后回传窗口标题，供管理界面自动填充“窗口标题”
            *detectedTitle = windowTitle(target);
        }
        if (createdByUs)
        {
            closeLaunchedProcess(target, static_cast<quint64>(launchedPid));
        }
    }
    progress.close();
    return image;
#else
    Q_UNUSED(software)
    Q_UNUSED(parent)
    Q_UNUSED(detectedTitle)
    return QImage();
#endif
}

QImage SoftwareCapture::captureWebPage(const SoftwareItem &software, QString *detectedTitle)
{
#ifdef Q_OS_WIN
    if (!software.path.startsWith(QStringLiteral("http://")) && !software.path.startsWith(QStringLiteral("https://")))
    {
        return QImage();
    }

    // 无边框置顶工具窗口承载真实 CEF 浏览器：当前 CEF 使用窗口模式（非离屏渲染），
    // 浏览器窗口必须真实可见才能完成首帧绘制，否则抓到的只是空白画面。
    QWidget holder(nullptr, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    holder.setAttribute(Qt::WA_ShowWithoutActivating);
    holder.setStyleSheet(QStringLiteral("background:#010A1B;"));
    holder.resize(WEB_PREVIEW_WIDTH, WEB_PREVIEW_HEIGHT);
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (screen == nullptr)
    {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen != nullptr)
    {
        const QRect available = screen->availableGeometry();
        holder.move(available.x() + qMax(0, (available.width() - WEB_PREVIEW_WIDTH) / 2),
                    available.y() + qMax(0, (available.height() - WEB_PREVIEW_HEIGHT) / 2));
    }

    bool loadFinished = false;
    QCefView *cefView = new QCefView(software.path, nullptr, &holder);
    cefView->setGeometry(holder.rect());
    // 主框架加载结束或失败都视为可以抓图，避免一直等到超时
    QObject::connect(cefView, &QCefView::loadEnd, cefView,
                     [&loadFinished](const QCefBrowserId &, const QCefFrameId &, bool isMainFrame, int) {
                         if (isMainFrame)
                         {
                             loadFinished = true;
                         }
                     });
    QObject::connect(cefView, &QCefView::loadError, cefView,
                     [&loadFinished](const QCefBrowserId &, const QCefFrameId &, bool isMainFrame, int, const QString &,
                                     const QString &) {
                         if (isMainFrame)
                         {
                             loadFinished = true;
                         }
                     });
    if (detectedTitle != nullptr)
    {
        QObject::connect(cefView, &QCefView::titleChanged, cefView,
                         [detectedTitle](const QString &title) { *detectedTitle = title; });
    }
    holder.show();

    QElapsedTimer timer;
    timer.start();
    while (!loadFinished && timer.elapsed() < WEB_LOAD_TIMEOUT_MS)
    {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        QThread::msleep(80);
    }
    // 加载结束后再等待渲染进程完成首屏绘制，避免截到半透明或未排版完的画面
    timer.restart();
    while (timer.elapsed() < WEB_RENDER_SETTLE_MS)
    {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        QThread::msleep(80);
    }

    // 优先按窗口句柄后台抓取；CEF 走 GPU 合成时该方式可能得到空白图，再回退到屏幕拷贝
    QImage image = grabWindowImage(reinterpret_cast<HWND>(cefView->winId()));
    if (image.isNull() || isBlankImage(image))
    {
        holder.raise();
        SetForegroundWindow(reinterpret_cast<HWND>(holder.winId()));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        QThread::msleep(300);
        image = grabScreenRegion(holder.frameGeometry());
    }
    holder.hide();
    return image;
#else
    Q_UNUSED(software)
    Q_UNUSED(detectedTitle)
    return QImage();
#endif
}

QImage SoftwareCapture::capturePptFirstSlide(const SoftwareItem &software, QWidget *parent, QString *detectedTitle)
{
#ifdef Q_OS_WIN
    const QString filePath = QDir::fromNativeSeparators(software.path);
    if (!QFileInfo::exists(filePath))
    {
        return QImage();
    }

    // 拉起 Office/WPS 并导出第一页可能持续数秒，用进度提示避免界面看起来卡死
    QProgressDialog progress(QStringLiteral("正在导出演示文稿第一页，请稍候…"), QString(), 0, 0, parent);
    progress.setWindowTitle(QStringLiteral("生成预览图"));
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setCancelButton(nullptr);
    progress.setMinimumDuration(0);
    progress.show();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    const QImage image = exportFirstSlideByCom(filePath);
    progress.close();
    if (!image.isNull() && detectedTitle != nullptr)
    {
        // PowerPoint/WPS 的窗口标题中包含“文档名”，用不带扩展名的文件名作为匹配前缀最稳定；
        // 自动填充后用户仍可在对话框中手工改写
        *detectedTitle = QFileInfo(filePath).completeBaseName();
    }
    return image;
#else
    Q_UNUSED(software)
    Q_UNUSED(parent)
    Q_UNUSED(detectedTitle)
    return QImage();
#endif
}
