#ifndef SFTPTHRWORKER_H
#define SFTPTHRWORKER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QDirIterator>
#include <iostream>
#include <execinfo.h>

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
    explicit SftpThrWorker(QObject *parent = nullptr);
    QString SFTP_SEND_PATH;

signals:
    void finished();
    void logappend(QString logstr);
    void localFileUpdate(SendFileInfo *sendFileInfo);
    void remoteFileUpdate(QString rfname, QString rfsize, QDateTime rftime, bool isDir);

public slots:
    void doWork();

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
    int sock;
    FILE *local;

    bool thread_run;
    LIBSSH2_SESSION *session;
    LIBSSH2_SFTP *sftp_session;
    LIBSSH2_SFTP_HANDLE *sftp_handle;

    QString logstr;
    QString noCharFileName;

    SendFileInfo sftp_SendFileInfo;
    SendFileInfoList sftp_SendFileInfoList;

    bool sshInit();
    bool connectSocket();
    bool initSession();
    bool initSFTP();
    bool openSFTP();
    bool sendData();

    void closeSFTP();
    void closeSession();

    void CopyFile(SendFileInfo data);
    void RefreshLocalFileList();
    void ScanSendDataFiles();
    int GetFirstNumPosFromFileName(QString filename);
    bool isLegalFileName(QString filename);
    void remoteConnect();
};

#endif // SFTPTHRWORKER_H
