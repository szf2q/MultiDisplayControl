#ifndef CONFIGSTORE_H
#define CONFIGSTORE_H

#include <QJsonObject>
#include <QString>

#include "ScreenConfig.h"

/**
 * @brief 配置读写入口
 *
 * 统一负责管理界面与展示程序之间的配置持久化：
 *  - config.json：管理界面的唯一数据源；
 *  - config.ini：兼容旧版本展示逻辑的导出文件；
 *  - config.json 不存在时回退读取 config.ini，保证旧部署可直接升级。
 *
 * 所有方法均为静态方法，内部不保存任何状态。
 */
class ConfigStore
{
  public:
    /**
     * @brief 获取 config.json 的完整路径
     * @param dir 可执行文件所在目录
     * @return 配置文件完整路径
     */
    static QString jsonPath(const QString &dir);

    /**
     * @brief 获取 config.ini 的完整路径
     * @param dir 可执行文件所在目录
     * @return 配置文件完整路径
     */
    static QString iniPath(const QString &dir);

    /**
     * @brief 加载配置：优先 config.json，不存在时回退 config.ini
     * @param dir   可执行文件所在目录
     * @param config 输出参数，加载成功时写入完整配置
     * @param error 输出参数，失败原因
     * @return true 表示加载成功
     */
    static bool load(const QString &dir, MultiScreenConfig *config, QString *error = nullptr);

    /**
     * @brief 保存配置：备份旧文件后写 config.json，并导出 config.ini
     * @param dir    可执行文件所在目录
     * @param config 待保存配置
     * @param error  输出参数，失败原因
     * @return true 表示保存成功
     */
    static bool save(const QString &dir, const MultiScreenConfig &config, QString *error = nullptr);

    /**
     * @brief 根据软件清单与屏幕分配，重新生成每块屏幕的运行时应用列表
     * @param config 待刷新的配置
     */
    static void rebuildRuntimeApps(MultiScreenConfig *config);

    /**
     * @brief 根据路径推断软件类型
     * @param path 网页地址或文件路径
     * @return web / desktop / ppt
     */
    static QString detectType(const QString &path);

    /**
     * @brief 判断是否为网页地址
     * @param path 待判断路径
     * @return true 表示 http(s) 地址
     */
    static bool isWebPath(const QString &path);

    /**
     * @brief 判断是否为演示文稿
     * @param path 待判断路径
     * @return true 表示 .ppt/.pptx 文件
     */
    static bool isPptPath(const QString &path);

    /**
     * @brief 生成软件清单唯一标识
     * @return sw-<8位十六进制>
     */
    static QString makeSoftwareId();

    /**
     * @brief 生成屏幕配置唯一标识
     * @return scr-<8位十六进制>
     */
    static QString makeScreenId();

  private:
    /**
     * @brief 从 JSON 文件加载配置
     * @param filePath JSON 文件路径
     * @param config   输出配置
     * @param error    失败原因
     * @return true 表示成功
     */
    static bool loadFromJson(const QString &filePath, MultiScreenConfig *config, QString *error);

    /**
     * @brief 按旧版 config.ini 规则加载配置，并合成软件清单
     * @param dir    可执行文件所在目录
     * @param config 输出配置
     * @param error  失败原因
     * @return true 表示成功
     */
    static bool loadFromIni(const QString &dir, MultiScreenConfig *config, QString *error);

    /**
     * @brief 软件清单序列化
     * @param item 软件条目
     * @return JSON 对象
     */
    static QJsonObject softwareToJson(const SoftwareItem &item);

    /**
     * @brief 单屏配置序列化
     * @param screen 屏幕配置
     * @return JSON 对象
     */
    static QJsonObject screenToJson(const ScreenConfig &screen);

    /**
     * @brief 原子写入 JSON
     * @param filePath 目标文件
     * @param config   配置
     * @param error    失败原因
     * @return true 表示成功
     */
    static bool writeJson(const QString &filePath, const MultiScreenConfig &config, QString *error);

    /**
     * @brief 导出兼容旧加载逻辑的 ini
     * @param filePath 目标文件
     * @param config   配置
     * @param error    失败原因
     * @return true 表示成功
     */
    static bool writeIni(const QString &filePath, const MultiScreenConfig &config, QString *error);

    /**
     * @brief 备份已存在文件为 <文件名>.bak
     * @param filePath 原文件路径
     * @return true 表示无需备份或备份成功
     */
    static bool backupFile(const QString &filePath);

    /**
     * @brief 在软件清单中按 id 查找条目
     * @param config 配置
     * @param id     软件 id
     * @return 命中返回条目指针，否则返回 nullptr
     */
    static const SoftwareItem *findSoftware(const MultiScreenConfig &config, const QString &id);
};

#endif  // CONFIGSTORE_H
