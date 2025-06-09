#ifndef FULLSIZETABWIDGET_H
#define FULLSIZETABWIDGET_H

#include <windows.h>
#include <QDebug>
#include <QTabWidget>

class FullSizeTabWidget : public QTabWidget
{
  public:
    FullSizeTabWidget(QWidget *parent = nullptr) : QTabWidget(parent)
    {
    }
    QSize sizeHint() const override
    {
        return QSize(width(), height());
    }
};

#endif  // FULLSIZETABWIDGET_H
