#ifndef PROCESSUTILSLINUXIMPL_H
#define PROCESSUTILSLINUXIMPL_H
#include "ProcessUtils.h"

class ProcessUtilsLinux : public ProcessUtils
{
  public:
    ProcessUtilsLinux() : ProcessUtils(){};
    virtual QVector<ProcessInfo> getProcessInfo(const QString &processName) override;
    virtual QVector<ProcessInfo> getProcessInfo(const QString &processName, const QString &windowTitle) override;
    virtual QVector<ProcessInfo> getProcessInfo(quint64 pid) override;
};

#endif  // PROCESSUTILSLINUXIMPL_H
