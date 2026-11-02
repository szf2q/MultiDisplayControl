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

#ifndef USE_QCEFVIEW
class QGraphicsScene;
class QWebEngineProfile;
#endif

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
#ifndef USE_QCEFVIEW
    /**
     * @brief 网页视图使用的 QGraphicsScene 列表（仅 QWebEngineView 后端使用）
     * @note QGraphicsScene 会接管通过 addWidget 加入的 QWidget（即 QWebEngineView）的所有权，
     *       因此必须在析构函数中显式删除场景（级联释放 view/page），且删除顺序需早于 QWebEngineProfile
     */
    QList<QGraphicsScene *> webSceneList;
    /**
     * @brief 网页视图使用的 QWebEngineProfile 列表（仅 QWebEngineView 后端使用）
     * @note profile 无父对象，必须在 page/view 释放之后再手动释放，否则会导致仍在使用的 profile 被提前销毁
     */
    QList<QWebEngineProfile *> webProfileList;
#endif
    void initWindow();
    void initTabWidget();
    void tabIndexChanged(int tabIndex);
    void displayScreenIndex();
    void startExternalProgram(AppConfig &app, bool isReload = false);
    bool startProcess(AppConfig &app);
    ProcessInfo findProcessInfo(AppConfig &app);
    /**
     * @brief 加载网页应用（按编译期后端开关分派到具体实现）
     * @param app 应用配置
     */
    void loadWebPage(AppConfig &app);
#ifndef USE_QCEFVIEW
    /**
     * @brief 使用 QWebEngineView（QtWebEngine）加载网页应用
     * @param app 应用配置
     */
    void loadWebPageByWebEngine(AppConfig &app);
#else
    /**
     * @brief 使用 QCefView（CEF 内核）加载网页应用
     * @param app 应用配置
     */
    void loadWebPageByCef(AppConfig &app);
#endif
    void addWindowToTabWidget(QWidget *widget, AppConfig &app);
    void startAndPlayPpt(AppConfig &app);
    void playPpt(QAxWidget *ppt, QAxObject *presentation, QAxObject *slideShowWindow, AppConfig &app);
};
#endif  // MAINWINDOW_H
