#include "configdlg.h"
#include "ui_configdlg.h"

configdlg::configdlg(config *confg, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::configdlg)
{
    ui->setupUi(this);

    this->setWindowTitle(QString("Configuration"));
    this->setFixedSize(this->geometry().width(),this->geometry().height());

    cfg = confg;

    folderIcon.addPixmap(style()->standardPixmap(QStyle::SP_DirClosedIcon),
                         QIcon::Normal, QIcon::Off);
    folderIcon.addPixmap(style()->standardPixmap(QStyle::SP_DirOpenIcon),
                         QIcon::Normal, QIcon::On);
    bookmarkIcon.addPixmap(style()->standardPixmap(QStyle::SP_FileIcon));
}

configdlg::~configdlg()
{
    delete ui;
}


void configdlg::on_btnGetdata_clicked()
{
 //   config *cfg = new config();

    QString title = ui->leTitle->text();
    QString name = ui->leName->text();
    QString value = cfg->get(title,name);
    ui->leValue->setText(value);
}
void configdlg::on_btnSetdata_clicked()
{
//    config *cfg = new config();

    QString title = ui->leTitle->text();
    QString name = ui->leName->text();
    QString value = ui->leValue->text();
    if( cfg->set(title,name,value))
    {
//        cfg->save();
        reloadtreewidget();

    }
    else
    {
        qDebug() << "Error Set value ";
    }


}
void configdlg::reloadtreewidget()
{
    disconnect(ui->treeWidget, SIGNAL(itemChanged(QTreeWidgetItem*,int)),
               this, SLOT(updateDomElement(QTreeWidgetItem*,int)));
    ui->treeWidget->clear();

    QDomElement root = cfg->domDocument.documentElement();

    QDomElement child = root.firstChildElement("title");
    while( !child.isNull()) {
        parseElement(child);
        child = child.nextSiblingElement("title");
    }

    connect(ui->treeWidget, SIGNAL(itemChanged(QTreeWidgetItem*,int)),
            this, SLOT(updateDomElement(QTreeWidgetItem*,int)));
}

void configdlg::on_btnLoad_clicked()
{
//    config *cfg = new config();
      cfg->load();
      reloadtreewidget();


}

void configdlg::updateDomElement(QTreeWidgetItem *item, int column)
{
    QDomElement element = domElementForItem.value(item);

    if (!element.isNull()) {
        if (column == 0) {
            element.setAttribute("name",item->text(0));
        }
        else
        {
           element.setAttribute("value",item->text(1));
        }
    }
}

QTreeWidgetItem *configdlg::createItem(const QDomElement &element,
                                      QTreeWidgetItem *parentItem)
{
    QTreeWidgetItem *item;
    if (parentItem) {
        item = new QTreeWidgetItem(parentItem);
    } else {
        item = new QTreeWidgetItem(ui->treeWidget);
    }
    domElementForItem.insert(item, element);
    return item;
}

void configdlg::parseElement(const QDomElement &element,
                              QTreeWidgetItem *parentItem)
{
    QTreeWidgetItem *item = createItem(element, parentItem);

    QString titlename = element.attribute("name");
    if (titlename.isEmpty())
        titlename = QObject::tr("Folder");

    item->setFlags(item->flags() | Qt::ItemIsEditable);
    item->setIcon(0, folderIcon);
    item->setText(0, titlename);

    //bool folded = (element.attribute("folded") != "no");
    ui->treeWidget->setItemExpanded(item, true); //!folded);

    QDomElement child = element.firstChildElement();
    while (!child.isNull()) {
        if ( !child.hasAttribute("value"))
        {
            parseElement(child, item);
        }
        else
        {
            QTreeWidgetItem *childItem = createItem(child, item);

            QString name = child.attribute("name");
            QString value = child.attribute("value");

            childItem->setFlags(item->flags() | Qt::ItemIsEditable);
            childItem->setIcon(0, bookmarkIcon);
            childItem->setText(0, name);
            childItem->setText(1, value);
        }
        child = child.nextSiblingElement();
    }
}

void configdlg::on_btnSave_clicked()
{
    cfg->save();
}



void configdlg::on_btnDelete_clicked()
{
    QList<QTreeWidgetItem *>items = ui->treeWidget->selectedItems();

    int count = items.size();
    for(int i=0; i < count; i++)
    {
        QDomElement element = domElementForItem.value(items.at(i));
        element.parentNode().removeChild((element));

    }

    reloadtreewidget();

}
