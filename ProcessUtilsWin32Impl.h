#ifndef PROCESSUTILSWIN32_H
#define PROCESSUTILSWIN32_H

#ifndef WINDOWS_H
#    define WINDOWS_H
#    include <windows.h>
#endif
#include <psapi.h>
#include <stdio.h>
#include <tchar.h>
#include <tlhelp32.h>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <iostream>
#include "ProcessUtils.h"

class ProcessUtilsWin32 : public ProcessUtils
{
  public:
    ProcessUtilsWin32() : ProcessUtils(){};
    ~ProcessUtilsWin32() override;
    ProcessInfo getProcessInfoByPath(const QString &processPath) override;
    QVector<ProcessInfo> getProcessInfoByWindowTitle(const QString &winTitle) override;
    QVector<ProcessInfo> getProcessInfo(const QString &processName) override;
    QVector<ProcessInfo> getProcessInfo(const QString &processName, const QString &windowTitle) override;
    QVector<ProcessInfo> getProcessInfo(quint64 pid) override;
    QString getProcessPath(quint64 pid) override;
    bool startProcess(const QString &processPath, const QString &startupArgs = "",
                      const bool runAsAdmin = false) override;
    void killProcess(QProcess *process) override;
    void killProcess(quint64 pid) override;
    void killProcess(QString processName) override;

    bool isProcessExist(const QString &processName) override;
};

#endif  // PROCESSUTILSWIN32_H
