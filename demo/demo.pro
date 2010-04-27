#
#  qmpwidget - A Qt widget for embedding MPlayer
#  Copyright (C) 2010 by Jonas Gehring
#

TEMPLATE = app
TARGET = qmpdemo
DESTDIR = ..

QT += network opengl

INCLUDEPATH += ../src
LIBPATH += ..
LIBS += -lqmpwidget

include(demo.pri)
