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

    ui->progressBar->setValue(0);

    init();
}

MainWindow::~MainWindow()
{    
    delete qTimer;

    delete ui;
}

void MainWindow::init()
{
    pcfg = new config();
    init_config();    

    plog = new Syslogger(this,"mainwindow",true,commonvalues::loglevel);
    QString logstr = QString("Program Start(%1)").arg(Program_Version);
    plog->write(logstr,LOG_ERR); qDebug() << logstr;



    //pmainthr = new mainthread(plog);

    pcenterdlg = new CenterDlg(pcfg);
    pconfigdlg = new configdlg(pcfg);

    mp_tWorker = new ThreadWorker();
    mp_Thread = new QThread();
    m_pftp = new QFtp();
    mp_tWorker->moveToThread(mp_Thread);
    //설정값 저장


    QObject::connect(mp_Thread, SIGNAL(started()), mp_tWorker, SLOT(doWork()));
    QObject::connect(mp_tWorker, SIGNAL(finished()), mp_Thread, SLOT(quit()));
    QObject::connect(mp_tWorker, SIGNAL(finished()), this, SLOT(quitThread()));
    QObject::connect(mp_tWorker, SIGNAL(finished()), mp_tWorker, SLOT(deleteLater()));
    QObject::connect(mp_Thread, SIGNAL(finished()), mp_Thread, SLOT(deleteLater()));
    QObject::connect(mp_tWorker, SIGNAL(logappend(QString)),this,SLOT(logappend(QString)));
    QObject::connect(m_pftp, SIGNAL(commandFinished(int,bool)),this, SLOT(ftpCommandFinished(int,bool)));
    QObject::connect(m_pftp, SIGNAL(stateChanged(int)),this, SLOT(ftpStatusChanged(int)));
    QObject::connect(mp_tWorker, SIGNAL(initFtpReq(QString)),this, SLOT(initFtpReqHandler(QString)));
    QObject::connect(m_pftp, SIGNAL(listInfo(QUrlInfo)),this, SLOT(addToList(QUrlInfo)));
    QObject::connect(mp_tWorker, SIGNAL(localFileUpdate(SendFileInfo *)), this, SLOT(localFileUpdate(SendFileInfo *)));
    QObject::connect(m_pftp, SIGNAL(dataTransferProgress(qint64 ,qint64)),this, SLOT(loadProgress(qint64 ,qint64)));

    initaction();

    mp_tWorker->SetConfig(commonvalues::center_list.value(0),m_pftp);
    mp_Thread->start();

    //init_mainthr();

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
    bool bvalue;
    uint uivalue;
    float fvalue;

    int i=0;
    QString title = "CENTER|LIST" + QString::number(i);
    while( ( svalue = pcfg->get(title,"IP") ) != NULL)
    {
        CenterInfo centerinfo;
        if( ( svalue = pcfg->get(title,"CenterName") ) != NULL ) { centerinfo.centername = svalue; }
        if( ( svalue = pcfg->get(title,"IP") ) != NULL ) { centerinfo.ip = svalue; }
        if( pcfg->getuint(title,"TCPPort",&uivalue)) { centerinfo.tcpport = uivalue; }
        if( pcfg->getuint(title,"AliveInterval",&uivalue)) { centerinfo.connectioninterval = uivalue; }
        if( pcfg->getuint(title,"ProtocolType",&uivalue)) { centerinfo.protocol_type = uivalue; }
        if( pcfg->getuint(title,"FTPPort",&uivalue)) { centerinfo.ftpport = uivalue; }
        if( ( svalue = pcfg->get(title,"FTPUserID") ) != NULL ) { centerinfo.userID = svalue; }
        if( ( svalue = pcfg->get(title,"FTPPassword") ) != NULL ) { centerinfo.password = svalue; }
        if( ( svalue = pcfg->get(title,"FTPPath") ) != NULL ) { centerinfo.ftpPath = svalue; }
        if( pcfg->getuint(title,"FileNameSelect",&uivalue)) { centerinfo.fileNameSelect = uivalue; }

        centerinfo.plblstatus = new QLabel();
        centerinfo.plblstatus->setText( QString("%1:%2")
                    .arg(centerinfo.ip).arg(centerinfo.ftpport));
        centerinfo.plblstatus->setStyleSheet("QLabel { background-color : red; }");

        commonvalues::center_list.append(centerinfo);

        i++;
        title = "CENTER|LIST" + QString::number(i);
    }

    if( pcfg->getuint("CENTER","FTPRetry",&uivalue)) { commonvalues::ftpretry = uivalue; }


  /*       Log Level value             */
   if( pcfg->getuint("LOG","Level",&uivalue)) { commonvalues::loglevel = uivalue; }

}

bool MainWindow::loglinecheck()
{
    bool brtn = false;
    int count = ui->teFTPlog->document()->lineCount();

    if ( count > m_maxlogline )
    {
        qDebug() << "teFTPlog clear";
        brtn = true;
    }
    return brtn;
}

void MainWindow::checkcenterstatus()
{
    int count = commonvalues::center_list.size();

    if( commonvalues::center_count != count )
    {
        if(commonvalues::center_count < count)
        {
            for(int i=commonvalues::center_count; i < count; i++)
            {
                ui->statusBar->addWidget(commonvalues::center_list.value(i).plblstatus);
            }

        }
        commonvalues::center_count = count;
    }

    count = commonvalues::center_list.size();
    for(int i = 0; i < count; i++)
    {
        commonvalues::center_list.value(i).plblstatus->setText( QString("%1:%2")
                                        .arg(commonvalues::center_list.at(i).ip).arg(commonvalues::center_list.value(i).ftpport));
        if(commonvalues::center_list.value(i).status )
        {
            commonvalues::center_list.value(i).plblstatus->setStyleSheet("QLabel { background-color : green; }");

        }
        else
        {
            commonvalues::center_list.value(i).plblstatus->setStyleSheet("QLabel { background-color : red; }");
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
    }
    m_mseccount++;
}

void MainWindow::closeEvent(QCloseEvent *)
{
    if(pconfigdlg != NULL)pconfigdlg->close();
    if(pcenterdlg != NULL)pcenterdlg->close();

    qTimer->stop();
}

void MainWindow::on_connectButton_clicked()
{
    mp_tWorker = new ThreadWorker();
    mp_Thread = new QThread();
    mp_tWorker->moveToThread(mp_Thread);
    m_pftp = new QFtp();
    //설정값 저장
    mp_tWorker->SetConfig(commonvalues::center_list.value(0),m_pftp);

    QObject::connect(mp_Thread, SIGNAL(started()), mp_tWorker, SLOT(doWork()));
    QObject::connect(mp_tWorker, SIGNAL(finished()), mp_Thread, SLOT(quit()));
    QObject::connect(mp_tWorker, SIGNAL(finished()), this, SLOT(quitThread()));
    QObject::connect(mp_tWorker, SIGNAL(finished()), mp_tWorker, SLOT(deleteLater()));
    QObject::connect(mp_Thread, SIGNAL(finished()), mp_Thread, SLOT(deleteLater()));
    QObject::connect(mp_tWorker, SIGNAL(logappend(QString)),this,SLOT(logappend(QString)));
    QObject::connect(m_pftp, SIGNAL(listInfo(QUrlInfo)),this, SLOT(addToList(QUrlInfo)));
    QObject::connect(mp_tWorker, SIGNAL(localFileUpdate(SendFileInfo *)), this, SLOT(localFileUpdate(SendFileInfo *)));
    QObject::connect(m_pftp, SIGNAL(dataTransferProgress(qint64 ,qint64)),this, SLOT(loadProgress(qint64 ,qint64)));
    mp_Thread->start();
}

void MainWindow::quitThread()
{
    mp_Thread = nullptr;
    mp_tWorker = nullptr;
    //m_pftp = nullptr;
}
#if 1
void MainWindow::ftpCommandFinished(int id, bool error)
{
    if (mp_tWorker->m_pftp->currentCommand() == QFtp::Put )
    {
        if(error)
        {
            mp_tWorker->m_iFTPTrans = -1; //fail
            qDebug() << mp_tWorker->m_pftp->errorString();
            //CancelConnection();
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
            qDebug() << mp_tWorker->m_pftp->errorString();
            //CancelConnection();
        }
        else
            mp_tWorker->m_iFTPRename = 1; //success
    }
    else if(mp_tWorker->m_pftp->currentCommand() == QFtp::Close )
    {
        if(error)
        {
            qDebug() << mp_tWorker->m_pftp->errorString();
        }

        //CancelConnection();

    }
    else if(mp_tWorker->m_pftp->currentCommand() == QFtp::ConnectToHost )
    {
        if(error)
        {
            qDebug() << mp_tWorker->m_pftp->errorString();
            //CancelConnection();
        }

    }
    else if(mp_tWorker->m_pftp->currentCommand() == QFtp::Login )
    {
        if(error)
        {
            qDebug() << mp_tWorker->m_pftp->errorString();
            //CancelConnection();
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
