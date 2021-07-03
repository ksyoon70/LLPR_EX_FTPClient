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
#include "mainwindow.h"

#define SFTP_TIMEOUT				(3000)
#define MAX_SFTP_OUTGOING_SIZE      (30000)

SftpThrWorker::SftpThrWorker(QString host, qint32 port,QObject *parent) : QObject(parent)
{
    SFTP_SEND_PATH = commonvalues::FileSearchPath;
    QDir sdir(SFTP_SEND_PATH);       //sftp serch directory
    if(!sdir.exists())
    {
         sdir.mkpath(SFTP_SEND_PATH);
    }

    m_iSFTPRename = 0;
    m_RetryInterval = 5;

    m_Auth_pw = 1;

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

    //m_socket = *socket;
    m_socket = new QTcpSocket();

    this->host = host;
    this->port = port;

    mp_session = nullptr;
    mp_sftp_session = nullptr;
    mp_sftp_handle = nullptr;

    plog = new Syslogger(this,"SftpThrWorker",true,commonvalues::loglevel,logpath,config.blogsave);

    thread_run = true;

     m_lpBuffer = new char[FTP_BUFFER + 1];


}
SftpThrWorker::~SftpThrWorker()
{
    if(m_lpBuffer)
        delete [] m_lpBuffer;
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

    sshInit();

    //m_socket = new QTcpSocket();


    //QObject::connect(m_socket, SIGNAL(QAbstractSocket::connected()), this, SLOT(connected()));
    //QObject::connect(m_socket, SIGNAL(QAbstractSocket::disconnected()), this, SLOT(disconnected()));

    connectSocket(this->host, this->port);

    while(thread_run)
    {
        if(commonvalues::SftpSocketConn == true)
        {
            if(mp_session == nullptr)
            {
                if(!Sftp_Init_Session())
                {
                    SftpShutdown();
                    logstr = QString("Session fail");
                    emit logappend(logstr);
                    QThread::msleep(3000);
                    continue;
                }
            }
        }
        else
        {

            connectSocket(this->host, this->port);
            QThread::msleep(3000);
            continue;
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
                    plog->write(logstr,LOG_NOTICE); //qDebug() << logstr;
                    emit logappend(logstr);
                    sftp_SendFileInfoList.RemoveFirstFile(sftp_SendFileInfo);
                }
                catch (exception ex)
                {
                    #ifdef QT_QML_DEBUG
                    qDebug() << ex.what();
                    #endif
                    logstr = QString("SFTP : 파일 크기 에러(0)-->삭제: 예외처리 %1 %2 %3").arg(__FILE__).arg(__LINE__).arg(ex.what());
                    plog->write(logstr,LOG_ERR);
                }
                catch (...) {

                    logstr = QString("SFTP : 파일 크기 에러(0)-->삭제: 예외처리 %1 %2").arg(__FILE__).arg(__LINE__);
                    plog->write(logstr,LOG_ERR);

                }
                continue;
            }

            fpath.append(sftp_SendFileInfo.filename);
            cvt = fpath.toLocal8Bit();
            localfile = cvt.data();

            fp = fopen(localfile, "rb");

            if(!isLegalFileName(sftp_SendFileInfo.filename))
            {
                try
                {
                    file.close();
                    logstr = QString("파일이름 이상-->삭제: %1").arg(sftp_SendFileInfo.filename);
                    plog->write(logstr, LOG_NOTICE);
                    emit logappend(logstr);
                    sftp_SendFileInfoList.RemoveFirstFile(sftp_SendFileInfo);
                }
                catch(exception ex)
                {
                    logstr = QString("파일이름 이상-->삭제: 예외처리 %1 %2 %3").arg(__FILE__).arg(__LINE__).arg(ex.what());
                    plog->write(logstr,LOG_ERR);
                }
                catch (...)
                {
                    logstr = QString("파일이름 이상-->삭제: 예외처리 %1 %2").arg(__FILE__).arg(__LINE__);
                    plog->write(logstr,LOG_ERR);
                }

                QThread::msleep(100);
                continue;
            }

            if(!fp)
            {
                logstr = QString("SFTP : 파일 열기 실패..");
                continue;
            }
            else
            {
                logstr = QString("SFTP : 파일 열기 성공!!");
            }

            plog->write(logstr,LOG_NOTICE);
            emit logappend(logstr);

            if(sftpput(sftp_SendFileInfo.filename,sftp_SendFileInfo.filename))
            {

                sftp_SendFileInfoList.RemoveFirstFile(sftp_SendFileInfo);

                if(lastCount != sftp_SendFileInfoList.Count())
                {
                    lastCount = sftp_SendFileInfoList.Count();
                    logstr = QString("SFTP 전송 파일 리스트 : %1").arg(lastCount);
                    plog->write(logstr, LOG_NOTICE);
                    emit logappend(logstr);



                }

                 QThread::msleep(100);

                isRetry = false;
            }
            else
            {
                isRetry = true;
                emit remoteFileUpdate(NULL, NULL, QDateTime::currentDateTime(), false);
                SftpShutdown();
                QThread::msleep(3000);

            }


        }  //파일이 있는 경우
    }
}

void SftpThrWorker::disconnected()
{

}

void SftpThrWorker::connected()
{

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

    plog->write(logstr,LOG_NOTICE);
    emit logappend(logstr);

    return sshSuc;
}
#if 1
bool SftpThrWorker::Sftp_Init_Session()
{   

    bool status = true;
    int rc;
    int sock;

    sock = m_socket->socketDescriptor();

    mp_session = libssh2_session_init();
    if(!mp_session)
    {
        return false;
    }

    try {

        libssh2_session_set_blocking((LIBSSH2_SESSION *)mp_session, 1);

        QDateTime old2 = QDateTime::currentDateTime();
        QDateTime now2 = old2;

        do
        {
            rc = libssh2_session_handshake((LIBSSH2_SESSION *)mp_session, sock);
            now2 = QDateTime::currentDateTime();

            QThread::msleep(10);
            if(((old2.secsTo(now2)) >= SFTP_TIMEOUT) || (sock == 0))
            {
                rc = 1;
                break;
            }

        }while(rc == LIBSSH2_ERROR_EAGAIN);

        if(rc) {
            logstr = QString("Failure establishing SSH session: %1").arg(rc);
            status = false;
        }
        else
        {
            const char *fingerprint;
            /* At this point we havn't yet authenticated.  The first thing to do is
            * check the hostkey's fingerprint against our known hosts Your app may
            * have it hard coded, may go to a file, may present it to the user,
            * that's your call
            */
            fingerprint = libssh2_hostkey_hash((LIBSSH2_SESSION *)mp_session, LIBSSH2_HOSTKEY_HASH_SHA1);
            char buf[10];
            for(int i = 0; i < 20; i++) {
                sprintf(buf,"%02X ", (unsigned char)fingerprint[i]);
            }

            if(m_Auth_pw)
            {
                /* We could authenticate via password */
                QDateTime old1 = QDateTime::currentDateTime();
                QDateTime now1;
                while((rc = libssh2_userauth_password((LIBSSH2_SESSION *)mp_session, commonvalues::center_list[0].userID.toStdString().c_str(), commonvalues::center_list[0].password.toStdString().c_str())) ==
                    LIBSSH2_ERROR_EAGAIN)
                {
                    now1 = QDateTime::currentDateTime();
                    if((old1.secsTo(now1)) >= SFTP_TIMEOUT)
                    {
                        rc = 1;
                        break;
                    }
                    QThread::msleep(10);
                }
                if(rc) {
                    logstr = QString("Authentication by password failed.");
                    status = false;
                }
                else
                {
                    QDateTime old = QDateTime::currentDateTime();
                    QDateTime now;
                    do {
                        mp_sftp_session = libssh2_sftp_init((LIBSSH2_SESSION *)mp_session);

                        if(!mp_sftp_session &&
                            (libssh2_session_last_errno((LIBSSH2_SESSION *)mp_session) != LIBSSH2_ERROR_EAGAIN)) {
                                logstr = QString("Unable to init SFTP session");
                                status = false;
                                break;
                        }
                        QThread::msleep(10);
                        now = QDateTime::currentDateTime();
                    } while(!mp_sftp_session && ((old.secsTo(now)) < SFTP_TIMEOUT));

                }
            }
        }
        if(status == true)
        {
            logstr = QString("SFTP 서버에 접속하였습니다");
        }
        else
        {
            if(mp_sftp_session != NULL)
            {
                libssh2_sftp_shutdown((LIBSSH2_SFTP *)mp_sftp_session);
            }
            mp_sftp_session = NULL;
            logstr = QString("SFTP 서버에 접속실패 했습니다");
        }

        plog->write(logstr,LOG_NOTICE);
        emit logappend(logstr);

    }
    catch(exception ex)
    {
        logstr = QString("SFTP 초기화 이상: 예외처리 %1 %2 %3").arg(__FILE__).arg(__LINE__).arg(ex.what());
        plog->write(logstr,LOG_ERR);
        status = false;
    }
    catch (...)
    {
        logstr = QString("SFTP 초기화 이상: 예외처리 %1 %2").arg(__FILE__).arg(__LINE__);
        plog->write(logstr,LOG_ERR);
        status = false;
    }



    return status;
}
#endif
#if 0
bool SftpThrWorker::initSFTP()
{
    mpp_sftp_session = libssh2_sftp_init((LIBSSH2_SESSION *)mpp_session);

    logstr = QString("SFTP : SFTP 초기화!!");
    log->write(logstr,LOG_NOTICE);
    emit logappend(logstr);

    return true;
}
#endif
#if 0
bool SftpThrWorker::openSFTP()
{
    bool sftpSuc = false;

    QString Remotefilepath = QString("%1/%2").arg(config.ftpPath).arg(noCharFileName);
    QByteArray cvt;
    cvt = Remotefilepath.toLocal8Bit();
    const char* _sftppath = cvt.data();

    //closeSFTP();

    if(commonvalues::SftpSocketConn == true)
    {
        sftp_handle = libssh2_sftp_open((LIBSSH2_SFTP *)*mpp_sftp_session, _sftppath,
                                        LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_APPEND | LIBSSH2_FXF_TRUNC,
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
#endif
bool SftpThrWorker::sftpput(QString local, QString remote)
{
    int32_t	nByteRead;
    uint32_t dwNumberOfBytesWritten = 0;
    uint32_t	fileLength, nSumOfnByteRead;
    bool	canceled = false;
    QString errMsg;
    QString tmpRemote;
    QString renamedRemoteFullPath;		//원격 전체 path
    QString remoteFullPath;		//원격 전체 path
    int nByteWrite;
    bool status = true;

    QString FTP_SEND_PATH = commonvalues::FileSearchPath;

    int pos =  GetFirstNumPosFromFileName(local);

    QString rname;
    bool brename = false;

    if(pos == 0)
    {
        rname = local;
    }
    else
    {
        rname = local.mid(pos);
        brename = true;
    }

    CenterInfo config = commonvalues::center_list.value(0);
    remoteFullPath = QString("%1/%2").arg(config.ftpPath).arg(local);
    QString path = QString("%1/%2").arg(FTP_SEND_PATH).arg(config.centername);
    QString Localfilepath = QString("%1/%2").arg(path).arg(local);
    QFile	localFile(Localfilepath);
    renamedRemoteFullPath = QString("%1/%2").arg(config.ftpPath).arg(rname);

    try {

        if(mp_sftp_session != NULL)
        {
            do {
                mp_sftp_handle =
                    libssh2_sftp_open((LIBSSH2_SFTP *)mp_sftp_session , renamedRemoteFullPath.toUtf8().constData(),
                    LIBSSH2_FXF_WRITE|LIBSSH2_FXF_CREAT|LIBSSH2_FXF_APPEND | LIBSSH2_FXF_EXCL,
                    //LIBSSH2_FXF_TRUNC,
                    LIBSSH2_SFTP_S_IRUSR|LIBSSH2_SFTP_S_IWUSR|
                    LIBSSH2_SFTP_S_IRGRP|LIBSSH2_SFTP_S_IROTH);
                if(!mp_sftp_handle &&
                    (libssh2_session_last_errno((LIBSSH2_SESSION *)mp_session) != LIBSSH2_ERROR_EAGAIN)) {
                        logstr = QString("Unable to open file with SFTP.");
                        plog->write(logstr,LOG_NOTICE);
                        emit logappend(logstr);
                        return false;
                }
            } while(!mp_sftp_handle);
        }
        else
        {
            return false;
        }

    }
    catch (...) {

        logstr = QString("알 수 없는 문제 입니다.");
        plog->write(logstr,LOG_NOTICE);
        emit logappend(logstr);
        return false;
    }

    if(mp_sftp_handle) {
        try
        {
            QFileInfo info = QFileInfo(Localfilepath);
            fileLength = info.size();

            nSumOfnByteRead = 0;

            if(fileLength > 0) {

                    try {
                            int rc = 0;
                            if(!localFile.open(QIODevice::ReadOnly))
                                return false;
                            nByteRead = localFile.read(m_lpBuffer,FTP_BUFFER);// FTP_BUFFER은 실험값임...

                            nByteWrite = nByteRead;
                            QDateTime old = QDateTime::currentDateTime();
                            QDateTime now = old;
                            do {
                                /* write data in a loop until we block */
                                int max_size = qMin(MAX_SFTP_OUTGOING_SIZE,nByteWrite);
                                rc = libssh2_sftp_write((LIBSSH2_SFTP_HANDLE *)mp_sftp_handle, &m_lpBuffer[nSumOfnByteRead], max_size);
                                //rc = libssh2_sftp_write((LIBSSH2_SFTP_HANDLE *)mp_sftp_handle, &m_lpBuffer[nSumOfnByteRead], nByteWrite);
                                if(rc < 0)
                                {
                                }
                                else
                                {
                                    nByteWrite -= rc;
                                    nSumOfnByteRead += rc;
                                    if(nByteWrite <= 0)
                                    {
                                        nByteWrite = 0;
                                    }
                                }
                                QThread::msleep(10);
                                now = QDateTime::currentDateTime();
                            } while(nByteWrite && ((old.secsTo(now)) < SFTP_TIMEOUT));
                            //}while(nByteWrite);

                    }
                    catch(...)
                    {
                        //SftpShutdown();
                        return false;
                    }

                    if(nByteWrite == 0)
                    {
                        //파일 송신이 정상적으로 된 경우
                    }
                    else
                    {
                        //파일 송신 에러가 발생한경우.
                        logstr = QString("sftp 송신중 에러 발생.");
                        plog->write(logstr,LOG_NOTICE);
                        emit logappend(logstr);
                        return false;
                    }

                    //::PostMessage(this->GetSafeHwnd(),JWWM_FTP_CALLBACK,CON_STATE_DATA_SENDING, (LPARAM)(long)((float)nSumOfnByteRead*100/(float)fileSize));


                    //localFile.close();
            }
            else {

                logstr = QString("파일을 읽을 수 없읍니다.");
                plog->write(logstr,LOG_NOTICE);
                emit logappend(logstr);
                return true;
            }
        }
        catch(exception ex)
        {
            logstr = QString("SFTP 파일 이상 : 예외처리 %1 %2 %3").arg(__FILE__).arg(__LINE__).arg(ex.what());
            plog->write(logstr,LOG_ERR);
            emit logappend(logstr);
            status = false;
        }
        catch(...)
        {
            logstr = QString("SFTP 파일 이상 : 예외처리 %1 %2").arg(__FILE__).arg(__LINE__);
            plog->write(logstr,LOG_ERR);
            emit logappend(logstr);
            status = false;
        }

        if(mp_sftp_session != NULL && mp_sftp_handle != NULL)
        {
            int rc = 0;
            QDateTime old = QDateTime::currentDateTime();
            QDateTime now = old;
            try{
                //파일을 찾고 파일이 있으면 이름을 바꾼다.

                LIBSSH2_SFTP_ATTRIBUTES attrs;
                do{
                    rc = libssh2_sftp_fstat_ex((LIBSSH2_SFTP_HANDLE *)mp_sftp_handle, &attrs, 0);
                    QThread::msleep(10);
                    now = QDateTime::currentDateTime();
                } while((rc < 0) && ((old.secsTo(now)) < SFTP_TIMEOUT));

                if(rc < 0) {
                    logstr = QString("libssh2_sftp_fstat_ex failed.");
                    plog->write(logstr,LOG_ERR);
                    emit logappend(logstr);
                    //SftpShutdown();
                    return false;
                }
                else
                {
                    if(attrs.filesize > 0)
                    {

                        if(mp_sftp_handle)
                        {
                            libssh2_sftp_close((LIBSSH2_SFTP_HANDLE *)mp_sftp_handle);
                            mp_sftp_handle = NULL;
                        }
                        old = QDateTime::currentDateTime();
                        now = old;
                        do{
                            rc = libssh2_sftp_rename((LIBSSH2_SFTP *)mp_sftp_session,renamedRemoteFullPath.toUtf8().constData(),remoteFullPath.toUtf8().constData());
                            QThread::msleep(10);
                            now = QDateTime::currentDateTime();
                        } while((rc < 0) && ((old.secsTo(now)) < SFTP_TIMEOUT));

                        if(rc < 0)
                        {
                            switch(rc)
                            {
                            case LIBSSH2_ERROR_SFTP_PROTOCOL:
                                break;
                            case LIBSSH2_ERROR_REQUEST_DENIED:
                                break;
                            case LIBSSH2_ERROR_EAGAIN:
                                break;
                            default:
                                break;
                            }

                            //이미 서버에 같은 파일이 있고 파일의 크기가 동일하면 성공한 것으로 본다.
                            if(attrs.filesize == fileLength)
                            {
                                canceled = false;

                            }
                            else
                            {
                                canceled = true;
                            }

                            //해당 파일을 삭제 한다.
                            old = QDateTime::currentDateTime();
                            now = old;
                            do{
                                rc = libssh2_sftp_unlink((LIBSSH2_SFTP *)mp_sftp_session,renamedRemoteFullPath.toUtf8().constData());
                                QThread::msleep(10);
                                now = QDateTime::currentDateTime();
                            } while((rc < 0) && ((old.secsTo(now)) < SFTP_TIMEOUT));

                        }
                        else
                        {
                            //파일 이름 변경에 성공 하였다.
                            canceled = false;
                        }


                    }
                    else
                    {
                        canceled = true;
                    }
                }
            }
            catch(...)
            {
                // 이상한 파일은 전송하지 않는다.
                //::PostMessage(this->GetSafeHwnd(),JWWM_FTP_CALLBACK,CON_STATE_DATA_SENDING, (LPARAM)0);
                canceled = false;
            }
        }
        else
        {
            logstr = QString("원격지에 파일을 쓸 수 없읍니다.");
            plog->write(logstr,LOG_NOTICE);
            emit logappend(logstr);
        }

    }
    else {
        logstr = QString("원격지에 파일을 쓸 수 없읍니다.");
        plog->write(logstr,LOG_NOTICE);
        emit logappend(logstr);
        canceled = true;
    }

    if(mp_sftp_handle)
    {
        libssh2_sftp_close((LIBSSH2_SFTP_HANDLE *)mp_sftp_handle);
        mp_sftp_handle = NULL;
    }
    if(canceled)
        return false;
    else
        return true;
}
bool SftpThrWorker::sendData(int size)
{
    bool status = false;
    //char mem[2048*1536];
    char *pbuf = new char[2048*1536];
    int rc = 0;
    int sum = 0;
    int nByteRead;
    int nByteWrite;

    // 파일 전송
    try
    {
        nByteRead = fread(pbuf, 1, size, fp);
        nByteWrite = nByteRead;

        QDateTime old = QDateTime::currentDateTime();
        QDateTime now = old;
        do
        {
                // write data in a loop until we block
                rc = libssh2_sftp_write((LIBSSH2_SFTP_HANDLE *)mp_sftp_handle, reinterpret_cast<char*>(pbuf + sum), qMin(MAX_SFTP_OUTGOING_SIZE,nByteWrite));

                if(rc<0)
                {
                    //logstr = QString("SFTP : sftp_write error (%1)!!").arg(rc);
                    //logappend(logstr);
                    status = false;

                    delete [] pbuf;

                    return status;
                }
                else
                {
                    //emit transferProgress(nByteRead - nByteWrite, nByteRead);
                    nByteWrite -=  rc;
                    sum += rc;
                }
                QThread::msleep(10);
                now = QDateTime::currentDateTime();

        //}while(nByteWrite && (old.secsTo(now) < SFTP_TIMEOUT));
          }while(nByteWrite);

        if(nByteWrite == 0)
        {
            logstr = QString("SFTP : %1 전송 성공!!").arg(sftp_SendFileInfo.filename);
            logappend(logstr);
            status = true;
        }
        else
        {
            logstr = QString("SFTP : %1 전송 실패..").arg(sftp_SendFileInfo.filename);
            logappend(logstr);
            status = false;
        }

    }
    catch(exception ex)
    {
        logstr = QString("SFTP 파일 이상 : 예외처리 %1 %2 %3").arg(__FILE__).arg(__LINE__).arg(ex.what());
        plog->write(logstr,LOG_ERR);
        status = false;
    }
    catch (...)
    {
        logstr = QString("SFTP 파일 이상 : 예외처리 %1 %2").arg(__FILE__).arg(__LINE__);
        plog->write(logstr,LOG_ERR);
        status = false;
    }

    //fclose(local);

    delete [] pbuf;

    return status;
}

void SftpThrWorker::closeSFTP()
{
    if(mp_sftp_handle != nullptr)
    {
        libssh2_sftp_close((LIBSSH2_SFTP_HANDLE *)mp_sftp_handle);
    }

    mp_sftp_handle = nullptr;
}
void SftpThrWorker::closeSession()
{
    if(mp_session != nullptr)
    {
        libssh2_session_disconnect((LIBSSH2_SESSION *)mp_session, "Normal Shutdown");
        libssh2_session_free((LIBSSH2_SESSION *)mp_session);
    }

    mp_session = nullptr;
}

bool SftpThrWorker::SftpShutdown()
{
    if(mp_sftp_handle)
    {
        libssh2_sftp_close((LIBSSH2_SFTP_HANDLE *)mp_sftp_handle);
        mp_sftp_handle = nullptr;
    }
    if(mp_sftp_session)
    {
        libssh2_sftp_shutdown((LIBSSH2_SFTP *)mp_sftp_session);
        mp_sftp_session = nullptr;
    }

    libssh2_exit();

    if(mp_session != nullptr)
    {
        while(libssh2_session_disconnect((LIBSSH2_SESSION *)mp_session, "Normal Shutdown")
            == LIBSSH2_ERROR_EAGAIN);
        libssh2_session_free((LIBSSH2_SESSION *)mp_session);

    }

    mp_session = nullptr;

    return true;
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

         plog->GetCurrentDate(TempDate);
         plog->GetCurrentHour(TempHour);

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
        plog->write(logstr,LOG_ERR);
    }
    catch( ... )
    {
#ifdef QT_QML_DEBUG
        qDebug() << QString("CopyFile:예외처리");
#endif
        QString logstr = QString("CopyFile:예외처리 %1 %2").arg(__FILE__).arg(__LINE__);
        plog->write(logstr,LOG_ERR);
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
        plog->write(logstr, LOG_NOTICE);
        return;
    }

    if( sftp_SendFileInfoList.Count() > 0)
    {
        logstr = QString("SFTP 전송 파일 리스트 : %1").arg(sftp_SendFileInfoList.Count());
        plog->write(logstr,LOG_NOTICE);
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
    libssh2_sftp_closedir((LIBSSH2_SFTP_HANDLE *)mp_sftp_handle);
    mp_sftp_handle = nullptr;

    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc;
    char fname[512];
    QString filename;
    QString filesize;
    const char *remotePath;
    remotePath= config.ftpPath.toStdString().c_str();

    if(mp_sftp_session != nullptr)
    {
        mp_sftp_handle = libssh2_sftp_opendir((LIBSSH2_SFTP *)mp_sftp_session, remotePath);
        libssh2_sftp_fstat_ex((LIBSSH2_SFTP_HANDLE *)mp_sftp_handle, &attrs, 0);

        if(mp_sftp_handle != nullptr)
        {
            do
            {
                rc = libssh2_sftp_readdir((LIBSSH2_SFTP_HANDLE *)mp_sftp_handle, fname, sizeof(fname), &attrs);
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

    libssh2_sftp_closedir((LIBSSH2_SFTP_HANDLE *)mp_sftp_handle);
    mp_sftp_handle = nullptr;
}

bool SftpThrWorker::connectSocket(QString host, qint32 port)
{
    bool connectState = false;
    if(m_socket->state() == QAbstractSocket::UnconnectedState)
    {
        m_socket->connectToHost(host, port, QAbstractSocket::ReadWrite);
        if(m_socket->waitForConnected(3000))
        {
            connectState = true;
        }

    }
    else if(m_socket->state() == QAbstractSocket::ConnectedState)
    {
        connectState = true;
    }
    else
    {
        connectState = false;
    }

    return connectState;
}
