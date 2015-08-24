TEMPLATE = lib
TARGET = scoring
QT += declarative core xml

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += qt plugin c++11

QMAKE_CXXFLAGS += -std=c++11

TARGET = $$qtLibraryTarget($$TARGET)
uri = org.huggle.extension.qt

# Input
SOURCES += \
    scoring.cpp

HEADERS += \
    scoring.hpp

#OTHER_FILES = qmldir

#!equals(_PRO_FILE_PWD_, $$OUT_PWD) {
#    copy_qmldir.target = $$OUT_PWD/qmldir
#    copy_qmldir.depends = $$_PRO_FILE_PWD_/qmldir
#    copy_qmldir.commands = $(COPY_FILE) \"$$replace(copy_qmldir.depends, /, $$QMAKE_DIR_SEP)\" \"$$replace(copy_qmldir.target, /, $$QMAKE_DIR_SEP)\"
#    QMAKE_EXTRA_TARGETS += copy_qmldir
#    PRE_TARGETDEPS += $$copy_qmldir.target
#}

#qmldir.files = qmldir
#unix {
#    maemo5 | !isEmpty(MEEGO_VERSION_MAJOR) {
#        installPath = /usr/lib/qt4/imports/$$replace(uri, \\., /)
#    } else {
#        installPath = $$[QT_INSTALL_IMPORTS]/$$replace(uri, \\., /)
#    }
#    qmldir.path = $$installPath
#    target.path = $$installPath
#    INSTALLS += target qmldir
#}


#win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../huggle3-qt-lx/huggle-build/ -llibcore
#else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../huggle3-qt-lx/huggle-build/ -llibcored

INCLUDEPATH += $$PWD/../../../
DEPENDPATH += $$PWD/../../../
OTHER_FILES += \
    enwiki.json

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/ -lcore
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/ -lcored

INCLUDEPATH += $$PWD/
DEPENDPATH += $$PWD/
