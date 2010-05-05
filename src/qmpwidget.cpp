/*
 *  qmpwidget - A Qt widget for embedding MPlayer
 *  Copyright (C) 2010 by Jonas Gehring
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *      * Neither the name of the copyright holders nor the
 *        names of its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
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


#include <QAbstractSlider>
#include <QKeyEvent>
#include <QLocalSocket>
#include <QPainter>
#include <QProcess>
#include <QStringList>
#include <QThread>
#include <QtDebug>

#ifdef QT_OPENGL_LIB
 #include <QGLWidget>
#endif

#include "qmpwidget.h"

#ifdef QMP_USE_YUVPIPE
 #include "qmpyuvreader.cpp"
#endif // QMP_USE_YUVPIPE


// A plain video widget
class QMPPlainVideoWidget : public QWidget
{
	Q_OBJECT

	public:
		QMPPlainVideoWidget(QWidget *parent = 0)
			: QWidget(parent)
		{
			setAttribute(Qt::WA_NoSystemBackground);
			setMouseTracking(true);
		}

	public slots:
		void displayImage(const QImage &image)
		{
			m_pixmap = QPixmap::fromImage(image);
			update();
		}

	protected:
		void paintEvent(QPaintEvent *event)
		{
			Q_UNUSED(event);
			QPainter p(this);
			p.setCompositionMode(QPainter::CompositionMode_Source);
			if (!m_pixmap.isNull()) {
				p.drawPixmap(rect(), m_pixmap);
			} else {
				p.fillRect(rect(), Qt::black);
			}
			p.end();
		}

	private:
		QPixmap m_pixmap;
};


#ifdef QT_OPENGL_LIB

// A OpenGL video widget
class QMPOpenGLVideoWidget : public QGLWidget
{
	Q_OBJECT

	public:
		QMPOpenGLVideoWidget(QWidget *parent = 0)
			: QGLWidget(parent), m_tex(-1)
		{
			setMouseTracking(true);
		}

	public slots:
		void displayImage(const QImage &image)
		{
			makeCurrent();
			if (m_tex >= 0) {
				deleteTexture(m_tex);
			}
			m_tex = bindTexture(image);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			updateGL();
		}

	protected:
		void initializeGL()
		{
			glEnable(GL_TEXTURE_2D);
			glClearColor(0, 0, 0, 0);
			glClearDepth(1);
		}

		void resizeGL(int w, int h)
		{
			glViewport(0, 0, w, qMax(h, 1));
		}

		void paintGL()
		{
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glLoadIdentity();
			if (m_tex >= 0) {
				glBindTexture(GL_TEXTURE_2D, m_tex);
				glBegin(GL_QUADS);
				glTexCoord2f(0, 0); glVertex2f(-1, -1);
				glTexCoord2f(1, 0); glVertex2f( 1, -1);
				glTexCoord2f(1, 1); glVertex2f( 1,  1);
				glTexCoord2f(0, 1); glVertex2f(-1,  1);
				glEnd();
			}
		}

	private:
		int m_tex;
};

#endif // QT_OPENGL_LIB


// A custom QProcess designed for the MPlayer slave interface
class QMPProcess : public QProcess
{
	Q_OBJECT

	public:
		QMPProcess(QObject *parent = 0)
			: QProcess(parent), m_state(QMPwidget::NotStartedState), m_mplayerPath("mplayer")
#ifdef QMP_USE_YUVPIPE
			  , m_yuvReader(NULL)
#endif
		{
			resetValues();

#ifdef Q_WS_WIN
			m_mode = QMPwidget::EmbeddedMode;
			m_videoOutput = "directx,directx:noaccel";
#elif defined(Q_WS_X11)
			m_mode = QMPwidget::EmbeddedMode;
 #ifdef QT_OPENGL_LIB
			m_videoOutput = "gl2,gl,xv";
 #else
			m_videoOutput = "xv";
 #endif
#elif defined(Q_WS_MAC)
			m_mode = QMPwidget::PipeMode;
 #ifdef QT_OPENGL_LIB
			m_videoOutput = "gl,quartz";
 #else
			m_videoOutput = "quartz";
 #endif
#endif

			connect(this, SIGNAL(readyReadStandardOutput()), this, SLOT(readStdout()));
			connect(this, SIGNAL(readyReadStandardError()), this, SLOT(readStderr()));
		}

		~QMPProcess()
		{
#ifdef QMP_USE_YUVPIPE
			if (m_yuvReader != NULL) {
				m_yuvReader->stop();
			}
#endif
		}

		void startMPlayer(QWidget *widget, const QStringList &args)
		{
			if (m_mode == QMPwidget::PipeMode) {
#ifdef QMP_USE_YUVPIPE
				m_yuvReader = new QMPYuvReader(this);
#else
				m_mode = QMPwidget::EmbeddedMode;
#endif
			}

			QStringList myargs;
			myargs += "-slave";
			myargs += "-noquiet";
			myargs += "-identify";
			myargs += "-nomouseinput";
			myargs += "-nokeepaspect";
			myargs += "-monitorpixelaspect";
			myargs += "1";
			myargs += "-input";
			myargs += "nodefault-bindings:conf=/dev/null";

			if (m_mode == QMPwidget::EmbeddedMode) {
				myargs += "-wid";
				myargs += QString::number((int)widget->winId());
				if (!m_videoOutput.isEmpty()) {
					myargs += "-vo";
					myargs += m_videoOutput;
				}
			} else {
#ifdef QMP_USE_YUVPIPE
				myargs += "-vo";
				myargs += QString("yuv4mpeg:file=%1").arg(m_yuvReader->m_pipe);
#endif
			}

			myargs += args;
			QProcess::start(m_mplayerPath, myargs);

			if (m_mode == QMPwidget::PipeMode) {
#ifdef QMP_USE_YUVPIPE
				connect(m_yuvReader, SIGNAL(imageReady(const QImage &)), widget, SLOT(displayImage(const QImage &)));
				m_yuvReader->start();
#endif
			}
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
		void streamPositionChanged(double position);
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
				changeState(QMPwidget::LoadingState);
			} else if (line.startsWith("Cache fill:")) {
				changeState(QMPwidget::BufferingState);
			} else if (line.startsWith("Starting playback...")) {
				m_mediaInfo.ok = true; // No more info here
				changeState(QMPwidget::PlayingState);
			} else if (line.startsWith("File not found: ")) {
				changeState(QMPwidget::ErrorState);
			} else if (line.startsWith("ID_")) {
				parseMediaInfo(line);
			} else if (line.startsWith("A:") || line.startsWith("V:")) {
				parsePosition(line);
			} else if (line.startsWith("Exiting...")) {
				changeState(QMPwidget::NotStartedState);
			}
		}

		// Parses MPlayer's media identification output
		void parseMediaInfo(const QString &line)
		{
			QStringList info = line.split("=");
			if (info.count() < 2) {
				return;
			}

			if (info[0] == "ID_VIDEO_FORMAT") {
				m_mediaInfo.videoFormat = info[1];
			} else if (info[0] == "ID_VIDEO_BITRATE") {
				m_mediaInfo.videoBitrate = info[1].toInt();
			} else if (info[0] == "ID_VIDEO_WIDTH") {
				m_mediaInfo.size.setWidth(info[1].toInt());
			} else if (info[0] == "ID_VIDEO_HEIGHT") {
				m_mediaInfo.size.setHeight(info[1].toInt());
			} else if (info[0] == "ID_VIDEO_FPS") {
				m_mediaInfo.framesPerSecond = info[1].toDouble();

			} else if (info[0] == "ID_AUDIO_FORMAT") {
				m_mediaInfo.audioFormat = info[1];
			} else if (info[0] == "ID_AUDIO_BITRATE") {
				m_mediaInfo.audioBitrate = info[1].toInt();
			} else if (info[0] == "ID_AUDIO_RATE") {
				m_mediaInfo.sampleRate = info[1].toInt();
			} else if (info[0] == "ID_AUDIO_NCH") {
				m_mediaInfo.numChannels = info[1].toInt();

			} else if (info[0] == "ID_LENGTH") {
				m_mediaInfo.length = info[1].toDouble();
			} else if (info[0] == "ID_SEEKABLE") {
				m_mediaInfo.seekable = (bool)info[1].toInt();

			} else if (info[0].startsWith("ID_CLIP_INFO_NAME")) {
				m_currentTag = info[1];
			} else if (info[0].startsWith("ID_CLIP_INFO_VALUE") && !m_currentTag.isEmpty()) {
				m_mediaInfo.tags.insert(m_currentTag, info[1]);
			}
		}

		// Parsas MPlayer's position output
		void parsePosition(const QString &line)
		{
			static QRegExp rx("[ :]");
			QStringList info = line.split(rx, QString::SkipEmptyParts);

			double oldpos = m_streamPosition;
			for (int i = 0; i < info.count(); i++) {
				if (info[i] == "V" && info.count() > i) {
					m_streamPosition = info[i+1].toDouble();
					break;
				}
			}

			if (oldpos != m_streamPosition) {
				emit streamPositionChanged(m_streamPosition);
			}
		}

	private:
		// Changes the current state, possibly emitting multiple signals
		void changeState(QMPwidget::State state, const QString &comment = QString())
		{
#ifdef QMP_USE_YUVPIPE
			if (m_yuvReader != NULL && (state == QMPwidget::ErrorState || state == QMPwidget::NotStartedState)) {
				m_yuvReader->stop();
				m_yuvReader->deleteLater();
			}
#endif

			m_state = state;
			emit stateChanged(m_state);

			switch (m_state) {
				case QMPwidget::NotStartedState:
					resetValues();
					break;

				case QMPwidget::ErrorState:
					emit error(comment);
					resetValues();
					break;

				default: break;
			}
		}

		// Resets the media info and position values
		void resetValues()
		{
			m_mediaInfo = QMPwidget::MediaInfo();
			m_streamPosition = -1;
		}

	public:
		QMPwidget::State m_state;

		QString m_mplayerPath;
		QString m_videoOutput;
		QString m_pipe;
		QMPwidget::Mode m_mode;

		QMPwidget::MediaInfo m_mediaInfo;
		double m_streamPosition; // This is the video position

		QString m_currentTag;

#ifdef QMP_USE_YUVPIPE
		QMPYuvReader *m_yuvReader;
#endif
};


// Initialize the media info structure
QMPwidget::MediaInfo::MediaInfo()
	: videoBitrate(0), framesPerSecond(0), sampleRate(0), numChannels(0),
	  ok(false), length(0), seekable(false)
{

}


/*!
 * \brief Constructor
 * \param parent Parent widget
 */
QMPwidget::QMPwidget(QWidget *parent)
	: QWidget(parent), m_slider(NULL)
{
	setFocusPolicy(Qt::StrongFocus);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

#ifdef QT_OPENGL_LIB
	m_widget = new QMPOpenGLVideoWidget(this);
#else
	m_widget = new QMPPlainVideoWidget(this);
#endif

	QPalette p = palette();
	p.setColor(QPalette::Window, Qt::black);
	setPalette(p);

	m_seekTimer.setInterval(50);
	m_seekTimer.setSingleShot(true);
	connect(&m_seekTimer, SIGNAL(timeout()), this, SLOT(delayedSeek()));

	m_process = new QMPProcess(this);
	connect(m_process, SIGNAL(stateChanged(int)), this, SLOT(mpStateChanged(int)));
	connect(m_process, SIGNAL(streamPositionChanged(double)), this, SLOT(mpStreamPositionChanged(double)));
	connect(m_process, SIGNAL(error(const QString &)), this, SIGNAL(error(const QString &)));
}

/*!
 * \brief Destructor
 * \details
 * This function will ask the MPlayer process to quit and block until it has really
 * finished.
 */
QMPwidget::~QMPwidget()
{
	if (m_process->processState() == QProcess::Running) {
		m_process->quit();
	}
	delete m_process;
}

/*!
 * \brief Returns the current MPlayer process state
 * \returns The process state
 */
QMPwidget::State QMPwidget::state() const
{
	return m_process->m_state;
}

/*!
 * \brief Returns the current media info object 
 * \details
 * Please check QMPwidget::MediaInfo::ok to make sure the media
 * information has been fully parsed.
 * \returns The media info object
 */
QMPwidget::MediaInfo QMPwidget::mediaInfo() const
{
	return m_process->m_mediaInfo;
}

/*!
 * \brief Returns the current playback position
 * \returns The current playback position in seconds
 */
double QMPwidget::tell() const
{
	return m_process->m_streamPosition;
}

/*!
 * \brief Sets the video playback mode
 * \details
 * Please see \ref playbackmodes for a discussion of the available modes.
 * \param Mode The video playback mode
 */
void QMPwidget::setMode(Mode mode)
{
#ifdef QMP_USE_YUVPIPE
	m_process->m_mode = mode;
#else
	Q_UNUSED(mode)
#endif
}

/*!
 * \brief Returns the current video playback mode
 * \returns The current video playback mode
 */
QMPwidget::Mode QMPwidget::mode() const
{
	return m_process->m_mode;
}

void QMPwidget::setVideoOutput(const QString &output)
{
	m_process->m_videoOutput = output;
}

QString QMPwidget::videoOutput() const
{
	return m_process->m_videoOutput;
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
void QMPwidget::setMPlayerPath(const QString &path)
{
	m_process->m_mplayerPath = path;
}

/*!
 * \brief Returns the current path to the MPlayer executable
 */
QString QMPwidget::mplayerPath() const
{
	return m_process->m_mplayerPath;
}

/*!
 * \brief Sets a seeking slider for this widget
 */
void QMPwidget::setSlider(QAbstractSlider *slider)
{
	if (m_slider) {
		m_slider->disconnect(this);
		disconnect(m_slider);
	}

	connect(slider, SIGNAL(valueChanged(int)), this, SLOT(seek(int)));

	if (m_process->m_mediaInfo.ok) {
		slider->setRange(0, m_process->m_mediaInfo.length);
	}
	if (m_process->m_mediaInfo.ok) {
		slider->setEnabled(m_process->m_mediaInfo.seekable);
	}
	m_slider = slider;
}

/*!
 * \brief Returns a suitable size hint for this widget
 * \details
 * This function is used internally by Qt.
 */
QSize QMPwidget::sizeHint() const
{
	if (m_process->m_mediaInfo.ok && !m_process->m_mediaInfo.size.isNull()) {
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
void QMPwidget::start(const QStringList &args)
{
	if (m_process->processState() == QProcess::Running) {
		m_process->quit();
	}

	m_process->startMPlayer((QMPwidget *)m_widget, args);
}

/*!
 * \brief Resumes playback
 */
void QMPwidget::play()
{
	if (m_process->m_state == PausedState) {
		m_process->pause();
	}
}

/*!
 * \brief Pauses playback
 */
void QMPwidget::pause()
{
	if (m_process->m_state == PlayingState) {
		m_process->pause();
	}
}

/*!
 * \brief Stops playback
 */
void QMPwidget::stop()
{
	m_process->stop();
}

/*!
 * \brief Media playback seeking
 *
 * \param offset Seeking offset in seconds
 * \param whence Seeking mode
 * \returns \p true If the seeking mode is valid
 */
bool QMPwidget::seek(int offset, int whence)
{
	return seek(double(offset), whence);
}

/*!
 * \brief Media playback seeking
 *
 * \param offset Seeking offset in seconds
 * \param whence Seeking mode
 * \returns \p true If the seeking mode is valid
 */
bool QMPwidget::seek(double offset, int whence)
{
	m_seekTimer.stop(); // Cancel all current seek requests

	int mode;
	switch (whence) {
		case RelativeSeek:
			mode = 0;
			break;
		case PercentageSeek:
			mode = 1;
			break;
		case AbsoluteSeek:
			mode = 2;
			break;

		default:
			return false;
	}

	// Schedule seek request
	m_seekCommand = QString("seek %1 %2").arg(offset).arg(whence);
	m_seekTimer.start();
	return true;
}

/*!
 * \brief Toggles full-screen mode
 */
void QMPwidget::toggleFullScreen()
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
void QMPwidget::writeCommand(const QString &command)
{
	m_process->writeCommand(command);
}

/*!
 * \brief Mouse double click event handler
 * \details
 * This implementation will toggle full screen and accept the event
 */
void QMPwidget::mouseDoubleClickEvent(QMouseEvent *event)
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
void QMPwidget::keyPressEvent(QKeyEvent *event)
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
			stop();
			break;

		case Qt::Key_Minus:
			writeCommand("audio_delay -0.1");
			break;
		case Qt::Key_Plus:
			writeCommand("audio_delay 0.1");
			break;

		case Qt::Key_Left:
			seek(-10, RelativeSeek);
			break;
		case Qt::Key_Right:
			seek(10, RelativeSeek);
			break;
		case Qt::Key_Down:
			seek(-60, RelativeSeek);
			break;
		case Qt::Key_Up:
			seek(60, RelativeSeek);
			break;
		case Qt::Key_PageDown:
			seek(-600, RelativeSeek);
			break;
		case Qt::Key_PageUp:
			seek(600, RelativeSeek);
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

void QMPwidget::resizeEvent(QResizeEvent *event)
{
	Q_UNUSED(event);
	updateWidgetSize();
}

void QMPwidget::updateWidgetSize()
{
	if (!m_process->m_mediaInfo.size.isNull()) {
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

void QMPwidget::delayedSeek()
{
	if (!m_seekCommand.isEmpty()) {
		writeCommand(m_seekCommand);
		m_seekCommand = QString();
	}
}

void QMPwidget::mpStateChanged(int state)
{
	if (m_slider != NULL && state == PlayingState && m_process->m_mediaInfo.ok) {
		m_slider->setRange(0, m_process->m_mediaInfo.length);
		m_slider->setEnabled(m_process->m_mediaInfo.seekable);
	}

	emit stateChanged(state);
}

void QMPwidget::mpStreamPositionChanged(double position)
{
	if (m_slider != NULL && m_seekCommand.isEmpty() && m_slider->value() != qRound(position)) {
		m_slider->disconnect(this);
		m_slider->setValue(qRound(position));
		connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(seek(int)));
	}
}


#include "qmpwidget.moc"


/* Documentation follows */

/*!
 * \mainpage
 *
 * \htmlonly
 * <img src="logo.png" height="80px" align="right" />
 * \endhtmlonly
 *
 * \section toc Contents
 * \li \ref intro
 * \li \ref usage
 * \li \ref playbackmodes
 * \li \ref shortcuts
 * \li \ref license
 *
 * \section intro Introduction
 *
 * QMPwidget is a small class which allows Qt developers to embed an MPlayer instance into
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
 * \section usage Usage information
 *
 * Please note that in the following, it is assumed you are already using
 * <a href="http://doc.qt.nokia.com/qmake-running.html">QMake</a> to manage your
 * build system.
 *
 * \subsection inclib Including QMPwidget as a library
 *
 * Copy the \p src directory of the source distribution to your project tree. You
 * might want to rename it, e.g. to \p qmpwidget. Afterwards, rename \p src.pro
 * to match your directory name, e.g. \p qmpwidget.pro, and adjust it to your needs
 * (see below for available options). Finally, add the directory to your existing
 * \p SUBDIRS defintion of the parent directory's project file.
 *
 * By default, QMPwidget will result in a single static library located in the parent
 * directory. Therefore, the parts of the program using the widget need to link
 * against it. Furthermore, the directory containing the \p QMPwidget source should be
 * included in the respective \p INCLUDEPATH definitions.
 *
 * \subsection incclass Including QMPwidget as a class
 *
 * \section playbackmodes Video playback modes
 *
 * Normally, embedding of MPlayer is done by attaching the process to an existing window.
 * Unfortunately, this doesn't work on Mac OS X at all, so QMPwidget provides an additional
 * "pipe mode" for running MPlayer on this operating system. Although this mode works on all
 * operating systems, the standard mode should perform significantly better in terms of
 * CPU usage and audio / video synchronization.
 *
 * The pipe mode is included if the QMake configuration variable \p pipemode is set.
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
 * \class QMPwidget
 * \brief A Qt widget for embedding MPlayer
 * \details
 * Please refer to the <a href="index.html">main page</a> for detailed usage information and
 * discussion.
 */

/*!
 * \enum QMPwidget::State
 * \brief MPlayer state
 * \details
 * This enumeration is somewhat identical to <a href="http://doc.trolltech.com/phonon.html#State-enum">
 * Phonon's State enumeration</a>, except that it has an additional
 * member which is used when the MPlayer process has not been started yet (NotStartedState)
 *
 * <table>
 *  <tr><th>Constant</th><th>Value</th><th>Description</th></tr>
 *  <tr>
 *   <td>\p QMPwidget::NotStartedState</td>
 *   <td>\p -1</td>
 *   <td>The Mplayer process has not been started yet or has already terminated.</td>
 *  </tr>
 *  <tr>
 *   <td>\p QMPwidget::LoadingState</td>
 *   <td>\p 0</td>
 *   <td>The MPlayer process has just been started, but playback has not been started yet.</td>
 *  </tr>
 *  <tr>
 *   <td>\p QMPwidget::StoppedState</td>
 *   <td>\p 1</td>
 *   <td></td>
 *  </tr>
 *  <tr>
 *   <td>\p QMPwidget::PlayingState</td>
 *   <td>\p 2</td>
 *   <td></td>
 *  </tr>
 *  <tr>
 *   <td>\p QMPwidget::BufferingState</td>
 *   <td>\p 3</td>
 *   <td></td>
 *  </tr>
 *  <tr>
 *   <td>\p QMPwidget::PausedState</td>
 *   <td>\p 4</td>
 *   <td></td>
 *  </tr>
 *  <tr>
 *   <td>\p QMPwidget::ErrorState</td>
 *   <td>\p 5</td>
 *   <td></td>
 *  </tr>
 * </table>
 */

/*!
 * \enum QMPwidget::Mode
 * \brief Video playback mode
 * \details
 * This enumeration describes valid modes for video playback. Please see \ref playbackmodes for a
 * detailed description of both modes.
 *
 * <table>
 *  <tr><th>Constant</th><th>Value</th><th>Description</th></tr>
 *  <tr>
 *   <td>\p QMPwidget::EmbeddedMode</td>
 *   <td>\p 0</td>
 *   <td>MPlayer will render directly into a Qt widget.</td>
 *  </tr>
 *  <tr>
 *   <td>\p QMPwidget::PipedMode</td>
 *   <td>\p 1</td>
 *   <td>MPlayer will write the video data into a FIFO which will be parsed in a seperate thread.\n
 * The frames will be rendered by QMPwidget.</td>
 *  </tr>
 * </table>
 */

/*!
 * \fn void QMPwidget::stateChanged(int state)
 * \brief Emitted if the state has changed
 * \details
 * This signal is emitted when the state of the MPlayer process changes.
 *
 * \param state The new state
 */

/*!
 * \fn void QMPwidget::error(const QString &reason)
 * \brief Emitted if the state has changed to QMPwidget::ErrorState
 * \details
 * This signal is emitted when the state of the MPlayer process changes to QMPwidget::ErrorState.
 *
 * \param reason Textual error description (may be empty)
 */
