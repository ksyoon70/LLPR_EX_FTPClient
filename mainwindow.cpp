#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "commonvalues.h"

#define SFTP_TIMEOUT				(1000)
#define MAX_SFTP_OUTGOING_SIZE      (500000)
MainWindow * MainWindow::pMainWindow = nullptr;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    pMainWindow = this;

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

    m_lpBuffer = new char[FTP_BUFFER + 1];

    init();
}

MainWindow::~MainWindow()
{

    if(mp_dWorker)
    {
        mp_dWorker->Stop();
    }

    if(m_lpBuffer)
        delete [] m_lpBuffer;

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
        m_Auth_pw = 1;
        mp_session = nullptr;

        //m_socket = new QTcpSocket();

        QString title = "CENTER|LIST" + QString::number(0);
        QString ipaddr = pcfg->get(title,"IP");
        QString socketport = pcfg->get(title,"FTPPort");
        mp_stWorker = new SftpThrWorker(ipaddr,socketport.toInt());
        mp_swThread = new QThread();
        mp_stWorker->moveToThread(mp_swThread);

        QObject::connect(mp_swThread, SIGNAL(started()), mp_stWorker, SLOT(doWork()));
        QObject::connect(mp_swThread, SIGNAL(finished()), mp_swThread, SLOT(deleteLater()));
        QObject::connect(mp_stWorker, SIGNAL(finished()), mp_swThread, SLOT(quit()));
        QObject::connect(mp_stWorker, SIGNAL(finished()), this, SLOT(quitWThread()));
        QObject::connect(mp_stWorker, SIGNAL(finished()), mp_stWorker, SLOT(deleteLater()));
        QObject::connect(mp_stWorker, SIGNAL(logappend(QString)),this,SLOT(logappend(QString)));
        QObject::connect(mp_stWorker, SIGNAL(localFileUpdate(SendFileInfo *)), this, SLOT(localFileUpdate(SendFileInfo *)));
        QObject::connect(mp_stWorker, SIGNAL(transferProgress(qint64, qint64)), this, SLOT(loadProgress(qint64,qint64)));
        QObject::connect(mp_stWorker, SIGNAL(remoteFileUpdate(QString, QString, QDateTime, bool)), this, SLOT(remoteFileUpdate(QString, QString, QDateTime, bool)));

        //QObject::connect(m_socket, SIGNAL(connected()), this, SLOT(connected()));
        //QObject::connect(m_socket, SIGNAL(disconnected()), this, SLOT(disconnected()));
        QObject::connect(mp_stWorker->m_socket, SIGNAL(connected()), this, SLOT(connected()),Qt::DirectConnection);
        QObject::connect(mp_stWorker->m_socket, SIGNAL(disconnected()), this, SLOT(disconnected()),Qt::DirectConnection);
        //QObject::connect(mp_stWorker, SIGNAL(sftpput(QString,QString)), this, SLOT(sftpput(QString,QString)));



        //sshInit();
        mp_swThread->start();
    }


    //삭제 쓰레드 생성

    mp_dWorker = new DeleteWorker();
    mp_dThread = new QThread();
    mp_dWorker->SetConfig(&commonvalues::center_list[0]);
    mp_dWorker->moveToThread(mp_dThread);


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

        /*if(commonvalues::SftpSocketConn == false)
        {
            QString title = "CENTER|LIST" + QString::number(0);
            QString ipaddr = pcfg->get(title,"IP");
            //QString socketport = pcfg->get(title,"TCPPort");
            QString socketport = pcfg->get(title,"FTPPort");

            connectSocket(ipaddr, socketport.toInt());
        }*/

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

int MainWindow::GetFirstNumPosFromFileName(QString filename)
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

void MainWindow::showEvent(QShowEvent *ev)
{
    QMainWindow::showEvent(ev);
    showEventHelper();
}

void MainWindow::showEventHelper()
{
    if(QString::compare(commonvalues::TransferType, "SFTP") == 0)
    {
        // sshInit();
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

void MainWindow::connected()
{
    commonvalues::center_list[0].status = true;
    commonvalues::SftpSocketConn = true;

    QString logstr = QString("SFTP : Socket 연결 성공!!");
    logappend(logstr);

    ui->remoteFileList->setEnabled(true);
    ui->reRefreshButton->setEnabled(true);
    //Sftp_Init_Session();
    //mp_swThread->start();
}
void MainWindow::disconnected()
{
    commonvalues::center_list[0].status = false;
    commonvalues::SftpSocketConn = false;

    QString logstr = QString("SFTP : Socket 연결 끊김..");
    logappend(logstr);

    ui->remoteFileList->clear();
    ui->remoteFileList->setEnabled(false);
    ui->reRefreshButton->setEnabled(false);

    //m_socket->disconnectFromHost();
    //m_socket->close();
}

bool MainWindow::connectSocket(QString host, qint32 port)
{
    bool connectState = false;
    if(m_socket->state() == QAbstractSocket::UnconnectedState)
    {
        m_socket->connectToHost(host, port, QAbstractSocket::ReadWrite);
        //m_socket->connectToHost(host, port);
#if 0
        if(m_socket->waitForConnected(3000))
        {

            commonvalues::center_list[0].status = true;
            commonvalues::SftpSocketConn = true;

            QString logstr = QString("SFTP : Socket 연결 성공!!");
            logappend(logstr);

            ui->remoteFileList->setEnabled(true);
            ui->reRefreshButton->setEnabled(true);

            Sftp_Init_Session();

            connectState = true;

            mp_swThread->start();

        }
        else
        {
            commonvalues::center_list[0].status = false;
            commonvalues::SftpSocketConn = false;

            QString logstr = QString("SFTP : Socket 연결 실패..");
            logappend(logstr);

            ui->remoteFileList->clear();
            ui->remoteFileList->setEnabled(false);
            ui->reRefreshButton->setEnabled(false);

            m_socket->disconnectFromHost();
            m_socket->close();
            connectState = false;

        }
#endif
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

bool MainWindow::sshInit()
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

MainWindow *MainWindow::getMainWinPtr()
{
    return pMainWindow;
}

bool MainWindow::sftpput(QString local, QString remote)
{
    int32_t	nByteRead;
    uint32_t dwNumberOfBytesWritten = 0;
    uint32_t	fileLength, nSumOfnByteRead;
    //MSG		message;
    bool	canceled = false;
    QString errMsg;
    QString tmpRemote;
    QString renamedRemoteFullPath;		//원격 전체 path
    QString remoteFullPath;		//원격 전체 path
    int nByteWrite;
    bool status = true;

    QString FTP_SEND_PATH = commonvalues::FileSearchPath;
#if 1

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

#else
    TCHAR firstChar;

    int len = remote.GetLength();
    int nPos;
    //앞이 문자이면 그 부분을 삭제하고 전송 후 문자를 붙여준다. by 윤경섭 20.07.02
    for(nPos = 0; nPos < len; nPos++)
    {
        firstChar = remote.GetAt(nPos);
        if(isdigit(firstChar))
        {
            break;
        }
    }

    if(nPos < len)
    {
        TCHAR *p = (LPSTR)(LPCTSTR)remote;
        tmpRemote.Format("%s",p + nPos);
    }
    else
    {
        tmpRemote.Format("%s",remote);
    }


    //확장자를 바꾼다.
    PathRenameExtension((LPTSTR)(LPCTSTR)tmpRemote,_T(".tmp"));

    if(_tcslen(INI_FTP_CONNECT_REMDIR) == 0)
    {
        tmpRemoteFullPath.Format("%s",tmpRemote);
        remoteFullPath.Format("%s",remote);
    }
    else
    {
        tmpRemoteFullPath.Format("%s/%s",INI_FTP_CONNECT_REMDIR,tmpRemote);
        remoteFullPath.Format("%s/%s",INI_FTP_CONNECT_REMDIR,remote);
    }

    if(fileSize == 0) return FALSE;

    m_sendFileName = remote;
    ::PostMessage(this->GetSafeHwnd(),JWWM_FTP_CALLBACK,CON_STATE_SEND_FILENAME, NULL);
    // UpdateData(FALSE);


    g_eventStop.ResetEvent();	// 전송도중 중지하기 위해 쓰는 이벤트 변수 초기화
#endif

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

bool MainWindow::Sftp_Init_Session()
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
