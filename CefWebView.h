#ifndef CEFWEBVIEW_H
#define CEFWEBVIEW_H

#include <QWidget>

#ifdef USE_QCEFVIEW

class QCefView;
class QUrl;

/**
 * @brief 基于 QCefView（CEF 内核）的网页视图封装类
 * @note 本类把所有 QCefView / CEF 的 API 调用集中在一处，业务代码只依赖 load(const QUrl&) 一个接口。
 *       这样做的目的是：QCefView 不同版本的接口名称存在差异（例如 1.x 为 navigateToUrl()，
 *       2.x/3.x 为 loadUrl()，且工厂方法的有无也不同），升级或更换版本时只需修改本文件，
 *       不必改动业务代码。
 * @note 当前锁定的 QCefView 版本为 v1.2.1，配套 CEF 内核 109.1.18（最后一个支持 Windows 7 的版本）。
 * @warning 使用本类前必须先在 main() 中创建 QCefContext 完成 CEF 初始化，否则创建 QCefView 会失败。
 */
class CefWebView : public QWidget
{
    Q_OBJECT

  public:
    /**
     * @brief 构造函数
     * @param parent 父窗口，默认为空
     */
    explicit CefWebView(QWidget *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~CefWebView() override;

    /**
     * @brief 加载指定地址的网页
     * @param url 网页地址
     */
    void load(const QUrl &url);

  protected:
    /**
     * @brief 处理控件显示事件，同步显示内部 CEF 视图
     * @param event 显示事件
     * @note 窗口模式下 CEF 使用真实原生子窗口，Qt 的控件可见性不会自动同步到该原生窗口，
     *       因此必须在这里显式同步，否则非当前标签页的网页会残留在桌面上形成浮动窗口。
     */
    void showEvent(QShowEvent *event) override;

    /**
     * @brief 处理控件隐藏事件，同步隐藏内部 CEF 视图
     * @param event 隐藏事件
     */
    void hideEvent(QHideEvent *event) override;

    /**
     * @brief 处理控件尺寸变化，同步内部 CEF 视图的几何尺寸，保证网页始终铺满整个控件
     * @param event 尺寸变化事件
     */
    void resizeEvent(QResizeEvent *event) override;

  private:
    /** 内部真正的 QCefView 实例，以本控件为父对象，随本控件一并销毁 */
    QCefView *cefView = nullptr;
};

#endif  // USE_QCEFVIEW

#endif  // CEFWEBVIEW_H
