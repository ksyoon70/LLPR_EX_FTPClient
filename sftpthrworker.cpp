#include "sftpthrworker.h"
#include <QDir>
#include <QFile>
#include <QApplication>

#include <libssh2.h>
#include <libssh2_sftp.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <QProcess>

#include "commonvalues.h"
#include "threadworker.h"

SftpThrWorker::SftpThrWorker(QTcpSocket *socket, QObject *parent) : QObject(parent)
{
    SFTP_SEND_PATH = commonvalues::FileSearchPath;
    QDir sdir(SFTP_SEND_PATH);       //sftp serch directory
    if(!sdir.exists())
    {
         sdir.mkpath(SFTP_SEND_PATH);
    }

    m_iSFTPRename = 0;
    m_RetryInterval = 5;

    config = commonvalues::center_list.value(0);

    QString logpath = config.logPath;
    QString dir = logpath;
    QDir mdir(dir);
    if(!mdir.exists())
    {
         mdir.mkpath(dir);
    }

    QString backupPath = config.backupPath;
    QDir backupdir(backupPath);
    if(!backupdir.exists())
    {
         backupdir.mkpath(backupPath);
    }

    m_socket = socket;

    session = nullptr;
    sftp_session = nullptr;
    sftp_handle = nullptr;

    log = new Syslogger(this,"SftpThrWorker",true,commonvalues::loglevel,logpath,config.blogsave);

    thread_run = true;
}

void SftpThrWorker::doWork()
{
    int lastCount = 0;
    bool isRetry = false;
    QDateTime lastTime = QDateTime::currentDateTime().addDays(-1);

    QString lastFilename;
    QString curFilename;

    const char *ipcvt;

    ipcvt = config.ip.trimmed().toLocal8Bit();
    hostaddr = inet_addr(ipcvt);

    username = config.userID.toStdString().c_str();
    password = config.password.toStdString().c_str();
    sftppath = config.ftpPath.toStdString().c_str();

    while(thread_run)
    {
        if(commonvalues::SftpSocketConn == true)
        {
            if(isRetry == true)
            {
                if(sshInit())
                {
                    QThread::msleep(100);
                    if(initSession())
                    {
                        QThread::msleep(100);
                        if(initSFTP())
                        {
                            QThread::msleep(100);
                            remoteConnect();
                            isRetry = false;
                        }
                        else
                        {
                            closeSFTP();
                            logstr = QString("initSFTP fail");
                            emit logappend(logstr);
                            continue;
                        }
                    }
                    else
                    {
                        closeSession();
                        logstr = QString("Session fail");
                        emit logappend(logstr);
                        continue;
                    }
                }
                else
                {
                    logstr = QString("sshInit fail");
                    emit logappend(logstr);
                    continue;
                }
            }
        }
        else
        {
            if(commonvalues::SftpSocketConn == false)
            {
                shutdown();
                isRetry = true;
                continue;
            }
        }

        lastFilename = curFilename;
        ScanSendDataFiles();
        sftp_SendFileInfo = sftp_SendFileInfoList.GetFirstFile();
        curFilename = QString(sftp_SendFileInfo.filename);
        if(QString::compare(lastFilename,curFilename) != 0)
        {
            RefreshLocalFileList();
        }

        if(sftp_SendFileInfoList.Count() == 0)
        {
            ScanSendDataFiles();
            QThread::msleep(500);

            if(sftp_SendFileInfoList.Count() == 0)
                continue;
        }
        else if(sftp_SendFileInfoList.Count() > 0)
        {
            if(sftp_SendFileInfo.filename.isNull() || sftp_SendFileInfo.filename.isEmpty())
            {
                QThread::msleep(50);
                continue;
            }

            if(isRetry)
            {
                if(lastTime.secsTo(QDateTime::currentDateTime()) < m_RetryInterval)
                {
                    QThread::msleep(10);
                    continue;
                }
            }

            QByteArray cvt;
            QString fpath = QString("%1/%2/").arg(SFTP_SEND_PATH).arg(config.centername);

            lastTime = QDateTime::currentDateTime();
            QFile file(sftp_SendFileInfo.filepath);
            if(!file.open(QIODevice::ReadOnly))
            {
                continue;
            }

            //파일 싸이즈가 0이면 삭제한다.
            if(file.size() == 0)
            {
                // 2초간 기다려본다.
                QThread::msleep(2000);
                try {

                    file.close();
                    logstr = QString("SFTP : 파일 크기 에러(0)-->삭제: %1").arg(sftp_SendFileInfo.filename);
                    log->write(logstr,LOG_NOTICE); //qDebug() << logstr;
                    emit logappend(logstr);
                    sftp_SendFileInfoList.RemoveFirstFile(sftp_SendFileInfo);
                }
                catch (exception ex)
                {
                    #ifdef QT_QML_DEBUG
                    qDebug() << ex.what();
                    #endif
                    logstr = QString("SFTP : 파일 크기 에러(0)-->삭제: 예외처리 %1 %2 %3").arg(__FILE__).arg(__LINE__).arg(ex.what());
                    log->write(logstr,LOG_ERR);
                }
                catch (...) {

                    logstr = QString("SFTP : 파일 크기 에러(0)-->삭제: 예외처리 %1 %2").arg(__FILE__).arg(__LINE__);
                    log->write(logstr,LOG_ERR);

                }
                continue;
            }

            int pos = GetFirstNumPosFromFileName(sftp_SendFileInfo.filename);

            if(pos == 0)
            {
                noCharFileName = sftp_SendFileInfo.filename;
            }
            else
            {
                noCharFileName = sftp_SendFileInfo.filename.mid(pos);
            }

            fpath.append(sftp_SendFileInfo.filename);
            cvt = fpath.toLocal8Bit();
            localfile = cvt.data();

            local = fopen(localfile, "rb");

            if(!isLegalFileName(sftp_SendFileInfo.filename))
            {
                try
                {
                    file.close();
                    logstr = QString("파일이름 이상-->삭제: %1").arg(sftp_SendFileInfo.filename);
                    log->write(logstr, LOG_NOTICE);
                    emit logappend(logstr);
                    sftp_SendFileInfoList.RemoveFirstFile(sftp_SendFileInfo);
                }
                catch(exception ex)
                {
                    logstr = QString("파일이름 이상-->삭제: 예외처리 %1 %2 %3").arg(__FILE__).arg(__LINE__).arg(ex.what());
                    log->write(logstr,LOG_ERR);
                }
                catch (...)
                {
                    logstr = QString("파일이름 이상-->삭제: 예외처리 %1 %2").arg(__FILE__).arg(__LINE__);
                    log->write(logstr,LOG_ERR);
                }

                QThread::msleep(100);
                continue;
            }

            if(!local)
            {
                logstr = QString("SFTP : 파일 열기 실패..");
                continue;
            }
            else
            {
                logstr = QString("SFTP : 파일 열기 성공!!");
            }

            log->write(logstr,LOG_NOTICE);
            emit logappend(logstr);

            if(openSFTP() && commonvalues::SftpSocketConn == true)
            {
                if(sendData())
                {
                    if(sftp_session != NULL && sftp_handle != NULL)
                    {
                        LIBSSH2_SFTP_ATTRIBUTES attrs;
                        if(libssh2_sftp_fstat_ex((LIBSSH2_SFTP_HANDLE *)sftp_handle, &attrs, 0) < 0)
                        {
                            logstr = QString("SFTP : libssh2_sftp_fstat fail..");
                            log->write(logstr, LOG_NOTICE);
                            emit logappend(logstr);
                        }
                        else
                        {
                            libssh2_sftp_seek64((LIBSSH2_SFTP_HANDLE *)sftp_handle, attrs.filesize);
                        }

                        closeSFTP();

                        if(attrs.filesize == file.size())
                        {
                            QThread::msleep(10);

                            QByteArray srcdata = noCharFileName.toLocal8Bit();
                            QByteArray dstdata = sftp_SendFileInfo.filename.toLocal8Bit();

                            const char* srcname = srcdata.data();
                            const char* dstname = dstdata.data();

                            m_iSFTPRename = libssh2_sftp_rename((LIBSSH2_SFTP *)sftp_session, srcname, dstname);

                            if(m_iSFTPRename == 0)
                            {
                                logstr = QString("SFTP 파일 이름변경 성공!! : %1 -> %2").arg(noCharFileName).arg(sftp_SendFileInfo.filename);
                                log->write(logstr,LOG_NOTICE);
                                emit logappend(logstr);

                                CenterInfo config = commonvalues::center_list.value(0);
                                if(config.bimagebackup)
                                {
                                    //이미지를 백업하는 옵션이 있으면...
                                    CopyFile(sftp_SendFileInfo);
                                }
                            }
                            else
                            {
                                logstr = QString("SFTP 파일 이름변경 실패.. : %1 -> %2").arg(noCharFileName).arg(sftp_SendFileInfo.filename);
                                log->write(logstr,LOG_NOTICE);
                                emit logappend(logstr);

                                // 동일 파일 삭제
                                cvt = noCharFileName.toLocal8Bit();
                                const char* delfilename = cvt.data();

                                libssh2_sftp_unlink((LIBSSH2_SFTP *)sftp_session, delfilename);

                                logstr = QString("SFTP 동일 파일 삭제 : %1").arg(noCharFileName);
                                log->write(logstr,LOG_NOTICE);
                                emit logappend(logstr);
                            }

                            file.close();
                        }
                        else
                        {
                            QThread::msleep(1000);
                            continue;
                        }
                    }

                    sftp_SendFileInfoList.RemoveFirstFile(sftp_SendFileInfo);

                    if(lastCount != sftp_SendFileInfoList.Count())
                    {
                        lastCount = sftp_SendFileInfoList.Count();
                        logstr = QString("SFTP 전송 파일 리스트 : %1").arg(lastCount);
                        log->write(logstr, LOG_NOTICE);
                        emit logappend(logstr);

                        QThread::msleep(500);
                    }

                    isRetry = false;
                }
                else
                {
                    isRetry = true;
                    emit remoteFileUpdate(NULL, NULL, QDateTime::currentDateTime(), false);
                }
            }
            else
            {
                isRetry = true;
                emit remoteFileUpdate(NULL, NULL, QDateTime::currentDateTime(), false);
            }
        }
    }
}
bool SftpThrWorker::sshInit()
{
    bool sshSuc = true;
    int sshinit = 0;

    sshinit = libssh2_init(0);
    if(sshinit != 0)
    {
        logstr = QString("SFTP : libssh2 초기화 실패..");
        sshSuc = false;
    }
    else
    {
        logstr = QString("SFTP : libssh2 초기화 성공!!");
        sshSuc = true;
    }

    log->write(logstr,LOG_NOTICE);
    emit logappend(logstr);

    return sshSuc;
}

bool SftpThrWorker::initSession()
{
    bool sessionSuc = true;
    int sessioninit = 0;
    int auth_pw = 1;
    sock = m_socket->socketDescriptor();

    session = libssh2_session_init();
    if(!session)
    {
        return false;
    }

    libssh2_session_set_blocking((LIBSSH2_SESSION *)session, 1);
    sessioninit = libssh2_session_handshake((LIBSSH2_SESSION *)session, sock); //m_socket->socketDescriptor());
    QThread::msleep(100);

    if(sessioninit == LIBSSH2_ERROR_EAGAIN)
    {
        logstr = QString("SFTP : SSH Session 설정 실패..");
        emit logappend(logstr);
        return false;
    }
    else
    {
        logstr = QString("SFTP : SSH Session 설정 성공!!");
        sessionSuc = true;
    }

    log->write(logstr,LOG_NOTICE);
    emit logappend(logstr);

    libssh2_hostkey_hash((LIBSSH2_SESSION *)session, LIBSSH2_HOSTKEY_HASH_SHA1);

    if(auth_pw)
    {        
        QThread::msleep(100);
        if(libssh2_userauth_password((LIBSSH2_SESSION *)session, username, password))
        {
            logstr = QString("SFTP : ID/PW 인증 실패..");

            return false;
        }
        else
        {
            logstr = QString("SFTP : ID/PW 인증 성공!!");
            sessionSuc = true;
        }

        log->write(logstr,LOG_NOTICE);
        emit logappend(logstr);
    }

    return sessionSuc;
}

bool SftpThrWorker::initSFTP()
{
    sftp_session = libssh2_sftp_init((LIBSSH2_SESSION *)session);

    logstr = QString("SFTP : SFTP 초기화!!");
    log->write(logstr,LOG_NOTICE);
    emit logappend(logstr);

    return true;
}

bool SftpThrWorker::openSFTP()
{
    bool sftpSuc = false;

    QString Remotefilepath = QString("%1/%2").arg(config.ftpPath).arg(noCharFileName);
    QByteArray cvt;
    cvt = Remotefilepath.toLocal8Bit();
    const char* _sftppath = cvt.data();

    closeSFTP();

    if(commonvalues::SftpSocketConn == true)
    {
        sftp_handle = libssh2_sftp_open((LIBSSH2_SFTP *)sftp_session, _sftppath,
                                        LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC,
                                        LIBSSH2_SFTP_S_IRUSR|LIBSSH2_SFTP_S_IWUSR|
                                        LIBSSH2_SFTP_S_IRGRP|LIBSSH2_SFTP_S_IROTH);

        if(!sftp_handle)
        {
            logstr = QString("SFTP : SFTP 연결 실패!!");
            closeSFTP();

            sftpSuc = false;
        }
        else
        {
            logstr = QString("SFTP : SFTP 연결 성공!!");

            sftpSuc = true;
        }
    }

    log->write(logstr,LOG_NOTICE);
    emit logappend(logstr);

    return sftpSuc;
}

bool SftpThrWorker::sendData()
{
    bool sendSuc = false;
    char mem[2048*1536];
    int rc;
    int nByteRead;
    int nByteWrite;

    // 파일 전송
    try
    {
        nByteRead = fread(mem, 1, sizeof(mem), local);
        nByteWrite = nByteRead;

        do
        {
            if(sftp_handle)
            {
                // write data in a loop until we block
                rc = libssh2_sftp_write((LIBSSH2_SFTP_HANDLE *)sftp_handle, mem, nByteWrite);
                QThread::msleep(100);

                if(rc<0)
                {
                    logstr = QString("SFTP : sftp_write error (%1)!!").arg(rc);
                    logappend(logstr);
                    sendSuc = false;

                    return sendSuc;
                }
                else
                {
                    //emit transferProgress(nByteRead - nByteWrite, nByteRead);
                    nByteWrite -= rc;
                }
                QThread::msleep(10);
            }
            else
            {
                logstr = QString("SFTP : sftp_handle error!!");
                logappend(logstr);
                sendSuc = false;
            }

        }while(nByteWrite);

        if(nByteWrite == 0)
        {
            logstr = QString("SFTP : %1 전송 성공!!").arg(sftp_SendFileInfo.filename);
            logappend(logstr);
            sendSuc = true;
        }
        else
        {
            logstr = QString("SFTP : %1 전송 실패..").arg(sftp_SendFileInfo.filename);
            logappend(logstr);
            sendSuc = false;
        }

    }
    catch(exception ex)
    {
        logstr = QString("SFTP 파일 이상 : 예외처리 %1 %2 %3").arg(__FILE__).arg(__LINE__).arg(ex.what());
        log->write(logstr,LOG_ERR);
        sendSuc = false;
    }
    catch (...)
    {
        logstr = QString("SFTP 파일 이상 : 예외처리 %1 %2").arg(__FILE__).arg(__LINE__);
        log->write(logstr,LOG_ERR);
        sendSuc = false;
    }


    return sendSuc;
}

void SftpThrWorker::closeSFTP()
{
    if(sftp_handle != nullptr)
    {
        libssh2_sftp_close((LIBSSH2_SFTP_HANDLE *)sftp_handle);
        libssh2_sftp_shutdown((LIBSSH2_SFTP *)sftp_session);
    }

    sftp_handle = nullptr;
}
void SftpThrWorker::closeSession()
{
    if(session != nullptr)
    {
        libssh2_session_disconnect((LIBSSH2_SESSION *)session, "Normal Shutdown");
        libssh2_session_free((LIBSSH2_SESSION *)session);
    }

    session = nullptr;
}

void SftpThrWorker::shutdown()
{
    if(sftp_handle)
    {
        libssh2_sftp_close((LIBSSH2_SFTP_HANDLE *)sftp_handle);
        sftp_handle = nullptr;
    }
    if(sftp_session)
    {
        libssh2_sftp_shutdown((LIBSSH2_SFTP *)sftp_session);
        sftp_session = nullptr;
    }

    libssh2_exit();

    //mp_SftpStatic->Disconnect();

    if(session != nullptr)
    {
        while(libssh2_session_disconnect((LIBSSH2_SESSION *)session, "Normal Shutdown")
            == LIBSSH2_ERROR_EAGAIN);
        libssh2_session_free((LIBSSH2_SESSION *)session);

    }

    session = nullptr;
}

void SftpThrWorker::CopyFile(SendFileInfo data)
{
    try
    {
        QFile file(data.filepath);

        CenterInfo config = commonvalues::center_list.value(0);

        char TempPath[MAX_PATH];
        char newFilePath[MAX_PATH];
         //----------------------------------------------------------------------------
         char TempDate[20];
         char TempHour[20];
         char szTimeStamp[50];

         time_t  now_time;
         struct tm *today;

         time(&now_time);
         today = localtime(&now_time);
         strftime(szTimeStamp, 30, "%Y-%m-%d %H:%M:%S", today);

         log->GetCurrentDate(TempDate);
         log->GetCurrentHour(TempHour);

         QString backupPath = config.backupPath;

         sprintf(TempPath,"%s/%s/%s", backupPath.toUtf8().constData(),TempDate,TempHour);
         QDir mdir(TempPath);
         if(!mdir.exists())
         {
             mdir.mkpath(TempPath);
         }
         sprintf(newFilePath,"%s/%s/%s/%s", backupPath.toUtf8().constData(),TempDate,TempHour,data.filename.toUtf8().constData());
         file.copy(newFilePath);

    }
    catch(exception ex)
    {
#ifdef QT_QML_DEBUG
        qDebug() << ex.what();
#endif
        QString logstr = QString("CopyFile:예외처리 %1 %2 %3").arg(__FILE__).arg(__LINE__).arg(ex.what());
        log->write(logstr,LOG_ERR);
    }
    catch( ... )
    {
#ifdef QT_QML_DEBUG
        qDebug() << QString("CopyFile:예외처리");
#endif
        QString logstr = QString("CopyFile:예외처리 %1 %2").arg(__FILE__).arg(__LINE__);
        log->write(logstr,LOG_ERR);
    }
}

void SftpThrWorker::RefreshLocalFileList()
{
    emit localFileUpdate(nullptr);
    foreach( SendFileInfo item, sftp_SendFileInfoList.fileList )
    {
        // will items always be processed in numerical order by index?
        // do something with "item";
        SendFileInfo *pItem = new SendFileInfo();
        memcpy(pItem,&item,sizeof(SendFileInfo));
        emit localFileUpdate(pItem);
    }
}

void SftpThrWorker::ScanSendDataFiles()
{
    QString logstr;
    QString path = QString("%1/%2").arg(SFTP_SEND_PATH).arg(config.centername);
    QDir dir(path);
    try
    {
        if( !dir.exists())
        {
            dir.mkdir(path);
        }
    }
    catch( ... )
    {
        qDebug() << QString("ScanSendDataFiles-Directory Check  Exception");
        return;
    }

    QFileInfoList filepaths;
    sftp_SendFileInfoList.ClearAll();

    QDirIterator dirIter(dir.absolutePath(), QDir::AllEntries | QDir::NoSymLinks | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    int filecount = 0;

    try
    {
        while(dirIter.hasNext())
        {
            dirIter.next();
            if(QFileInfo(dirIter.filePath()).isFile())
            {
                if(QFileInfo(dirIter.filePath()).suffix().toLower() == "jpg"
                        || QFileInfo(dirIter.filePath()).suffix().toLower() == "jpeg"
                        || QFileInfo(dirIter.filePath()).suffix().toLower() == "txt")
                {
                    filecount++;
                    QString fpath = dirIter.fileInfo().absoluteFilePath();
                    if(sftp_SendFileInfo.ParseFilepath(fpath))
                    {
                        sftp_SendFileInfoList.AddFile(sftp_SendFileInfo);
                    }
                    if(filecount > MAXSFTPCOUNT - 1)
                    {
                        break;
                    }
                }
            }
        }
    }
    catch( ... )
    {
        logstr = QString("ScanSendDataFiles-Search Entryile Exception : %1 %2").arg(__FILE__).arg(__LINE__);
        log->write(logstr, LOG_NOTICE);
        return;
    }

    if( sftp_SendFileInfoList.Count() > 0)
    {
        logstr = QString("SFTP 전송 파일 리스트 : %1").arg(sftp_SendFileInfoList.Count());
        log->write(logstr,LOG_NOTICE);
        qDebug() <<  logstr;
        emit logappend(logstr);
    }
}

int SftpThrWorker::GetFirstNumPosFromFileName(QString filename)
{
    int pos = 0;
    foreach(const QChar c, filename)
    {
        // Check for control characters
        if(c.isNumber())
        {
            return pos;
        }
        pos++;
    }
    return pos;
}

bool SftpThrWorker::isLegalFileName(QString filename)
{
    filename = filename.toLower();

    filename = filename.remove(QChar('.'), Qt::CaseInsensitive);    //.jpg에서 .을 삭제함.
    filename = filename.remove(QChar('_'), Qt::CaseInsensitive);    //_을 삭제함.

    foreach(const QChar c, filename)
    {
        // Check for control characters
        if(!c.isLetterOrNumber())
            return false;
    }

    return true;
}

void SftpThrWorker::remoteConnect()
{
    emit remoteFileUpdate(NULL, NULL, QDateTime::currentDateTime(), false);
    libssh2_sftp_closedir((LIBSSH2_SFTP_HANDLE *)sftp_handle);
    sftp_handle = nullptr;

    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc;
    char fname[512];
    QString filename;
    QString filesize;
    const char *remotePath;
    remotePath= config.ftpPath.toStdString().c_str();

    if(sftp_session != nullptr)
    {
        sftp_handle = libssh2_sftp_opendir((LIBSSH2_SFTP *)sftp_session, remotePath);
        libssh2_sftp_fstat_ex((LIBSSH2_SFTP_HANDLE *)sftp_handle, &attrs, 0);

        if(sftp_handle != nullptr)
        {
            do
            {
                rc = libssh2_sftp_readdir((LIBSSH2_SFTP_HANDLE *)sftp_handle, fname, sizeof(fname), &attrs);
                if(rc > 0)
                {
                    filename = QString("%1").arg(fname);
                    filesize = QString("%1").arg(attrs.filesize);
                    QDateTime qdate = QDateTime::fromTime_t(attrs.mtime);
                    bool isDir = LIBSSH2_SFTP_S_ISDIR(attrs.permissions);

                    emit remoteFileUpdate(filename, filesize, qdate, isDir);
                }
                else
                {
                    break;
                }
            }
            while(1);
        }
        else
        {
            logstr = QString("SFTP : remoteConnect fail (sftp_handle Error)");
            emit logappend(logstr);
        }
    }
    else
    {
        logstr = QString("SFTP : remoteConnect fail (sftp_session Error)");
        emit logappend(logstr);
    }

    libssh2_sftp_closedir((LIBSSH2_SFTP_HANDLE *)sftp_handle);
    sftp_handle = nullptr;
}
