#include "commonvalues.h"
#include <syslog.h>
commonvalues::commonvalues(QObject *parent) : QObject(parent)
{

}

int commonvalues::center_count = 0;
QList<CenterInfo> commonvalues::center_list;
QList<QThread *> commonvalues::clientlist;
int commonvalues::ftpretry = 1;
QString commonvalues::FileSearchPath = "/media/data/FTP_Trans";
QString commonvalues::LogPath = "/media/data/Ftplog";
QString commonvalues::BackImagePath = "/media/data/backup";
QString commonvalues::TransferType = "FTP";
int commonvalues::loglevel = LOG_EMERG;
volatile bool commonvalues::socketConn = false;
volatile bool commonvalues::prevSocketConn = false;
