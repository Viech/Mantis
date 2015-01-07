QMAKE_CXXFLAGS += -std=c++11

QT += network

HEADERS += ircclient.h \
    common.h \
    calendar.h \
    unvquery.h \
    github.h \
    config.h

SOURCES += ircclient.cpp \
    mantis.cpp \
    calendar.cpp \
    unvquery.cpp \
    github.cpp \
    config.cpp

LIBS += -lircclient -pthread -lconfig++ -lcurl

OTHER_FILES += \
    ../example.config
