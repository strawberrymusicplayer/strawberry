#ifndef INCOMINGMSG_H
#define INCOMINGMSG_H

#include <QObject>
#include <QByteArray>
#include <string>

// Forward declarations
class QTcpSocket;

namespace nw { namespace remote { class Message; } }

class NetworkRemoteIncomingMsg : public QObject
{
  Q_OBJECT
public:
  explicit NetworkRemoteIncomingMsg(QObject *parent = nullptr);
  ~NetworkRemoteIncomingMsg(); 
  void Init(QTcpSocket* socket);
  void SetMsgType();
  qint32 GetMsgType();

private Q_SLOTS:
  void ReadyRead();

Q_SIGNALS:
  void InMsgParsed();

private:
  nw::remote::Message *msg_;
  QTcpSocket *socket_;
  long bytesIn_;
  QByteArray msgStream_;
  std::string msgString_;
  qint32 msgType_;
};

#endif // INCOMINGMSG_H
