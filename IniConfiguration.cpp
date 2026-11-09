#include "IniConfiguration.h"

#include <QDir>
#include <QStringList>

namespace
{
    /** @brief 本地实现的版本号，供 getVersion 返回 */
    const char kLocalVersion[] = "1.0.0";
}  // namespace

IniConfiguration::IniConfiguration(void) : m_set(nullptr)
{
}

IniConfiguration::IniConfiguration(const QString &fileName, const QString &path) : m_set(nullptr)
{
    // path 为空时按当前工作目录处理，避免拼接出空路径
    const QDir dir(path.isEmpty() ? QDir::currentPath() : path);
    m_set = new QSettings(dir.absoluteFilePath(fileName), QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt5 默认按系统编码读写 ini，历史配置文件为 UTF-8，这里显式指定避免中文乱码
    m_set->setIniCodec("UTF-8");
#endif
}

IniConfiguration::~IniConfiguration(void)
{
    delete m_set;
    m_set = nullptr;
}

QStringList IniConfiguration::getSections(const QString &regex)
{
    QStringList sections;
    if (m_set == nullptr)
    {
        return sections;
    }

    const QStringList groups = m_set->childGroups();
    if (regex.isEmpty())
    {
        return groups;
    }

    // 仅保留匹配正则的配置节，正则非法时返回空列表，避免抛出无意义的匹配结果
    const QRegularExpression pattern(regex);
    if (!pattern.isValid())
    {
        return sections;
    }
    for (int i = 0; i < groups.size(); i++)
    {
        if (pattern.match(groups[i]).hasMatch())
        {
            sections.append(groups[i]);
        }
    }
    return sections;
}

QStringList IniConfiguration::getKeys(const QString &section, const QString &regex)
{
    QStringList keys;
    if (m_set == nullptr)
    {
        return keys;
    }

    m_set->beginGroup(section);
    const QStringList allKeys = m_set->childKeys();
    m_set->endGroup();

    if (regex.isEmpty())
    {
        return allKeys;
    }

    // 正则非法时返回空列表，调用方按“无匹配项”处理
    const QRegularExpression pattern(regex);
    if (!pattern.isValid())
    {
        return keys;
    }
    for (int i = 0; i < allKeys.size(); i++)
    {
        if (pattern.match(allKeys[i]).hasMatch())
        {
            keys.append(allKeys[i]);
        }
    }
    return keys;
}

QVariant IniConfiguration::getValue(const QString &section, const QString &key, const QVariant &value)
{
    if (m_set == nullptr)
    {
        return value;
    }
    // QSettings 用 “节/键” 定位配置项；配置项不存在时返回调用方给的默认值
    return m_set->value(section + QLatin1Char('/') + key, value);
}

void IniConfiguration::setValue(const QString &section, const QString &key, const QVariant &value)
{
    if (m_set == nullptr)
    {
        return;
    }
    m_set->setValue(section + QLatin1Char('/') + key, value);
    // 立即落盘，避免进程异常退出时丢失刚写入的配置
    m_set->sync();
}

void IniConfiguration::removeSection(const QString &section)
{
    if (m_set == nullptr)
    {
        return;
    }
    m_set->beginGroup(section);
    // 传空键名表示移除该节下的全部配置项
    m_set->remove(QString());
    m_set->endGroup();
    m_set->sync();
}

void IniConfiguration::removeValue(const QString &section, const QString &key)
{
    if (m_set == nullptr)
    {
        return;
    }
    m_set->remove(section + QLatin1Char('/') + key);
    m_set->sync();
}

void IniConfiguration::printConfig()
{
    if (m_set == nullptr)
    {
        return;
    }
    const QStringList keys = m_set->allKeys();
    for (int i = 0; i < keys.size(); i++)
    {
        qDebug().noquote() << keys[i] << '=' << m_set->value(keys[i]).toString();
    }
}

QString IniConfiguration::getVersion(void)
{
    return QString::fromLatin1(kLocalVersion);
}
