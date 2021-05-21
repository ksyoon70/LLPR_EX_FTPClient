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

    QString qdt = QDateTime::currentDateTime().toString("[HH:mm:ss:zzz]") + logstr;

    if(brtn)
    {
        ui->teFTPlog->setPlainText(qdt);
    }
    else ui->teFTPlog->append(qdt);

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
            mp_tWorker->SetConfig(commonvalues::center_list.value(0),m_pftp);
            //mp_tWorker->InitConnection();
            //Connect2FTP();
        }
        break;
    case QFtp::HostLookup:
        mp_tWorker->lastHostlookup = QDateTime::currentDateTime().addDays(-1);
        break;
    case QFtp::Connecting:
        mp_tWorker->lastHostlookup = QDateTime::currentDateTime().addDays(-1);
        break;
    case QFtp::Connected:
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
    mp_tWorker->SetConfig(commonvalues::center_list.value(0),m_pftp);
}
#endif
