QT       += core gui network axcontainer
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# 网页渲染后端（编译期开关）：
#   默认使用 QCefView（CEF 内核，当前锁定 CEF 109.1.18 / Chromium 109），
#   可独立升级 Chromium 内核，以规避较新前端技术栈在旧内核上的兼容性问题；
#   如需回退到 QWebEngineView（QtWebEngine，内核随 Qt 版本固定为 Chromium 77），
#   注释掉下面这一行 DEFINES 即可。
# 重要限制：
#   1. CEF 官方 Windows 二进制由 MSVC 构建，MinGW 无法链接，Windows 下必须使用 MSVC 工具链；
#   2. 为兼容 Windows 7，CEF 需选用 Chromium <= 109 的发行版（Chromium 110 起已放弃 Win7）；
#   3. 开启本开关后必须预先准备好第三方依赖（3rdparty/QCefView 与 3rdparty/cef），
#      否则构建直接失败；依赖可用 tools/build-qcefview-deps.bat 一键生成。
DEFINES += USE_QCEFVIEW

contains(DEFINES, USE_QCEFVIEW) {
    # ---- QCefView 与 CEF 的头文件/库路径，请按实际部署位置调整 ----
    QCEFVIEW_ROOT = $$PWD/3rdparty/QCefView
    CEF_ROOT      = $$PWD/3rdparty/cef

    INCLUDEPATH += $$QCEFVIEW_ROOT/include
    INCLUDEPATH += $$CEF_ROOT

    win32 {
        LIBS += -L$$QCEFVIEW_ROOT/lib -lQCefView
        LIBS += -L$$CEF_ROOT/Release -llibcef
        # 运行期部署说明（由 QCefView 的加载方式决定，不能随意摆放）：
        #   1. QCefView.dll 必须放在 exe 同级目录；
        #   2. CEF 运行库必须放在 exe 同级的 CefView/ 子目录下，包括
        #      CefViewWing.exe（渲染子进程宿主）、libcef.dll、chrome_elf.dll、
        #      libEGL.dll、libGLESv2.dll、d3dcompiler_47.dll、vk_swiftshader.dll、
        #      vulkan-1.dll、snapshot_blob.bin、v8_context_snapshot.bin、
        #      icudtl.dat、*.pak、locales/ 等。
        #      原因：QCefView 内部会 SetDllDirectory("<exe目录>/CefView")，
        #      并把 browser_subprocess_path 指向 "<exe目录>/CefView/CefViewWing.exe"。
        #   以上文件可在 3rdparty/QCefView/bin 下找到，直接整体拷贝即可。
    } else:unix {
        LIBS += -L$$QCEFVIEW_ROOT/lib -lQCefView
        LIBS += -L$$CEF_ROOT/Release -lcef
        # 运行期须将 libcef.so、libQCefView.so、icudtl.dat、locales/、*.pak 放入可搜索路径
    }

    SOURCES += CefWebView.cpp
    HEADERS += CefWebView.h
} else {
    win32:msvc {
        QT   += webenginewidgets webchannel
    }
}

CONFIG += c++11



win32{
#    QMAKE_LFLAGS_WINDOWS += /SUBSYSTEM:WINDOWS
#    QMAKE_LFLAGS_CONSOLE = /SUBSYSTEM:CONSOLE

#    DEFINES += _ATL_XP_TARGETING
#    QMAKE_CFLAGS += /D_USING_V140_SDK71_
#    QMAKE_CXXFLAGS += /D_USING_V140_SDK71_
#    QMAKE_CXXFLAGS += /winsdkversion:10.0.18362.0
}

TARGET = MultiDisplayControl


# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    ExternalAppWindow.cpp \
    GlobalKeyWatcher.cpp \
    GlobalVariants.cpp \
    ProcessUtilsWin32Impl.cpp \
    main.cpp

HEADERS += \
    ExternalAppWindow.h \
    FullSizeTabWidget.h \
    GlobalKeyWatcher.h \
    GlobalVariants.h \
    ProcessFactory.h \
    ProcessInfo.h \
    ProcessUtils.h \
    ProcessUtilsLinuxImpl.h \
    ProcessUtilsWin32Impl.h \
    ScreenConfig.h

FORMS += \
    ExternalAppWindow.ui

TRANSLATIONS += \
    MultiDisplayControl_zh_CN.ts


OUT_DIR = release
CONFIG(release, debug|release){
    win32{
        OUT_DIR = release
    }
    unix{
        contains(QT_ARCH, arm64){
            OUT_DIR = release_kylin_arm
        }else{
            OUT_DIR = release_kylin_x86
        }
    }
}else{
    win32{
        OUT_DIR = debug
    }
    unix{
        contains(QT_ARCH, arm64){
            OUT_DIR = debug_kylin_arm
        }else{
            OUT_DIR = debug_kylin_x86
        }
    }
}
#qmake或构建时控制台打印输出变量的值
#message("OUT_PWD = $$OUT_PWD")

#指定目标文件（dll/so/exe）的存储路径
DESTDIR = $$OUT_PWD/$$OUT_DIR
#指定moc_*.h/cpp文件的存储路径
MOC_DIR = $$OUT_PWD/$$OUT_DIR/tmp
#指定*.o文件的存储路径
OBJECTS_DIR = $$MOC_DIR
#指定qrc_*.h/cpp文件的存储路径
RCC_DIR = $$MOC_DIR
#指定ui_*.h文件的存储路径
UI_DIR = $$MOC_DIR

LIBS += -L$$DESTDIR -ltz.utils.base
win32{
    msvc{
        LIBS+=-luser32
        QMAKE_CXXFLAGS += /source-charset:utf-8 /execution-charset:utf-8
        QMAKE_CFLAGS += /source-charset:utf-8 /execution-charset:utf-8
    }else{
        LIBS += -l Kernel32.dll -l Psapi
    }
}

win32-g++{
#    LIBS += -l Kernel32 -l Psapi
    QMAKE_CXXFLAGS += -finput-charset=utf-8 -fexec-charset=utf-8
    QMAKE_CFLAGS += -finput-charset=utf-8 -fexec-charset=utf-8
}
#QMAKE_RESOURCE_FLAGS += -charset utf-8
#QMAKE_MOC += -DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII
#QMAKE_UIC += -tr tr8
#QMAKE_RCC += -charset utf-8

#指定动态链接库的优先查找路径
unix{
    INSTALLS += $$DESTDIR
    QMAKE_LFLAGS += "-Wl,-rpath,\'$$DESTDIR\'"
    #指定动态链接库的优先查找路径（仅linux有效）
    QMAKE_RPATHDIR += $$DESTDIR
}

#INCLUDEPATH += F:\Qt\Qt5.14.2\5.14.2\msvc2017_64\include
#INCLUDEPATH += F:\Qt\Qt5.14.2\5.14.2\msvc2017_64\include\QtWebEngineWidgets

RESOURCES += \
    resource.qrc

DISTFILES += \
    debug/config.ini \
    release/config.ini
