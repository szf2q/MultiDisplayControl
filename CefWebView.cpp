#include "CefWebView.h"

#ifdef USE_QCEFVIEW

#include <QDebug>
#include <QResizeEvent>
#include <QUrl>
#include <QCefView.h>

CefWebView::CefWebView(const QUrl &url, QWidget *parent) : QWidget(parent)
{
    // 通过 QCefView 的构造函数直接创建 CEF 视图（当前锁定版本 v1.2.1 无静态工厂方法）。
    // 若调用方已给出真实地址则直接交给 QCefView，避免先加载 about:blank 再跳转的二次导航；
    // 未给出地址时仍以 about:blank 占位，真实地址随后由 load() 通过 navigateToUrl() 指定。
    // 说明：QCefView 内部会调用 QCefContext::instance()，因此使用前必须先在 main() 中
    //       创建 QCefContext 完成 CEF 初始化，否则此处会导致空指针解引用。
    const QString initialUrl = url.isEmpty() ? QStringLiteral("about:blank") : url.toString();
    cefView = new QCefView(initialUrl, nullptr, this);
    cefView->setGeometry(rect());
    // 窗口模式下 CEF 会创建真实原生子窗口，若该原生窗口在本控件尚未显示时就被创建，
    // 会以独立浮动窗口的形式残留在桌面上；这里先显式隐藏，可见性交由 showEvent/hideEvent 同步。
    cefView->hide();
}

CefWebView::~CefWebView()
{
    // cefView 以本控件为父对象（QCefView 继承自 QWidget），会随本控件一并销毁，此处无需手动释放
}

void CefWebView::load(const QUrl &url)
{
    if (cefView == nullptr)
    {
        return;
    }
    qDebug() << "CefWebView load url = " << url.toString();
    cefView->navigateToUrl(url.toString());
}

void CefWebView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (cefView != nullptr)
    {
        cefView->setGeometry(rect());
        cefView->show();
    }
}

void CefWebView::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    if (cefView != nullptr)
    {
        cefView->hide();
    }
}

void CefWebView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (cefView != nullptr)
    {
        cefView->setGeometry(rect());
    }
}

#endif  // USE_QCEFVIEW
