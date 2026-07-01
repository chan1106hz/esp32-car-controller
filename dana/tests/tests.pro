QT       += core testlib serialport
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG   += c++11 testcase
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES += \
    main.cpp \
    tst_checksum.cpp \
    tst_parser.cpp

HEADERS += \
    tst_checksum.h \
    tst_parser.h

INCLUDEPATH += ..
