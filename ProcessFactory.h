#ifndef PROCESSFACTORY_H
#define PROCESSFACTORY_H

#include <memory>
#include "ProcessUtils.h"
#ifdef Q_OS_LINUX
#    include "ProcessUtilsLinuxImpl.h"
#endif
#ifdef Q_OS_WIN
#    include "ProcessUtilsWin32Impl.h"
#endif

template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args &&...args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

class ProcessFactory
{
  public:
    //    static std::unique_ptr<ProcessUtils> newInstance()
    static ProcessUtils *newInstance()
    {
#ifdef Q_OS_WIN
        return new ProcessUtilsWin32();
#elif defined(Q_OS_LINUX)
        return new ProcessUtilsLinux();
#else
        qWarning() << "Unsupported platform!";
        reutrn nullptr;
#endif
    }
};

#endif  // PROCESSFACTORY_H
