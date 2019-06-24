QT       += core gui xml xmlpatterns webenginewidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = SubstanceManager
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

CONFIG += c++11

# For main app
SOURCES += *.cpp
HEADERS += *.h
FORMS += *.ui

RC_ICONS = appicon.ico

# Themes
RESOURCES += *.qrc \
    ffficons.qrc \
    prbview.qrc

# the rest
INCLUDEPATH += lzma

LIBS += -lkernel32 -luser32 -lshell32 -lgdi32

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
