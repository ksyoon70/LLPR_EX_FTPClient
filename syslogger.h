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

class Syslogger : public QObject
{
    Q_OBJECT

public:
    explicit Syslogger(QObject *parent = 0);
    explicit Syslogger(QObject *parent,QString ident,bool showDate);
    explicit Syslogger(QObject *parent,QString ident,bool showDate,int loglevel);
    ~Syslogger();
    void write(QString value,int loglevel = LOG_DEBUG);


public:
    bool m_showDate;
    //로그레벨보다 높으면 저장함.
    int m_loglevel;
    int m_limitwritetime;

private:
    QString m_ident;

signals:

};

#endif // SYSLOGGER_H
