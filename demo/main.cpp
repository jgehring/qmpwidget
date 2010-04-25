/*
 *  qmpwidget - A Qt widget for embedding MPlayer
 *  Copyright (C) 2010 by Jonas Gehring
 */


#include <QApplication>
#include <QShowEvent>

#include "qmpwidget.h"


// Custom player
class Player : public QMPWidget
{
	public:
		Player() : QMPWidget() { }

	protected:
		// Key press event handling, resembling a basic MPlayer interface
		void keyPressEvent(QKeyEvent *event)
		{
			switch (event->key()) {
				case Qt::Key_P:
				case Qt::Key_Space:
					if (state() == PlayingState) {
						pause();
					} else if (state() == PausedState) {
						play();
					}
					break;

				case Qt::Key_F:
					setWindowState(windowState() ^ Qt::WindowFullScreen);
					break;

				case Qt::Key_Q:
				case Qt::Key_Escape:
					close();
					break;

				case Qt::Key_Plus:
					writeCommand("audio_delay 0.1");
					break;
				case Qt::Key_Minus:
					writeCommand("audio_delay -0.1");
					break;

				case Qt::Key_Left:
					writeCommand("seek -10");
					break;
				case Qt::Key_Right:
					writeCommand("seek 10");
					break;
				case Qt::Key_Down:
					writeCommand("seek -60");
					break;
				case Qt::Key_Up:
					writeCommand("seek 60");
					break;
				case Qt::Key_PageDown:
					writeCommand("seek -600");
					break;
				case Qt::Key_PageUp:
					writeCommand("seek 600");
					break;

				case Qt::Key_Asterisk:
					writeCommand("volume 10");
					break;
				case Qt::Key_Slash:
					writeCommand("volume -10");
					break;

				case Qt::Key_X:
					writeCommand("sub_delay 0.1");
					break;
				case Qt::Key_Z:
					writeCommand("sub_delay -0.1");
					break;

				default: break;
			}
		}

		void showEvent(QShowEvent *event)
		{
			if (!event->spontaneous()) {
				QMPWidget::start(QApplication::arguments());
			}
		}
};


// Program entry point
int main(int argc, char **argv)
{
	QApplication app(argc, argv);

	Player player;
	player.show();

	return app.exec();
}
