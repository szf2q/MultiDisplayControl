#ifndef DISPLAYRUNTIME_H
#define DISPLAYRUNTIME_H

#include <QList>
#include <QMap>
#include <QObject>
#include <QScreen>
#include <QStringList>

#include "ScreenConfig.h"

class ExternalAppWindow;

/**
 * @brief 多屏展示运行时
 *
 * 统一负责屏幕窗口的创建、销毁与配置热更新：
 *  - 首次启动按配置创建全部屏幕窗口并加载应用；
 *  - 管理界面保存配置后调用 apply() 做增量更新，只处理发生变化的屏幕与应用，
 *    尽可能不重启已正常运行的应用。
 *
 * 窗口仍然登记在 GlobalVariants::winList 中（key 为屏幕序号字符串），
 * 因此 Alt+1~7 / Alt+R / Alt+F5 / Alt+Q 等既有热键逻辑保持不变。
 */
class DisplayRuntime : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit DisplayRuntime(QObject *parent = nullptr);

    /**
     * @brief 析构函数，自动关闭全部展示窗口
     */
    ~DisplayRuntime() override;

    /**
     * @brief 首次启动：按配置创建全部屏幕窗口并加载应用
     * @param config 全量配置（约定 screenIndex 已按序号 1..N 归一化）
     * @return true 表示至少创建了一个屏幕窗口
     */
    bool start(const MultiScreenConfig &config);

    /**
     * @brief 增量应用新配置
     *
     * 处理顺序：先关闭不再需要的屏幕窗口，再创建新增屏幕窗口并并行预启动，
     * 最后逐个完成内容加载；已存在的屏幕按应用级差异增删改与重排。
     *
     * @param config   新的全量配置
     * @param warnings 输出参数，用于向管理界面反馈未能热应用的项
     */
    void apply(const MultiScreenConfig &config, QStringList *warnings);

    /**
     * @brief 关闭并销毁全部展示窗口
     */
    void shutdown();

    /**
     * @brief 获取最近一次成功应用的配置
     * @return 配置副本
     */
    MultiScreenConfig currentConfig() const;

  private:
    /**
     * @brief 解析配置屏幕对应的物理屏
     *
     * 匹配优先级：物理屏名称 → 保存时的几何 → 排序后的序号。
     *
     * @param screen     屏幕配置
     * @param ordinal    配置中的序号（从 1 开始）
     * @param sortScreen 是否按现有规则对物理屏排序
     * @return 命中的物理屏，未命中返回 nullptr
     */
    QScreen *resolveScreen(const ScreenConfig &screen, int ordinal, bool sortScreen) const;

    /**
     * @brief 按现有规则排序后的物理屏列表
     * @param sortScreen 是否排序
     * @return 物理屏列表
     */
    static QList<QScreen *> sortedScreens(bool sortScreen);

    /**
     * @brief 创建单个屏幕窗口并预启动其外部程序
     * @param config   屏幕配置
     * @param screen   目标物理屏
     * @param warnings 输出参数，记录未能创建的告警
     * @return 创建的窗口，失败返回 nullptr
     */
    ExternalAppWindow *createScreenWindow(const ScreenConfig &config, QScreen *screen, QStringList *warnings);

    /**
     * @brief 关闭并销毁指定屏幕窗口
     * @param key winList 中的键
     */
    void removeScreenWindow(const QString &key);

    /**
     * @brief 按应用级差异更新已存在的屏幕窗口
     * @param win      已存在的窗口
     * @param newScreen 新屏幕配置
     * @param oldScreen 上一次应用的屏幕配置（用于计算差异）
     * @param screen    目标物理屏
     * @param warnings  输出参数，记录告警
     */
    void updateScreenWindow(ExternalAppWindow *win, const ScreenConfig &newScreen, const ScreenConfig &oldScreen,
                            QScreen *screen, QStringList *warnings);

    /**
     * @brief 判断两个应用配置的关键字段是否一致（决定是否需要重启应用）
     * @param a 应用配置 a
     * @param b 应用配置 b
     * @return true 表示路径/窗口标题/启动参数完全一致
     */
    static bool isAppUnchanged(const AppConfig &a, const AppConfig &b);

    /**
     * @brief 按 id 查找屏幕配置在列表中的项
     * @param key winList 键（屏幕序号字符串）
     * @return 命中的屏幕配置，未命中返回 false
     */
    bool findAppliedScreen(const QString &key, ScreenConfig *screen) const;

    /** @brief 最近一次应用的配置，用于计算增量差异 */
    MultiScreenConfig m_config;
};

#endif  // DISPLAYRUNTIME_H
