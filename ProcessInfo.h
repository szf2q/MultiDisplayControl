#ifndef PROCESSINFO_H
#define PROCESSINFO_H

#include <windows.h>
#include <QProcess>
#include <QString>
#include <QWindow>
#include <QtGlobal>

struct ProcessInfo
{
    quint64 pid = 0;
    QString processName = "";
    QString titleName = "";
    QString className = "";
    QString processPath = "";
    HWND hwnd = 0;
    QProcess *process = nullptr;
    QWindow *externalWindow = nullptr;
    QString tabName = "";
    /** 关联的软件清单 id，用于热更新时精确匹配标签与进程 */
    QString appId = "";
};
#endif  // PROCESSINFO_H
