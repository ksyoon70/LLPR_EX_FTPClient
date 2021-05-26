#ifndef DELETEWORKER_H
#define DELETEWORKER_H

#include <QObject>
#include <QWaitCondition>
#include <QMutex>
#include <QFileInfo>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QTime>
#include <QDate>
#include <QDateTime>
#include <QThread>
#include <dataclass.h>
#include <syslogger.h>
#include "commonvalues.h"
#include <iostream>
#include <execinfo.h>
using namespace std;
class DeleteWorker : public QObject
{
    Q_OBJECT
public:
    explicit DeleteWorker(QObject *parent = nullptr);
    void SetConfig(CenterInfo * configinfo);
signals:
    void finished();
public slots:
    void doWork();
    void doRun();
    void Stop();  //모든작업을 중지한다.
private:
    void DeleteDir(QString filepath);
    bool removeDir(const QString & dirName);  //QT 4에서 필요시

    volatile bool thread_run;
    QWaitCondition m_Condition;
    QMutex m_mutex;
    CenterInfo* mp_cinfo;
    int m_daysToStore;  //파일 저장 기간
    Syslogger *log;

};

#endif // DELETEWORKER_H
