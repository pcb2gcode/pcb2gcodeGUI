QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

isEmpty(PREFIX) {
    PREFIX = /usr/local
}

TARGET = pcb2gcodeGUI
target.path = $$PREFIX/bin

INSTALLS += target

TEMPLATE = app

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

