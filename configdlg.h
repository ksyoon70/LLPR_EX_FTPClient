#ifndef CONFIGDLG_H
#define CONFIGDLG_H

#include <QDialog>
#include <QtWidgets>
#include <QDomDocument>
#include <QHash>
#include <QIcon>

#include "config.h"

namespace Ui {
class configdlg;
}

class configdlg : public QDialog
{
    Q_OBJECT

public:
    explicit configdlg(config *confg, QWidget *parent = 0);
    ~configdlg();
    QTreeWidgetItem *createItem(const QDomElement &element,
                                    QTreeWidgetItem *parentItem = 0);
    void parseElement(const QDomElement &element,
                      QTreeWidgetItem *parentItem = 0);
    void reloadtreewidget();

public slots:
    void on_btnGetdata_clicked();
    void on_btnLoad_clicked();
    void on_btnSave_clicked();
    void on_btnSetdata_clicked();
    void updateDomElement(QTreeWidgetItem *item, int column);
    void on_btnDelete_clicked();

private:
    Ui::configdlg *ui;
    config *cfg;
    QHash<QTreeWidgetItem *, QDomElement> domElementForItem;
    QIcon folderIcon;
    QIcon bookmarkIcon;
};

#endif // CONFIGDLG_H
