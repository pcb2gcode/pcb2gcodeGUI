QT       += core gui svg

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
    argaction.cpp \
    graphics_view_zoom.cpp \
    cmdlineargs.cpp

HEADERS  += mainwindow.h \
    outputwindow.h \
    argaction.h \
    settings.h \
    graphics_view_zoom.h \
    cmdlineargs.h

FORMS    += mainwindow.ui \
    outputwindow.ui \
    cmdlineargs.ui

RESOURCES += \
    resource.qrc
