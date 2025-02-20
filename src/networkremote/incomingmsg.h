#ifndef INCOMINGMSG_H
#define INCOMINGMSG_H

#include <QObject>
#include <QByteArray>
#include <string>

// Forward declarations
class QTcpSocket;
class Application;
namespace nw { namespace remote { class Message; } }

class NetworkRemoteIncomingMsg : public QObject
{
  Q_OBJECT
public:
  explicit NetworkRemoteIncomingMsg(Application *app, QObject *parent = nullptr);
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
  Application *app_;
  qint32 msgType_;
};

#endif // INCOMINGMSG_H
