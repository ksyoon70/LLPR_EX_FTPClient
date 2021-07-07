#ifndef COMMONVALUES_H
#define COMMONVALUES_H

#include <QObject>
#include <QLabel>
#include "dataclass.h"

class commonvalues : public QObject
{
    Q_OBJECT
public:
    explicit commonvalues(QObject *parent = nullptr);

    /*        Center  value            */
       static int center_count;
       static QList<CenterInfo> center_list;
       static QList<QThread *> clientlist;
       enum FTPRetryType
       {
           FTPRetry_ON = 0x00,
           FTPRetry_OFF = 0x01
       };
       static int ftpretry;
       static QString FileSearchPath;
       static QString LogPath;
       static QString BackImagePath;
       static QString TransferType;

       static volatile bool socketConn;
       static volatile bool prevSocketConn;     //이전 소켓 상태

       static int loglevel;
};

#endif // COMMONVALUES_H
