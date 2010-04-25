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
