#include "threadworker.h"
#include <QApplication>
#include <QThread>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include "commonvalues.h"

ThreadWorker::ThreadWorker(QObject *parent) : QObject(parent)
{

    QString path = QApplication::applicationDirPath();
    FTP_SEND_PATH = path + "/FTP_Trans";
    m_RetryInterval = 5; //sec

    m_iFTPTrans = 0;
    m_iFTPRenameFail = 0;
    m_iFTPTransFail = 0;
    QString logpath = QApplication::applicationDirPath() + "/log";
    QString dir = logpath;
    QDir mdir(dir);
    if(!mdir.exists())
    {
         mdir.mkpath(dir);
    }

    CenterInfo config = commonvalues::center_list.value(0);

    QString backupPath = QString("%1/%2").arg(QApplication::applicationDirPath()).arg(config.backupPath);
    QDir backupdir(backupPath);
    if(!backupdir.exists())
    {
         backupdir.mkpath(backupPath);
    }


    log = new Syslogger(this,"mainwindow",true,commonvalues::loglevel,logpath);

    thread_run = true;

}
void ThreadWorker::doWork()
{
    QFtp::State cur_state = QFtp::Unconnected;
    last_state = QFtp::Unconnected;
    int lastCount = 0;
    bool isRetry = false;

    QDateTime lastTime = QDateTime::currentDateTime().addDays(-1);
    lastHostlookup = QDateTime::currentDateTime().addDays(-1);

    QString lastFilename;
    QString curFilename;

    while(thread_run)
    {
        lastFilename = curFilename;
        ScanSendDataFiles();
        SendFileInfo nowFileInfo =  sendFileList.GetFirstFile();
        curFilename = QString(nowFileInfo.filename);
        if(QString::compare(lastFilename,curFilename) != 0)
        {
            RefreshLocalFileList();
        }

        //check connection  && connect
        if(m_pftp != nullptr)
        {
            last_state =  cur_state;
            cur_state = m_pftp->state();

            if(cur_state != QFtp::HostLookup || cur_state != QFtp::Connecting)
            {
                lastHostlookup = QDateTime::currentDateTime().addDays(-1);
            }

            if(cur_state == QFtp::Unconnected)
            {
                QString logstr = QString("서버[%1] 연결 시도...").arg(config.ip.trimmed());
                log->write(logstr,LOG_NOTICE); qDebug() << logstr;
                emit logappend(logstr);
                Connect2FTP();

            }
            else if(cur_state == QFtp::Connecting)
            {
                if(lastHostlookup.secsTo(QDateTime::currentDateTime()) >5)
                {
                    emit initFtpReq(QString("서버[%1] 연결 타임아웃...").arg(config.ip.trimmed()));
                }
            }
            else if(cur_state == QFtp::HostLookup)
            {
                if(lastHostlookup.secsTo(QDateTime::currentDateTime()) >5)
                {
                    emit initFtpReq(QString("서버[%1] HostLookup 타임아웃...").arg(config.ip.trimmed()));
                }
            }
            else
            {
                if(last_state == QFtp::Unconnected)
                {
                    QString logstr = QString("서버[%1] 연결 성공").arg(config.ip.trimmed());
                    log->write(logstr,LOG_NOTICE); qDebug() << logstr;
                    emit logappend(logstr);
                }

                if(sendFileList.Count() == 0)
                {
                    QThread::msleep(1000);
                    continue;
                }

                SendFileInfo sendFile = sendFileList.GetFirstFile();
                if(sendFile.filename.isNull() || sendFile.filename.isEmpty())
                {
                    QThread::msleep(50);
                    continue;
                }

                if(isRetry)
                {
                    if(lastTime.secsTo(QDateTime::currentDateTime()) < m_RetryInterval )
                    {
                        QThread::msleep(10);
                        continue;
                    }
                }

                //FTP Send
                lastTime = QDateTime::currentDateTime();
                QFile file(sendFile.filepath);
                if(!file.open(QIODevice::ReadOnly)) continue;

                ///파일 싸이즈가 0이면 삭제한다.
                if(file.size() == 0)
                {
                    file.close();
                    sendFileList.RemoveFirstFile(sendFile);
                    QString logstr = QString("FTP파일 크기 에러(0)-->삭제: %1").arg(sendFile.filename);
                    log->write(logstr,LOG_NOTICE); qDebug() << logstr;
                    emit logappend(logstr);
                    continue;
                }

                QString fname = sendFile.filename.mid(0,1); //H,X
                QString rname = sendFile.filename.mid(1); //~~~.jpg , ~~~.txt
                if(sendFile.filename.mid(0,1).compare("M") != 0 )
                    m_pftp->put(&file,rname,QFtp::Binary);
                else
                    m_pftp->put(&file,sendFile.filename,QFtp::Binary);
                m_iFTPTrans = 0x00;
                while(m_iFTPTrans == 0x00 && thread_run)
                {
                    //2초동안 전송을 못 하면 실패
                    if(lastTime.secsTo(QDateTime::currentDateTime()) > 5 )
                        break;
                    //QThread::msleep(100);
                }
                file.close();

                if( m_iFTPTrans > 0)
                {
                    m_iFTPTransFail = 0;

                    QString logstr = QString("FTP파일 전송성공 : %1").arg(sendFile.filename);
                    log->write(logstr,LOG_NOTICE); qDebug() << logstr;
                    emit logappend(logstr);

                    if(sendFile.filename.mid(0,1).compare("M") != 0)
                    {
                        m_pftp->rename(rname,sendFile.filename);
                        m_iFTPRename = 0x00;
                        while(m_iFTPRename == 0x00 && thread_run)
                        {
                            //2초동안 전송을 못 하면 실패
                            if(lastTime.secsTo(QDateTime::currentDateTime()) > 2 )
                            {
                                m_iFTPRename = -1;
                                break;
                            }
                            QThread::msleep(10);
                        }
                        if(m_iFTPRename > 0 )
                        {
                            QString logstr = QString("FTP파일 이름변경 성공 : %1 -> %2").arg(rname).arg(sendFile.filename);
                            log->write(logstr,LOG_NOTICE); qDebug() << logstr;
                            emit logappend(logstr);
                        }
                        else
                        {
                            m_iFTPTrans = -1;
                            m_iFTPRenameFail++;
                            QString logstr = QString("FTP파일 이름변경 실패 : %1 -> %2").arg(rname).arg(sendFile.filename);
                            //동일한 파일이 있으면 삭제한다.
                            m_pftp->remove(rname);
                            m_pftp->remove(sendFile.filename);
                            log->write(logstr,LOG_NOTICE); qDebug() << logstr;
                            emit logappend(logstr);
                        }
                    }

                    //if(m_iFTPTrans > 0 || m_iFTPRenameFail > 3)
                    if(m_iFTPTrans > 0)
                    {
                        CenterInfo config = commonvalues::center_list.value(0);
                        if(config.bimagebackup)
                        {
                            //이미지를 백업하는 옵션이 있으면...
                            CopyFile(sendFile);
                        }
                        sendFileList.RemoveFirstFile(sendFile);

                        m_iFTPRenameFail = 0;
                        isRetry = false;
                    }
                    else
                    {
                       // m_pftp->close();
                       // isRetry = true;
                    }
                }
                else
                {
                    m_iFTPTransFail++;
                    QString logstr = QString("FTP파일 전송실패(fcnt:%1) : %2").arg(m_iFTPTransFail).arg(sendFile.filename);
                    log->write(logstr,LOG_NOTICE); qDebug() << logstr;
                    emit logappend(logstr);
                    emit initFtpReq(logstr);
                }
            }
        }
        else
        {
            QThread::msleep(1000);
        }

        if(lastCount != sendFileList.Count())
        {
            lastCount = sendFileList.Count();
            QString logstr = QString("FTP전송 파일 리스트 : %1").arg(lastCount);
            log->write(logstr,LOG_NOTICE); qDebug() << logstr;
            emit logappend(logstr);
        }
        QThread::msleep(100);
    }

    QString logstr = QString("서버 연결 종료");
    //log->write(logstr,LOG_NOTICE); qDebug() << logstr;
    emit logappend(logstr);
    emit finished();
}

void ThreadWorker::ScanSendDataFiles()
{
    QString logstr;
    QString path = QString("%1/%2").arg(FTP_SEND_PATH).arg(config.centername);
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
    sendFileList.ClearAll();

    try
    {
        QStringList filters;
        filters << "*.jpg" << "*.jpeg" << "*.txt";
        filepaths = dir.entryInfoList(filters);
    }
    catch( ... )
    {
        qDebug() << QString("ScanSendDataFiles-Search Entryile Exception");
        return;
    }

    foreach (QFileInfo filepath, filepaths)
    {
        SendFileInfo info;
        QString fpath = filepath.absoluteFilePath();
        if( info.ParseFilepath(fpath))
        {
            sendFileList.AddFile(info);
            /*if( sendFileList.AddFile(info))
            {
                logstr = QString("FTP전송 데이터 추가 : %1").arg(fpath);
                log->write(logstr,LOG_NOTICE);  qDebug() <<  logstr;

            }
            else DeleteFile(fpath);*/
        }
        else
            DeleteFile(fpath);

        if( sendFileList.Count() > MAXFTPCOUNT )
        {
            break;
        }
    }

    if( sendFileList.Count() > 0)
    {
        logstr = QString("FTP전송 파일 리스트 : %1").arg(sendFileList.Count());
        log->write(logstr,LOG_NOTICE);  qDebug() <<  logstr;
    }
}

void ThreadWorker::CancelConnection()
{

    if(m_pftp != nullptr)
    {
        m_pftp->abort();
        m_pftp->deleteLater();
    }

    m_pftp = nullptr;

    QThread::msleep(1000);

    //thread_run = false;
}

void ThreadWorker::InitConnection()
{
    m_pftp = new QFtp();
\
  /*  connect(m_pftp, SIGNAL(ommandFinished(int,bool)),
            this, SLOT(ftpCommandFinished(int,bool)));

    connect(m_pftp, SIGNAL(stateChanged(int)),
            this, SLOT(ftpStatusChanged(int)));*/

}

void ThreadWorker::Connect2FTP()
{
    QString ipaddr = config.ip.trimmed();
    int port = config.ftpport;
    QString useName = config.userID;
    QString password = config.password;
    QString path = config.ftpPath;

    QFtp::State cur_state = m_pftp->state();
    if(cur_state == QFtp::Unconnected || cur_state == QFtp::Closing )
    {

        m_pftp->connectToHost(ipaddr,port);

        if (!useName.isEmpty())
            m_pftp->login(useName, password);
        else
            m_pftp->login();

        if (!path.isNull() && !path.isEmpty() )
            m_pftp->cd(path);
    }

    QThread::msleep(2000);
}

void ThreadWorker::SetConfig(CenterInfo configinfo,QFtp *pFtp)
{
    config = configinfo;
    m_pftp = pFtp;
}

void ThreadWorker::DeleteFile(QString filepath)
{
    try
    {
        QFile file(filepath);
        file.remove();
    }
    catch( ... )
    {
        qDebug() << QString("DeleteFile exception");
    }
}

void ThreadWorker::CopyFile(SendFileInfo data)
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

         QString backupPath = QString("%1/%2").arg(QApplication::applicationDirPath()).arg(config.backupPath);

         sprintf(TempPath,"%s/%s/%s", backupPath.toUtf8().constData(),TempDate,TempHour);
         QDir mdir(TempPath);
         if(!mdir.exists())
         {
             mdir.mkpath(TempPath);
         }
         sprintf(newFilePath,"%s/%s/%s/%s", backupPath.toUtf8().constData(),TempDate,TempHour,data.filename.toUtf8().constData());
         file.copy(newFilePath);

    }
    catch( ... )
    {
        qDebug() << QString("CopyFile Expection");
    }
}

void ThreadWorker::RefreshLocalFileList()
{
    emit localFileUpdate(nullptr);
    foreach( SendFileInfo item, sendFileList.fileList )
    {
        // will items always be processed in numerical order by index?
        // do something with "item";
        SendFileInfo *pItem = new SendFileInfo();
        memcpy(pItem,&item,sizeof(SendFileInfo));
        emit localFileUpdate(pItem);
    }

}

#if 0
void ThreadWorker::ftpCommandFinished(int id, bool error)
{
    //QString logstr;
    if (m_pftp->currentCommand() == QFtp::Put )
    {
        if(error)
        {
            m_iFTPTrans = -1; //fail
            qDebug() << m_pftp->errorString();
            //logstr = QString("Push error");
            //emit logappend(logstr);
            CancelConnection();
        }
        else
        {
            //logstr = QString("Push ok");
            //emit logappend(logstr);
            m_iFTPTrans = 1; //success
        }
    }
    else if(m_pftp->currentCommand() == QFtp::Rename )
    {
        if(error)
        {
            m_iFTPRename = -1; //fail
            qDebug() << m_pftp->errorString();
            //logstr = QString("rename error");
            //emit logappend(logstr);
            CancelConnection();
        }
        else
        {
            //logstr = QString("rename ok");
            //emit logappend(logstr);
            m_iFTPRename = 1; //success
        }
    }
    else if(m_pftp->currentCommand() == QFtp::Close )
    {
        if(error)
        {
            qDebug() << m_pftp->errorString();
        }

        CancelConnection();

    }
    else if(m_pftp->currentCommand() == QFtp::ConnectToHost )
    {
        if(error)
        {
            qDebug() << m_pftp->errorString();
            CancelConnection();
        }

    }
    else if(m_pftp->currentCommand() == QFtp::Login )
    {
        if(error)
        {
            qDebug() << m_pftp->errorString();
            CancelConnection();
        }

    }

}

void ThreadWorker::ftpStatusChanged(int state)
{
    //QString logstr;
    switch(state){
    case QFtp::Unconnected:
        {
            CenterInfo config = commonvalues::center_list.value(0);
            //logstr = QString("서버[%1] 연결 끊김").arg(config.ip.trimmed());
            //emit logappend(logstr);
            //CancelConnection();
            //InitConnection();
            //Connect2FTP();
        }
        break;
    case QFtp::HostLookup:
        lastHostlookup = QDateTime::currentDateTime().addDays(-1);
        //logstr = QString("HostLookup");
        //emit logappend(logstr);
        break;
    case QFtp::Connecting:
        lastHostlookup = QDateTime::currentDateTime().addDays(-1);
        //logstr = QString("Connecting");
        //emit logappend(logstr);
        break;
    case QFtp::Connected:
        //logstr = QString("Connected");
        //emit logappend(logstr);
        break;
    case QFtp::Close:
        //logstr = QString("CLose");
        //emit logappend(logstr);
        //CancelConnection();
        //InitConnection();
        //Connect2FTP();
        break;
    default:
        break;
    }
}
#endif
bool SendFileInfo::SaveFile(QString path, QString fname, QByteArray filedata)
{
    try
    {
        filename = fname;
        filepath = QString("%1/%2").arg(path).arg(fname);

        QDir dir(path);
        if (!dir.exists())
            dir.mkpath(path);

        QFile file(filepath);

        if (file.exists()) return false;

        if(file.open(QIODevice::WriteOnly))
        {
            QDataStream out(&file);
            int flen = filedata.length();
            int wflen = 0;
            while( flen > wflen )
            {
                int wlen = out.writeRawData(filedata.data(),flen);
                if(wlen <= 0)
                    break;
                wflen += wlen;
            }
            //            out << filedata;
             file.close();

            if(flen > wflen)
                return false;
        }
        else
            return false;
    }
    catch ( ... )
    {
        qDebug() << QString("SaveFile Expection : %1/%2").arg(path).arg(fname);
        return false;
    }
    return true;
}

bool SendFileInfo::ParseFilepath(QString _filepath)
{
    try
    {
        filename = _filepath.mid(_filepath.lastIndexOf('/') + 1);
//        QString filename2 = filename.mid(0, filename.lastIndexOf(".jpg"));

//        int index = filename2.indexOf("H");
//        if (index != 0)
//        {
//            index = filename2.indexOf("MS");
//            if (index != 0)
//                    return false;
//        }
        filepath = _filepath;


    }
    catch (...)
    {
        qDebug() << QString("ParseFilepath Expection : %1").arg(filepath);
        return false;
    }
    return true;
}


bool SendFileInfoList::AddFile(SendFileInfo data)
{
    bool brtn = true;
    mutex.lock();
    try
    {
        fileList.append(data);
    }
    catch( ... )
    {
        qDebug() << QString("AddFile Expection");
        brtn = false;
    }
    mutex.unlock();
    return brtn;

}

SendFileInfo SendFileInfoList::GetFirstFile()
{
    SendFileInfo fileinfo;
    mutex.lock();
    if (fileList.count() > 0)
        fileinfo = fileList.first();
//    else
//        fileinfo = NULL;
    mutex.unlock();
    return fileinfo;
}

void SendFileInfoList::RemoveFirstFile(SendFileInfo data)
{
    mutex.lock();
    try
    {
        QFile file(data.filepath);
        file.remove();
        fileList.removeFirst();//   .removeOne(data);
        for(int index=0; index < fileList.count(); index++)
        {
              if( data.filepath.compare(fileList[index].filepath) == 0 )
              {
                fileList.removeAt(index);
                break;
              }

        }
    }
    catch( ... )
    {
        qDebug() << QString("RemoveFile Expection");
    }
    mutex.unlock();
}

void SendFileInfoList::ClearAll()
{
    mutex.lock();
    fileList.clear();
    mutex.unlock();
}

int SendFileInfoList::Count()
{
    return fileList.count();
}
