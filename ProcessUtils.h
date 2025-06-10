#ifndef PROCESSUTILS_H
#define PROCESSUTILS_H
#include <QDebug>
#include <QProcess>
#include "ProcessInfo.h"

/**
 * @brief 进程操作工具接口类，支持启动进程、获取进程列表、获取进程详细信息等功能。
 */
class ProcessUtils
{
  public:
    /**
     * @brief 析构函数，用于释放资源
     */
    virtual ~ProcessUtils()
    {
        qDebug() << "ProcessUtils destructor";
    };
    /**
     * @brief runAsAdministrators 以管理员身份运行
     */
    virtual int runAsAdministrators() = 0;
    /**
     * @brief startProcess 启动进程
     * @param processPath 进程路径
     * @return 返回QProcess对象的指针
     */
    virtual bool startProcess(const QString &processPath, const QString &startupArgs = "",
                              const bool runAsAdmin = false) = 0;
    /**
     * @brief killProcess 关闭软件通过QProcess自己启动的进程，关闭进程后会自动释放QProcess对象
     * @param process QProcess对象
     */
    virtual void killProcess(QProcess *process) = 0;
    /**
     * @brief killProcess 通过进程id关闭指定进程，可关闭其它应用启动的进程
     * @param pid 进程id
     */
    virtual void killProcess(quint64 pid) = 0;
    /**
     * @brief killProcess 通过进程名称关闭指定进程，可关闭其它应用启动的进程
     * @param processName 进程名称
     */
    virtual void killProcess(QString processName) = 0;
    /**
     * @brief getProcessInfoByPath 获取进程信息
     * @param processPath 进程路径
     * @return 进程信息集合
     */
    virtual ProcessInfo getProcessInfoByPath(const QString &processPath) = 0;
    /**
     * @brief getProcessInfoByWindowTitle 获取进程信息
     * @param winTitle 窗体的名称
     * @return 进程信息集合
     */
    virtual QVector<ProcessInfo> getProcessInfoByWindowTitle(const QString &winTitle) = 0;
    /**
     * @brief getProcessInfo 获取进程信息
     * @param processName 进程名称
     * @return 进程信息集合
     */
    virtual QVector<ProcessInfo> getProcessInfo(const QString &processName) = 0;
    /**
     * @brief getProcessInfo 获取进程信息
     * @param processName 进程名称
     * @param windowTitle 窗口标题，可以是部分内容
     * @return 进程信息集合
     */
    virtual QVector<ProcessInfo> getProcessInfo(const QString &processName, const QString &windowTitle) = 0;

    /**
     * @brief getProcessInfo 获取进程信息
     * @param pid 进程id
     * @return 进程信息
     */
    virtual QVector<ProcessInfo> getProcessInfo(quint64 pid) = 0;
    /**
     * @brief getProcessPath 根据进程id获取进程的路径
     * @param pid 进程id
     * @return 进程路径
     */
    virtual QString getProcessPath(quint64 pid) = 0;

    /**
     * @brief isProcessExist 判断进程是否存在
     * @param processName 进程名称
     * @return true表示进程存在，false表示进程不存在
     */
    virtual bool isProcessExist(const QString &processName) = 0;
};

#endif  // PROCESSUTILS_H
