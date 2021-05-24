#-------------------------------------------------
#
# Project created by QtCreator 2019-12-17T13:21:37
#
#-------------------------------------------------

QT       += core gui ftp xml

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = LLPR_EX_FTPClient
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
        main.cpp \
        mainwindow.cpp \
    configdlg.cpp \
    config.cpp \
    centerdlg.cpp \
    commonvalues.cpp \
    syslogger.cpp \
    threadworker.cpp

HEADERS += \
        mainwindow.h \
    configdlg.h \
    config.h \
    centerdlg.h \
    commonvalues.h \
    syslogger.h \
    dataclass.h \
    threadworker.h

FORMS += \
        mainwindow.ui \
    configdlg.ui \
    centerdlg.ui


CONFIG += c++11

RESOURCES += \
    images/cdtoparent.png \
    images/dir.png \
    images/file.png
