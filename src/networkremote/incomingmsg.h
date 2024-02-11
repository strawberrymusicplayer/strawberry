#ifndef INCOMINGMSG_H
#define INCOMINGMSG_H

#include <QObject>

class IncomingMsg : public QObject
{
     Q_OBJECT
public:
    explicit IncomingMsg(QObject *parent = nullptr);

signals:

};

#endif // INCOMINGMSG_H
