#ifndef SFTPTHRWORKER_H
#define SFTPTHRWORKER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QDirIterator>
#include <iostream>
#include <execinfo.h>
#include <QTcpSocket>

#include <libssh2.h>
#include <libssh2_sftp.h>

#include "syslogger.h"
#include "dataclass.h"
#include "threadworker.h"
using namespace std;

#define FTP_BUFFER (3145728)

class SftpThrWorker : public QObject
{
    Q_OBJECT
public:
    explicit SftpThrWorker(QString host, qint32 port, QObject *parent = nullptr);
     ~SftpThrWorker();
    QString SFTP_SEND_PATH;

signals:
    void finished();
    void logappend(QString logstr);
    void localFileUpdate(SendFileInfo *sendFileInfo);
    void remoteFileUpdate(QString rfname, QString rfsize, QDateTime rftime, bool isDir);
    void transferProgress(qint64 bytesSent, qint64 bytesTotal);

public slots:
    void doWork();
    bool SftpShutdown();


public:
    Syslogger *plog;
    CenterInfo config;
    const int MAXSFTPCOUNT = 10;

    const char *username;
    const char *password;
    const char *localfile;
    const char *sftppath;
    unsigned long hostaddr;
    QTcpSocket *m_socket;
    bool fd_flag;
    int sock;
    int m_Auth_pw;

    bool thread_run;
    LIBSSH2_SESSION volatile *mp_session;
    LIBSSH2_SFTP volatile *mp_sftp_session;
    LIBSSH2_SFTP_HANDLE volatile *mp_sftp_handle;

    QString logstr;
    QString noCharFileName;

    SendFileInfo sftp_SendFileInfo;
    SendFileInfoList sftp_SendFileInfoList;

    bool sshInit();
    bool Sftp_Init_Session();
    void CopyFile(SendFileInfo data);
    void RefreshLocalFileList();
    void ScanSendDataFiles();
    int GetFirstNumPosFromFileName(QString filename);
    bool isLegalFileName(QString filename);
    bool connectSocket(QString host, qint32 port);
    bool CloseSocket();
    bool sftpput(QString local, QString remote);
    void SetUpDateRemoteDir();
    void remoteConnect();

    QString host;
    qint32 port;
    char *m_lpBuffer;
    volatile bool m_updateRemoteDir;
    QDateTime startTick;
    QDateTime endTick;

};

#endif // SFTPTHRWORKER_H
