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
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QWindow>
#include <QtWebEngineWidgets>
#include <iostream>
#include <memory>
#include <vector>
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
    void initWindow();
    void initTabWidget();
    void tabIndexChanged(int tabIndex);
    void displayScreenIndex();
    void startExternalProgram(AppConfig &app, bool isReload = false);
    bool startProcess(AppConfig &app);
    ProcessInfo findProcessInfo(AppConfig &app);
    void loadWebPage(AppConfig &app);
    void addWindowToTabWidget(QWidget *widget, AppConfig &app);
    void startAndPlayPpt(AppConfig &app);
    void playPpt(QAxWidget *ppt, QAxObject *presentation, QAxObject *slideShowWindow, AppConfig &app);
};
#endif  // MAINWINDOW_H
