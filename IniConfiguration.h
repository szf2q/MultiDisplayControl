#ifndef INICONFIGURATION_H
#define INICONFIGURATION_H

#include <QDebug>
#include <QFile>
#include <QRegularExpression>
#include <QSettings>
#include <QString>
#include <QTextCodec>
#include <QVariant>
#include <QCoreApplication>

/**
 * @brief The IniConfiguration class ini配置文件
 */
class IniConfiguration
{
  public:
    /**
     * @brief IniConfiguration 初始化，没有实际文件
     */
    IniConfiguration(void);
    /**
     * @brief IniConfiguration 初始化
     * @param fileName 文件名
     * @param path 文件路径
     */
    IniConfiguration(const QString &fileName, const QString &path = nullptr);
    /**
     * @brief IniConfiguration 禁用拷贝构造函数
     */
    IniConfiguration(const IniConfiguration &) = delete;
    /**
     * @brief operator = 禁用赋值函数
     * @return
     */
    IniConfiguration &operator=(const IniConfiguration &) = delete;

    ~IniConfiguration(void);
    /**
     * @brief getSections 获取配置节
     * @param regex 配置节匹配正则表达式
     * @return 配置节
     */
    QStringList getSections(const QString &regex = nullptr);
    /**
     * @brief getKeys 获取配置项
     * @param section 配置节
     * @param regex 配置项匹配正则表达式
     * @return 配置项
     */
    QStringList getKeys(const QString &section, const QString &regex = nullptr);
    /**
     * @brief getValue 值
     * @param section 配置节
     * @param key 配置项
     * @param value 默认值
     * @return 值
     */
    QVariant getValue(const QString &section, const QString &key, const QVariant &value = "");
    /**
     * @brief setValue 设置值
     * @param section 配置节
     * @param key 配置项
     * @param value 配置值
     */
    void setValue(const QString &section, const QString &key, const QVariant &value);
    /**
     * @brief removeSection 移除配置节
     * @param section 配置节
     */
    void removeSection(const QString &section);
    /**
     * @brief removeValue 移除配置项
     * @param section 配置节
     * @param key 配置项
     */
    void removeValue(const QString &section, const QString &key);
    /**
     * @brief printConfig 打印配置文件
     */
    void printConfig();

    /**
     * @brief 获取版本号
     * @return 版本号
     */
    QString getVersion(void);

  private:
    QSettings *m_set;
};

#endif  // INICONFIGURATION_H
