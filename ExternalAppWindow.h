#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QAxObject>
#include <QAxWidget>
#include <QDesktopWidget>
#include <QDockWidget>
#include <QFileInfo>
#include <QHash>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QProcess>
#include <QScreen>
#include <QSet>
#include <QStringList>
#include <QTabBar>
#include <QTabWidget>
#include <QWindow>
#include <iostream>
#include <memory>
#include <vector>
#include "CefWebView.h"
#include "FullSizeTabWidget.h"
#include "ProcessFactory.h"
#include "ProcessInfo.h"
#include "ProcessUtils.h"
#include "ScreenConfig.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class ExternalAppWindow;
}  // namespace Ui
QT_END_NAMESPACE

class QTimer;

class ExternalAppWindow : public QMainWindow
{
    Q_OBJECT

  public:
    ExternalAppWindow(ScreenConfig config, QWidget *parent = nullptr);
    ~ExternalAppWindow();
    bool isPrimaryScreen = false;
    ScreenConfig config;

    void setScreen(int screenIndex);
    void setScreen(QScreen *screen, bool force = false);
    void setToDefaultScreen(bool force = false);
    QScreen *getDefaultScreen();
    QScreen *getCurrentScreen();
    void loadApps(bool isReload = false);
    void readLoadApps();
    /**
     * @brief 重新加载当前选中标签页中的应用（故障恢复用）
     *
     * 仅重建当前标签页：外部程序先结束旧进程再重新启动并嵌入，网页重建视图并重新加载，
     * PPT 放映由 COM 实例承载、重载会与现有放映冲突，故保持不变。
     * 其它标签页的内容与窗口句柄均不受影响，因此某个应用崩溃时只需恢复它自己，
     * 既不需要重启整个控制程序，也不会打断同屏其它应用。
     */
    void reloadCurrentTab();
    /**
     * @brief 按步长切换当前标签页（循环切换）
     *
     * 供 Alt+Tab（step=1）/ Alt+Shift+Tab（step=-1）等快捷键调用：
     * 同时更新内容区当前页与悬浮标签栏选中项，保证两者始终一致。
     *
     * @param step 步长，正值向后切换，负值向前切换
     */
    void switchToNextTab(int step = 1);
    /**
     * @brief 预启动本屏所有外部程序（不等待窗口出现，幂等）
     *
     * loadApps() 内部会按配置顺序逐个“启动进程 → 等待窗口出现 → 嵌入标签页”，
     * 其中等待窗口出现是主要耗时点。先调用本方法把所有外部程序并行拉起，
     * 后续嵌入时窗口通常已就绪，从而把多个程序的启动耗时重叠起来。
     */
    void prestartApps();

    // ==================== 热更新接口 ====================
    /**
     * @brief 应用新的屏幕配置（标题、序号、显示序号开关、标签栏偏移、绑定的物理屏）
     * @param cfg          新的屏幕配置
     * @param targetScreen 目标物理屏，为空时保持当前绑定
     * @note 本方法只更新窗口自身属性与标签标题，不处理应用增删与顺序，
     *       应用差异由 DisplayRuntime 通过 addApp/removeApp/reorderTabs 完成。
     */
    void applyScreenConfig(const ScreenConfig &cfg, QScreen *targetScreen);

    /**
     * @brief 按标签当前顺序返回应用 id 列表
     * @return 软件 id 列表，未知标签会被跳过
     */
    QStringList appIdsInTabOrder() const;

    /**
     * @brief 判断指定软件是否已存在于本屏
     * @param appId 软件 id
     * @return true 表示已存在
     */
    bool hasApp(const QString &appId) const;

    /**
     * @brief 在指定位置新增一个应用标签
     * @param app         应用配置
     * @param insertIndex 插入位置，小于 0 或超出范围时追加到末尾
     */
    void addApp(const AppConfig &app, int insertIndex = -1);

    /**
     * @brief 移除指定应用：结束进程/销毁视图并移除标签
     * @param appId 软件 id
     */
    void removeApp(const QString &appId);

    /**
     * @brief 按给定顺序重排标签，不重启任何应用
     * @param orderedAppIds 期望的标签顺序（软件 id）
     */
    void reorderTabs(const QStringList &orderedAppIds);

    /**
     * @brief 更新指定应用的标签名称
     * @param appId   软件 id
     * @param tabName 新标签名称
     */
    void updateAppTabName(const QString &appId, const QString &tabName);

    /**
     * @brief 刷新全部标签标题（显示序号开关或屏幕序号变化时使用）
     */
    void refreshTabTitles();

    /**
     * @brief 设置屏幕高亮状态（管理界面选中时在大屏四周显示红色边框）
     *
     * 红色边框用于在多屏场景下标识当前正在管理界面编辑的是哪块屏幕，
     * 通过重写 paintEvent 直接在窗口客户区四周绘制，不影响内部嵌入的外部程序。
     *
     * @param highlighted true 显示红色边框，false 恢复无边框
     */
    void setHighlighted(bool highlighted);

  protected:
    /**
     * @brief 自绘：高亮态下绘制四周红色边框
     * @param event 绘制事件
     */
    void paintEvent(QPaintEvent *event) override;

    /**
     * @brief 事件过滤器：同步被嵌入的外部程序窗口尺寸
     *
     * 外部程序的窗口由纯 Win32 方式嵌入到标签页内的原生宿主控件中，
     * Qt 并不知道该外部窗口的存在，因此需要在宿主控件尺寸变化（含重新显示）时，
     * 手动把外部窗口拉伸到宿主客户区大小。
     *
     * @param watched 事件来源对象（此处为宿主控件）
     * @param event   事件对象
     * @return true 表示事件已被处理并终止派发，false 表示继续按默认流程派发
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    /**
     * @brief PPT 放映会话
     *
     * 记录 COM 控件、演示文稿与循环守护定时器，供热更新移除 PPT 时结束放映。
     */
    struct PptSession
    {
        QAxWidget *control = nullptr;
        QAxObject *presentation = nullptr;
        QTimer *timer = nullptr;
        /** aboutToQuit 连接句柄，结束会话前必须断开，避免退出时访问已释放对象 */
        QMetaObject::Connection quitConnection;
    };

    /**
     * @brief 启动外部窗口尺寸校正定时器（幂等，重复调用只会创建一次）
     *
     * 外部程序窗口本身带可缩放边框，用户仍可用鼠标把窗口拖小、露出宿主背景。
     * 由于跨进程清除 WS_THICKFRAME 会导致目标程序崩溃，这里改为周期检查：
     * 一旦发现外部窗口尺寸与宿主控件不一致，就立即把它校正回铺满状态。
     */
    void startNativeHostFitTimer();

    /**
     * @brief 释放本屏已登记的全部应用资源
     *
     * 依次结束 PPT 放映会话、外部程序进程与标签页，并清空宿主控件映射、
     * 软件 id 映射与预启动标记。供“重新加载”与析构共用：
     * 旧实现重新加载时只清空列表和标签，会遗留外部进程、窗口包装与失效指针。
     */
    void releaseAllApps();

    /**
     * @brief 第二阶段：按配置顺序创建网页标签，并收集待等待的外部程序与 PPT 下标
     *
     * 网页标签创建后立即处理一次事件循环，保证标签栏尽早刷新；
     * 普通外部程序与 PPT 只记录下标，留到后续阶段并行等待 / 串行加载，
     * 避免单个应用阻塞导致后面所有标签排队。
     *
     * @param isReload            是否为重新加载（重新加载时不重复预启动）
     * @param pendingExternalIndexes 输出参数，窗口尚未就绪、需要并行等待的外部程序下标
     * @param pptAppIndexes       输出参数，需要串行加载的 PPT 应用下标
     */
    void createTabsInConfigOrder(bool isReload, QVector<int> *pendingExternalIndexes, QVector<int> *pptAppIndexes);

    /**
     * @brief 第三阶段：并行等待待嵌入外部程序的窗口，并逐个嵌入标签页
     * @param pendingExternalIndexes 待等待的应用下标集合
     */
    void embedPendingExternalApps(const QVector<int> &pendingExternalIndexes);

    /**
     * @brief 第四阶段：串行加载 PPT 应用
     *
     * PPT 通过 ActiveQt 调用 WPS/PowerPoint COM 接口，必须在主线程串行执行，
     * 放到最后可避免放映窗口的启动耗时阻塞前面所有标签的创建。
     *
     * @param pptAppIndexes PPT 应用下标集合
     */
    void loadPptApps(const QVector<int> &pptAppIndexes);

    /**
     * @brief 批量加载结束后切回配置中的第一个应用
     *
     * 网页标签先建、外部程序与 PPT 后建，按就绪顺序插入会让当前页停在后建标签上，
     * 这里统一归位到配置首项，避免启动后出现标签选中跳变。
     *
     * @param isReload 是否为重新加载（重新加载时保持现场选中项不变）
     */
    void selectFirstAppAfterLoad(bool isReload);

    /**
     * @brief chrome 专用嵌入：通过 Qt foreign window 包装后挂到标签页
     * @param app         应用配置
     * @param process     已探测到的进程信息
     * @param insertIndex 标签插入位置
     */
    void embedChromeWindow(AppConfig &app, ProcessInfo &process, int insertIndex);

    /**
     * @brief 纯 Win32 方式嵌入普通桌面程序窗口
     *
     * 不使用 QWindow::fromWinId + createWindowContainer（实测会导致目标 Qt 程序崩溃），
     * 而是把外部窗口直接设为标签页内原生宿主控件的子窗口，父子关系由 Windows 维护。
     *
     * @param app         应用配置
     * @param process     已探测到的进程信息
     * @param insertIndex 标签插入位置
     */
    void embedNativeWindow(AppConfig &app, ProcessInfo &process, int insertIndex);

    /**
     * @brief 初始化标签页内容控件的公共属性
     *
     * 统一设置尺寸、伸缩策略、深色占位底色与软件 id 属性，
     * 供新增标签与替换标签两条路径共用，避免重复代码。
     *
     * @param widget 内容控件（宿主控件 / 网页视图 / PPT 容器）
     * @param app    应用配置
     */
    void prepareTabPage(QWidget *widget, const AppConfig &app);

    /**
     * @brief 查找已存在同 id（或旧配置下同标签文本）的标签索引
     * @param app      应用配置
     * @param tabTitle 目标标签文本
     * @return 标签索引，未找到返回 -1
     */
    int findExistingTabIndex(const AppConfig &app, const QString &tabTitle) const;

    /**
     * @brief 把已登记进程项的 tabName 同步为实际标签文本
     * @param app      应用配置
     * @param tabTitle 实际标签文本（含序号前缀）
     */
    void syncRegisteredTabName(const AppConfig &app, const QString &tabTitle);

    /**
     * @brief 用新控件原位替换指定索引的标签
     * @param tabIndex 被替换的标签索引
     * @param widget   新内容控件
     * @param app      应用配置
     * @param tabTitle 标签文本
     */
    void replaceTabAt(int tabIndex, QWidget *widget, AppConfig &app, const QString &tabTitle);

    /**
     * @brief 在指定位插入新标签
     * @param widget      内容控件
     * @param app         应用配置
     * @param tabTitle    标签文本
     * @param insertIndex 插入位置，小于 0 或越界时追加到末尾
     */
    void appendTab(QWidget *widget, AppConfig &app, const QString &tabTitle, int insertIndex);

    QScreen *defaultScreen = nullptr;
    QScreen *currentScreen = nullptr;
    QList<QScreen *> screenList;
    QWidget *tabBarWidget = nullptr;
    QTabBar *tabBar = nullptr;
    QWidget *screenIndexWidget = nullptr;
    QLabel *screenIndexLbl = nullptr;
    Ui::ExternalAppWindow *ui;
    ProcessUtils *processUtils;
    QVector<ProcessInfo> externalAppList = QVector<ProcessInfo>();
    /**
     * @brief 原生宿主控件与外部程序窗口句柄的映射
     * @note key 为标签页内承载外部窗口的原生 QWidget，value 为被嵌入的外部窗口句柄。
     *       用于在宿主控件尺寸变化时同步外部窗口大小。
     */
    QHash<QWidget *, WId> nativeHostWindows;
    /**
     * @brief 需要让客户区铺满宿主控件的窗口集合（开启“去掉边框”时注册）
     *
     * 这类窗口仍保留 WS_THICKFRAME，窗口外框比客户区大一圈可缩放边框；
     * 尺寸校正时需按窗口外框补偿这圈边框，才能让客户区正好等于宿主控件大小。
     */
    QSet<QWidget *> nativeFillClientHosts;
    /**
     * @brief 宿主控件对应的“外部程序自身假定的外框尺寸”
     *
     * Qt 程序会缓存创建窗口时的外框尺寸来换算内容区，跨进程去掉标题栏后它算出的内容区
     * 会比真实客户区小一圈，底部露出宿主底色。这里按原外框尺寸把窗口放大，抵消这段差值。
     * 非 Qt 程序不需要补偿，因此不会出现在该映射中。
     */
    QHash<QWidget *, QSize> nativeAppFrameSizes;
    /** @brief 外部窗口尺寸校正定时器，用于抵消鼠标拖动边框造成的尺寸变化 */
    QTimer *nativeHostFitTimer = nullptr;
    /** @brief 已通过 prestartApps() 拉起过的外部程序路径，避免重复启动 */
    QSet<QString> prestartedAppPaths;
    /** @brief 预启动阶段明确启动失败的外部程序路径，嵌入阶段直接跳过，避免无谓的长时间等待 */
    QSet<QString> failedAppPaths;
    /**
     * @brief 是否处于首次批量加载标签页的过程中
     *
     * 加载期间标签是按“能多快建好就多快建好”的顺序插入的，先建好的标签会成为当前页，
     * 表现为启动后先闪出配置里的第二个应用。该标记用于加载期间抑制自动切页，
     * 统一由 loadApps() 结束时切回配置中的第一个应用。
     */
    bool appsLoading = false;
    /** @brief 是否处于高亮选中态（管理界面选中时大屏四周显示红色边框） */
    bool m_highlighted = false;
    /** @brief 软件 id 到标签页控件的映射，用于热更新时稳定定位标签 */
    QHash<QString, QWidget *> appTabWidgets;
    /** @brief 软件 id 到 PPT 放映会话的映射，用于热更新时结束放映 */
    QHash<QString, PptSession> pptSessions;

    void initWindow();
    void initTabWidget();
    void tabIndexChanged(int tabIndex);
    void displayScreenIndex();
    void startExternalProgram(AppConfig &app, bool isReload = false, int insertIndex = -1);
    /**
     * @brief 重新加载单个应用（供 reloadCurrentTab() 调用）
     * @param app 应用配置
     */
    void reloadApp(AppConfig &app);
    /**
     * @brief 结束某个应用对应的外部进程，并清理其窗口句柄与宿主控件映射
     * @param app 应用配置，按软件 id 匹配本屏已登记的外部进程
     */
    void stopExternalApp(AppConfig &app);
    bool startProcess(AppConfig &app);
    /**
     * @brief 探测一次外部程序的可嵌入窗口
     *
     * 桌面应用优先按可执行文件路径定位（不依赖窗口标题），chrome 及路径匹配失败时
     * 退回“进程名 + 窗口标题”匹配。
     *
     * @param app 应用配置
     * @return 命中的进程信息，hwnd 为 0 表示本次未找到窗口
     */
    ProcessInfo probeProcessWindow(AppConfig &app);
    /**
     * @brief 轮询等待外部程序窗口出现
     *
     * 最长等待 8 秒，期间连续 3 次命中同一个窗口才认为窗口已稳定创建。
     *
     * @param app 应用配置
     * @return 找到的进程信息，hwnd 为 0 表示最终未找到窗口
     */
    ProcessInfo findProcessInfo(AppConfig &app);
    /**
     * @brief 并行等待多个外部程序的窗口就绪
     *
     * 同一轮轮询中同时探测所有待嵌入应用，每个应用独立累计连续命中次数，
     * 总等待时长约等于“最慢的那个应用”，而不是各应用等待时间之和。
     *
     * @param appIndexes 待等待应用在 config.appList 中的下标集合
     * @return 下标到已稳定窗口进程信息的映射；超时未命中的应用不会出现在结果中
     */
    QHash<int, ProcessInfo> waitForProcessWindows(const QVector<int> &appIndexes);
    /**
     * @brief 把探测到的进程信息补齐配置字段并登记到 externalAppList
     * @param app     应用配置
     * @param process 待登记的进程信息
     */
    void registerExternalProcess(AppConfig &app, ProcessInfo &process);
    /**
     * @brief 把已经找到的外部程序窗口嵌入为标签页
     * @param app         应用配置
     * @param process     已探测到的进程信息，hwnd 必须有效
     * @param insertIndex 标签插入位置，小于 0 时追加到末尾
     */
    void embedProcessWindow(AppConfig &app, ProcessInfo &process, int insertIndex);
    /**
     * @brief 加载网页应用（QCefView / CEF 内核实现）
     * @param app         应用配置
     * @param insertIndex 标签插入位置，小于 0 时追加到末尾
     */
    void loadWebPage(AppConfig &app, int insertIndex = -1);
    /**
     * @brief 把已创建好的内容控件加入标签页
     * @param widget      内容控件（宿主控件 / 网页视图 / PPT 容器）
     * @param app         应用配置
     * @param insertIndex 插入位置，小于 0 时追加到末尾；已存在同 id 标签时原地替换
     */
    void addWindowToTabWidget(QWidget *widget, AppConfig &app, int insertIndex = -1);
    void startAndPlayPpt(AppConfig &app, int insertIndex = -1);
    void playPpt(QAxWidget *ppt, QAxObject *presentation, QAxObject *slideShowWindow, AppConfig &app,
                 int insertIndex = -1, PptSession *session = nullptr);
    /**
     * @brief PPT 循环守护：轮询放映视图状态，消除每轮结尾的"结束放映"黑屏
     *
     * WPS/PowerPoint 在循环放映模式下，放完最后一页会先进入黑屏再回到第一页。
     * 本方法由 50ms 定时器周期调用：一旦检测到黑屏（3）或白屏（4），立即恢复放映并跳到第一页；
     * 检测到放映窗口已关闭时重新发起放映；检测到放映结束（5）时重建一次放映会话。
     *
     * @param ppt                承载放映的 COM 控件
     * @param presentation       已打开的演示文稿
     * @param slideShowSettings  放映设置对象，重新发起放映时需要
     * @param app                应用配置副本，用于重新发起放映时定位窗口
     * @param insertIndex        标签插入位置
     */
    void guardPptLoop(QAxWidget *ppt, QAxObject *presentation, QAxObject *slideShowSettings,
                      const AppConfig &app, int insertIndex);

    /**
     * @brief 构造标签显示文本
     * @param app 应用配置
     * @return 含可选的屏幕序号前缀的标签文本
     */
    QString buildTabTitle(const AppConfig &app) const;
    /**
     * @brief 按软件 id 查找标签页索引
     * @param appId 软件 id
     * @return 标签索引，未找到返回 -1
     */
    int findTabIndexByAppId(const QString &appId) const;
    /**
     * @brief 获取当前标签对应的软件 id
     * @return 软件 id，当前无标签时返回空字符串
     */
    QString currentAppId() const;
    /**
     * @brief 依据 ui->tabWidget 的标签顺序重建悬浮标签栏，并保持当前选中项
     */
    void syncFloatingTabBar();
    /**
     * @brief 按标签实际总宽度重新布局悬浮标签栏
     *
     * 标签栏宽度不再固定为 500，而是按所有标签的宽度累加后自适应：
     * 右边缘保持默认布局位置，宽度增加时向左扩展，避免最后一个标签被裁掉。
     */
    void layoutFloatingTabBar();
    /**
     * @brief 结束并清理指定 PPT 放映会话
     * @param appId 软件 id
     */
    void stopPptSession(const QString &appId);
};
#endif  // MAINWINDOW_H
