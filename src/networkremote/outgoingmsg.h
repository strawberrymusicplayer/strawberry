#ifndef OUTGOINGMSG_H
#define OUTGOINGMSG_H

#include <QObject>

class OutgoingMsg : public QObject
{
     Q_OBJECT
public:
    explicit OutgoingMsg(QObject *parent = nullptr);

signals:

};

#endif // OUTGOINGMSG_H
