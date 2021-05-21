#include "commonvalues.h"

commonvalues::commonvalues(QObject *parent) : QObject(parent)
{

}

int commonvalues::center_count = 0;
QList<CenterInfo> commonvalues::center_list;
QList<QThread *> commonvalues::clientlist;
int commonvalues::ftpretry = 1;

int commonvalues::loglevel = 0;
