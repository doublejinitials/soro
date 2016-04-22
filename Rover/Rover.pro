QT += core network
QT -= gui

CONFIG += c++11

TARGET = Rover
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    ../shared/Soro/channel.cpp \
    ../shared/Soro/logger.cpp \
    ../shared/Soro/iniparser.cpp \
    ../shared/Soro/soroini.cpp \
    gpsserver.cpp \
    ../shared/Soro/mbedchannel.cpp \
    roverprocess.cpp

HEADERS += \
    ../shared/Soro/armmessage.h \
    ../shared/Soro/drivemessage.h \
    ../shared/Soro/gimbalmessage.h \
    ../shared/Soro/soro_global.h \
    ../shared/Soro/socketaddress.h \
    ../shared/Soro/channel.h \
    ../shared/Soro/logger.h \
    ../shared/Soro/iniparser.h \
    ../shared/Soro/soroini.h \
    gpsserver.h \
    ../shared/Soro/mbedchannel.h \
    roverprocess.h

INCLUDEPATH += $$PWD/../shared $$PWD/../shared/Soro
DEPENDPATH += $$PWD/../shared $$PWD/../shared/Soro

