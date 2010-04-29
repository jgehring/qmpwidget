/*
 *  qmpwidget - A Qt widget for embedding MPlayer
 *  Copyright (C) 2010 by Jonas Gehring
 * 	All rights reserved.
 *
 * 	Redistribution and use in source and binary forms, with or without
 * 	modification, are permitted provided that the following conditions are met:
 *      * Redistributions of source code must retain the above copyright
 * 	      notice, this list of conditions and the following disclaimer.
 * 	    * Redistributions in binary form must reproduce the above copyright
 * 	      notice, this list of conditions and the following disclaimer in the
 * 	      documentation and/or other materials provided with the distribution.
 * 	    * Neither the name of the copyright holders nor the
 * 	      names of its contributors may be used to endorse or promote products
 * 	      derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */


#include <QApplication>
#include <QShowEvent>
#include <QSlider>
#include <QVBoxLayout>

#include "qmpwidget.h"


// Custom player
class Player : public QMPWidget
{
	Q_OBJECT

	public:
		Player(QWidget *parent = 0) : QMPWidget(parent)
		{
			connect(this, SIGNAL(stateChanged(int)), this, SLOT(stateChanged(int)));
		}

	private slots:
		void stateChanged(int state)
		{
			if (state == QMPWidget::NotStartedState) {
				close();
			} else if (state == QMPWidget::PlayingState && mediaInfo().ok) {
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
			if (!event->spontaneous() && state() == QMPWidget::NotStartedState) {
				QStringList args = QApplication::arguments();
				args.pop_front();
				QMPWidget::start(args);
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
