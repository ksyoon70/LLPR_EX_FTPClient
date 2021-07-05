#-------------------------------------------------
#
# Project created by QtCreator 2019-12-17T13:21:37
#
#-------------------------------------------------

QT       += core gui ftp xml
QT       += network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

TARGET = LLPR_EX_FTPClient
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_NO_DEBUG_OUTPUT QT_NO_WARNING_OUTPUT

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
    deleteworker.cpp \
        main.cpp \
        mainwindow.cpp \
    configdlg.cpp \
    config.cpp \
    centerdlg.cpp \
    commonvalues.cpp \
    syslogger.cpp \
    threadworker.cpp \
    sftpthrworker.cpp

HEADERS += \
    deleteworker.h \
        mainwindow.h \
    configdlg.h \
    config.h \
    centerdlg.h \
    commonvalues.h \
    syslogger.h \
    dataclass.h \
    threadworker.h \
    sftpthrworker.h

FORMS += \
        mainwindow.ui \
    configdlg.ui \
    centerdlg.ui


CONFIG += c++11

RESOURCES += \
    images/cdtoparent.png \
    images/dir.png \
    images/file.png \
    images/red.png \
    images/blue.png \
    icons/ftp-icon.png

# SFTP를 사용하기 위한 라이브러리
LIBS += \
        /usr/lib/x86_64-linux-gnu/libssh2.so

#  이방법은 윈도우에서 아이콘을 세팅 할때
#RC_ICONS += \
#    icons/ftp-icon.ico \
#    icons/ftp-icon.png
