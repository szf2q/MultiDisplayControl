QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11
win32{
    msvc{
        QMAKE_LFLAGS += /NODEFAULTLIB:msvcrt
        QMAKE_CXXFLAGS += /MT
    }
}

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
    ProcessUtilsWin32Impl.cpp \
    main.cpp

HEADERS += \
    ExternalAppWindow.h \
    FullSizeTabWidget.h \
    GlobalKeyWatcher.h \
    ProcessFactory.h \
    ProcessInfo.h \
    ProcessUtils.h \
    ProcessUtilsLinuxImpl.h \
    ProcessUtilsWin32Impl.h \
    ScreenConfig.h

FORMS += \
    ExternalAppWindow.ui

TRANSLATIONS += \
    SoftwareSwitchingTool_zh_CN.ts

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
    LIBS += -l Kernel32 -lPsapi
}



#指定动态链接库的优先查找路径
QMAKE_LFLAGS += "-Wl,-rpath,\'$$DESTDIR\'"
#win32{
#    QMAKE_LFLAGS += /MANIFESTUAC:\"level=\'requireAdministrator\' uiAccess=\'false\'\"
#    RC_FILE = app.manifest
#}


unix{
    INSTALLS += $$DESTDIR
    #指定动态链接库的优先查找路径（仅linux有效）
    QMAKE_RPATHDIR += $$DESTDIR
}

RESOURCES += \
    resource.qrc

DISTFILES += \
    app.manifest \
    debug/config.ini
