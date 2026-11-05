#ifndef SCREENCONFIG_H
#define SCREENCONFIG_H

#include <QFileInfo>
#include <QGuiApplication>
#include <QList>
#include <QRect>
#include <QScreen>
#include <QString>

#include <cmath>

/** @brief 软件清单拖拽使用的 MIME 类型，内容为软件 id */
#define MDC_SOFTWARE_MIME "application/x-mdc-software-id"
/** @brief 屏幕卡片拖拽排序使用的 MIME 类型，内容为卡片当前行号 */
#define MDC_SCREEN_MIME "application/x-mdc-screen-index"

/**
 * @brief 全局软件清单条目
 *
 * 一条记录对应一个可展示的内容源，可被多块屏幕引用：
 *  - web：网页，路径为 http(s):// 地址，可在管理界面内实时预览；
 *  - desktop：桌面应用，路径为本地可执行文件，运行时按窗口标题抓取窗口并嵌入；
 *  - ppt：演示文稿，路径为 .ppt/.pptx 文件，运行时通过 COM 放映后嵌入。
 */
struct SoftwareItem
{
    /** 全局唯一标识，格式 sw-<8位十六进制> */
    QString id;
    /** 显示名称，用于软件清单与默认标签名 */
    QString name;
    /** 类型：web / desktop / ppt */
    QString type;
    /** 网页地址或本地文件路径 */
    QString path;
    /** 桌面应用匹配窗口标题；网页时可留空 */
    QString windowTitle;
    /** 桌面应用启动参数 */
    QString startupArgs;
    /** 备注说明，仅用于管理界面展示 */
    QString remark;
    /**
     * @brief 预览图相对路径，例如 previews/sw-xxxxxxxx.png
     *
     * 相对可执行文件所在目录保存，便于整体拷贝部署；
     * 为空表示尚未生成预览图，管理界面回退显示类型图标。
     */
    QString previewPath;
};

/**
 * @brief 屏幕上的一个软件分配记录
 *
 * assignments 的先后顺序即该屏幕标签页的显示顺序。
 */
struct ScreenAssignment
{
    /** 引用的软件清单 id */
    QString softwareId;
    /** 标签页名称覆盖值，为空时使用 SoftwareItem::name */
    QString tabName;
};

/**
 * @brief 运行时单个应用配置
 *
 * 由 ConfigStore 根据 softwareList + assignments 解析生成，供 ExternalAppWindow 使用。
 */
struct AppConfig
{
    /** 软件清单 id，运行时用于稳定匹配标签与进程，避免按标题文本匹配 */
    QString id;
    QString appPath;
    QString processName;
    QString windowTitle;
    QString tabName;
    QString startupArgs;
    /** 标签页索引，-1 表示尚未分配到标签页 */
    int tabIndex = -1;
};

/**
 * @brief 单块屏幕的配置
 */
struct ScreenConfig
{
    /** 屏幕配置唯一标识，格式 scr-<8位十六进制> */
    QString id;
    /** 绑定的物理屏名称（QScreen::name()），为空时按几何或序号回退匹配 */
    QString displayName;
    /** 保存时的物理屏几何，作为名称变化后的回退匹配依据 */
    QRect geometry;
    /** 保存时该物理屏是否为主屏 */
    bool primaryScreen = false;
    int screenIndex = 0;
    bool sortScreen = false;
    bool showIndex = true;
    int appCount = 0;
    QString screenTitle;
    bool debugMode = false;
    int tabOffsetX = 0;
    /** 是否去掉被嵌入桌面应用的标题栏与边框，使客户区铺满整块屏幕 */
    bool stripAppBorder = false;
    /** 软件分配列表，顺序即标签顺序 */
    QList<ScreenAssignment> assignments;
    /** 运行时应用列表，由 assignments 解析生成 */
    QList<AppConfig> appList;
};

/**
 * @brief 全量多屏配置
 */
struct MultiScreenConfig
{
    int count = 0;
    bool showIndex = true;
    bool sortScreen = true;
    bool debugMode = false;
    bool runAsAdministrator = false;
    /**
     * @brief 是否禁用 Windows 徽标键（Win 键）
     *
     * 开启后由 GlobalKeyWatcher 的低级键盘钩子吞掉 Win 键的按下/抬起事件，
     * Win+D、Win+R 等系统组合键随之失效，避免值班人员误触导致全屏展示被打断。
     * 该开关属于全局设置，保存后立即生效，无需重启。
     */
    bool disableWinKey = false;
    /**
     * @brief 是否去掉被嵌入桌面应用的标题栏与边框
     *
     * 开启后嵌入普通桌面应用时会清除 WS_CAPTION / WS_BORDER 并重新校正窗口尺寸，
     * 让应用客户区完整铺满整块屏幕；关闭时保持应用自身标题栏与边框（默认行为）。
     * 该开关只在应用嵌入时生效，修改后需重启主程序或重新加载应用。
     */
    bool stripAppBorder = false;
    /**
     * @brief 是否禁用 CEF 的 GPU 加速
     *
     * 默认 false（保留 GPU 加速）。多屏、4K 分辨率下关闭 GPU 会让网页退化为软件渲染，
     * 多个网页同时展示时会出现明显卡顿；仅在个别机型/驱动出现黑屏等渲染异常时开启。
     * 该开关只在 CEF 初始化时生效，修改后需重启主程序。
     */
    bool disableGpuAcceleration = false;
    /** 单块屏幕配置列表，顺序即屏幕序号 */
    QList<ScreenConfig> screenList;
    /** 全局软件清单 */
    QList<SoftwareItem> softwareList;
};

/**
 * @brief 判断应用路径是否为网页地址
 *
 * 运行时与界面层此前各自重复书写 startsWith("http://") 判断，规则容易随修改漂移，
 * 这里统一到一处：仅 http/https 视为网页，比较时忽略大小写（HTTP:// 也合法）。
 *
 * @param path 应用路径
 * @return true 表示网页地址
 */
inline bool isWebAppPath(const QString &path)
{
    return path.startsWith("http://", Qt::CaseInsensitive) || path.startsWith("https://", Qt::CaseInsensitive);
}

/**
 * @brief 判断应用路径是否为演示文稿
 *
 * 只按扩展名判断，避免旧实现 contains(".ppt") 把目录名里含 ppt 的普通程序误判为演示文稿。
 *
 * @param path 应用路径
 * @return true 表示 ppt/pptx/pps/ppsx 演示文稿
 */
inline bool isPptAppPath(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == "ppt" || suffix == "pptx" || suffix == "pps" || suffix == "ppsx";
}

/**
 * @brief 物理屏排序比较器：主屏优先，其余先按列（每 100 像素为一列）后按行
 *
 * 旧实现按“|Δx| < 100 时比较 y，否则比较 x”成对比较，不满足严格弱序，
 * 对三块及以上屏幕可能让 std::sort 读到越界元素而崩溃。这里改为对每块屏计算
 * 固定的排序键（列号、行号、名称），任意两块屏之间的比较结果都保持一致。
 *
 * @param a 屏幕 a
 * @param b 屏幕 b
 * @return true 表示 a 应排在 b 之前
 */
inline bool isScreenOrderedBefore(QScreen *a, QScreen *b)
{
    if (a == b)
    {
        return false;
    }
    const bool isPrimaryA = (a == QGuiApplication::primaryScreen());
    const bool isPrimaryB = (b == QGuiApplication::primaryScreen());
    if (isPrimaryA != isPrimaryB)
    {
        return isPrimaryA;
    }
    // 用 floor 而不是整数除法，保证负坐标（主屏左侧的屏幕）也能落在同一列分桶规则上
    const int columnA = (int)std::floor(a->geometry().x() / 100.0);
    const int columnB = (int)std::floor(b->geometry().x() / 100.0);
    if (columnA != columnB)
    {
        return columnA < columnB;
    }
    if (a->geometry().y() != b->geometry().y())
    {
        return a->geometry().y() < b->geometry().y();
    }
    // 名称作为最终兜底键，保证坐标完全相同的屏幕也有确定的先后顺序
    return a->name() < b->name();
}

#endif  // SCREENCONFIG_H
