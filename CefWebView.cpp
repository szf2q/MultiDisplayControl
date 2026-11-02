#include "CefWebView.h"

#ifdef USE_QCEFVIEW

#include <QDebug>
#include <QResizeEvent>
#include <QUrl>
#include <QCefView.h>

CefWebView::CefWebView(QWidget *parent) : QWidget(parent)
{
    // 通过 QCefView 的工厂方法创建 CEF 视图，并挂到本控件下由本控件负责生命周期。
    // 不同版本接口差异说明：
    //   - QCefView 1.x：静态工厂 QCefView::createQCefView(url, parent, setting) + setUrl()
    //   - QCefView 2.x/3.x：构造函数可直接创建（QCefView(url, parent)），加载地址接口为 loadUrl()
    // 若编译报错提示接口不存在，请按实际使用的版本调整下面两处调用。
    cefView = QCefView::createQCefView(QStringLiteral("about:blank"), this);
    if (cefView == nullptr)
    {
        qWarning() << "CefWebView: create QCefView failed, please check whether QCefContext has been initialized";
        return;
    }
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
    cefView->setUrl(url.toString());
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
