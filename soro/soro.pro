#-------------------------------------------------
#
# Project created by QtCreator 2016-01-26T22:21:06
#
#-------------------------------------------------

QT       += network

QT       -= gui

TARGET = soro
TEMPLATE = lib

DEFINES += SORO_LIBRARY

SOURCES += \
    channel.cpp \
    logger.cpp

HEADERS +=\
        soro_global.h \
    channel.h \
    logger.h

unix {
    target.path = /usr/lib
    INSTALLS += target
}