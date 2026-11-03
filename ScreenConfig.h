#ifndef SCREENCONFIG_H
#define SCREENCONFIG_H

#include <QList>
#include <QRect>
#include <QString>

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
    /** 单块屏幕配置列表，顺序即屏幕序号 */
    QList<ScreenConfig> screenList;
    /** 全局软件清单 */
    QList<SoftwareItem> softwareList;
};

#endif  // SCREENCONFIG_H
