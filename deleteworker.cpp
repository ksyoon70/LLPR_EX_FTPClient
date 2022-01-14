#include "deleteworker.h"

DeleteWorker::DeleteWorker(QObject *parent) : QObject(parent)
{
    thread_run = true;

}

void DeleteWorker::SetConfig(CenterInfo *configinfo)
{
    mp_cinfo = configinfo;
    m_daysToStore = mp_cinfo->daysToStore;

    QString logpath = configinfo->logPath;
    QString dir = logpath;
    QDir mdir(dir);
    if(!mdir.exists())
    {
         mdir.mkpath(dir);
    }

    log = new Syslogger(this,"ThreadWorker",true,commonvalues::loglevel,logpath,configinfo->blogsave);
}

void DeleteWorker::doWork()
{

    while(thread_run)
    {
        m_mutex.lock();
        m_Condition.wait(&m_mutex);

        QFileInfoList filepaths;

        //QString path = QString("%1/%2").arg(QApplication::applicationDirPath()).arg("log");
        QString path = mp_cinfo->logPath;

        if(path.compare(commonvalues::curLogPath))
        {
            path = commonvalues::curLogPath;
        }
        QDir logdir(path);

        if(m_daysToStore != commonvalues::curDaysToStore)
        {
            m_daysToStore = commonvalues::curDaysToStore;
        }

        QDate NowTime = QDate::currentDate();

        qint64 ts;

        log->write("로그 삭제 시작");

        try
        {
            //로그 파일 삭제
            QStringList filters;
            filters << "*";
            filepaths = logdir.entryInfoList(filters);

            foreach (QFileInfo fileInfo, filepaths)
            {
                QString fpath = fileInfo.fileName();
                if(fileInfo.isDir() && fpath.length() ==  8 && IsDirNameAllNumber(fpath))
                {
                    ulong val;

                    val = atol((char *)fpath.toUtf8().constData());
                    const char *pdir = fpath.toUtf8().constData();

                    if(val)
                    {
                        char year[5]={0,};
                        char mon[3]={0,};
                        char day[3] ={0,};
                        int nyear,nmon,nday;
                        memcpy(year,(char *)pdir,4);
                        nyear = atoi(year);
                        memcpy(mon,(char *)pdir+4,2);
                        nmon = atoi(mon);

                        memcpy(day,(char *)pdir+6,2);
                        nday = atoi(day);

                        QDate ct(nyear,nmon,nday);

                        ts = ct.daysTo(NowTime);

                        if(ts >= m_daysToStore)   //시간이 지나면 디렉토리를 삭제함.
                        {
                            DeleteDir(fileInfo.absoluteFilePath());
                            QString logstr;
                            logstr = QString("디렉토리삭제 : %1").arg(fileInfo.absoluteFilePath());
                            log->write(logstr);
                            QThread::msleep(500);
                        }
                    }
                }

            }
        }
        catch( ... )
        {
            log->write("로그 삭제 Exception");
        }

        log->write("로그 삭제 종료");
        log->write("백업 삭제 시작");

        //path = QString("%1/%2").arg(QApplication::applicationDirPath()).arg(mp_cinfo->backupPath);
        path = mp_cinfo->backupPath;
        QDir backupdir(path);

        try
        {
            //백업 파일 삭제
            QStringList filters;
            filters << "*";
            filepaths = backupdir.entryInfoList(filters);

            foreach (QFileInfo fileInfo, filepaths)
            {
                QString fpath = fileInfo.fileName();
                if(fileInfo.isDir() && fpath.length() ==  8 && IsDirNameAllNumber(fpath))
                {
                    ulong val;

                    val = atol((char *)fpath.toUtf8().constData());
                    const char *pdir = fpath.toUtf8().constData();

                    if(val)
                    {
                        char year[5]={0,};
                        char mon[3]={0,};
                        char day[3] ={0,};
                        int nyear,nmon,nday;
                        memcpy(year,(char *)pdir,4);
                        nyear = atoi(year);
                        memcpy(mon,(char *)pdir+4,2);
                        nmon = atoi(mon);

                        memcpy(day,(char *)pdir+6,2);
                        nday = atoi(day);

                        QDate ct(nyear,nmon,nday);

                        ts = ct.daysTo(NowTime);

                        if(ts >= m_daysToStore)   //시간이 지나면 디렉토리를 삭제함.
                        {
                            DeleteDir(fileInfo.absoluteFilePath());
                            QString logstr;
                            logstr = QString("디렉토리삭제 : %1").arg(fileInfo.absoluteFilePath());
                            log->write(logstr);
                            QThread::msleep(500);
                        }
                    }
                }

            }
        }
        catch( ... )
        {
            log->write("백업 삭제 Exception");
        }

        log->write("백업 삭제 종료");
        m_mutex.unlock();
    }
    emit finished();

}

void DeleteWorker::doRun()
{
    m_Condition.wakeOne();
}

void DeleteWorker::Stop()
{
    thread_run = false;
    m_Condition.wakeOne();
}

void DeleteWorker::DeleteDir(QString filepath)
{
    try
    {
        QDir dir(filepath);
        dir.removeRecursively();  //디렉토리와 그 하위 파일 및 디렉토리를 삭제하는 함수 Qt5 이상에서만 지원 됨. Qt4에서는 구현하는 함수가 필요
    }
    catch(exception ex)
    {
#ifdef QT_QML_DEBUG
        qDebug() << ex.what();
#endif
        QString logstr;
        logstr = QString("DeleteDir exception: %1 %2 %3").arg(__FILE__).arg(__LINE__).arg(ex.what());
        log->write(logstr,LOG_ERR);
    }
    catch( ... )
    {
#ifdef QT_QML_DEBUG
        qDebug() << QString("DeleteDir exception");
#endif
        QString logstr;
        logstr = QString("DeleteDir exception: %1 %2").arg(__FILE__).arg(__LINE__);
        log->write(logstr,LOG_ERR);
    }
}

bool DeleteWorker::removeDir(const QString &dirName)
{
    bool result = true;
        QDir dir(dirName);

        if (dir.exists(dirName)) {
            Q_FOREACH(QFileInfo info, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden  | QDir::AllDirs | QDir::Files, QDir::DirsFirst)) {
                if (info.isDir()) {
                    result = removeDir(info.absoluteFilePath());
                } else {
                    result = QFile::remove(info.absoluteFilePath());
                }

                if (!result) {
                    return result;
                }
            }
            result = dir.rmdir(dirName);
        }
        return result;
}

bool DeleteWorker::IsDirNameAllNumber(QString filepath)
{
    foreach (const QChar& c, filepath)
    {
        // Check for characters
         if (!c.isNumber())
            return false;
    }

    return true;
}
