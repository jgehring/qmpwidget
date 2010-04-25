/*
 *  qmpwidget - A Qt widget for embedding MPlayer
 *  Copyright (C) 2010 by Jonas Gehring
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
			: QProcess(parent), m_state(QMPWidget::NotStartedState), m_mplayerPath("mplayer")
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

		// Parses a line of MPlayer output
		void parseLine(const QString &line)
		{
			if (line.startsWith("Playing ")) {
				changeState(QMPWidget::LoadingState);
			} else if (line.startsWith("Starting playback...")) {
				changeState(QMPWidget::PlayingState);
			} else if (line.startsWith("File not found: ")) {
				changeState(QMPWidget::ErrorState);
			} else if (line == "Exiting... (Quit)") {
				changeState(QMPWidget::NotStartedState);
			}
		}

	signals:
		void stateChanged(int state);
		void error(const QString &reason);

	private slots:
		void readStdout()
		{
			QStringList lines = QString::fromLocal8Bit(readAllStandardOutput()).split("\n", QString::SkipEmptyParts);
			for (int i = 0; i < lines.count(); i++) {
				parseLine(lines[i]);
			}
		}

		void readStderr()
		{
			QStringList lines = QString::fromLocal8Bit(readAllStandardOutput()).split("\n", QString::SkipEmptyParts);
			for (int i = 0; i < lines.count(); i++) {
				parseLine(lines[i]);
			}
		}

	private:
		// Changes the current state, possibly emitting multiple signals
		void changeState(QMPWidget::State state, const QString &comment = QString())
		{
			m_state = state;
			emit stateChanged(m_state);

			if (m_state == QMPWidget::ErrorState) {
				emit error(comment);
			}
		}

	public:
		QMPWidget::State m_state;
		QString m_mplayerPath;
};


// Constructor
QMPWidget::QMPWidget(QWidget *parent)
	: QWidget(parent)
{
	setFocusPolicy(Qt::StrongFocus);

	m_process = new QMPProcess(this);
	connect(m_process, SIGNAL(stateChanged(int)), this, SIGNAL(stateChanged(int)));
	connect(m_process, SIGNAL(error(const QString &)), this, SIGNAL(error(const QString &)));
}

// Destructor
QMPWidget::~QMPWidget()
{
	if (m_process->processState() == QProcess::Running) {
		m_process->quit();
	}
	delete m_process;
}

QMPWidget::State QMPWidget::state() const
{
	return m_process->m_state;
}

// Starts the playback with the given arguments
void QMPWidget::start(const QStringList &args)
{
	if (m_process->processState() == QProcess::Running) {
		m_process->quit();
	}

	m_process->startMPlayer(winId(), args);
}

void QMPWidget::play()
{
	if (m_process->m_state == PausedState) {
		m_process->pause();
	}
}

void QMPWidget::pause()
{
	if (m_process->m_state == PlayingState) {
		m_process->pause();
	}
}

void QMPWidget::stop()
{
	m_process->stop();
}

// Writes a command to the MPlayer input
void QMPWidget::writeCommand(const QString &command)
{
	m_process->writeCommand(command);
}


#include "qmpwidget.moc"
