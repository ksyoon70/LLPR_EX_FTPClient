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


class SftpThrWorker : public QObject
{
    Q_OBJECT
public:
    explicit SftpThrWorker(QString host, qint32 port, QObject *parent = nullptr);
    QString SFTP_SEND_PATH;

signals:
    void finished();
    void logappend(QString logstr);
    void localFileUpdate(SendFileInfo *sendFileInfo);
    void remoteFileUpdate(QString rfname, QString rfsize, QDateTime rftime, bool isDir);
    void transferProgress(qint64 bytesSent, qint64 bytesTotal);
    void sftpput(QString local, QString remote);

public slots:
    void doWork();
    void disconnected();
    void connected();

public:
    Syslogger *log;
    CenterInfo config;
    int m_iSFTPRename;
    int m_RetryInterval;
    const int MAXSFTPCOUNT = 10;

    const char *username;
    const char *password;
    const char *localfile;
    const char *sftppath;
    unsigned long hostaddr;
    QTcpSocket *m_socket;
    bool fd_flag;
    int sock;
    FILE *fp;
    int m_Auth_pw;

    bool thread_run;
    LIBSSH2_SESSION volatile **mpp_session;
    LIBSSH2_SFTP volatile **mpp_sftp_session;
    LIBSSH2_SFTP_HANDLE volatile *sftp_handle;

    QString logstr;
    QString noCharFileName;

    SendFileInfo sftp_SendFileInfo;
    SendFileInfoList sftp_SendFileInfoList;

    bool sshInit();
    //bool connectSocket();
    //bool connectToHost(QString host, qint32 port);
    //bool initSession();
    //bool initSFTP();
    bool openSFTP();
    bool sendData(int size);

    void closeSFTP();
    void closeSession();
    void shutdown();

    void CopyFile(SendFileInfo data);
    void RefreshLocalFileList();
    void ScanSendDataFiles();
    int GetFirstNumPosFromFileName(QString filename);
    bool isLegalFileName(QString filename);
    void remoteConnect();
    bool connectSocket(QString host, qint32 port);
    QString host;
    qint32 port;
};

#endif // SFTPTHRWORKER_H
