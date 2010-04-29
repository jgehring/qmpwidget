#
#  qmpwidget - A Qt widget for embedding MPlayer
#  Copyright (C) 2010 by Jonas Gehring
#

HEADERS += \
	qmpwidget.h

SOURCES += \
	qmpwidget.cpp

yuvpipe: {
DEFINES += QMP_USE_YUVPIPE
}
