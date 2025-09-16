QT       += core gui svg 

equals(QT_MAJOR_VERSION, 5): QT += widgets
greaterThan(QT_MAJOR_VERSION, 5): QT += svgwidgets

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
