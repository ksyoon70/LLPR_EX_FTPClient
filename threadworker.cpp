#include "threadworker.h"
#include <QApplication>
#include <QThread>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include "commonvalues.h"

ThreadWorker::ThreadWorker(QObject *parent) : QObject(parent)
{

    //QString path = QApplication::applicationDirPath();
    FTP_SEND_PATH = commonvalues::FileSearchPath;
    m_RetryInterval = 5; //sec

    QDir sdir(FTP_SEND_PATH);       //ftp serch directory
    if(!sdir.exists())
    {
         sdir.mkpath(FTP_SEND_PATH);
    }

    m_iFTPTrans = 0;
    m_iFTPRenameFail = 0;
    m_iFTPTransFail = 0;

    int max_iter = commonvalues::center_list.size();

    for(int i = 0; i < max_iter;  i++)
    {
        config = commonvalues::center_list.value(i);
        cf_index = i;
          // commtype가 Normal 이나 ftponly 인 경우만 허용한다.
        if(config.protocol_type == 0 || config.protocol_type == 2)
            break;
    }

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


    log = new Syslogger(this,"ThreadWorker",true,commonvalues::loglevel,logpath,config.blogsave);

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

        try {

            if(FTP_SEND_PATH != commonvalues::FileSearchPath)
            {
                FTP_SEND_PATH = commonvalues::FileSearchPath;
            }

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
                    QThread::msleep(5000);
                    QString logstr = QString("서버[%1] 연결 시도...").arg(config.ip.trimmed());
                    log->write(logstr,LOG_NOTICE); //qDebug() << logstr;
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
                        log->write(logstr,LOG_NOTICE); //qDebug() << logstr;
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
                        // 2초간 기다려본다.
                        QThread::msleep(2000);
                        try {

                            file.close();
                            QString logstr = QString("FTP파일 크기 에러(0)-->삭제: %1").arg(sendFile.filename);
                            log->write(logstr,LOG_NOTICE); //qDebug() << logstr;
                            emit logappend(logstr);
                            sendFileList.RemoveFirstFile(sendFile);


                        }
                        catch (exception ex)
                        {
                            #ifdef QT_QML_DEBUG
                            qDebug() << ex.what();
                            #endif
                            QString logstr = QString("FTP파일 크기 에러(0)-->삭제: 예외처리 %1 %2 %3").arg(__FILE__).arg(__LINE__).arg(ex.what());
                            log->write(logstr,LOG_ERR);
                        }
                        catch (...) {

                            QString logstr = QString("FTP파일 크기 에러(0)-->삭제: 예외처리 %1 %2").arg(__FILE__).arg(__LINE__);
                            log->write(logstr,LOG_ERR);

                        }
                        continue;

                    }

                    if(!isLegalFileName(sendFile.filename))
                    {
                        try {

                            file.close();
                            QString logstr = QString("파일이름 이상-->삭제: %1").arg(sendFile.filename);
                            log->write(logstr,LOG_NOTICE); //qDebug() << logstr;
                            emit logappend(logstr);
                            sendFileList.RemoveFirstFile(sendFile);


                        }
                        catch (exception ex)
                        {
                            #ifdef QT_QML_DEBUG
                            qDebug() << ex.what();
                            #endif
                            QString logstr = QString("파일이름 이상-->삭제: 예외처리 %1 %2 %3").arg(__FILE__).arg(__LINE__).arg(ex.what());
                            log->write(logstr,LOG_ERR);
                        }
                        catch (...) {

                            QString logstr = QString("파일이름 이상-->삭제: 예외처리 %1 %2").arg(__FILE__).arg(__LINE__);
                            log->write(logstr,LOG_ERR);

                        }
                        QThread::msleep(100);
                        continue;
                    }


                    //QChar char1 = sendFile.filename.at(0);
                    //QChar char2 = sendFile.filename.at(1);

                    QString rname;
                    bool brename = false;
                    /*

                    if( char1.isLetter() == true && char2.isLetter() == true)
                    {
                        rname = sendFile.filename.mid(2);
                        brename = true;
                    }
                    else if(char1.isLetter() == true)
                    {
                        rname = sendFile.filename.mid(1);
                        brename = true;
                    }
                    else
                    {
                        rname = sendFile.filename;
                    }
                    */
                    int pos =  GetFirstNumPosFromFileName(sendFile.filename);

                    if(pos == 0)
                    {
                        rname = sendFile.filename;
                    }
                    else
                    {
                        rname = sendFile.filename.mid(pos);
                        brename = true;
                    }


                    //QString fname = sendFile.filename.mid(0,1); //H,X
                    //QString rname = sendFile.filename.mid(1); //~~~.jpg , ~~~.txt

                    m_pftp->put(&file,rname,QFtp::Binary);

                    m_iFTPTrans = 0x00;
                    while(m_iFTPTrans == 0x00 && thread_run)
                    {
                        //2초동안 전송을 못 하면 실패 --> 파일이 서버에 많으면 전송을 못하는 문제가있다.
                        if(lastTime.secsTo(QDateTime::currentDateTime()) > 10 )
                            break;
                        QThread::msleep(100);
                    }
                    file.close();

                    if( m_iFTPTrans > 0)
                    {
                        m_iFTPTransFail = 0;

                        QString logstr = QString("FTP파일 전송성공 : %1").arg(sendFile.filename);
                        log->write(logstr,LOG_NOTICE); //qDebug() << logstr;
                        emit logappend(logstr);

                        if(brename == true)
                        {
                            m_pftp->rename(rname,sendFile.filename);
                            m_iFTPRename = 0x00;
                            while(m_iFTPRename == 0x00 && thread_run)
                            {
                                //2초동안 전송을 못 하면 실패 --> 파일이 서버에 많으면 전송을 못하는 문제가있다.
                                if(lastTime.secsTo(QDateTime::currentDateTime()) > 10 )
                                {
                                    m_iFTPRename = -1;
                                    break;
                                }
                                QThread::msleep(100);
                            }
                            if(m_iFTPRename > 0 )
                            {
                                QString logstr = QString("FTP파일 이름변경 성공 : %1 -> %2").arg(rname).arg(sendFile.filename);
                                log->write(logstr,LOG_NOTICE); //qDebug() << logstr;
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
                                log->write(logstr,LOG_NOTICE); //qDebug() << logstr;
                                emit logappend(logstr);
                            }
                        }

                        //if(m_iFTPTrans > 0 || m_iFTPRenameFail > 3)
                        if(m_iFTPTrans > 0)
                        {
                            CenterInfo config = commonvalues::center_list.value(cf_index);
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
                        log->write(logstr,LOG_NOTICE); //qDebug() << logstr;
                        emit logappend(logstr);
                        emit initFtpReq(logstr);
                        m_iFTPTransFail = 0;
                        m_iFTPRenameFail = 0;
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
                log->write(logstr,LOG_NOTICE); //qDebug() << logstr;
                emit logappend(logstr);
            }
            QThread::msleep(100);

        }
        catch (exception ex)
        {
#ifdef QT_QML_DEBUG
            qDebug() << ex.what();
#endif
            QString logstr = QString("ThreadWorker 예외처리 %1 %2 %3").arg(__FILE__).arg(__LINE__).arg(ex.what());
            log->write(logstr,LOG_ERR);
        }
        catch (...) {
            QString logstr = QString("ThreadWorker 예외 %1 %2").arg(__FILE__).arg(__LINE__);
            log->write(logstr,LOG_ERR);
        }

    }

    QString logstr = QString("서버 연결 종료");
    log->write(logstr,LOG_NOTICE); //qDebug() << logstr;
    emit logappend(logstr);
    emit finished();
}

void ThreadWorker::ScanSendDataFiles()
{
    QString logstr;
    QString path = QString("%1/%2").arg(FTP_SEND_PATH).arg(config.centername);
    QString spath = QString("%1").arg(FTP_SEND_PATH);
    QDir sdir(spath);
    if( !sdir.exists())
    {
        sdir.mkdir(spath);
    }

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
#ifdef QT_QML_DEBUG
        qDebug() << QString("ScanSendDataFiles-Directory Check  Exception");
#endif
        logstr = QString("ScanSendDataFiles-Directory Check  Exception: %1 %2").arg(__FILE__).arg(__LINE__);
        log->write(logstr,LOG_ERR);
        return;
    }

    QFileInfoList filepaths;
    sendFileList.ClearAll();

#if  0
    try
    {
        QStringList filters;
        filters << "*.jpg" << "*.jpeg" << "*.txt";
        filepaths = dir.entryInfoList(filters);
    }
    catch( ... )
    {
#ifdef QT_QML_DEBUG
        qDebug() << QString("ScanSendDataFiles-Search Entryile Exception");
#endif
        logstr = QString("ScanSendDataFiles-Search Entryile Exception: %1 %2").arg(__FILE__).arg(__LINE__);
        log->write(logstr,LOG_ERR);
        return;
    }

    foreach (QFileInfo filepath, filepaths)
    {
        SendFileInfo info;
        QString fpath = filepath.absoluteFilePath();
        if( info.ParseFilepath(fpath))
        {
            sendFileList.AddFile(info);
        }
        else
            DeleteFile(fpath);

        if( sendFileList.Count() > MAXFTPCOUNT )
        {
            break;
        }
    }
#else
    QDirIterator dirIter(dir.absolutePath(), QDir::AllEntries | QDir::NoSymLinks | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    int filecount = 0;
    try {

            while(dirIter.hasNext())
            {
                dirIter.next();
                if(QFileInfo(dirIter.filePath()).isFile())
                {
                    if(QFileInfo(dirIter.filePath()).suffix().toLower() == "jpg" || QFileInfo(dirIter.filePath()).suffix().toLower() == "jpeg" || QFileInfo(dirIter.filePath()).suffix().toLower() == "txt" )
                    {
                        //qDebug()<<"File Path: " <<dirIter.filePath();
                        filecount++;
                        SendFileInfo info;
                        QString fpath = dirIter.fileInfo().absoluteFilePath();
                        if( info.ParseFilepath(fpath))
                        {
                            sendFileList.AddFile(info);
                        }
                        if(filecount > MAXFTPCOUNT)
                        {
                            break;
                        }
                    }
                }
            }
        }
        catch( ... )
        {
    #ifdef QT_QML_DEBUG
            qDebug() << QString("ScanSendDataFiles-Search Entryile Exception");
    #endif
            logstr = QString("ScanSendDataFiles-Search Entryile Exception: %1 %2").arg(__FILE__).arg(__LINE__);
            log->write(logstr,LOG_ERR);
            return;
        }



#endif

    if( sendFileList.Count() > 0)
    {
        logstr = QString("FTP전송 파일 리스트 : %1").arg(sendFileList.Count());
        log->write(logstr,LOG_NOTICE);
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
#ifdef QT_QML_DEBUG
        qDebug() << QString("DeleteFile exception");
#endif
        QString logstr;
        logstr = QString("DeleteFile exception: %1 %2").arg(__FILE__).arg(__LINE__);
        log->write(logstr,LOG_ERR);
    }
}

void ThreadWorker::CopyFile(SendFileInfo data)
{
    try
    {
        QFile file(data.filepath);

        CenterInfo config = commonvalues::center_list.value(cf_index);

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

bool ThreadWorker::isLegalFileName(QString filename)
{
    filename = filename.toLower();

    filename = filename.remove(QChar('.'), Qt::CaseInsensitive);  //.jpg에서 .을 삭제함.
    filename = filename.remove(QChar('_'), Qt::CaseInsensitive);  //_을 삭제함.

    foreach (const QChar& c, filename)
    {
        // Check for control characters
         if (!c.isLetterOrNumber())
            return false;
    }

    return true;

}

int ThreadWorker::GetFirstNumPosFromFileName(QString filename)
{
    int pos = 0;
    foreach (const QChar& c, filename)
    {
        // Check for control characters
         if (c.isNumber())
         {
             return pos;
         }
         pos++;
    }
    return pos;
}
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
#ifdef QT_QML_DEBUG
        qDebug() << QString("SaveFile Expection : %1/%2").arg(path).arg(fname);
#endif
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
#ifdef QT_QML_DEBUG
        qDebug() << QString("ParseFilepath Expection : %1").arg(filepath);
#endif
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
        //qDebug() << QString("AddFile Expection");
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
#ifdef QT_QML_DEBUG
        qDebug() << QString("RemoveFile Expection");
#endif
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
