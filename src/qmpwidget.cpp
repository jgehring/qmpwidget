/*
 *  qmpwidget - A Qt widget for embedding MPlayer
 *  Copyright (C) 2010 by Jonas Gehring
 */


#include <QKeyEvent>
#include <QProcess>
#include <QStringList>
#include <QtDebug>

#include "qmpwidget.h"


// Constructor
QMPWidget::QMPWidget(QWidget *parent)
	: QWidget(parent), m_state(NotStartedState)
{
	setFocusPolicy(Qt::StrongFocus);

	m_process = new QProcess(this);
	connect(m_process, SIGNAL(readyReadStandardOutput()), this, SLOT(readStdout()));
	connect(m_process, SIGNAL(readyReadStandardError()), this, SLOT(readStderr()));
}

// Destructor
QMPWidget::~QMPWidget()
{
	if (m_process->state() == QProcess::Running) {
		writeCommand("quit");
		m_process->waitForFinished(-1);
	}
	delete m_process;
}

QMPWidget::State QMPWidget::state() const
{
	return (State)m_state;
}

// Starts the playback with the given arguments
void QMPWidget::start(const QStringList &args)
{
	if (m_process->state() == QProcess::Running) {
		writeCommand("quit");
		m_process->waitForFinished(-1);
	}

	changeState(LoadingState);
	QStringList myargs;
	myargs += "-slave";
	myargs += "-noquiet";
	myargs += "-nomouseinput";
	myargs += "-nokeepaspect";
	myargs += "-wid";
	myargs += QString::number(winId());
	myargs += "-input";
	myargs += "nodefault-bindings:conf=/dev/null";
	myargs += args;
	m_process->start("mplayer", myargs);
}

void QMPWidget::play()
{
	if (m_state == PausedState) {
		writeCommand("pause");
	}
}

void QMPWidget::pause()
{
	if (m_state == PlayingState) {
		writeCommand("pause");
	}
}

void QMPWidget::stop()
{
	writeCommand("stop");
}

// Writes a command to the MPlayer input
void QMPWidget::writeCommand(const QString &command)
{
	m_process->write(command.toLocal8Bit()+"\n");
}

// Reads output from MPlayer
void QMPWidget::readStdout()
{
	QStringList lines = QString::fromLocal8Bit(m_process->readAllStandardOutput()).split("\n", QString::SkipEmptyParts);
	for (int i = 0; i < lines.count(); i++) {
		parseLine(lines[i]);
	}
}

// Reads output from MPlayer
void QMPWidget::readStderr()
{
	QStringList lines = QString::fromLocal8Bit(m_process->readAllStandardError()).split("\n", QString::SkipEmptyParts);
	for (int i = 0; i < lines.count(); i++) {
		parseLine(lines[i]);
	}
}

// Changes the current state, possibly emitting multiple signals
void QMPWidget::changeState(State state, const QString &comment)
{
	m_state = state;
	emit stateChanged(m_state);

	if (m_state == ErrorState) {
		emit error(comment);
	}
}

// Parses a line of MPlayer output
void QMPWidget::parseLine(const QString &line)
{
	if (line.startsWith("Playing ")) {
		changeState(LoadingState);
	} else if (line.startsWith("Starting playback...")) {
		changeState(PlayingState);
	} else if (line.startsWith("File not found: ")) {
		changeState(ErrorState);
	} else if (line == "Exiting... (Quit)") {
		changeState(NotStartedState);
	}
}
