#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "commonvalues.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->setWindowTitle(Program_Version);

    m_maxlogline = 500;

    ui->localFileList->setEnabled(true);
    ui->localFileList->setRootIsDecorated(false);
    ui->localFileList->setHeaderLabels(QStringList() << tr("Name") << tr("Size") << tr("Time"));
    ui->localFileList->header()->setStretchLastSection(false);


    ui->remoteFileList->setEnabled(false);
    ui->remoteFileList->setRootIsDecorated(false);
    ui->remoteFileList->setHeaderLabels(QStringList() << tr("Name") << tr("Size") << tr("Time"));
    ui->remoteFileList->header()->setStretchLastSection(false);
    ui->reRefreshButton->setEnabled(false);

    ui->progressBar->setValue(0);

    NowTime = QDate::currentDate();

    init();
}

MainWindow::~MainWindow()
{

    if(mp_dWorker)
    {
        mp_dWorker->Stop();
    }
    delete qTimer;
    delete ui;
}

void MainWindow::init()
{
    pcfg = new config();
    init_config();

    //log 디렉토리 생성
    QString logpath = commonvalues::center_list[0].logPath;
    QDir mdir(logpath);
    if(!mdir.exists())
    {
         mdir.mkpath(logpath);
    }

    plog = new Syslogger(this,"mainwindow",true,commonvalues::loglevel,logpath,commonvalues::center_list[0].blogsave);
    QString logstr = QString("Program Start(%1)").arg(Program_Version);
    plog->write(logstr,LOG_ERR); //qDebug() << logstr;

    pcenterdlg = new CenterDlg(pcfg);
    pconfigdlg = new configdlg(pcfg);

    if(QString::compare(commonvalues::TransferType, "FTP") == 0)
    {
        // FTP Mode //
        mp_tWorker = new ThreadWorker();
        mp_wThread = new QThread();
        m_pftp = new QFtp();
        mp_tWorker->moveToThread(mp_wThread);

        //설정값 저장
        QObject::connect(mp_wThread, SIGNAL(started()), mp_tWorker, SLOT(doWork()));
        QObject::connect(mp_wThread, SIGNAL(finished()), mp_wThread, SLOT(deleteLater()));
        QObject::connect(mp_tWorker, SIGNAL(finished()), mp_wThread, SLOT(quit()));
        QObject::connect(mp_tWorker, SIGNAL(finished()), this, SLOT(quitWThread()));
        QObject::connect(mp_tWorker, SIGNAL(finished()), mp_tWorker, SLOT(deleteLater()));
        QObject::connect(mp_tWorker, SIGNAL(logappend(QString)),this,SLOT(logappend(QString)));
        QObject::connect(mp_tWorker, SIGNAL(localFileUpdate(SendFileInfo *)), this, SLOT(localFileUpdate(SendFileInfo *)));
        QObject::connect(mp_tWorker, SIGNAL(initFtpReq(QString)),this, SLOT(initFtpReqHandler(QString)));
        QObject::connect(m_pftp, SIGNAL(commandFinished(int,bool)),this, SLOT(ftpCommandFinished(int,bool)));
        QObject::connect(m_pftp, SIGNAL(stateChanged(int)),this, SLOT(ftpStatusChanged(int)));
        QObject::connect(m_pftp, SIGNAL(listInfo(QUrlInfo)),this, SLOT(addToList(QUrlInfo)));
        QObject::connect(m_pftp, SIGNAL(dataTransferProgress(qint64 ,qint64)),this, SLOT(loadProgress(qint64 ,qint64)));

        mp_tWorker->SetConfig(commonvalues::center_list.value(0),m_pftp);

        mp_wThread->start();
    }
    else if(QString::compare(commonvalues::TransferType, "SFTP") == 0)
    {
        // SFTP Mode //
        mp_stWorker = new SftpThrWorker();
        mp_swThread = new QThread();
        mp_stWorker->moveToThread(mp_swThread);

        QObject::connect(mp_swThread, SIGNAL(started()), mp_stWorker, SLOT(doWork()));
        QObject::connect(mp_swThread, SIGNAL(finished()), mp_swThread, SLOT(deleteLater()));
        QObject::connect(mp_stWorker, SIGNAL(finished()), mp_swThread, SLOT(quit()));
        QObject::connect(mp_stWorker, SIGNAL(finished()), this, SLOT(quitWThread()));
        QObject::connect(mp_stWorker, SIGNAL(finished()), mp_stWorker, SLOT(deleteLater()));
        QObject::connect(mp_stWorker, SIGNAL(logappend(QString)),this,SLOT(logappend(QString)));
        QObject::connect(mp_stWorker, SIGNAL(localFileUpdate(SendFileInfo *)), this, SLOT(localFileUpdate(SendFileInfo *)));

        QObject::connect(mp_stWorker, SIGNAL(remoteFileUpdate(QString, QString, QDateTime, bool)), this, SLOT(remoteFileUpdate(QString, QString, QDateTime, bool)));

        mp_swThread->start();
    }


    //삭제 쓰레드 생성
#if 1
    mp_dWorker = new DeleteWorker();
    mp_dThread = new QThread();
    mp_dWorker->SetConfig(&commonvalues::center_list[0]);
    mp_dWorker->moveToThread(mp_dThread);
#endif

#if 1
    QObject::connect(mp_dThread, SIGNAL(started()), mp_dWorker, SLOT(doWork()));
    QObject::connect(mp_dThread, SIGNAL(finished()), mp_dThread, SLOT(deleteLater()));
    QObject::connect(mp_dWorker, SIGNAL(finished()), mp_dThread, SLOT(quit()));
    QObject::connect(mp_dWorker, SIGNAL(finished()), this, SLOT(quitDThread()));
    QObject::connect(mp_dWorker, SIGNAL(finished()), mp_dWorker, SLOT(deleteLater()));
#endif
    initaction();

#if 1
    mp_dThread->start();
#endif
    //QTimer init
    qTimer = new QTimer();
    connect(qTimer,SIGNAL(timeout()),this,SLOT(onTimer()));
    qTimer->start(1000);
    m_mseccount=0;
}

void MainWindow::initaction()
{
    connect(ui->actionCenter,SIGNAL(triggered()),this,SLOT(centerdlgview()));
    connect(ui->actionConfiguration,SIGNAL(triggered()),this,SLOT(configurationdlgview()));
}

void MainWindow::init_config()
{
    //check config file exists
    if( !pcfg->check())
    {
        MakeDefaultConfig();
    }
    else
    {
        //load config file
        pcfg->load();
        //apply commonvalues
        applyconfig2common();

    }
}

void MainWindow::applyconfig2common()
{
    QString svalue;
    uint uivalue;
    bool bvalue;

    int i=0;
    QString title = "CENTER|LIST" + QString::number(i);
    while( ( svalue = pcfg->get(title,"IP") ) != NULL)
    {
        CenterInfo centerinfo;
        if( ( svalue = pcfg->get(title,"TransferType") ) != NULL)
        {
            centerinfo.transfertype = svalue;
            commonvalues::TransferType = svalue;
        }
        else
        {
            pcfg->set(title,"TransferType","FTP");
            centerinfo.transfertype = "FTP";
        }
        if( ( svalue = pcfg->get(title,"CenterName") ) != NULL )
        {
            centerinfo.centername = svalue;
        }
        else
        {
            pcfg->set(title,"CenterName","center");
            centerinfo.centername = "center";
        }
        if( ( svalue = pcfg->get(title,"IP") ) != NULL )
        {
            centerinfo.ip = svalue;
        }
        else
        {
            pcfg->set(title,"IP","192.168.10.73");
            centerinfo.ip = "192.168.10.73";
        }
        if( pcfg->getuint(title,"TCPPort",&uivalue))
        {
            centerinfo.tcpport = uivalue;
        }
        else
        {
            pcfg->set(title,"TCPPort","9111");
            centerinfo.tcpport = 9111;
        }
        if( pcfg->getuint(title,"AliveInterval",&uivalue))
        {
            centerinfo.connectioninterval = uivalue;
        }
        else
        {
            pcfg->set(title,"AliveInterval","30");
            centerinfo.connectioninterval = 30;
        }
        if( pcfg->getuint(title,"ProtocolType",&uivalue))
        {
            centerinfo.protocol_type = uivalue;
        }
        else
        {
            pcfg->set(title,"ProtocolType","2");
            centerinfo.protocol_type = 2;
        }
        if( pcfg->getuint(title,"FTPPort",&uivalue))
        {
            centerinfo.ftpport = uivalue;
        }
        else
        {
            pcfg->set(title,"FTPPort","21");
            centerinfo.ftpport = 21;
        }
        if( ( svalue = pcfg->get(title,"FTPUserID") ) != NULL )
        {
            centerinfo.userID = svalue;
        }
        else
        {
            pcfg->set(title,"FTPUserID","tes");
            centerinfo.userID = "tes";
        }
        if( ( svalue = pcfg->get(title,"FTPPassword") ) != NULL )
        {
            centerinfo.password = svalue;
        }
        else
        {
            pcfg->set(title,"FTPPassword","tes");
            centerinfo.password = "tes";
        }
        if( ( svalue = pcfg->get(title,"FTPPath") ) != NULL )
        {
            centerinfo.ftpPath = svalue;
        }
        else
        {
            pcfg->set(title,"FTPPath","");
            centerinfo.ftpPath = "";
        }
        if( pcfg->getuint(title,"FileNameSelect",&uivalue))
        {
            centerinfo.fileNameSelect = uivalue;
        }
        else
        {
            pcfg->set(title,"FileNameSelect","0");
            centerinfo.fileNameSelect = 0;
        }
        if( ( svalue = pcfg->get("CENTER","BackupPath") ) != NULL )
        {
            centerinfo.backupPath = svalue;
            commonvalues::BackImagePath = svalue;
            //default 디렉토리가 존재하는지 확인한다.
            QDir dir(commonvalues::BackImagePath);

            if(!dir.exists())
            {
                //디렉토리를 만들수 있는지 확인한다.
                if( dir.mkpath(commonvalues::BackImagePath))
                {

                }
                else
                {
                    commonvalues::BackImagePath = QString("%1/%2").arg(QApplication::applicationDirPath()).arg("backup");
                }

                pcfg->set("CENTER","BackupPath",commonvalues::BackImagePath);
                centerinfo.backupPath = commonvalues::BackImagePath;
            }
        }
        else
        {
            //default 디렉토리가 존재하는지 확인한다.
            QDir dir(commonvalues::BackImagePath);

            if(dir.exists())
            {
            }
            else
            {
                //디렉토리를 만들수 있는지 확인한다.
                if( dir.mkpath(commonvalues::BackImagePath))
                {

                }
                else
                {
                    commonvalues::BackImagePath = QString("%1/%2").arg(QApplication::applicationDirPath()).arg("backup");
                }
            }
            pcfg->set("CENTER","BackupPath",commonvalues::BackImagePath);
            centerinfo.backupPath = commonvalues::BackImagePath;
        }
        if( ( pcfg->getbool(title,"ImageBackupYesNo",&bvalue) ) != false )
        {
            centerinfo.bimagebackup = bvalue;  //백업 여부
        }
        else
        {
            pcfg->set(title,"ImageBackupYesNo","True");
            centerinfo.bimagebackup = true;
        }

        if( ( pcfg->getbool(title,"FtpLogSaveYesNo",&bvalue) ) != false )
        {
            centerinfo.blogsave = bvalue;  //Ftp로그 저장 여부
        }
        else
        {
            pcfg->set(title,"FtpLogSaveYesNo","True");
            centerinfo.blogsave = true;
        }

        if( ( svalue = pcfg->get("CENTER","LogSavePath") ) != NULL )
        {
            centerinfo.logPath = svalue;
            commonvalues::LogPath = svalue;

            //default 디렉토리가 존재하는지 확인한다.
            QDir dir(commonvalues::LogPath);

            if(!dir.exists())
            {
                //디렉토리를 만들수 있는지 확인한다.
                if( dir.mkpath(commonvalues::LogPath))
                {

                }
                else
                {
                    commonvalues::LogPath = QString("%1/%2").arg(QApplication::applicationDirPath()).arg("Ftplog");
                }

                pcfg->set("CENTER","LogSavePath",commonvalues::LogPath);
                centerinfo.logPath = commonvalues::LogPath;
            }
        }
        else
        {
            //default 디렉토리가 존재하는지 확인한다.
            QDir dir(commonvalues::LogPath);

            if(dir.exists())
            {
            }
            else
            {
                //디렉토리를 만들수 있는지 확인한다.
                if( dir.mkpath(commonvalues::LogPath))
                {

                }
                else
                {
                    commonvalues::LogPath = QString("%1/%2").arg(QApplication::applicationDirPath()).arg("Ftplog");
                }
            }
            pcfg->set("CENTER","LogSavePath",commonvalues::LogPath);
            centerinfo.logPath = commonvalues::LogPath;
        }
        if( ( pcfg->getbool(title,"ImageBackupYesNo",&bvalue) ) != false )
        {
            centerinfo.bimagebackup = bvalue;  //백업 여부
        }
        else
        {
            pcfg->set(title,"ImageBackupYesNo","True");
            centerinfo.bimagebackup = true;
        }

        if( ( pcfg->getuint(title,"DaysToStore",&uivalue)) != false )
        {
            centerinfo.daysToStore = uivalue;  //백업 여부
        }
        else
        {
            pcfg->set(title,"DaysToStore","10");
            centerinfo.daysToStore = 10;
        }
        if( pcfg->getuint("CENTER","FTPRetry",&uivalue))
        {
            commonvalues::ftpretry = uivalue;
        }
        else
        {
            pcfg->set("CENTER","FTPRetry","0");
            commonvalues::ftpretry = 0;
        }

        if( ( svalue = pcfg->get("CENTER","FTPSavePath") ) != NULL )
        {
            commonvalues::FileSearchPath = svalue;

            //default 디렉토리가 존재하는지 확인한다.
            QDir dir(commonvalues::FileSearchPath);

            if(!dir.exists())
            {
                //디렉토리를 만들수 있는지 확인한다.
                if( dir.mkpath(commonvalues::FileSearchPath))
                {

                }
                else
                {
                    commonvalues::FileSearchPath = QString("%1/%2").arg(QApplication::applicationDirPath()).arg("FTP_Trans");
                }

                pcfg->set("CENTER","FTPSavePath",commonvalues::FileSearchPath);
            }
        }
        else
        {
            //default 디렉토리가 존재하는지 확인한다.
            QDir dir(commonvalues::FileSearchPath);

            if(dir.exists())
            {

            }
            else
            {
                //디렉토리를 만들수 있는지 확인한다.
                if( dir.mkpath(commonvalues::FileSearchPath))
                {

                }
                else
                {
                    commonvalues::FileSearchPath = QString("%1/%2").arg(QApplication::applicationDirPath()).arg("FTP_Trans");
                }
            }
            pcfg->set("CENTER","FTPSavePath",commonvalues::FileSearchPath);

        }


      /*       Log Level value             */
       if( pcfg->getuint("LOG","Level",&uivalue))
       {
           commonvalues::loglevel = uivalue;
       }
       else
       {
           //pcfg->set("LOG","Level","0");
           commonvalues::loglevel = LOG_EMERG;
       }

        pcfg->save();


        centerinfo.plbIcon = new QLabel();
        QPixmap pixmap(QPixmap(":/images/red.png").scaledToHeight(ui->statusBar->height()/2));
        centerinfo.plbIcon->setPixmap(pixmap);
        centerinfo.plbIcon->setToolTip(tr("끊김"));
        ui->statusBar->addWidget(centerinfo.plbIcon,Qt::AlignRight);

        centerinfo.plblstatus = new QLabel();
        centerinfo.plblstatus->setText( QString("%1:%2")
                    .arg(centerinfo.ip).arg(centerinfo.ftpport));
        centerinfo.plblstatus->setStyleSheet("QLabel { background-color : red; }");

        commonvalues::center_list.append(centerinfo);
        ui->statusBar->addWidget(centerinfo.plblstatus);



        i++;
        title = "CENTER|LIST" + QString::number(i);
    }



}

void MainWindow::MakeDefaultConfig()
{

    int i=0;

    pcfg->create();
    pcfg->load();

    QString title = "CENTER|LIST" + QString::number(i);

    CenterInfo centerinfo;

    pcfg->set(title, "TransferType", "FTP");
    centerinfo.transfertype = "FTP";

    pcfg->set(title,"CenterName","center");
    centerinfo.centername = "center";

    pcfg->set(title,"IP","192.168.10.73");
    centerinfo.ip = "192.168.10.73";

    pcfg->set(title,"TCPPort","9111");
    centerinfo.tcpport = 9111;

    pcfg->set(title,"AliveInterval","30");
    centerinfo.connectioninterval = 30;

    pcfg->set(title,"ProtocolType","2");
    centerinfo.protocol_type = 2;

    pcfg->set(title,"FTPPort","21");
    centerinfo.ftpport = 21;

    pcfg->set(title,"FTPUserID","tes");
    centerinfo.userID = "tes";

    pcfg->set(title,"FTPPassword","tes");
    centerinfo.password = "tes";

    pcfg->set(title,"FTPPath","");
    centerinfo.ftpPath = "";

    pcfg->set(title,"FileNameSelect","0");
    centerinfo.fileNameSelect = 0;

    //default 디렉토리가 존재하는지 확인한다.
    QDir backupdir(commonvalues::BackImagePath);

    if(backupdir.exists())
    {
        pcfg->set("CENTER","BackupPath",commonvalues::BackImagePath);
    }
    else
    {
        //디렉토리를 만들수 있는지 확인한다.
        if( backupdir.mkpath(commonvalues::BackImagePath))
        {

        }
        else
        {
            commonvalues::BackImagePath = QString("%1/%2").arg(QApplication::applicationDirPath()).arg("backup");
        }
    }
    pcfg->set("CENTER","BackupPath",commonvalues::BackImagePath);
    centerinfo.backupPath = commonvalues::BackImagePath;

    pcfg->set(title,"ImageBackupYesNo","True");
    centerinfo.bimagebackup = true;

    pcfg->set(title,"FtpLogSaveYesNo","True");
    centerinfo.blogsave = true;

    //default 디렉토리가 존재하는지 확인한다.
    QDir logdir(commonvalues::LogPath);

    if(logdir.exists())
    {
        pcfg->set("CENTER","LogSavePath",commonvalues::LogPath);
    }
    else
    {
        //디렉토리를 만들수 있는지 확인한다.
        if( backupdir.mkpath(commonvalues::LogPath))
        {

        }
        else
        {
            commonvalues::LogPath = QString("%1/%2").arg(QApplication::applicationDirPath()).arg("Ftplog");
        }
    }
    pcfg->set("CENTER","LogSavePath",commonvalues::LogPath);

    pcfg->set(title,"DaysToStore","10");
    centerinfo.daysToStore = 10;

    pcfg->set("CENTER","FTPRetry","0");
    commonvalues::ftpretry = 0;

    //pcfg->set("LOG","Level","0");  ///이걸 쓰면 name와 value가 바뀐다.
    commonvalues::loglevel = LOG_EMERG;


    //default 디렉토리가 존재하는지 확인한다.
    QDir dir(commonvalues::FileSearchPath);

    if(dir.exists())
    {
        pcfg->set("CENTER","FTPSavePath",commonvalues::FileSearchPath);
    }
    else
    {
        //디렉토리를 만들수 있는지 확인한다.
        if( dir.mkpath(commonvalues::FileSearchPath))
        {

        }
        else
        {
            commonvalues::FileSearchPath = QString("%1/%2").arg(QApplication::applicationDirPath()).arg("FTP_Trans");
        }
    }
    pcfg->set("CENTER","FTPSavePath",commonvalues::FileSearchPath);


    pcfg->save();

    centerinfo.plbIcon = new QLabel();
    QPixmap pixmap(QPixmap(":/images/red.png").scaledToHeight(ui->statusBar->height()/2));
    centerinfo.plbIcon->setPixmap(pixmap);
    centerinfo.plbIcon->setToolTip(tr("끊김"));
    ui->statusBar->addWidget(centerinfo.plbIcon,Qt::AlignRight);

    centerinfo.plblstatus = new QLabel();
    centerinfo.plblstatus->setText( QString("%1:%2")
                .arg(centerinfo.ip).arg(centerinfo.ftpport));
    centerinfo.plblstatus->setStyleSheet("QLabel { background-color : red; }");

    commonvalues::center_list.append(centerinfo);
    ui->statusBar->addWidget(centerinfo.plblstatus);
}

bool MainWindow::loglinecheck()
{
    bool brtn = false;
    int count = ui->teFTPlog->document()->lineCount();

    if ( count > m_maxlogline )
    {
#ifdef QT_QML_DEBUG
        qDebug() << "teFTPlog clear";
#endif
        brtn = true;
    }
    return brtn;
}

void MainWindow::checkcenterstatus()
{
    int count = commonvalues::center_list.size();

    for(int i = 0; i < count; i++)
    {
        commonvalues::center_list.value(i).plblstatus->setText( QString("%1:%2")
                                        .arg(commonvalues::center_list.at(i).ip).arg(commonvalues::center_list.value(i).ftpport));
        if(commonvalues::center_list.value(i).status )
        {
            commonvalues::center_list.value(i).plblstatus->setStyleSheet("QLabel { background-color : green; }");
            QPixmap pixmap(QPixmap(":/images/blue.png").scaledToHeight(ui->statusBar->height()/2));
            commonvalues::center_list.value(i).plbIcon->setPixmap(pixmap);
            commonvalues::center_list.value(i).plbIcon->setToolTip(tr("연결"));

        }
        else
        {
            commonvalues::center_list.value(i).plblstatus->setStyleSheet("QLabel { background-color : red; }");
            QPixmap pixmap(QPixmap(":/images/red.png").scaledToHeight(ui->statusBar->height()/2));
            commonvalues::center_list.value(i).plbIcon->setPixmap(pixmap);
            commonvalues::center_list.value(i).plbIcon->setToolTip(tr("연결"));
        }
    }
}

void MainWindow::centerdlgview()
{
    pcenterdlg->show();
    pcenterdlg->raise();
}

void MainWindow::configurationdlgview()
{
    pconfigdlg->show();
    pconfigdlg->raise();
}

void MainWindow::logappend(QString logstr)
{
    bool brtn = loglinecheck();

    QString qdt = QDateTime::currentDateTime().toString("[HH:mm:ss:zzz] ") + logstr;

    if(brtn)
    {
        //ui->teFTPlog->setPlainText(qdt);
        ui->teFTPlog->clear();
        ui->teFTPlog->append(qdt);
    }
    else
    {
        ui->teFTPlog->append(qdt);
    }

    QTextCursor cursor(ui->teFTPlog->textCursor());
    cursor.movePosition(QTextCursor::EndOfLine);
    ui->teFTPlog->setTextCursor(cursor);

}

void MainWindow::restart()
{
    this->close();
}

void MainWindow::onTimer()
{
    if( m_mseccount%5 == 0)
    {
        //status bar -> display center connection status
        checkcenterstatus();
        OldTime =  NowTime;
        NowTime = QDate::currentDate();
        if(OldTime.daysTo(NowTime) >= 1)
        {
            //날짜가 바뀌면 파일을 지운다.
            mp_dWorker->doRun();
        }

    }
    m_mseccount++;
}

void MainWindow::closeEvent(QCloseEvent *)
{
    if(pconfigdlg != NULL)pconfigdlg->close();
    if(pcenterdlg != NULL)pcenterdlg->close();

    qTimer->stop();
}

void MainWindow::quitWThread()
{
    mp_wThread = nullptr;
    mp_tWorker = nullptr;
}

void MainWindow::quitDThread()
{
    mp_dThread = nullptr;
    mp_dWorker = nullptr;
}
#if 1
void MainWindow::ftpCommandFinished(int id, bool error)
{
    if (mp_tWorker->m_pftp->currentCommand() == QFtp::Put )
    {
        if(error)
        {
            mp_tWorker->m_iFTPTrans = -1; //fail
#ifdef QT_QML_DEBUG
            qDebug() << mp_tWorker->m_pftp->errorString();
#endif
        }
        else
            mp_tWorker->m_iFTPTrans = 1; //success
    }
    else if (mp_tWorker->m_pftp->currentCommand() == QFtp::Login)
        m_pftp->list();
    else if(mp_tWorker->m_pftp->currentCommand() == QFtp::Rename )
    {
        if(error)
        {
            mp_tWorker->m_iFTPRename = -1; //fail
#ifdef QT_QML_DEBUG
            qDebug() << mp_tWorker->m_pftp->errorString();
#endif
        }
        else
            mp_tWorker->m_iFTPRename = 1; //success
    }
    else if(mp_tWorker->m_pftp->currentCommand() == QFtp::Close )
    {
        if(error)
        {
#ifdef QT_QML_DEBUG
            qDebug() << mp_tWorker->m_pftp->errorString();
#endif
        }

    }
    else if(mp_tWorker->m_pftp->currentCommand() == QFtp::ConnectToHost )
    {
        if(error)
        {
#ifdef QT_QML_DEBUG
            qDebug() << mp_tWorker->m_pftp->errorString();
#endif
        }

    }
    else if(mp_tWorker->m_pftp->currentCommand() == QFtp::Login )
    {
        if(error)
        {
#ifdef QT_QML_DEBUG
            qDebug() << mp_tWorker->m_pftp->errorString();
#endif
        }

    }

}

void MainWindow::ftpStatusChanged(int state)
{
    switch(state){
    case QFtp::Unconnected:
        {
            CenterInfo config = commonvalues::center_list.value(0);
            QString logstr = QString("서버[%1] 연결 끊김").arg(config.ip.trimmed());
            logappend(logstr);
            mp_tWorker->CancelConnection();
            m_pftp = new QFtp();
            QObject::connect(m_pftp, SIGNAL(commandFinished(int,bool)),this, SLOT(ftpCommandFinished(int,bool)));
            QObject::connect(m_pftp, SIGNAL(stateChanged(int)),this, SLOT(ftpStatusChanged(int)));
            QObject::connect(m_pftp, SIGNAL(listInfo(QUrlInfo)),this, SLOT(addToList(QUrlInfo)));
            QObject::connect(mp_tWorker, SIGNAL(localFileUpdate(SendFileInfo *)), this, SLOT(localFileUpdate(SendFileInfo *)));
            QObject::connect(m_pftp, SIGNAL(dataTransferProgress(qint64 ,qint64)),this, SLOT(loadProgress(qint64 ,qint64)));
            mp_tWorker->SetConfig(commonvalues::center_list.value(0),m_pftp);
            ui->remoteFileList->setEnabled(false);
            ui->remoteFileList->clear();
            ui->progressBar->setValue(0);
            ui->reRefreshButton->setEnabled(false);
            commonvalues::center_list[0].status = false;

        }
        break;
    case QFtp::HostLookup:
        mp_tWorker->lastHostlookup = QDateTime::currentDateTime().addDays(-1);
        break;
    case QFtp::Connecting:
        mp_tWorker->lastHostlookup = QDateTime::currentDateTime().addDays(-1);
        break;
    case QFtp::Connected:
        ui->remoteFileList->setEnabled(true);
        ui->reRefreshButton->setEnabled(true);
        commonvalues::center_list[0].status = true;
        break;
    case QFtp::Close:
        //CancelConnection();
        //InitConnection();
        //Connect2FTP();
        break;
    default:
        break;
    }
}

void MainWindow::initFtpReqHandler(QString str)
{
    logappend(str);
    mp_tWorker->CancelConnection();
    m_pftp = new QFtp();
    QObject::connect(m_pftp, SIGNAL(commandFinished(int,bool)),this, SLOT(ftpCommandFinished(int,bool)));
    QObject::connect(m_pftp, SIGNAL(stateChanged(int)),this, SLOT(ftpStatusChanged(int)));
    QObject::connect(m_pftp, SIGNAL(listInfo(QUrlInfo)),this, SLOT(addToList(QUrlInfo)));
    QObject::connect(mp_tWorker, SIGNAL(localFileUpdate(SendFileInfo *)), this, SLOT(localFileUpdate(SendFileInfo *)));
    QObject::connect(m_pftp, SIGNAL(dataTransferProgress(qint64 ,qint64)),this, SLOT(loadProgress(qint64 ,qint64)));
    mp_tWorker->SetConfig(commonvalues::center_list.value(0),m_pftp);
    commonvalues::center_list[0].status = false;
}

void MainWindow::addToList(const QUrlInfo &urlInfo)
{
    QTreeWidgetItem *item = new QTreeWidgetItem;
    item->setText(0, urlInfo.name());
    item->setText(1, QString::number(urlInfo.size()));
    item->setText(2, urlInfo.lastModified().toString("MMM dd yyyy"));

    QPixmap pixmap(urlInfo.isDir() ? ":/images/dir.png" : ":/images/file.png");
    item->setIcon(0, pixmap);

    isDirectory[urlInfo.name()] = urlInfo.isDir();
    ui->remoteFileList->addTopLevelItem(item);
    if (!ui->remoteFileList->currentItem()) {
        ui->remoteFileList->setCurrentItem(ui->remoteFileList->topLevelItem(0));
        ui->remoteFileList->setEnabled(true);
    }
}

void MainWindow::localFileUpdate(SendFileInfo * pItem)
{
    if(pItem == nullptr)
    {
        ui->localFileList->clear();
    }
    else
    {
        QTreeWidgetItem *item = new QTreeWidgetItem;
        item->setText(0, pItem->filename);
        QString filepath = QString("%1").arg(pItem->filepath);
        item->setText(1, QString::number(QFile(filepath).size()));
        QFile file(filepath);
        QFileInfo fileInfo;
        fileInfo.setFile(file);
        QDateTime created = fileInfo.lastModified();
        item->setText(2, created.toString("MMM dd yyyy"));

        QPixmap pixmap(fileInfo.isDir() ? ":/images/dir.png" : ":/images/file.png");
        item->setIcon(0, pixmap);

        ui->localFileList->addTopLevelItem(item);
        if (!ui->localFileList->currentItem()) {
            ui->localFileList->setCurrentItem(ui->localFileList->topLevelItem(0));
            ui->localFileList->setEnabled(true);
        }
        pItem = nullptr;

    }

}
void MainWindow::loadProgress(qint64 bytesSent, qint64 bytesTotal)    //Update progress bar
{
    ui->label_Byte->setText(QString("%1 / %2").arg(bytesSent).arg(bytesTotal));
    ui->progressBar->setMaximum(bytesTotal); //Max
    ui->progressBar->setValue(bytesSent);  //The current value
}
#endif

void MainWindow::on_reRefreshButton_clicked()
{
    if(QString::compare(commonvalues::TransferType, "FTP") == 0)
    {
        if(m_pftp)
        {
            QFtp::State cur_state = m_pftp->state();
            if(cur_state != QFtp::Unconnected)
            {
                ui->remoteFileList->clear();
                m_pftp->list();  //서버측의 리스트를 업데이트 한다.
            }
        }
    }
    else if(QString::compare(commonvalues::TransferType, "SFTP") == 0)
    {
        ui->remoteFileList->clear();
        mp_stWorker->remoteConnect();
    }
}

void MainWindow::remoteFileUpdate(QString rfname, QString rfsize, QDateTime rftime, bool isDir)
{
    QTreeWidgetItem *item = new QTreeWidgetItem;

    item->setText(0, rfname);
    item->setText(1, rfsize);
    item->setText(2, rftime.toString("MMM dd yyyy"));

    if(rfname == NULL)
    {
        ui->remoteFileList->clear();
        commonvalues::center_list[0].status = false;
        ui->remoteFileList->setEnabled(false);
        ui->reRefreshButton->setEnabled(false);
        return;
    }
    else
    {
        commonvalues::center_list[0].status = true;
        ui->remoteFileList->setEnabled(true);
        ui->reRefreshButton->setEnabled(true);
    }

    QPixmap pixmap(isDir ? ":/images/dir.png" : ":/images/file.png");
    item->setIcon(0, pixmap);

    ui->remoteFileList->addTopLevelItem(item);
    if (!ui->remoteFileList->currentItem()) {
        ui->remoteFileList->setCurrentItem(ui->remoteFileList->topLevelItem(0));
        ui->remoteFileList->setEnabled(true);
    }
}
