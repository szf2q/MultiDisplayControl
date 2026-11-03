#include "ConfigStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QUuid>

#include "IniConfiguration.h"

QString ConfigStore::jsonPath(const QString &dir)
{
    return QDir(dir).filePath(QStringLiteral("config.json"));
}

QString ConfigStore::iniPath(const QString &dir)
{
    return QDir(dir).filePath(QStringLiteral("config.ini"));
}

bool ConfigStore::isWebPath(const QString &path)
{
    return path.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) ||
           path.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);
}

bool ConfigStore::isPptPath(const QString &path)
{
    return path.contains(QStringLiteral(".ppt"), Qt::CaseInsensitive);
}

QString ConfigStore::detectType(const QString &path)
{
    if (isWebPath(path))
    {
        return QStringLiteral("web");
    }
    if (isPptPath(path))
    {
        return QStringLiteral("ppt");
    }
    return QStringLiteral("desktop");
}

QString ConfigStore::makeSoftwareId()
{
    // 只取前 8 位十六进制，保证可读性的同时满足单机配置的唯一性
    return QStringLiteral("sw-") + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

QString ConfigStore::makeScreenId()
{
    return QStringLiteral("scr-") + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

const SoftwareItem *ConfigStore::findSoftware(const MultiScreenConfig &config, const QString &id)
{
    for (int i = 0; i < config.softwareList.size(); i++)
    {
        if (config.softwareList[i].id == id)
        {
            return &config.softwareList[i];
        }
    }
    return nullptr;
}

void ConfigStore::rebuildRuntimeApps(MultiScreenConfig *config)
{
    if (config == nullptr)
    {
        return;
    }
    QHash<QString, SoftwareItem> softwareById;
    for (int i = 0; i < config->softwareList.size(); i++)
    {
        softwareById.insert(config->softwareList[i].id, config->softwareList[i]);
    }

    for (int i = 0; i < config->screenList.size(); i++)
    {
        ScreenConfig &screen = config->screenList[i];
        screen.appList.clear();
        for (int j = 0; j < screen.assignments.size(); j++)
        {
            const ScreenAssignment &assignment = screen.assignments[j];
            if (!softwareById.contains(assignment.softwareId))
            {
                // 分配引用了已删除的软件条目，直接跳过，保存时会由管理界面提示
                continue;
            }
            const SoftwareItem &software = softwareById.value(assignment.softwareId);
            AppConfig app;
            app.id = software.id;
            app.appPath = software.path;
            app.windowTitle = software.windowTitle.isEmpty() ? software.name : software.windowTitle;
            app.tabName = assignment.tabName.isEmpty() ? software.name : assignment.tabName;
            app.startupArgs = software.startupArgs;
            if (!isWebPath(software.path))
            {
                QFileInfo fileInfo(app.appPath);
                app.processName = fileInfo.fileName();
                app.appPath = app.appPath.replace("/", QDir::separator());
            }
            screen.appList.append(app);
        }
        screen.appCount = screen.appList.size();
    }
    config->count = config->screenList.size();
}

bool ConfigStore::load(const QString &dir, MultiScreenConfig *config, QString *error)
{
    if (config == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("配置输出指针为空");
        }
        return false;
    }
    *config = MultiScreenConfig();

    const QString jsonFile = jsonPath(dir);
    QString jsonError;
    if (QFile::exists(jsonFile))
    {
        if (loadFromJson(jsonFile, config, &jsonError))
        {
            return true;
        }
        qWarning() << "load config.json failed:" << jsonError;
    }

    // config.json 不存在（旧部署升级）或损坏时，尝试从旧版 config.ini 导入配置：
    // 导入只在内存中生效，仅当 config.json 不存在时才把结果落盘为新配置；
    // 损坏的 JSON 保留原样，避免覆盖用户数据后无法人工比对修复。
    const QString iniFile = iniPath(dir);
    if (!QFile::exists(iniFile))
    {
        if (error != nullptr)
        {
            *error = jsonError.isEmpty() ? QStringLiteral("未找到配置文件 %1").arg(jsonFile) : jsonError;
        }
        return false;
    }

    *config = MultiScreenConfig();
    QString iniError;
    if (!loadFromIni(dir, config, &iniError))
    {
        if (error != nullptr)
        {
            *error = jsonError.isEmpty() ? iniError : jsonError + QStringLiteral("；") + iniError;
        }
        return false;
    }
    if (!QFile::exists(jsonFile))
    {
        QString writeError;
        if (writeJson(jsonFile, *config, &writeError))
        {
            qWarning() << "imported config.ini to config.json";
        }
        else
        {
            qWarning() << "import config.ini failed, write config.json failed:" << writeError;
        }
    }
    return true;
}

bool ConfigStore::loadFromJson(const QString &filePath, MultiScreenConfig *config, QString *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("无法打开 %1").arg(filePath);
        }
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("config.json 解析失败：%1").arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject root = document.object();
    const QJsonObject global = root.value(QStringLiteral("global")).toObject();
    config->showIndex = global.value(QStringLiteral("showIndex")).toBool(true);
    config->sortScreen = global.value(QStringLiteral("sortScreen")).toBool(true);
    config->debugMode = global.value(QStringLiteral("debugMode")).toBool(false);
    config->runAsAdministrator = global.value(QStringLiteral("runAsAdministrator")).toBool(false);

    config->softwareList.clear();
    const QJsonArray softwareArray = root.value(QStringLiteral("software")).toArray();
    for (int i = 0; i < softwareArray.size(); i++)
    {
        const QJsonObject object = softwareArray.at(i).toObject();
        SoftwareItem item;
        item.id = object.value(QStringLiteral("id")).toString();
        item.name = object.value(QStringLiteral("name")).toString();
        item.type = object.value(QStringLiteral("type")).toString();
        item.path = object.value(QStringLiteral("path")).toString();
        item.windowTitle = object.value(QStringLiteral("windowTitle")).toString();
        item.startupArgs = object.value(QStringLiteral("startupArgs")).toString();
        item.remark = object.value(QStringLiteral("remark")).toString();
        item.previewPath = object.value(QStringLiteral("previewPath")).toString();
        if (item.path.isEmpty())
        {
            continue;
        }
        if (item.id.isEmpty())
        {
            item.id = makeSoftwareId();
        }
        if (item.type.isEmpty())
        {
            item.type = detectType(item.path);
        }
        if (item.name.isEmpty())
        {
            item.name = QFileInfo(item.path).fileName();
            if (item.name.isEmpty())
            {
                item.name = item.path;
            }
        }
        config->softwareList.append(item);
    }

    config->screenList.clear();
    const QJsonArray screenArray = root.value(QStringLiteral("screens")).toArray();
    for (int i = 0; i < screenArray.size(); i++)
    {
        const QJsonObject object = screenArray.at(i).toObject();
        ScreenConfig screen;
        screen.id = object.value(QStringLiteral("id")).toString();
        if (screen.id.isEmpty())
        {
            screen.id = makeScreenId();
        }
        screen.screenIndex = object.value(QStringLiteral("index")).toInt(i + 1);
        screen.screenTitle = object.value(QStringLiteral("title")).toString();
        screen.tabOffsetX = object.value(QStringLiteral("tabOffsetX")).toInt(0);
        screen.displayName = object.value(QStringLiteral("displayName")).toString();
        const QJsonObject geometry = object.value(QStringLiteral("geometry")).toObject();
        screen.geometry = QRect(geometry.value(QStringLiteral("x")).toInt(0),
                                geometry.value(QStringLiteral("y")).toInt(0),
                                geometry.value(QStringLiteral("width")).toInt(0),
                                geometry.value(QStringLiteral("height")).toInt(0));
        screen.primaryScreen = object.value(QStringLiteral("primary")).toBool(false);
        screen.showIndex = config->showIndex;
        screen.sortScreen = config->sortScreen;
        screen.debugMode = config->debugMode;

        const QJsonArray apps = object.value(QStringLiteral("apps")).toArray();
        for (int j = 0; j < apps.size(); j++)
        {
            const QJsonObject appObject = apps.at(j).toObject();
            ScreenAssignment assignment;
            assignment.softwareId = appObject.value(QStringLiteral("softwareId")).toString();
            assignment.tabName = appObject.value(QStringLiteral("tabName")).toString();
            if (!assignment.softwareId.isEmpty())
            {
                screen.assignments.append(assignment);
            }
        }
        config->screenList.append(screen);
    }

    rebuildRuntimeApps(config);
    return true;
}

bool ConfigStore::loadFromIni(const QString &dir, MultiScreenConfig *config, QString *error)
{
    IniConfiguration ini(QStringLiteral("config.ini"), dir);
    config->count = ini.getValue(QStringLiteral("screen"), QStringLiteral("count"), 0).toInt();
    config->showIndex = ini.getValue(QStringLiteral("screen"), QStringLiteral("show_index"), true).toBool();
    config->sortScreen = ini.getValue(QStringLiteral("screen"), QStringLiteral("sort_screen"), true).toBool();
    config->debugMode = ini.getValue(QStringLiteral("screen"), QStringLiteral("debug_mode"), false).toBool();
    config->runAsAdministrator =
        ini.getValue(QStringLiteral("screen"), QStringLiteral("runas_administrator"), false).toBool();

    // 旧配置同一路径可能带不同窗口标题，这里以“路径 + 标题”作为去重键，避免误合并
    QHash<QString, QString> softwareKeyToId;
    for (int screenIndex = 1; screenIndex <= config->count; screenIndex++)
    {
        ScreenConfig screen;
        const QString section = QStringLiteral("screen-%1").arg(screenIndex);
        const int appCount = ini.getValue(section, QStringLiteral("app_count"), 0).toInt();
        screen.id = makeScreenId();
        screen.screenIndex = screenIndex;
        screen.screenTitle = ini.getValue(section, QStringLiteral("screen_title"), QString()).toString();
        screen.sortScreen = config->sortScreen;
        screen.showIndex = config->showIndex;
        screen.debugMode = config->debugMode;
        screen.tabOffsetX = ini.getValue(section, QStringLiteral("tab_offset_x"), 0).toInt();
        if (screen.tabOffsetX < 0)
        {
            screen.tabOffsetX = 0;
        }
        // 旧配置没有物理屏绑定信息，displayName 留空，运行时按序号回退匹配
        screen.displayName.clear();

        for (int i = 1; i <= appCount; i++)
        {
            const QString keyPrefix = QStringLiteral("app_%1").arg(i);
            const QString appPath = ini.getValue(section, keyPrefix + QStringLiteral("_path"), QString()).toString();
            const QString appTitle = ini.getValue(section, keyPrefix + QStringLiteral("_title"), QString()).toString();
            const QString appTabName =
                ini.getValue(section, keyPrefix + QStringLiteral("_tab_name"), QString()).toString();
            const QString appStartupArgs =
                ini.getValue(section, keyPrefix + QStringLiteral("_startup_args"), QString()).toString();
            if (appPath.isEmpty() || appTitle.isEmpty())
            {
                continue;
            }

            const QString softwareKey = appPath + QStringLiteral("|") + appTitle;
            QString softwareId = softwareKeyToId.value(softwareKey);
            if (softwareId.isEmpty())
            {
                SoftwareItem item;
                item.id = makeSoftwareId();
                item.name = appTitle;
                item.type = detectType(appPath);
                item.path = appPath;
                item.windowTitle = appTitle;
                item.startupArgs = appStartupArgs;
                config->softwareList.append(item);
                softwareId = item.id;
                softwareKeyToId.insert(softwareKey, softwareId);
            }

            ScreenAssignment assignment;
            assignment.softwareId = softwareId;
            assignment.tabName = appTabName.isEmpty() ? appTitle : appTabName;
            screen.assignments.append(assignment);
        }

        // 旧配置中每个分配都会在上面的循环里生成对应软件条目，这里直接以分配数作为应用数
        screen.appCount = screen.assignments.size();
        if (screen.appCount > 0)
        {
            config->screenList.append(screen);
        }
    }

    // 所有屏幕已追加完毕，统一生成运行时应用列表
    rebuildRuntimeApps(config);
    if (config->screenList.isEmpty() && config->count > 0)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("config.ini 中没有有效的屏幕或应用配置");
        }
        return false;
    }
    return true;
}

QJsonObject ConfigStore::softwareToJson(const SoftwareItem &item)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), item.id);
    object.insert(QStringLiteral("name"), item.name);
    object.insert(QStringLiteral("type"), item.type);
    object.insert(QStringLiteral("path"), item.path);
    object.insert(QStringLiteral("windowTitle"), item.windowTitle);
    object.insert(QStringLiteral("startupArgs"), item.startupArgs);
    object.insert(QStringLiteral("remark"), item.remark);
    object.insert(QStringLiteral("previewPath"), item.previewPath);
    return object;
}

QJsonObject ConfigStore::screenToJson(const ScreenConfig &screen)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), screen.id);
    object.insert(QStringLiteral("index"), screen.screenIndex);
    object.insert(QStringLiteral("title"), screen.screenTitle);
    object.insert(QStringLiteral("tabOffsetX"), screen.tabOffsetX);
    object.insert(QStringLiteral("displayName"), screen.displayName);

    QJsonObject geometry;
    geometry.insert(QStringLiteral("x"), screen.geometry.x());
    geometry.insert(QStringLiteral("y"), screen.geometry.y());
    geometry.insert(QStringLiteral("width"), screen.geometry.width());
    geometry.insert(QStringLiteral("height"), screen.geometry.height());
    object.insert(QStringLiteral("geometry"), geometry);
    object.insert(QStringLiteral("primary"), screen.primaryScreen);

    QJsonArray apps;
    for (int i = 0; i < screen.assignments.size(); i++)
    {
        QJsonObject app;
        app.insert(QStringLiteral("softwareId"), screen.assignments[i].softwareId);
        app.insert(QStringLiteral("tabName"), screen.assignments[i].tabName);
        apps.append(app);
    }
    object.insert(QStringLiteral("apps"), apps);
    return object;
}

bool ConfigStore::writeJson(const QString &filePath, const MultiScreenConfig &config, QString *error)
{
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);

    QJsonObject global;
    global.insert(QStringLiteral("showIndex"), config.showIndex);
    global.insert(QStringLiteral("sortScreen"), config.sortScreen);
    global.insert(QStringLiteral("debugMode"), config.debugMode);
    global.insert(QStringLiteral("runAsAdministrator"), config.runAsAdministrator);
    root.insert(QStringLiteral("global"), global);

    QJsonArray softwareArray;
    for (int i = 0; i < config.softwareList.size(); i++)
    {
        softwareArray.append(softwareToJson(config.softwareList[i]));
    }
    root.insert(QStringLiteral("software"), softwareArray);

    QJsonArray screenArray;
    for (int i = 0; i < config.screenList.size(); i++)
    {
        screenArray.append(screenToJson(config.screenList[i]));
    }
    root.insert(QStringLiteral("screens"), screenArray);

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("无法写入 %1").arg(filePath);
        }
        return false;
    }
    const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size())
    {
        file.cancelWriting();
        if (error != nullptr)
        {
            *error = QStringLiteral("写入 %1 失败").arg(filePath);
        }
        return false;
    }
    if (!file.commit())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("提交 %1 失败").arg(filePath);
        }
        return false;
    }
    return true;
}

bool ConfigStore::backupFile(const QString &filePath)
{
    if (!QFile::exists(filePath))
    {
        return true;
    }
    const QString backupPath = filePath + QStringLiteral(".bak");
    if (QFile::exists(backupPath) && !QFile::remove(backupPath))
    {
        return false;
    }
    return QFile::copy(filePath, backupPath);
}

bool ConfigStore::save(const QString &dir, const MultiScreenConfig &config, QString *error)
{
    QDir directory(dir);
    if (!directory.exists() && !QDir().mkpath(dir))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("无法创建配置目录 %1").arg(dir);
        }
        return false;
    }

    MultiScreenConfig normalized = config;
    // 保存前按软件清单重新生成运行时数据，保证 appCount 与 assignments 一致
    rebuildRuntimeApps(&normalized);

    const QString jsonFile = jsonPath(dir);
    // 备份失败不阻断保存，但会在日志中记录
    if (!backupFile(jsonFile))
    {
        qWarning() << "backup config.json failed";
    }

    return writeJson(jsonFile, normalized, error);
}
