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
    int tabIndex;
};

struct ScreenConfig
{
    int screenIndex;
    bool sortScreen;
    bool showIndex = true;
    int appCount;
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
    QList<ScreenConfig> screenList;
};

#endif  // SCREENCONFIG_H
