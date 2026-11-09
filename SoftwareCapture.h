#ifndef SOFTWARECAPTURE_H
#define SOFTWARECAPTURE_H

#include <QImage>
#include <QString>
#include <QStringList>

#include "ScreenConfig.h"

class QWidget;

/**
 * @brief 软件预览图采集工具
 *
 * 新增软件时自动启动一次目标程序，抓取其第一个可见主窗口作为预览图，
 * 保存到可执行文件目录下的 previews/&lt;软件id&gt;.png，并把相对路径写入软件条目。
 *
 * 支持范围：
 *  - 桌面应用：直接启动可执行文件后抓取其窗口；
 *  - 演示文稿：通过 PowerPoint/WPS 的 COM 接口把第一页导出为图片（更稳定，且不会抓到编辑窗口
 *    当前的任意一页），失败时退回“打开文件后抓窗口”的旧方式；
 *  - 网页：用内置 CEF 渲染一个临时窗口，加载完成后抓取页面首屏。
 *
 * 非 Windows 平台（如银河麒麟）没有通用窗口抓取手段，统一返回失败并回退类型图标。
 */
class SoftwareCapture
{
  public:
    /**
     * @brief 获取预览图目录
     * @param appDir 可执行文件所在目录
     * @return 预览图目录的完整路径
     */
    static QString previewDirectory(const QString &appDir);

    /**
     * @brief 生成软件对应的预览图相对路径
     * @param softwareId 软件 id
     * @return 形如 previews/sw-xxxxxxxx.png 的相对路径
     */
    static QString relativePreviewPath(const QString &softwareId);

    /**
     * @brief 获取软件预览图的绝对路径（不判断文件是否存在）
     * @param appDir    可执行文件所在目录
     * @param softwareId 软件 id
     * @return 预览图完整路径
     */
    static QString absolutePreviewPath(const QString &appDir, const QString &softwareId);

    /**
     * @brief 解析软件当前可用的预览图绝对路径
     * @param appDir   可执行文件所在目录
     * @param software 软件条目
     * @return 预览图存在时返回绝对路径，否则返回空字符串
     */
    static QString resolveExistingPreview(const QString &appDir, const SoftwareItem &software);

    /**
     * @brief 启动软件一次并抓取第一个窗口作为预览图
     *
     * 抓取过程可能持续数秒，内部会显示进度提示并周期性处理事件；
     * 抓取完成后会关闭本次启动的进程（若命中的是已在运行的窗口则不关闭）。
     *
     * @param software          软件条目，需要 id、type、path、startupArgs、windowTitle 有效
     * @param appDir            可执行文件所在目录，预览图会写入其 previews 子目录
     * @param parent            进度提示的父窗口
     * @param detectedTitle     输出参数，可选；抓取成功时写入目标窗口标题，用于自动填充窗口标题
     * @return true 表示抓取并保存成功
     */
    static bool capturePreview(const SoftwareItem &software, const QString &appDir, QWidget *parent = nullptr,
                               QString *detectedTitle = nullptr);

    /**
     * @brief 删除软件对应的预览图文件
     * @param appDir    可执行文件所在目录
     * @param softwareId 软件 id
     */
    static void removePreview(const QString &appDir, const QString &softwareId);

  private:
    /**
     * @brief 启动一次桌面应用或演示文稿并抓取其主窗口
     * @param software      软件条目
     * @param parent        进度提示父窗口
     * @param detectedTitle 输出参数，可空
     * @return 抓取到的图像，失败返回空图
     */
    static QImage captureLaunchedWindow(const SoftwareItem &software, QWidget *parent, QString *detectedTitle);

    /**
     * @brief 使用内置 CEF 渲染网页并抓取首屏画面
     * @param software      软件条目，path 为网页地址
     * @param detectedTitle 输出参数，可空；成功时写入网页标题
     * @return 抓取到的图像，失败返回空图
     */
    static QImage captureWebPage(const SoftwareItem &software, QString *detectedTitle);

    /**
     * @brief 导出演示文稿第一页作为预览图
     * @param software      软件条目，path 为 .ppt/.pptx 文件
     * @param parent        进度提示父窗口
     * @param detectedTitle 输出参数，可空；成功时写入文档名（PowerPoint 窗口标题中包含文档名）
     * @return 第一页画面，失败返回空图（由调用方回退到窗口抓取）
     */
    static QImage capturePptFirstSlide(const SoftwareItem &software, QWidget *parent, QString *detectedTitle);

    /**
     * @brief 拆分命令行参数字符串（支持双引号包裹的参数）
     * @param args 原始参数字符串
     * @return 参数列表
     */
    static QStringList splitArguments(const QString &args);

    /**
     * @brief 根据软件类型构造启动命令
     * @param software  软件条目
     * @param program   输出参数，可执行文件路径
     * @param arguments 输出参数，启动参数
     * @param workDir   输出参数，工作目录
     * @return true 表示该类型支持抓取
     */
    static bool buildLaunchCommand(const SoftwareItem &software, QString *program, QStringList *arguments,
                                   QString *workDir);
};

#endif  // SOFTWARECAPTURE_H
