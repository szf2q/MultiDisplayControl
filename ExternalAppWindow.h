#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QAxObject>
#include <QAxWidget>
#include <QDesktopWidget>
#include <QDockWidget>
#include <QFileInfo>
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

  private:
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
