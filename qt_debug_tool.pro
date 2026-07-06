QT += widgets
CONFIG += c++17
CONFIG -= app_bundle
CONFIG += sdk_no_version_check

macx {
    QMAKE_LIBS_OPENGL = -framework OpenGL
}

TARGET = qt_debug_tool
TEMPLATE = app

SOURCES += qt_debug_tool.cpp
