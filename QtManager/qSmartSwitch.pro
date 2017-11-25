#-------------------------------------------------
#
# Project created by QtCreator 2017-09-26T04:26:21
#
#-------------------------------------------------

QT       += core gui network

QMAKE_CFLAGS +=
QMAKE_CXXFLAGS +=


#INCLUDEPATH += /opt/qtrpi/raspbian/sysroot/usr/include
#DEPENDPATH += /opt/qtrpi/raspbian/sysroot/usr/lib
#QMAKE_LIBS += -lwiringPi


greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = qSmartSwitch
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    callapp.cpp

HEADERS  += mainwindow.h \
    callapp.h

FORMS    += mainwindow.ui

target.path = /home/pi
#-style fusion -platform linuxfb:fb=/dev/fb0:size=480x320:mmsize=127x85:offset=0x0

INSTALLS += target

DISTFILES +=

RESOURCES += \
    resource.qrc

