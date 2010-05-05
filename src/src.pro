#
#  qmpwidget - A Qt widget for embedding MPlayer
#  Copyright (C) 2010 by Jonas Gehring
#

TEMPLATE = lib
DESTDIR = ..
TARGET = qmpwidget

QT += network 
CONFIG += staticlib

# Optional features
QT += opengl
CONFIG += pipemode

include(qmpwidget.pri)
