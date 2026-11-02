#include "CefWebView.h"

#ifdef USE_QCEFVIEW

#include <QDebug>
#include <QResizeEvent>
#include <QUrl>
#include <QCefView.h>

CefWebView::CefWebView(QWidget *parent) : QWidget(parent)
{
    // 通过 QCefView 的构造函数直接创建 CEF 视图（当前锁定版本 v1.2.1 无静态工厂方法），
    // 先以 about:blank 占位，真实地址随后由 load() 通过 navigateToUrl() 指定。
    // 说明：QCefView 内部会调用 QCefContext::instance()，因此使用前必须先在 main() 中
    //       创建 QCefContext 完成 CEF 初始化，否则此处会导致空指针解引用。
    cefView = new QCefView(QStringLiteral("about:blank"), nullptr, this);
    cefView->setGeometry(rect());
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

void CefWebView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (cefView != nullptr)
    {
        cefView->setGeometry(rect());
    }
}

#endif  // USE_QCEFVIEW
