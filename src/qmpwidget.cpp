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


#include <QKeyEvent>
#include <QProcess>
#include <QStringList>
#include <QtDebug>

#include "qmpwidget.h"


// A custom QProcess designed for the MPlayer slave interface
class QMPProcess : public QProcess
{
	Q_OBJECT

	public:
		QMPProcess(QObject *parent = 0)
			: QProcess(parent), m_state(QMPWidget::NotStartedState), m_mplayerPath("mplayer"),
			  m_mediaInfoAvailable(false)
		{
			connect(this, SIGNAL(readyReadStandardOutput()), this, SLOT(readStdout()));
			connect(this, SIGNAL(readyReadStandardError()), this, SLOT(readStderr()));
		}

		void setMPlayerPath(const QString &path)
		{
			m_mplayerPath = path;
		}

		void startMPlayer(int winId, const QStringList &args)
		{
			QStringList myargs;
			myargs += "-slave";
			myargs += "-noquiet";
			myargs += "-nomouseinput";
			myargs += "-nokeepaspect";
			myargs += "-wid";
			myargs += QString::number(winId);
#ifdef Q_OS_WIN
			myargs += "-monitorpixelaspect";
			myargs += "1";
			myargs += "-vo";
			myargs += "directx:noaccel";
#endif
			myargs += "-input";
			myargs += "nodefault-bindings:conf=/dev/null";
			myargs += args;

			QProcess::start(m_mplayerPath, myargs);
		}

		QProcess::ProcessState processState() const
		{
			return QProcess::state();
		}

		void writeCommand(const QString &command)
		{
			QProcess::write(command.toLocal8Bit()+"\n");
		}

		void quit()
		{
			writeCommand("quit");
			QProcess::waitForFinished(-1);
		}

		void pause()
		{
			writeCommand("pause");
		}

		void stop()
		{
			writeCommand("stop");
		}

	signals:
		void stateChanged(int state);
		void mediaInfoAvailable();
		void error(const QString &reason);

	private slots:
		void readStdout()
		{
			QStringList lines = QString::fromLocal8Bit(readAllStandardOutput()).split("\n", QString::SkipEmptyParts);
			for (int i = 0; i < lines.count(); i++) {
				lines[i].remove("\r");
				parseLine(lines[i]);
			}
		}

		void readStderr()
		{
			QStringList lines = QString::fromLocal8Bit(readAllStandardOutput()).split("\n", QString::SkipEmptyParts);
			for (int i = 0; i < lines.count(); i++) {
				lines[i].remove("\r");
				parseLine(lines[i]);
			}
		}

		// Parses a line of MPlayer output
		void parseLine(const QString &line)
		{
			if (line.startsWith("Playing ")) {
				changeState(QMPWidget::LoadingState);
			} else if (line.startsWith("Cache fill:")) {
				changeState(QMPWidget::BufferingState);
			} else if (line.startsWith("Starting playback...")) {
				changeState(QMPWidget::PlayingState);
			} else if (line.startsWith("File not found: ")) {
				changeState(QMPWidget::ErrorState);
			} else if (line.startsWith("VIDEO:")) {
				parseVideoInfo(line);
			} else if (line.startsWith("AUDIO:")) {
				parseAudioInfo(line);
			} else if (line.startsWith("Exiting...")) {
				changeState(QMPWidget::NotStartedState);
			}
		}

		void parseVideoInfo(const QString &line)
		{
			QStringList info = line.split(" ", QString::SkipEmptyParts);
			for (int i = 0; i < info.count(); i++) {
				if (info[i].startsWith("[")) {
					m_mediaInfo.videoCodec = info[i].mid(1, info[i].length()-2);
				} else if (info[i].split("x").count() == 2) {
					QStringList l = info[i].split("x");
					m_mediaInfo.size.setWidth(l[0].toInt());
					m_mediaInfo.size.setHeight(l[1].toInt());
				} else if (info[i].endsWith("bpp")) {
					m_mediaInfo.bpp = info[i].toInt();
				} else if (info[i] == "fps" && i > 0) {
					m_mediaInfo.fps = info[i-1].toDouble();
				} else if (info[i] == "kbps" && i > 0) {
					m_mediaInfo.videoBitrate = info[i-1].toDouble();
				}
			}

			m_mediaInfoAvailable = true;
			emit mediaInfoAvailable();
		}

		void parseAudioInfo(const QString &line)
		{
			QStringList info = line.split(" ", QString::SkipEmptyParts);
			for (int i = 0; i < info.count(); i++) {
				if (info[i] == "Hz," && i > 0) {
					m_mediaInfo.sampleRate = info[i-1].toInt();
				} else if (info[i] == "ch," && i > 0) {
					m_mediaInfo.channels = info[i-1].toInt();
				} else if (info[i].startsWith("kbit") && i > 0) {
					m_mediaInfo.audioBitrate = info[i-1].toDouble();
				}
			}

			m_mediaInfoAvailable = true;
			emit mediaInfoAvailable();
		}

	private:
		// Changes the current state, possibly emitting multiple signals
		void changeState(QMPWidget::State state, const QString &comment = QString())
		{
			m_state = state;
			emit stateChanged(m_state);

			switch (m_state) {
				case QMPWidget::NotStartedState:
					m_mediaInfoAvailable = false;
					break;

				case QMPWidget::ErrorState:
					emit error(comment);
					m_mediaInfoAvailable = false;
					break;

				default: break;
			}

			if (!m_mediaInfoAvailable) {
				m_mediaInfo = QMPWidget::MediaInfo();
			}
		}

	public:
		QMPWidget::State m_state;
		QString m_mplayerPath;
		bool m_mediaInfoAvailable;
		QMPWidget::MediaInfo m_mediaInfo;
};


/*!
 * \brief Constructor
 */
QMPWidget::QMPWidget(QWidget *parent)
	: QWidget(parent)
{
	setFocusPolicy(Qt::StrongFocus);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	m_widget = new QWidget(this);
	m_widget->setAutoFillBackground(true);
	m_widget->setMouseTracking(true);

	QPalette p = palette();
	p.setColor(QPalette::Window, Qt::black);
	setPalette(p);

	m_process = new QMPProcess(this);
	connect(m_process, SIGNAL(stateChanged(int)), this, SIGNAL(stateChanged(int)));
	connect(m_process, SIGNAL(mediaInfoAvailable()), this, SIGNAL(mediaInfoAvailable()));
	connect(m_process, SIGNAL(error(const QString &)), this, SIGNAL(error(const QString &)));

	connect(m_process, SIGNAL(mediaInfoAvailable()), this, SLOT(updateWidgetSize()));
}

/*!
 * \brief Destructor
 * \details
 * This function will ask the MPlayer process to quit and block until it has really
 * finished.
 */
QMPWidget::~QMPWidget()
{
	if (m_process->processState() == QProcess::Running) {
		m_process->quit();
	}
	delete m_process;
}

/*!
 * \brief Returns the current MPlayer process state
 *
 * \returns The process state
 */
QMPWidget::State QMPWidget::state() const
{
	return m_process->m_state;
}

QMPWidget::MediaInfo QMPWidget::mediaInfo() const
{
	return m_process->m_mediaInfo;
}

/*!
 * \brief Sets the path to the MPlayer executable
 * \details
 * Per default, it is assumed the MPlayer executable is
 * available in the current OS path. Therefore, this value is
 * set to "mplayer".
 *
 * \param path Path to the MPlayer executable
 */
void QMPWidget::setMPlayerPath(const QString &path)
{
	m_process->setMPlayerPath(path);
}

QSize QMPWidget::sizeHint() const
{
	if (m_process->m_mediaInfoAvailable) {
		return m_process->m_mediaInfo.size;
	}
	return QWidget::sizeHint();
}

/*!
 * \brief Starts the MPlayer process with the given arguments
 * \details
 * If there's another process running, it will be terminated first.
 *
 * \param args MPlayer command line arguments, typically a media file
 */
void QMPWidget::start(const QStringList &args)
{
	if (m_process->processState() == QProcess::Running) {
		m_process->quit();
	}

	m_process->startMPlayer((int)m_widget->winId(), args);
}

/*!
 * \brief Resumes playback
 */
void QMPWidget::play()
{
	if (m_process->m_state == PausedState) {
		m_process->pause();
	}
}

/*!
 * \brief Pauses playback
 */
void QMPWidget::pause()
{
	if (m_process->m_state == PlayingState) {
		m_process->pause();
	}
}

/*!
 * \brief Stops playback
 */
void QMPWidget::stop()
{
	m_process->stop();
}

/*!
 * \brief Toggles full-screen mode
 */
void QMPWidget::toggleFullScreen()
{
	if (!isFullScreen()) {
		m_windowFlags = windowFlags() & (Qt::Window);
		m_geometry = geometry();
		setWindowFlags((windowFlags() | Qt::Window));
		// From Phonon::VideoWidget
#ifdef Q_WS_X11
		show();
		raise();
		setWindowState(windowState() | Qt::WindowFullScreen);
#else
		setWindowState(windowState() | Qt::WindowFullScreen);
		show();
#endif
	} else {
		setWindowFlags((windowFlags() ^ (Qt::Window)) | m_windowFlags);
		setWindowState(windowState() & ~Qt::WindowFullScreen);
		setGeometry(m_geometry);
		show();
	}
}

/*!
 * \brief Sends a command to the MPlayer process
 * \details
 * Since MPlayer is being run in slave mode, it reads commands from the standard
 * input. It is assumed that the interface provided by this class might not be
 * sufficient for some situations, so you can use this functions to directly
 * control the MPlayer process.
 *
 * For a complete list of commands for MPlayer's slave mode, see
 * http://www.mplayerhq.hu/DOCS/tech/slave.txt .
 *
 * \param command The command line. A newline character will be added internally.
 */
void QMPWidget::writeCommand(const QString &command)
{
	m_process->writeCommand(command);
}

/*!
 * \brief Mouse double click event handler
 * \details
 * This implementation will toggle full screen and accept the event
 */
void QMPWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
	toggleFullScreen();
	event->accept();
}

/*!
 * \brief Keyboard press event handler
 * \details
 * This implementation tries to resemble the classic MPlayer interface. For a
 * full list of supported key codes, see \ref shortcuts.
 */
void QMPWidget::keyPressEvent(QKeyEvent *event)
{
	bool accept = true;
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
			toggleFullScreen();
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

		default:
			accept = false;
			break;
	}

	event->setAccepted(accept);
}

void QMPWidget::resizeEvent(QResizeEvent *event)
{
	Q_UNUSED(event);
	updateWidgetSize();
}

void QMPWidget::updateWidgetSize()
{
	if (m_process->m_mediaInfoAvailable && !m_process->m_mediaInfo.size.isNull()) {
		QSize mediaSize = m_process->m_mediaInfo.size;
		QSize widgetSize = size();

		double factor = qMin(double(widgetSize.width()) / mediaSize.width(), double(widgetSize.height()) / mediaSize.height());
		QRect wrect(0, 0, int(factor * mediaSize.width() + 0.5), int(factor * mediaSize.height()));
		wrect.moveTopLeft(rect().center() - wrect.center());
		m_widget->setGeometry(wrect);
	} else {
		m_widget->setGeometry(QRect(QPoint(0, 0), size()));
	}
}

#include "qmpwidget.moc"


/* Documentation follows */

/*!
 * \class QMPWidget
 * \brief A Qt widget for embedding MPlayer
 * \details
 * This is a small class which allows Qt developers to embed an MPlayer instance into
 * their application for convenient video playback. Starting with version 4.4, Qt
 * can be build with Phonon, the KDE multimedia framework (see
 * http://doc.trolltech.com/phonon-overview.html for an overview). However, the Phonon
 * API provides only a limited amount of functionality, which may be too limited for some
 * purposes.
 *
 * In contrast, this class provides a way of embedding a full-fledged movie
 * player within an application. This means you can use all of
 * <a href="http://www.mplayerhq.hu/design7/info.html">MPlayer's features</a>, but
 * also that you will need to ship a MPlayer binary with your application if you can't make
 * sure that it is already installed on a user system.
 *
 * For more information about MPlayer, please visit http://www.mplayerhq.hu/ .
 *
 * \section shortcuts Keyboard control
 * The following keyboard shortcuts are implemented. A table listing the
 * corresponding key codes can be found at the
 * <a href="http://doc.trolltech.com/qt.html#Key-enum">Qt documentation</a>.
 *
 * <table>
 *  <tr><th>Key(s)</th><th>Action</th></tr>
 *  <tr>
 *   <td>\p Qt::Key_P, \p Qt::Key_Space</td>
 *   <td>Toggle pause</td>
 *  </tr>
 *  <tr>
 *   <td>\p Qt::Key_F</td>
 *   <td>Toggle fullscreen</td>
 *  </tr>
 *  <tr>
 *   <td>\p Qt::Key_Q, \p Qt::Key_Escape</td>
 *   <td>Close the widget</td>
 *  </tr>
 *  <tr>
 *   <td>\p Qt::Key_Plus, \p Qt::Key_Minus</td>
 *   <td>Adjust audio delay by +/- 0.1 seconds</td>
 *  </tr>
 *  <tr>
 *   <td>\p Qt::Key_Left, \p Qt::Key_Right</td>
 *   <td>Seek backward/forward 10 seconds</td>
 *  </tr>
 *  <tr>
 *   <td>\p Qt::Key_Down, \p Qt::Key_Up</td>
 *   <td>Seek backward/forward 1 minute</td>
 *  </tr>
 *  <tr>
 *   <td>\p Qt::Key_PageDown, \p Qt::Key_PageUp</td>
 *   <td>Seek backward/forward 10 minutes</td>
 *  </tr>
 *  <tr>
 *   <td>\p Qt::Key_Asterisk, \p Qt::Key_Slash</td>
 *   <td>Increase or decrease PCM volume</td>
 *  </tr>
 *  <tr>
 *   <td>\p Qt::Key_X, \p Qt::Key_Z</td>
 *   <td>Adjust subtitle delay by +/- 0.1 seconds</td>
 *  </tr>
 * </table>
 *
 * \section implementation Implementation details
 * TODO
 *
 * \section license License
\verbatim
qmpwidget - A Qt widget for embedding MPlayer
Copyright (C) 2010 by Jonas Gehring
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
   * Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
   * Neither the name of the copyright holders nor the
     names of its contributors may be used to endorse or promote products
     derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
\endverbatim
 */

/*!
 * \enum QMPWidget::State
 * \brief MPlayer state
 * \details
 * This enumeration is somewhat identical to <a href="http://doc.trolltech.com/phonon.html#State-enum">
 * Phonon's State enum</a>, except that it has an additional
 * member that is used when the MPlayer process has not been started yet (NotStartedState)
 *
 * <table>
 *  <tr><th>Constant</th><th>Value</th><th>Description</th></tr>
 *  <tr>
 *   <td>\p QMPWidget::NotStartedState</td>
 *   <td>\p -1</td>
 *   <td>The Mplayer process has not been started yet or has already terminated.</td>
 *  </tr>
 *  <tr>
 *   <td>\p QMPWidget::LoadingState</td>
 *   <td>\p 0</td>
 *   <td>The MPlayer process has just been started, but playback has not been started yet.</td>
 *  </tr>
 *  <tr>
 *   <td>\p QMPWidget::StoppedState</td>
 *   <td>\p 1</td>
 *   <td></td>
 *  </tr>
 *  <tr>
 *   <td>\p QMPWidget::PlayingState</td>
 *   <td>\p 2</td>
 *   <td></td>
 *  </tr>
 *  <tr>
 *   <td>\p QMPWidget::BufferingState</td>
 *   <td>\p 3</td>
 *   <td></td>
 *  </tr>
 *  <tr>
 *   <td>\p QMPWidget::PausedState</td>
 *   <td>\p 4</td>
 *   <td></td>
 *  </tr>
 *  <tr>
 *   <td>\p QMPWidget::ErrorState</td>
 *   <td>\p 5</td>
 *   <td></td>
 *  </tr>
 * </table>
 */

/*!
 * \fn void QMPWidget::stateChanged(int state)
 * \brief Emitted if the state has changed
 * \details
 * This signal is emitted when the state of the MPlayer process changes.
 *
 * \param state The new state
 */

/*!
 * \fn void QMPWidget::error(const QString &reason)
 * \brief Emitted if the state has changed to QMPWidget::ErrorState
 * \details
 * This signal is emitted when the state of the MPlayer process changes to QMPWidget::ErrorState.
 *
 * \param reason Textual error description (may be empty)
 */
