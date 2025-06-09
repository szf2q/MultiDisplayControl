#ifndef CUSTOMWIDGET_H
#define CUSTOMWIDGET_H

#include <QWidget>
class CustomWidget : QWidget
{
  public:
    QWindow *externanWindow = nullptr;

    void setExterWindow(QWindow *win)
    {
        this->externanWindow = win;
    }

  protected:
    void focusInEvent(QFocusEvent *event) override
    {
        if (externanWindow)
        {
            externanWindow->requestActivate();
        }
    }
};

#endif  // CUSTOMWIDGET_H
