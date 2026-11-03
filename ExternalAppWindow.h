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
     * @brief 重新加载本屏幕的全部应用（故障恢复用）
     *
     * 按配置顺序逐个重载：外部程序先结束旧进程再重新启动并嵌入，网页重建视图并重新加载，
     * PPT 放映由 COM 实例承载、重载会与现有放映冲突，故保持不变。
     * 用于某个屏幕上的应用崩溃或界面异常时，仅恢复该屏幕，而不必重启整个控制程序。
     */
    void reloadApps();
    /**
     * @brief 预启动本屏所有外部程序（不等待窗口出现，幂等）
     *
     * loadApps() 内部会按配置顺序逐个“启动进程 → 等待窗口出现 → 嵌入标签页”，
     * 其中等待窗口出现是主要耗时点。先调用本方法把所有外部程序并行拉起，
     * 后续嵌入时窗口通常已就绪，从而把多个程序的启动耗时重叠起来。
     */
    void prestartApps();

  protected:
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
     * @brief 启动外部窗口尺寸校正定时器（幂等，重复调用只会创建一次）
     *
     * 外部程序窗口本身带可缩放边框，用户仍可用鼠标把窗口拖小、露出宿主背景。
     * 由于跨进程清除 WS_THICKFRAME 会导致目标程序崩溃，这里改为周期检查：
     * 一旦发现外部窗口尺寸与宿主控件不一致，就立即把它校正回铺满状态。
     */
    void startNativeHostFitTimer();

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
    /** @brief 外部窗口尺寸校正定时器，用于抵消鼠标拖动边框造成的尺寸变化 */
    QTimer *nativeHostFitTimer = nullptr;
    /** @brief 已通过 prestartApps() 拉起过的外部程序路径，避免重复启动 */
    QSet<QString> prestartedAppPaths;
    /** @brief 预启动阶段明确启动失败的外部程序路径，嵌入阶段直接跳过，避免无谓的长时间等待 */
    QSet<QString> failedAppPaths;
    void initWindow();
    void initTabWidget();
    void tabIndexChanged(int tabIndex);
    void displayScreenIndex();
    void startExternalProgram(AppConfig &app, bool isReload = false);
    /**
     * @brief 重新加载单个应用（供 reloadApps() 调用）
     * @param app 应用配置
     */
    void reloadApp(AppConfig &app);
    /**
     * @brief 结束某个应用对应的外部进程，并清理其窗口句柄与宿主控件映射
     * @param app 应用配置，按进程路径匹配本屏已登记的外部进程
     */
    void stopExternalApp(AppConfig &app);
    bool startProcess(AppConfig &app);
    ProcessInfo findProcessInfo(AppConfig &app);
    /**
     * @brief 加载网页应用（QCefView / CEF 内核实现）
     * @param app 应用配置
     */
    void loadWebPage(AppConfig &app);
    void addWindowToTabWidget(QWidget *widget, AppConfig &app);
    void startAndPlayPpt(AppConfig &app);
    void playPpt(QAxWidget *ppt, QAxObject *presentation, QAxObject *slideShowWindow, AppConfig &app);
};
#endif  // MAINWINDOW_H
