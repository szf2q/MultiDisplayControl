#ifndef SCREENCONFIG_H
#define SCREENCONFIG_H
#include <QList>

struct AppConfig
{
    QString appPath;
    QString processName;
    QString windowTitle;
    QString tabName;
    QString startupArgs;
    // 标签页索引，-1 表示尚未分配到标签页
    int tabIndex = -1;
};

struct ScreenConfig
{
    int screenIndex = 0;
    bool sortScreen = false;
    bool showIndex = true;
    int appCount = 0;
    QString screenTitle;
    bool debugMode = false;
    int tabOffsetX = 0;
    QList<AppConfig> appList;
};
struct MultiScreenConfig
{
    int count;
    bool showIndex = true;
    bool sortScreen = true;
    bool debugMode = false;
    bool runAsAdministrator = false;
    QList<ScreenConfig> screenList;
};

#endif  // SCREENCONFIG_H
