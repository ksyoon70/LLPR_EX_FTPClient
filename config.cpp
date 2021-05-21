#include "config.h"

config::config(QObject *parent) :
    QObject(parent)
{
    QString path = QApplication::applicationDirPath() + "/config/";
    filepath = path +"/config.xbel";
}
config::config(QString path, QObject *parent):
    QObject(parent)
{
    filepath = path;
}

//check config file
bool config::check()
{
    QFile check_file(filepath);
    if( check_file.exists() )
    {
        return true;
    }

    return false;
}

bool config::create()
{
    //check & create directory
    int i = filepath.lastIndexOf("/");
    QString dir = filepath.left(i);
    QDir mdir(dir);
    if(!mdir.exists())
    {
         mdir.mkpath(dir);
         qDebug() <<"Create Directory:" << dir;
    }

    //check & create file
    QFile file;
    file.setFileName(filepath);
    if(!file.exists())
    {
        if( file.open(QIODevice::Append | QIODevice::Text | QIODevice::Truncate))
        {
            QString data = "<?xml version='1.0' encoding='UTF-8'?>\n";
            data += "<xml>\n";
            data += "</xml>\n";
            file.write(data.toUtf8());
            file.close();

            return true;
        }
        else
        {
            qDebug("file open error");
            return false;
        }
    }

    qDebug("file exists");
    return false;

}

bool config::load()
{
    try
    {
        QFile *device = FileOpen(filepath);

        QString errorStr;
        int errorLine;
        int errorColumn;
        if (!domDocument.setContent(device, true, &errorStr, &errorLine,
                   &errorColumn))
        {
                    qDebug() << "load Error :"
                                << "Parse error at line " << errorLine
                                << ", column " << errorColumn
                                << ":\n" << errorStr ;
                   // return false;
        }
        FileClose(device);
    }
    catch( exception ex)
    {
        qDebug() << ex.what();
        return false;
    }
    return true;
}

bool config::save()
{
    try
    {
        QFile *device = FileOpen(filepath);
        const int IndentSize = 4;
        // QDomDocument domdoc = QDomDocument( domDocument );
        QTextStream out(device);
        domDocument.save(out, IndentSize);
        //out << domDocument.toString();
        FileClose(device);
    }
    catch (exception ex)
    {
        qDebug() << ex.what();
        return false;
    }
    return true;
}

QString config::get( QString title,QString name )
{

    QStringList titlelist = title.split("|");
    titlelist.append(name);
    int listcount = titlelist.size();
    QString data;

    //load config file
//    load();

    if( listcount > 1)
    {
       // QDomElement root = domDocument.documentElement();
        QDomElement child = domDocument.documentElement();
        for(int index=0; index < listcount; index++ )
        {
            if( !search(&child,titlelist[index]) )
            {
                return NULL;
            }
        }
        data = child.attribute("value");
        return data;

    }
    return NULL;
}

bool config::getuint(QString title, QString name,uint *value)
{
    QStringList titlelist = title.split("|");
    titlelist.append(name);
    int listcount = titlelist.size();
    QString data;

    //load config file
//    load();

    if( listcount > 1)
    {
       // QDomElement root = domDocument.documentElement();
        QDomElement child = domDocument.documentElement();
        for(int index=0; index < listcount; index++ )
        {
            if( !search(&child,titlelist[index]) )
            {
                return false;
            }
        }
        data = child.attribute("value");

        *value = data.toUInt();

        return true;

    }
    return false;
}

bool config::getbool(QString title, QString name,bool *value)
{
    QStringList titlelist = title.split("|");
    titlelist.append(name);
    int listcount = titlelist.size();
    QString data;

    //load config file
//    load();

    if( listcount > 1)
    {
       // QDomElement root = domDocument.documentElement();
        QDomElement child = domDocument.documentElement();
        for(int index=0; index < listcount; index++ )
        {
            if( !search(&child,titlelist[index]) )
            {
                return false;
            }
        }
        data = child.attribute("value");

        if( data == "True")
        {
            *value = true;
        }
        else *value = false;

        return true;

    }
    return false;
}

bool config::getfloat(QString title, QString name, float *value)
{
    QStringList titlelist = title.split("|");
    titlelist.append(name);
    int listcount = titlelist.size();
    QString data;

    //load config file
//    load();

    if( listcount > 1)
    {
       // QDomElement root = domDocument.documentElement();
        QDomElement child = domDocument.documentElement();
        for(int index=0; index < listcount; index++ )
        {
            if( !search(&child,titlelist[index]) )
            {
                return false;
            }
        }
        data = child.attribute("value");

        *value = data.toFloat();

        return true;

    }
    return false;
}


bool config::addtitle(QDomElement *root, QString name)
{
    QDomElement el = domDocument.createElement("title");
    root->appendChild(el);
    el.setAttribute("name",name);

    *root = el;
    return true;
}

bool config::set(QString title,QString name,QString value)
{
    QStringList titlelist = title.split("|");
    titlelist.append(name);
    int listcount = titlelist.size();
    QString data;

    //load config file
//    load();

    if( listcount > 1)
    {
       // QDomElement root = domDocument.documentElement();
        QDomElement child = domDocument.documentElement();
        for(int index=0; index < listcount; index++ )
        {
            QDomElement orgchild = child;
            if( !search(&child,titlelist[index]) )
            {

                if( addtitle(&orgchild,titlelist[index]))
                {
                    child = orgchild;
                }
                else return false;

            }
        }
        if( !value.isNull())
        {
            child.setAttribute("value", value);
        }
        return true;

    }
    return false;
}

bool config::setuint(QString title,QString name,uint value)
{
    QStringList titlelist = title.split("|");
    titlelist.append(name);
    int listcount = titlelist.size();
    QString data;

    //load config file
//    load();

    if( listcount > 1)
    {
       // QDomElement root = domDocument.documentElement();
        QDomElement child = domDocument.documentElement();
        for(int index=0; index < listcount; index++ )
        {
            QDomElement orgchild = child;
            if( !search(&child,titlelist[index]) )
            {

                if( addtitle(&orgchild,titlelist[index]))
                {
                    child = orgchild;
                }
                else return false;

            }
        }

        child.setAttribute("value", QString::number(value));

        return true;

    }
    return false;
}

bool config::setbool(QString title,QString name,bool value)
{
    QStringList titlelist = title.split("|");
    titlelist.append(name);
    int listcount = titlelist.size();
    QString data;

    //load config file
//    load();

    if( listcount > 1)
    {
       // QDomElement root = domDocument.documentElement();
        QDomElement child = domDocument.documentElement();
        for(int index=0; index < listcount; index++ )
        {
            QDomElement orgchild = child;
            if( !search(&child,titlelist[index]) )
            {

                if( addtitle(&orgchild,titlelist[index]))
                {
                    child = orgchild;
                }
                else return false;

            }
        }
        if(value)
        {
            child.setAttribute("value", "True");
        }
        else child.setAttribute("value", "False");

        return true;

    }
    return false;
}

bool config::setfloat(QString title, QString name, float value)
{
    QStringList titlelist = title.split("|");
    titlelist.append(name);
    int listcount = titlelist.size();
    QString data;

    //load config file
//    load();

    if( listcount > 1)
    {
       // QDomElement root = domDocument.documentElement();
        QDomElement child = domDocument.documentElement();
        for(int index=0; index < listcount; index++ )
        {
            QDomElement orgchild = child;
            if( !search(&child,titlelist[index]) )
            {

                if( addtitle(&orgchild,titlelist[index]))
                {
                    child = orgchild;
                }
                else return false;

            }
        }

        child.setAttribute("value", QString::number(value,'f',2));

        return true;

    }
    return false;
}
/////////////////////////
QFile *config::FileOpen(QString filename)
{
    QFile *file = new QFile(filename);
    if (!file->open(QFile::ReadWrite | QFile::Text)) {
  /*      QMessageBox::warning(this, tr("SAX Bookmarks"),
                             tr("Cannot read file %1:\n%2.")
                             .arg(fileName)
                             .arg(file.errorString())); */
        qDebug("file open Error");
        return NULL;
    }
    return file;
}
void config::FileClose(QFile *file)
{
    file->close();
    // //2016/11/11
    delete file;
}

bool config::search(QDomElement *source, QString name)
{
    QString data;
    *source = source->firstChildElement("title");
    while( !source->isNull())
    {
        data = source->attribute("name");
        if( data == name )
        {
            return true;
        }
        *source = source->nextSiblingElement("title");
    }
    return false;
}

bool config::remove(QString title)
{
    QStringList titlelist = title.split("|");
    int listcount = titlelist.size();
    QString data;

    //load config file
//    load();

    if( listcount > 0)
    {
       // QDomElement root = domDocument.documentElement();
        QDomElement child = domDocument.documentElement();
        for(int index=0; index < listcount; index++ )
        {
            if( !search(&child,titlelist[index]) )
            {
                return false;
            }
        }
        child.parentNode().removeChild((child));
        return true;

    }
    return false;

}

