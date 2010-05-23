#
#  qmpwidget - A Qt widget for embedding MPlayer
#  Copyright (C) 2010 by Jonas Gehring
#

HEADERS += \
	qmpwidget.h

SOURCES += \
	qmpwidget.cpp

!win32:pipemode: {
DEFINES += QMP_USE_YUVPIPE
HEADERS += qmpyuvreader.h
}
