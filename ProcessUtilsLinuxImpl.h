#ifndef PROCESSUTILSLINUXIMPL_H
#define PROCESSUTILSLINUXIMPL_H
#include "ProcessUtils.h"

class ProcessUtilsLinux : public ProcessUtils
{
  public:
    ProcessUtilsLinux() : ProcessUtils(){};
    QVector<ProcessInfo> getProcessInfo(const QString &processName) override;
    QVector<ProcessInfo> getProcessInfo(const QString &processName, const QString &windowTitle) override;
    QVector<ProcessInfo> getProcessInfo(quint64 pid) override;
};

#endif  // PROCESSUTILSLINUXIMPL_H
