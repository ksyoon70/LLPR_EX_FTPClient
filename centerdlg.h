#ifndef CENTERDLG_H
#define CENTERDLG_H

#include <QDialog>
#include <QStringListModel>

//#include "mainthread.h"
#include "config.h"

namespace Ui {
class CenterDlg;
}

class CenterDlg : public QDialog
{
    Q_OBJECT

public:
    explicit CenterDlg( config *pcfg, QWidget *parent = 0);
    ~CenterDlg();
    void showEvent(QShowEvent *e);
    void configload();
    void configsave();

private slots:
//    void on_btnInsert_clicked();
//    void on_btnModify_clicked();
//    void on_btnRemove_clicked();
    void on_lvCenterList_clicked(const QModelIndex &index);

private:
    Ui::CenterDlg *ui;
    //mainthread *mainthr;
    config *cfg;
    QStringListModel *pmodel;
};

#endif // CENTERDLG_H
