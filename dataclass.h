#ifndef DATACLASS_H
#define DATACLASS_H
#include <QObject>
#include <QLabel>

class CenterInfo
{
public:
    CenterInfo()
    {
        tcpport = 1944;
        connectioninterval = 1800;
        ftpport = 21;
        fileNameSelect = 0;
        protocol_type = 0;
    }
    QString centername;
    QString ip;
    int tcpport;
    int connectioninterval;

    enum Protocoltype
    {
        Normal = 0x00,      //TCP, FTP 모두 사용
        Remote = 0x01,      //TCP만 사용함. 프로토콜 다름
        FTP_Only = 0x02     //FTP만 사용함.
    };
    int protocol_type;

    int ftpport;
    QString userID;
    QString password;
    QString ftpPath;
    enum FileNameType
    {
        H_Char = 0x00,
        X_Char = 0x01
    };
    int fileNameSelect; //0=H , 1-X

    bool status;
    QLabel *plblstatus;
};


#endif // DATACLASS_H
