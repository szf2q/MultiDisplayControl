#ifndef PROCESSINFO_H
#define PROCESSINFO_H

#include <QProcess>
#include <QString>
#include <QWindow>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#else
/**
 * @brief 非 Windows 平台的窗口句柄占位类型
 *
 * ProcessInfo 被跨平台代码共用，Windows 下 hwnd 为真实的 HWND；
 * 非 Windows（如银河麒麟）没有 HWND 概念，这里用整型占位，
 * 避免无条件包含 windows.h 导致 Linux 下直接编译失败。
 */
typedef quintptr HWND;
#endif

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
