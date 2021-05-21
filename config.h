#ifndef CONFIG_H
#define CONFIG_H

#include <QObject>
#include <QDebug>
#include <QApplication>
#include <QFile>
#include <QDir>
#include <QDomDocument>
#include <QHash>
#include <QTextStream>
#include <QFile>


//using try ~ catch
#include <iostream>
#include <execinfo.h>
using namespace std;

class config : public QObject
{
    Q_OBJECT
public:
    explicit config(QObject *parent = 0);
    explicit config(QString path, QObject *parent = 0);
    bool check();
    bool create();
    bool load();
    bool save();
    QString get(QString title,QString name);
    bool getuint(QString title, QString name,uint *value);
    bool getbool(QString title, QString name,bool *value);
    bool getfloat(QString title, QString name,float *value);
    bool set(QString title,QString name,QString value);
    bool setuint(QString title, QString name,uint value);
    bool setbool(QString title, QString name,bool value);
    bool setfloat(QString title, QString name,float value);
    bool search(QDomElement *source, QString name);
    bool addtitle(QDomElement *root, QString name);
    bool remove(QString title);



private:
    QFile *FileOpen(QString filename);
    void FileClose(QFile *file);

signals:

public slots:

public:
    QString  filepath;
    QDomDocument domDocument;
};

#endif // CONFIG_H
