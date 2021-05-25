#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "config.h"
#include "configdlg.h"
#include "centerdlg.h"
#include "syslogger.h"
#include "QThread"
#include "threadworker.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void init();
    void initaction();
    void init_config();
    void applyconfig2common();
    void init_mainthr();
    bool loglinecheck();
    void checkcenterstatus();

private slots:
    void centerdlgview();
    void configurationdlgview();
    void logappend(QString logstr);
    void restart();

    void onTimer();
    void closeEvent(QCloseEvent *);
    void quitThread();
#if 1
    void ftpCommandFinished(int id,bool error);
    void ftpStatusChanged(int state);
    void initFtpReqHandler(QString str);
    void addToList(const QUrlInfo &urlInfo);
    void localFileUpdate(SendFileInfo *pItem);
    void loadProgress(qint64 bytesSent,qint64 bytesTotal);
#endif
private:
#define Program_Version  "LLPR_EX_FTPClient v1.1.0 (date: 2021/05/24)"
    Ui::MainWindow *ui;

    configdlg *pconfigdlg;
    CenterDlg *pcenterdlg;

    QTimer *qTimer;
    int m_mseccount;
    //class
    config *pcfg;
    Syslogger *plog;
    QThread *mp_Thread;
    ThreadWorker *mp_tWorker;
    QFtp *m_pftp;
    QHash<QString, bool> isDirectory;

    QString logpath;  //로그를 저장하는 패스

    int m_maxlogline;
};

#endif // MAINWINDOW_H
