#ifndef GLOBALVARIANTS_H
#define GLOBALVARIANTS_H

#include <QMap>
#include <QMutex>
#include <QThread>
#include "ExternalAppWindow.h"

class GlobalVariants
{
  public:
    static QMap<QString, ExternalAppWindow *> winList;
};

#endif  // GLOBALVARIANTS_H
