#-------------------------------------------------
#
# Project created by QtCreator 2014-12-30T13:44:55
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = pcb2gcodeGUI
TEMPLATE = app
CONFIG += c++11

SOURCES += main.cpp\
        mainwindow.cpp \
    outputwindow.cpp \
    argaction.cpp

HEADERS  += mainwindow.h \
    outputwindow.h \
    argaction.h \
    settings.h

FORMS    += mainwindow.ui \
    outputwindow.ui
