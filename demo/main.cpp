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
#include <QFileDialog>
#include <QGridLayout>
#include <QShowEvent>
#include <QSlider>

#include "qmpwidget.h"


// Custom player
class Player : public QMPwidget
{
	Q_OBJECT

	public:
		Player(const QStringList &args, const QString &url, QWidget *parent = 0)
			: QMPwidget(parent), m_url(url)
		{
			connect(this, SIGNAL(stateChanged(int)), this, SLOT(stateChanged(int)));
			QMPwidget::start(args);
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
				QMPwidget::load(m_url);
			}
		}

		void keyPressEvent(QKeyEvent *event)
		{
			if (event->key() == Qt::Key_O) {
				QString url = QFileDialog::getOpenFileName(this, tr("Play media file..."));
				if (!url.isEmpty()) {
					m_url = url;
					QMPwidget::load(m_url);
				}
			} else {
				QMPwidget::keyPressEvent(event);
			}
		}

	private:
		QString m_url;
};


// Program entry point
int main(int argc, char **argv)
{
	QApplication app(argc, argv);

	// Construct a simple widget with the player and a correspondig slider
	QWidget widget;

	QGridLayout layout(&widget);
	widget.setLayout(&layout);
	QSlider seekSlider(Qt::Horizontal, &widget);
	QSlider volumeSlider(Qt::Horizontal, &widget);
	volumeSlider.setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum));

	QStringList args = QApplication::arguments();
	QString url;
	args.pop_front();
	if (!args.isEmpty() && !args.last().startsWith("-")) {
		url = args.last();
		args.pop_back();
	}
	Player player(args, url, &widget);
	player.setSeekSlider(&seekSlider);
	player.setVolumeSlider(&volumeSlider);

	layout.addWidget(&player, 0, 0, 1, 2);
	layout.addWidget(&seekSlider, 1, 0);
	layout.addWidget(&volumeSlider, 1, 1);
	widget.show();

	return app.exec();
}


#include "main.moc"
