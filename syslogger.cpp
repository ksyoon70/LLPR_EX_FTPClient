#include "syslogger.h"

QMutex mutex;
void Syslogger::GetCurrentDate(char *return_Date)
{
  if(return_Date == NULL)
    return;

  // 현재 날짜와 시간을 이용하여 String을 생성한다.
  time_t  now_time;
  struct tm *today;

  time(&now_time);
  today = localtime(&now_time);
  strftime(return_Date, 10, "%Y%m%d", today);
}

// 현재 날짜에 대한 YYYYMMDD-HHmmSS 결과를 전달한다.
void Syslogger::GetCurrentDateTime(char *return_Date)
{
  if(return_Date == NULL)
    return;

  // 현재 날짜와 시간을 이용하여 String을 생성한다.
  time_t  now_time;
  struct tm *today;

  time(&now_time);
  today = localtime(&now_time);
  strftime(return_Date, 20, "%Y%m%d-%H%M%S", today);
}

void Syslogger::GetCurrentHour(char *return_Date)
{
    if(return_Date == NULL)
        return;

    // 현재 날짜와 시간을 이용하여 String을 생성한다.
    time_t  now_time;
    struct tm *today;

    time(&now_time);
    today = localtime(&now_time);
    strftime(return_Date, 20, "%H", today);
}

void Syslogger::Write_LogFile(const char *format, ...) {

    char TempPath[200];
    char FullPath[MAX_PATH];
     //----------------------------------------------------------------------------
     char TempDate[20];
     char TempHour[20];
     char szTimeStamp[50];

     FILE *openLogFile;
     va_list var_args;

     time_t  now_time;
     struct tm *today;

     time(&now_time);
     today = localtime(&now_time);
     strftime(szTimeStamp, 30, "%Y-%m-%d %H:%M:%S", today);

     GetCurrentDate(TempDate);
     GetCurrentHour(TempHour);

     mutex.lock();

     if(QString::compare(base_path, commonvalues::LogPath))
     {
         base_path = commonvalues::LogPath;
     }
     sprintf(TempPath,"%s/%s/%s", base_path.toUtf8().constData(),TempDate,TempHour);
     QDir mdir(TempPath);
     if(!mdir.exists())
     {
         mdir.mkpath(TempPath);
     }

     sprintf(FullPath, "%s/log_%s.log",TempPath,TempDate);

     if( (openLogFile = fopen(FullPath, "ab")) == NULL ) {
         mutex.unlock();
          return;
       }

     va_start( var_args, format);     // Initialize variable arguments.

     //fprintf( openLogFile, "[%s] ", szTimeStamp);
     vfprintf( openLogFile, format, var_args);
     fprintf( openLogFile, "\r\n");

     va_end(var_args);              // Reset variable arguments.

     // 3. open한 파일을 close.
     fclose(openLogFile);

     mutex.unlock();

}

Syslogger::Syslogger(QObject *parent) :
    QObject(parent)
{
    m_showDate = true;
    m_ident = "logfile";
    m_loglevel=LOG_ERR;
    m_limitwritetime = 10;
    m_saveLog = true;
}

Syslogger::Syslogger(QObject *parent,QString ident,bool showDate) :
    QObject(parent)
{
    m_showDate = showDate;
    m_ident = ident;
    m_loglevel=LOG_ERR;
    m_limitwritetime = 10;
    m_saveLog = true;
}
Syslogger::Syslogger(QObject *parent,QString ident,bool showDate,int loglevel) :
    QObject(parent)
{
    m_showDate = showDate;
    m_ident = ident;
    m_loglevel=loglevel;
    m_limitwritetime = 10;
    m_saveLog = true;
}

Syslogger::Syslogger(QObject *parent,QString ident,bool showDate,int loglevel, QString base_dir, bool saveLog) :
    QObject(parent)
{
    m_showDate = showDate;
    m_ident = ident;
    m_loglevel=loglevel;
    m_limitwritetime = 10;
    base_path = base_dir;
    m_saveLog = saveLog;
}

Syslogger::~Syslogger()
{

}

void Syslogger::write(QString value,int loglevel)
{

    QString text;
    QDateTime pretime = QDateTime::currentDateTime();

    if(m_showDate)
    {
        text = QString("%1 %2").arg(pretime.toString("[HH:mm:ss:zzz]")).arg(value);

    }
    else
    {
        text = QString("%1").arg(value);
    }
    //syslog(loglevel,"%s",text.toUtf8().constData());

    if(m_saveLog)
    {
        Write_LogFile("%s",text.toUtf8().constData());
    }

}
