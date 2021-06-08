#ifndef THREADWORKER_H
#define THREADWORKER_H

#include <QObject>
#include <QFtp>
#include <QMutex>
#include "syslogger.h"
#include "dataclass.h"
#include <iostream>
#include <execinfo.h>
#include <QDirIterator>
using namespace std;

class SendFileInfo
{
public:
    SendFileInfo()
    {
        filename = "";
        filepath = "";
    }

    bool SaveFile(QString path, QString fname, QByteArray filedata);
    bool ParseFilepath(QString _filepath); //센터로 보낼 데이터인지 파일명을 체크해서 리턴함.

public:
    QString filename;
    QString filepath;
};

class SendFileInfoList
{
public:
    bool AddFile(SendFileInfo data);
    SendFileInfo GetFirstFile();
    void RemoveFirstFile(SendFileInfo data);
    void ClearAll();
    int Count();
    QList<SendFileInfo> fileList;
private:
    const int MAX_FILE = 20;
    QMutex mutex;

};


class ThreadWorker : public QObject
{
    Q_OBJECT
public:
    explicit ThreadWorker(QObject *parent = nullptr);
    QString FTP_SEND_PATH;
signals:
    void finished();
    void initFtpReq(QString str);  //ftp 초기화 요청
    void logappend(QString logstr);
    void localFileUpdate(SendFileInfo *sendFileInfo);
public slots:
    void doWork();
    //void ftpCommandFinished(int id,bool error);
    //void ftpStatusChanged(int state);
public:

    QFtp *m_pftp;

    int m_iFTPTrans;
    int m_iFTPTransFail;
    int m_iFTPRename;
    int m_iFTPRenameFail;
    int m_RetryInterval;

    Syslogger *log;
    int m_loglevel;
    const int MAXFTPCOUNT = 10;

    SendFileInfoList sendFileList;
    bool thread_run;
    QFtp::State last_state;
    QDateTime lastHostlookup;
    CenterInfo config;
    void ScanSendDataFiles();

    void CancelConnection();
    void InitConnection();
    void Connect2FTP();
    void SetConfig(CenterInfo configinfo,QFtp *pFtp);
    void DeleteFile(QString filepath);
    void CopyFile(SendFileInfo data);
    void RefreshLocalFileList();
    bool isLegalFileName(QString filename);  //파일 이름이 유효한지에 대한 첵크 이상이 있으면 삭제 한다.

};

#endif // THREADWORKER_H
