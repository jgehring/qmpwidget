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


#ifndef QMPWIDGET_H_
#define QMPWIDGET_H_


#include <QWidget>

class QStringList;

class QMPProcess;


class QMPWidget : public QWidget
{
	Q_OBJECT

	public:
		enum State {
			NotStartedState = -1,
			LoadingState,
			StoppedState,
			PlayingState,
			BufferingState,
			PausedState,
			ErrorState
		};

		struct MediaInfo {
			QString videoCodec;
			QSize size;
			double fps;
			int bpp;
			double videoBitrate;

			QString audioCodec;
			int sampleRate;
			int channels;
			double audioBitrate;
		};

	public:
		QMPWidget(QWidget *parent = 0);
		~QMPWidget();

		State state() const;
		MediaInfo mediaInfo() const;

		void setMPlayerPath(const QString &path);

		virtual QSize sizeHint() const;

	public slots:
		void start(const QStringList &args);
		void play();
		void pause();
		void stop();
		void toggleFullScreen();

		void writeCommand(const QString &command);

	protected:
		virtual void mouseDoubleClickEvent(QMouseEvent *event);
		virtual void keyPressEvent(QKeyEvent *event);
		virtual void resizeEvent(QResizeEvent *event);

	private slots:
		void updateWidgetSize();

	signals:
		void stateChanged(int state);
		void mediaInfoAvailable();
		void error(const QString &reason);

	private:
		QMPProcess *m_process;
		QWidget *m_widget;
		Qt::WindowFlags m_windowFlags;
		QRect m_geometry;
};


#endif // QMPWIDGET_H_
