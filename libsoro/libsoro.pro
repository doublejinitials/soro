## Copyright 2016 The University of Oklahoma.
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.

QT += core network
QT -= gui

CONFIG += c++11
TARGET = soro
TEMPLATE = lib

DEFINES += SORO_LIBRARY

BUILD_DIR = ../build/libsoro
DESTDIR = ../lib
OBJECTS_DIR = $$BUILD_DIR
MOC_DIR = $$BUILD_DIR
RCC_DIR = $$BUILD_DIR
UI_DIR = $$BUILD_DIR
PRECOMPILED_DIR = $$BUILD_DIR

SOURCES += \
    armmessage.cpp \
    audioserver.cpp \
    channel.cpp \
    drivemessage.cpp \
#    flycapenumerator.cpp \
    gimbalmessage.cpp \
    logger.cpp \
    masterarmconfig.cpp \
    mbedchannel.cpp \
    mediaserver.cpp \
    mediastreamer.cpp \
    nmeamessage.cpp \
    roverconfigloader.cpp \
    socketaddress.cpp \
    usbcameraenumerator.cpp \
    videoserver.cpp \
    videoserverarray.cpp \
    confloader.cpp \
    gamepadutil.cpp

HEADERS += \
    latlng.h \
    armmessage.h \
    audioserver.h \
    channel.h \
    drivemessage.h \
#    flycapenumerator.h \
    gimbalmessage.h \
    logger.h \
    masterarmconfig.h \
    mbedchannel.h \
    mediaserver.h \
    mediastreamer.h \
    nmeamessage.h \
    roverconfigloader.h \
    socketaddress.h \
    usbcameraenumerator.h \
    videoserver.h \
    videoserverarray.h \
    soro_global.h \
    confloader.h \
    constants.h \
    enums.h \
    gamepadutil.h \
    util.h

#Flycapture sdk is currently not being used
#LIBS += -lflycapture
LIBS += -lQt5GStreamer-1.0