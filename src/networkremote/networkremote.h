#ifndef NETWORKREMOTE_H
#define NETWORKREMOTE_H

#include <QObject>

class NetworkRemote : public QObject
{
     Q_OBJECT
public:
    explicit NetworkRemote(QObject *parent = nullptr);

signals:

};

#endif // NETWORKREMOTE_H
