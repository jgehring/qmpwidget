/*
 *  qmpwidget - A Qt widget for embedding MPlayer
 *  Copyright (C) 2010 by Jonas Gehring
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <QApplication>
#include <QShowEvent>
#include <QSlider>
#include <QVBoxLayout>

#include "qmpwidget.h"


// Custom player
class Player : public QMPwidget
{
	Q_OBJECT

	public:
		Player(QWidget *parent = 0) : QMPwidget(parent)
		{
			connect(this, SIGNAL(stateChanged(int)), this, SLOT(stateChanged(int)));
		}

	private slots:
		void stateChanged(int state)
		{
			if (state == QMPwidget::NotStartedState) {
				QApplication::exit();
			} else if (state == QMPwidget::PlayingState && mediaInfo().ok) {
				if (parentWidget()) {
					parentWidget()->resize(mediaInfo().size.width(), mediaInfo().size.height());
				} else {
					resize(mediaInfo().size.width(), mediaInfo().size.height());
				}
			}
		}

	protected:
		void showEvent(QShowEvent *event)
		{
			if (!event->spontaneous() && state() == QMPwidget::NotStartedState) {
				QStringList args = QApplication::arguments();
				args.pop_front();
				QMPwidget::start(args);
			}
		}
};


// Program entry point
int main(int argc, char **argv)
{
	QApplication app(argc, argv);

	// Construct a simple widget with the player and a correspondig slider
	QWidget widget;

	QVBoxLayout layout(&widget);
	widget.setLayout(&layout);
	QSlider slider(Qt::Horizontal, &widget);

	Player player(&widget);
	player.setSlider(&slider);

	layout.addWidget(&player);
	layout.addWidget(&slider);
	widget.show();

	return app.exec();
}


#include "main.moc"
