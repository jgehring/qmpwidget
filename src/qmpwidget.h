/*
 *  qmpwidget - A Qt widget for embedding MPlayer
 *  Copyright (C) 2010 by Jonas Gehring
 */


#ifndef QMPWIDGET_H_
#define QMPWIDGET_H_


#include <QWidget>

class QProcess;
class QStringList;


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

	public:
		QMPWidget(QWidget *parent = 0);
		~QMPWidget();

		State state() const;

	public slots:
		void start(const QStringList &args);
		void play();
		void pause();
		void stop();

		void writeCommand(const QString &command);

	signals:
		void stateChanged(int state);
		void error(const QString &reason);

	private slots:
		void readStdout();
		void readStderr();

	private:
		void changeState(State state, const QString &comment = QString());
		void parseLine(const QString &line);

	private:
		QProcess *m_process;
		int m_state;
};


#endif // QMPWIDGET_H_
