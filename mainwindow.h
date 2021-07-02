#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "config.h"
#include "configdlg.h"
#include "centerdlg.h"
#include "syslogger.h"
#include "QThread"
#include "threadworker.h"
#include "deleteworker.h"
#include "sftpthrworker.h"
#include <QDate>
#include <QTcpSocket>
namespace Ui {
class MainWindow;
}

#define Program_Version  "LLPR_EX_FTPClient v1.1.0 (date: 2021/05/24)"
#define FTP_BUFFER (3145728)

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
    void MakeDefaultConfig();
    void init_mainthr();
    bool loglinecheck();
    void checkcenterstatus();
    bool connectSocket(QString host, qint32 port);
    bool Sftp_Init_Session();
    bool sshInit();
    static MainWindow *getMainWinPtr();
    //bool sftpput(QString local, QString remote);

private slots:
    void centerdlgview();
    void configurationdlgview();
    void logappend(QString logstr);
    void restart();

    void onTimer();
    void closeEvent(QCloseEvent *);
    void quitWThread();
    void quitDThread();

    void ftpCommandFinished(int id,bool error);
    void ftpStatusChanged(int state);
    void initFtpReqHandler(QString str);
    void addToList(const QUrlInfo &urlInfo);
    void localFileUpdate(SendFileInfo *pItem);
    void loadProgress(qint64 bytesSent,qint64 bytesTotal);
    void remoteFileUpdate(QString rfname, QString rfsize, QDateTime rftime, bool isDir);
    void disconnected();
    void connected();

    void on_reRefreshButton_clicked();
    int GetFirstNumPosFromFileName(QString filename);
    bool sftpput(QString local, QString remote);

protected:
    void showEvent(QShowEvent *ev);
    void showEventHelper();

private:

    Ui::MainWindow *ui;

    configdlg *pconfigdlg;
    CenterDlg *pcenterdlg;

    QTimer *qTimer;
    int m_mseccount;
    //class
    config *pcfg;
    Syslogger *plog;
    QThread *mp_wThread;
    ThreadWorker *mp_tWorker;
    QFtp *m_pftp;
    QHash<QString, bool> isDirectory;
    QTcpSocket *m_socket;
    LIBSSH2_SESSION volatile *mp_session;
    LIBSSH2_SFTP volatile *mp_sftp_session;
    LIBSSH2_SFTP_HANDLE volatile *mp_sftp_handle;

    QString logpath;  //로그를 저장하는 패스

    int m_maxlogline;

    DeleteWorker *mp_dWorker;
    QThread *mp_dThread;
    QDate NowTime;
    QDate OldTime;

    QThread *mp_swThread;
    SftpThrWorker *mp_stWorker;
    QString logstr;
    int m_Auth_pw;

    static MainWindow * pMainWindow;
    char	*m_lpBuffer;

};

#endif // MAINWINDOW_H
