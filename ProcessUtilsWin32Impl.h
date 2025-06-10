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
    /**
     * @brief runAsAdministrators 以管理员身份运行
     */
    virtual int runAsAdministrators() override;
    virtual ProcessInfo getProcessInfoByPath(const QString &processPath) override;
    virtual QVector<ProcessInfo> getProcessInfoByWindowTitle(const QString &winTitle) override;
    virtual QVector<ProcessInfo> getProcessInfo(const QString &processName) override;
    virtual QVector<ProcessInfo> getProcessInfo(const QString &processName, const QString &windowTitle) override;
    virtual QVector<ProcessInfo> getProcessInfo(quint64 pid) override;
    virtual QString getProcessPath(quint64 pid) override;
    virtual bool startProcess(const QString &processPath, const QString &startupArgs = "",
                              const bool runAsAdmin = false) override;
    virtual void killProcess(QProcess *process) override;
    virtual void killProcess(quint64 pid) override;
    virtual void killProcess(QString processName) override;

    virtual bool isProcessExist(const QString &processName) override;
};

#endif  // PROCESSUTILSWIN32_H
