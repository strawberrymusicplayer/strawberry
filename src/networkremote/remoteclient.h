#ifndef REMOTECLIENT_H
#define REMOTECLIENT_H

#include <QObject>

class RemoteClient : public QObject
{
     Q_OBJECT
public:
    explicit RemoteClient(QObject *parent = nullptr);

signals:

};

#endif // REMOTECLIENT_H
