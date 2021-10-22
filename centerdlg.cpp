#include "centerdlg.h"
#include "ui_centerdlg.h"
#include "commonvalues.h"

CenterDlg::CenterDlg(config *pcfg, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CenterDlg)
{
    ui->setupUi(this);

    this->setWindowTitle(QString("Center"));
    this->setFixedSize(this->geometry().width(),this->geometry().height());

    ui->leFTPPath->setVisible(false);

    pmodel = new QStringListModel(this);

    //mainthr = pmainthr;
    cfg = pcfg;


}

CenterDlg::~CenterDlg()
{
    delete ui;
}

void CenterDlg::showEvent(QShowEvent *e)
{
    configload();
    QDialog::showEvent(e);
}

void CenterDlg::configload()
{
    int count = commonvalues::center_list.size();

    QStringList clist;
    for(int i=0; i < count; i++)
    {
        clist << commonvalues::center_list[i].ip;
    }

    pmodel->setStringList(clist);
    ui->lvCenterList->setModel(pmodel);

//    ui->leLaneID->setText(QString("%1").arg(commonvalues::LaneID,3,10,QChar('0')));
//    ui->leManagerID->setText(QString("%1").arg(commonvalues::ManagerID,4,10,QChar('0')));
//    ui->leDirection->setText(commonvalues::LandDirection);
    ui->chkFTPRetry->setChecked(commonvalues::ftpretry == 0x00 ? false : true);
}

void CenterDlg::configsave()
{
    bool brtn = cfg->remove("CENTER");

    int centercount = commonvalues::center_list.size();
    if( centercount > 0)
    {
        for(int i=0; i < centercount; i++)
        {
            QString title = "CENTER|LIST" + QString::number(i);

            cfg->set(title,"TransferType",commonvalues::center_list[i].transfertype);
            cfg->set(title,"CenterName",commonvalues::center_list[i].centername);
            cfg->set(title,"IP",commonvalues::center_list[i].ip);
            cfg->setuint(title,"TCPPort",commonvalues::center_list[i].tcpport);
            cfg->setuint(title,"AliveInterval",commonvalues::center_list[i].connectioninterval);
            cfg->setuint(title,"FTPPort",commonvalues::center_list[i].ftpport);
            cfg->set(title,"FTPUserID",commonvalues::center_list[i].userID);
            cfg->set(title,"FTPPassword",commonvalues::center_list[i].password);
            cfg->set(title,"FTPPath",commonvalues::center_list[i].ftpPath);
            cfg->setuint(title,"FileNameSelect",commonvalues::center_list[i].fileNameSelect);
        }
    }
    cfg->setuint("CENTER","FTPRetry",commonvalues::ftpretry);

//    cfg->setuint("Local","LaneID",commonvalues::LaneID);
//    cfg->setuint("Local","ManagerID",commonvalues::ManagerID);
//    cfg->set("Local","LandDirection",commonvalues::LandDirection);


    cfg->save();
}

//void CenterDlg::on_btnInsert_clicked()
//{
//    int irow = pmodel->rowCount();

//    QString strcenter = ui->leCenterName->text();
//    QString strip = ui->leIP->text();
//    QString strtcpport = ui->leTCPPort->text();
//    QString strinterval = ui->leAliveInterval->text();
//    QString strftpport = ui->leFTPPort->text();
//    QString strftpuserID = ui->leFTPUser->text();
//    QString strftppass = ui->leFTPPassword->text();
//    int ifilenameselect = ui->chkFileName_HX->isChecked() ? 0x01 : 0x00;

//    if( (strip == "") || (strtcpport == "") || (strcenter == "") || (strftpport == ""))
//    {
//        qDebug() << "Error Value";

//        return;
//    }

//    pmodel->insertRow(irow);
//    QModelIndex index = pmodel->index(irow);
//    ui->lvCenterList->setCurrentIndex(index);
//    ui->lvCenterList->edit(index);

//    CenterInfo cinfo;
//    cinfo.centername = strcenter;
//    cinfo.ip = strip;
//    cinfo.tcpport = strtcpport.toInt();
//    cinfo.connectioninterval = strinterval.toInt();
//    cinfo.ftpport = strftpport.toInt();
//    cinfo.userID = strftpuserID;
//    cinfo.password = strftppass;
//    cinfo.ftpPath = "";
//    cinfo.fileNameSelect = ifilenameselect;

//    cinfo.plblstatus = new QLabel();
//    cinfo.plblstatus->setText( QString("%1:%2")
//                .arg(cinfo.ip).arg(cinfo.tcpport));
//    cinfo.plblstatus->setStyleSheet("QLabel { background-color : red; }");

//    commonvalues::center_list.append(cinfo);

////    QString test = ui->leLaneID->text();
////    commonvalues::LaneID = ui->leLaneID->text().toUInt();
////    test = ui->leManagerID->text();
////    commonvalues::ManagerID = ui->leManagerID->text().toUInt();
////    test = ui->leDirection->text();
////    commonvalues::LandDirection = ui->leDirection->text();
//    commonvalues::ftpretry = ui->chkFTPRetry->isChecked() ? 0x01 : 0x00;


//    configload();
//    //config modify
//    configsave();
//}

//void CenterDlg::on_btnModify_clicked()
//{
//    int irow = ui->lvCenterList->currentIndex().row();

//    //2016/11/03
//    if( irow >= 0)
//    {
//        QString strip = ui->leIP->text();
//        pmodel->setData(ui->lvCenterList->currentIndex(),strip);

//        commonvalues::center_list[irow].centername = ui->leCenterName->text().trimmed();
//        commonvalues::center_list[irow].ip = ui->leIP->text().trimmed();
//        commonvalues::center_list[irow].tcpport = ui->leTCPPort->text().toInt();
//        commonvalues::center_list[irow].connectioninterval = ui->leAliveInterval->text().toInt();
//        commonvalues::center_list[irow].ftpport = ui->leFTPPort->text().toInt();
//        commonvalues::center_list[irow].userID = ui->leFTPUser->text().trimmed();
//        commonvalues::center_list[irow].password = ui->leFTPPassword->text().trimmed();
//        commonvalues::center_list[irow].ftpPath = ui->leFTPPath->text().trimmed();
//        commonvalues::center_list[irow].fileNameSelect = ui->chkFileName_HX->isChecked() ? 0x01 : 0x00;

////        QString test = ui->leLaneID->text();
////        commonvalues::LaneID = ui->leLaneID->text().toUInt();
////        test = ui->leManagerID->text();
////        commonvalues::ManagerID = ui->leManagerID->text().toUInt();
////        test = ui->leDirection->text();
////        commonvalues::LandDirection = ui->leDirection->text();

//        commonvalues::ftpretry = ui->chkFTPRetry->isChecked() ? 0x01 : 0x00;

//        //config modify
//        configsave();
//    }
//}

//void CenterDlg::on_btnRemove_clicked()
//{
//    int irow = ui->lvCenterList->currentIndex().row();
//    //2016/11/03
//    if( irow >= 0)
//    {
//        pmodel->removeRow(irow);

//        commonvalues::center_list[irow].plblstatus->close();
//        commonvalues::center_list[irow].plblstatus->deleteLater();

//        commonvalues::center_list.removeAt(irow);
//        //config remove
//        configsave();
//    }
//}

void CenterDlg::on_lvCenterList_clicked(const QModelIndex &index)
{
    if( !index.isValid()) return;

    int irow = index.row();

    ui->leTransferType->setText( commonvalues::center_list[irow].transfertype);
    ui->leCenterName->setText( commonvalues::center_list[irow].centername );
    ui->leIP->setText( commonvalues::center_list[irow].ip );
    ui->leTCPPort->setText( QString::number(commonvalues::center_list[irow].tcpport));
    ui->leAliveInterval->setText( QString::number(commonvalues::center_list[irow].connectioninterval));
    ui->cbCommType->setCurrentIndex(commonvalues::center_list[irow].protocol_type );
    ui->leFTPPort->setText( QString::number(commonvalues::center_list[irow].ftpport));
    ui->leFTPUser->setText( commonvalues::center_list[irow].userID );
    ui->leFTPPassword->setText( commonvalues::center_list[irow].password );
    ui->chkFileName_HX->setChecked( commonvalues::center_list[irow].fileNameSelect == 0 ? false : true );
}
