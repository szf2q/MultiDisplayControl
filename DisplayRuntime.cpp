#include "DisplayRuntime.h"

#include <QDebug>
#include <QGuiApplication>

#include <algorithm>

#include "ExternalAppWindow.h"
#include "GlobalVariants.h"

DisplayRuntime::DisplayRuntime(QObject *parent) : QObject(parent)
{
}

DisplayRuntime::~DisplayRuntime()
{
    shutdown();
}

bool DisplayRuntime::start(const MultiScreenConfig &config)
{
    m_config = config;
    bool createdAny = false;
    QStringList createdKeys;
    for (int i = 0; i < config.screenList.size(); i++)
    {
        // 复制一份并补齐全局开关，保证每块屏的运行时配置与全局设置一致
        ScreenConfig screenConfig = config.screenList[i];
        screenConfig.screenIndex = screenConfig.screenIndex > 0 ? screenConfig.screenIndex : i + 1;
        screenConfig.showIndex = config.showIndex;
        screenConfig.sortScreen = config.sortScreen;
        screenConfig.debugMode = config.debugMode;
        if (screenConfig.appCount <= 0)
        {
            qDebug() << "start, screen has no app, skip, screenIndex = " << screenConfig.screenIndex;
            continue;
        }
        QScreen *screen = resolveScreen(screenConfig, screenConfig.screenIndex, config.sortScreen);
        if (screen == nullptr)
        {
            qDebug() << "start, physical screen not found, screenIndex = " << screenConfig.screenIndex
                     << ", displayName = " << screenConfig.displayName;
            continue;
        }
        qDebug() << "start screen: " << screenConfig.screenIndex << ", app count = " << screenConfig.appCount;
        ExternalAppWindow *win = createScreenWindow(screenConfig, screen, nullptr);
        if (win != nullptr)
        {
            createdAny = true;
            createdKeys << QString::number(screenConfig.screenIndex);
        }
    }

    // 全部窗口创建完成后再统一加载内容：外部程序已并行拉起，避免逐屏串行等待
    for (int i = 0; i < createdKeys.size(); i++)
    {
        ExternalAppWindow *win = GlobalVariants::winList.value(createdKeys[i], nullptr);
        if (win != nullptr)
        {
            win->loadApps(false);
        }
    }
    return createdAny;
}

void DisplayRuntime::apply(const MultiScreenConfig &config, QStringList *warnings)
{
    // 1. 归一化配置并解析物理屏，得到本次需要展示的屏幕集合
    QMap<QString, ScreenConfig> targetConfigs;
    QMap<QString, QScreen *> targetScreens;
    for (int i = 0; i < config.screenList.size(); i++)
    {
        ScreenConfig screenConfig = config.screenList[i];
        screenConfig.screenIndex = screenConfig.screenIndex > 0 ? screenConfig.screenIndex : i + 1;
        screenConfig.showIndex = config.showIndex;
        screenConfig.sortScreen = config.sortScreen;
        screenConfig.debugMode = config.debugMode;
        if (screenConfig.appCount <= 0)
        {
            // 未分配任何软件的屏幕不创建窗口，但配置保留
            continue;
        }
        QScreen *screen = resolveScreen(screenConfig, screenConfig.screenIndex, config.sortScreen);
        if (screen == nullptr)
        {
            QString message = QString("%1号屏（%2）未连接，已保留配置但未展示")
                                  .arg(screenConfig.screenIndex)
                                  .arg(screenConfig.displayName.isEmpty() ? QString("未命名") : screenConfig.displayName);
            qDebug() << "apply," << message;
            if (warnings != nullptr)
            {
                *warnings << message;
            }
            continue;
        }
        const QString key = QString::number(screenConfig.screenIndex);
        targetConfigs.insert(key, screenConfig);
        targetScreens.insert(key, screen);
    }

    // 2. 先关闭不再需要的屏幕窗口（析构函数会结束该屏的外部进程）
    const QStringList existingKeys = GlobalVariants::winList.keys();
    for (int i = 0; i < existingKeys.size(); i++)
    {
        if (!targetConfigs.contains(existingKeys[i]))
        {
            qDebug() << "apply, remove screen window, key = " << existingKeys[i];
            removeScreenWindow(existingKeys[i]);
        }
    }

    // 3. 已存在窗口做增量更新；新增窗口先创建并预启动
    QStringList createdKeys;
    for (QMap<QString, ScreenConfig>::const_iterator it = targetConfigs.constBegin();
         it != targetConfigs.constEnd(); ++it)
    {
        const QString &key = it.key();
        const ScreenConfig &screenConfig = it.value();
        QScreen *screen = targetScreens.value(key, nullptr);
        ExternalAppWindow *win = GlobalVariants::winList.value(key, nullptr);
        if (win == nullptr)
        {
            if (createScreenWindow(screenConfig, screen, warnings) != nullptr)
            {
                createdKeys << key;
            }
            continue;
        }
        ScreenConfig oldConfig;
        if (!findAppliedScreen(key, &oldConfig))
        {
            // 运行时窗口不是由本对象创建的（例如异常恢复），退化为按窗口内配置计算差异
            oldConfig = win->config;
        }
        updateScreenWindow(win, screenConfig, oldConfig, screen, warnings);
    }

    // 4. 新增窗口统一完成内容加载（保留并发启动优势）
    for (int i = 0; i < createdKeys.size(); i++)
    {
        ExternalAppWindow *win = GlobalVariants::winList.value(createdKeys[i], nullptr);
        if (win != nullptr)
        {
            win->loadApps(false);
        }
    }

    // 5. 记录本次配置，供下一次增量比较
    m_config = config;
}

void DisplayRuntime::shutdown()
{
    const QStringList keys = GlobalVariants::winList.keys();
    for (int i = 0; i < keys.size(); i++)
    {
        removeScreenWindow(keys[i]);
    }
}

MultiScreenConfig DisplayRuntime::currentConfig() const
{
    return m_config;
}

QScreen *DisplayRuntime::resolveScreen(const ScreenConfig &screen, int ordinal, bool sortScreen) const
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty())
    {
        return nullptr;
    }
    // 一级：物理屏名称完全匹配（最稳定，显示器改名后仍能命中）
    if (!screen.displayName.isEmpty())
    {
        for (int i = 0; i < screens.size(); i++)
        {
            if (screens[i]->name() == screen.displayName)
            {
                return screens[i];
            }
        }
    }
    // 二级：保存时的几何完全匹配（名称变化时回退）
    if (!screen.geometry.isEmpty())
    {
        for (int i = 0; i < screens.size(); i++)
        {
            if (screens[i]->geometry() == screen.geometry)
            {
                return screens[i];
            }
        }
    }
    // 三级：按现有排序规则取第 ordinal 个（兼容旧配置与未保存过布局的部署）
    const QList<QScreen *> ordered = sortedScreens(sortScreen);
    if (ordinal >= 1 && ordinal <= ordered.size())
    {
        return ordered[ordinal - 1];
    }
    return nullptr;
}

QList<QScreen *> DisplayRuntime::sortedScreens(bool sortScreen)
{
    QList<QScreen *> screens = QGuiApplication::screens();
    if (!sortScreen)
    {
        return screens;
    }
    // 与 ExternalAppWindow::initWindow 共用同一排序规则（主屏优先，其余先按列后按行），
    // 比较器满足严格弱序，避免多屏场景下 std::sort 产生越界访问
    std::sort(screens.begin(), screens.end(), &isScreenOrderedBefore);
    return screens;
}

ExternalAppWindow *DisplayRuntime::createScreenWindow(const ScreenConfig &config, QScreen *screen,
                                                      QStringList *warnings)
{
    if (screen == nullptr)
    {
        if (warnings != nullptr)
        {
            *warnings << QString("%1号屏未找到对应物理屏，跳过创建").arg(config.screenIndex);
        }
        return nullptr;
    }
    ExternalAppWindow *win = new ExternalAppWindow(config);
    // 构造函数按序号猜测物理屏，这里按配置中的稳定标识重新绑定，避免插拔后错位
    win->applyScreenConfig(config, screen);
    win->prestartApps();
    GlobalVariants::winList.insert(QString::number(config.screenIndex), win);
    return win;
}

void DisplayRuntime::removeScreenWindow(const QString &key)
{
    ExternalAppWindow *win = GlobalVariants::winList.take(key);
    if (win == nullptr)
    {
        return;
    }
    // 先隐藏再销毁：避免窗口关闭动画期间仍覆盖在全屏展示之上
    win->hide();
    delete win;
}

void DisplayRuntime::updateScreenWindow(ExternalAppWindow *win, const ScreenConfig &newScreen,
                                        const ScreenConfig &oldScreen, QScreen *screen, QStringList *warnings)
{
    if (win == nullptr)
    {
        return;
    }
    // 先更新窗口自身属性：标题、序号、序号浮层、标签栏偏移、绑定的物理屏
    win->applyScreenConfig(newScreen, screen);

    QStringList oldIds;
    QStringList newIds;
    for (int i = 0; i < oldScreen.appList.size(); i++)
    {
        oldIds << oldScreen.appList[i].id;
    }
    for (int i = 0; i < newScreen.appList.size(); i++)
    {
        newIds << newScreen.appList[i].id;
    }

    // 删除：新配置中不再存在的应用
    for (int i = 0; i < oldIds.size(); i++)
    {
        if (!newIds.contains(oldIds[i]))
        {
            qDebug() << "updateScreenWindow, remove app, id = " << oldIds[i];
            win->removeApp(oldIds[i]);
        }
    }

    // 新增与字段变更：路径/窗口标题/启动参数变化只能重启该应用
    for (int i = 0; i < newScreen.appList.size(); i++)
    {
        const AppConfig &app = newScreen.appList[i];
        const int oldIndex = oldIds.indexOf(app.id);
        if (oldIndex < 0)
        {
            win->addApp(app, i);
            continue;
        }
        if (!isAppUnchanged(app, oldScreen.appList[oldIndex]))
        {
            QString message = QString("%1号屏应用“%2”配置已变更，正在重启该应用")
                                  .arg(newScreen.screenIndex)
                                  .arg(app.tabName);
            qDebug() << "updateScreenWindow," << message;
            if (warnings != nullptr)
            {
                *warnings << message;
            }
            win->removeApp(app.id);
            win->addApp(app, i);
        }
    }

    // 同步窗口内配置副本，保证标题刷新与后续操作使用最新数据
    win->config.appList = newScreen.appList;
    win->config.assignments = newScreen.assignments;
    win->config.appCount = newScreen.appList.size();
    win->refreshTabTitles();

    // 顺序差异：只移动标签，不重启任何应用
    QStringList currentOrder = win->appIdsInTabOrder();
    QStringList expectedOrder;
    for (int i = 0; i < newScreen.appList.size(); i++)
    {
        const QString &appId = newScreen.appList[i].id;
        if (currentOrder.contains(appId) && !expectedOrder.contains(appId))
        {
            expectedOrder << appId;
        }
    }
    if (currentOrder != expectedOrder)
    {
        qDebug() << "updateScreenWindow, reorder tabs" << currentOrder << "->" << expectedOrder;
        win->reorderTabs(expectedOrder);
    }
}

bool DisplayRuntime::isAppUnchanged(const AppConfig &a, const AppConfig &b)
{
    return a.appPath == b.appPath && a.windowTitle == b.windowTitle && a.startupArgs == b.startupArgs;
}

bool DisplayRuntime::findAppliedScreen(const QString &key, ScreenConfig *screen) const
{
    bool ok = false;
    const int index = key.toInt(&ok);
    if (!ok)
    {
        return false;
    }
    for (int i = 0; i < m_config.screenList.size(); i++)
    {
        if (m_config.screenList[i].screenIndex == index)
        {
            if (screen != nullptr)
            {
                *screen = m_config.screenList[i];
            }
            return true;
        }
    }
    return false;
}
