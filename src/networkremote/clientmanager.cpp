#include "clientmanager.h"
#include "client.h"
#include "core/application.h"
#include "core/logging.h"

NetworkRemoteClientManager::NetworkRemoteClientManager(Application *app, QObject *parent)
  : QObject(parent),
    app_(app),
    clients_()
{
}

NetworkRemoteClientManager::~NetworkRemoteClientManager()
{
  qDeleteAll(clients_);
  clients_.clear();
}

void NetworkRemoteClientManager::AddClient(QTcpSocket *socket)
{
  qLog(Debug) << "New Client connection +++++++++++++++";
  QObject::connect(socket, &QAbstractSocket::errorOccurred, this, &NetworkRemoteClientManager::Error);
  QObject::connect(socket, &QAbstractSocket::stateChanged, this, &NetworkRemoteClientManager::StateChanged);
  NetworkRemoteClient *client = new NetworkRemoteClient(app_);
  client->Init(socket);
  clients_.append(client);  // clients_ is now a QList, not a pointer
  QObject::connect(client, &NetworkRemoteClient::ClientIsLeaving, this, [this, client]() {
  RemoveClient(client);
});
qLog(Debug) << "Socket State is " << socket->state();
qLog(Debug) << "There are now +++++++++++++++" << clients_.count() << "clients connected";
}

void NetworkRemoteClientManager::RemoveClient(NetworkRemoteClient *client)
{
  if (clients_.removeOne(client)) {
    client->deleteLater();
  }  
  qLog(Debug) << "There are now +++++++++++++++" << clients_.count() << "clients connected";
}

void NetworkRemoteClientManager::Error(QAbstractSocket::SocketError socketError)
{
  QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
  if (!socket) return;
  switch (socketError) {
    case QAbstractSocket::RemoteHostClosedError:
      qLog(Debug) << "Remote Host closed";
      break;
    case QAbstractSocket::HostNotFoundError:
      qLog(Debug) << "The host was not found. Please check the host name and port settings.";
      break;
    case QAbstractSocket::ConnectionRefusedError:
      qLog(Debug) << "The connection was refused by the peer.";
      break;
    default:
      qLog(Debug) << "The following error occurred:" << socket->errorString();
  }
}

void NetworkRemoteClientManager::StateChanged()
{
  QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
  if (!socket) return;
  qLog(Debug) << socket->state();
  qLog(Debug) << "State Changed";
  if (socket->state() == QAbstractSocket::UnconnectedState) {
    for (NetworkRemoteClient *client : clients_) {
      if (client->GetSocket() == socket) {
        RemoveClient(client);
        break;
      }
    }
  }
}
