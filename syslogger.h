#ifndef SYSLOGGER_H
#define SYSLOGGER_H

#include <QObject>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QDateTime>
#include <QTextStream>
#include <QDebug>
#include <syslog.h>
#include <QMutex>
#define MAX_PATH       (256)
class Syslogger : public QObject
{
    Q_OBJECT

public:
    explicit Syslogger(QObject *parent = 0);
    explicit Syslogger(QObject *parent,QString ident,bool showDate);
    explicit Syslogger(QObject *parent,QString ident,bool showDate,int loglevel);
    explicit Syslogger(QObject *parent,QString ident,bool showDate,int loglevel, QString base_dir, bool saveLog);
    ~Syslogger();
    void write(QString value,int loglevel = LOG_DEBUG);
    void Write_LogFile(const char *format, ...);
    void GetCurrentDate(char *return_Date);
    void GetCurrentDateTime(char *return_Date);
    void GetCurrentHour(char *return_Date);


public:
    bool m_showDate;
    //로그레벨보다 높으면 저장함.
    int m_loglevel;
    int m_limitwritetime;

private:
    QString m_ident;
    QString base_path;          //로그를 저장하는 기본 폴더
    bool m_saveLog;

signals:

};

#endif // SYSLOGGER_H
