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
quint32 commonvalues::DaysToStore = 10;
QString commonvalues::BackImagePath = "/media/data/backup";
QString commonvalues::TransferType = "FTP";
int commonvalues::loglevel = LOG_EMERG;
volatile bool commonvalues::socketConn = false;
volatile bool commonvalues::prevSocketConn = false;

/*      SSD 마운트 관련 변수       */
bool commonvalues::bSSDMount = true;
QString commonvalues::tFileSearchPath = "";
QString commonvalues::tLogPath = "";
quint32 commonvalues::tDaysToStore = 10;

QString commonvalues::SSD_DeviceInfo = "/dev/sda1";
QString commonvalues::SSD_Path = "/media/data";

QString commonvalues::curFileSearchPath = "/media/data/FTP_Trans";
QString commonvalues::curLogPath = "/media/data/Ftplog";
quint32 commonvalues::curDaysToStore = 10;

QLabel* commonvalues::pMountIcon;
