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
};
#endif  // PROCESSINFO_H
