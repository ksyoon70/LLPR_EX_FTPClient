#include "syslogger.h"


Syslogger::Syslogger(QObject *parent) :
    QObject(parent)
{
    m_showDate = true;
    m_ident = "logfile";
    m_loglevel=LOG_ERR;
    m_limitwritetime = 10;
}

Syslogger::Syslogger(QObject *parent,QString ident,bool showDate) :
    QObject(parent)
{
    m_showDate = showDate;
    m_ident = ident;
    m_loglevel=LOG_ERR;
    m_limitwritetime = 10;
}
Syslogger::Syslogger(QObject *parent,QString ident,bool showDate,int loglevel) :
    QObject(parent)
{
    m_showDate = showDate;
    m_ident = ident;
    m_loglevel=loglevel;
    m_limitwritetime = 10;
}

Syslogger::~Syslogger()
{

}

void Syslogger::write(QString value,int loglevel)
{
    if( loglevel <= m_loglevel)
    {
        QString text;
        QDateTime pretime = QDateTime::currentDateTime();

        if(m_showDate)
        {
            text = QString("%1 %2|%3").arg(pretime.toString("[HH:mm:ss:zzz]")).arg(m_ident).arg(value);

        }
        else
        {
            text = QString("%1|%2").arg(m_ident).arg(value);
        }
        syslog(loglevel,"%s",text.toUtf8().constData());

        QDateTime aftertime = QDateTime::currentDateTime();
        qint64 diffrecogtime = pretime.msecsTo(aftertime);
        if( diffrecogtime > m_limitwritetime )
        {
            QString errorlog;
            if(m_showDate)
            {
                errorlog = QString("%1 %2 | write limit over(%3ms)")
                    .arg(aftertime.toString("[ss:zzz]")).arg(m_ident).arg(diffrecogtime);
            }
            else errorlog = QString("%1|write limit over(%2ms)").arg(m_ident).arg(diffrecogtime);

            syslog(loglevel,errorlog.toUtf8().constData());
            qDebug() << errorlog;
        }

    }

}
