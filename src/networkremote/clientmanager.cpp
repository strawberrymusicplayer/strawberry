#include "clientmanager.h"
#include "core/application.h"
#include "core/logging.h"


ClientManager::ClientManager(Application *app, QObject *parent)
    : QObject{parent},
      app_(app)
{
    clients_ = new QVector<Client*>;
}

ClientManager::~ClientManager()
{}

void ClientManager::AddClient(QTcpSocket *socket)
{
  qLog(Debug) << "New Client connection +++++++++++++++";
  socket_ = socket;
  QObject::connect(socket_, &QAbstractSocket::errorOccurred, this, &ClientManager::Error);
  QObject::connect(socket_, &QAbstractSocket::stateChanged, this, &ClientManager::StateChanged);

  client_ = new Client(app_);
  client_->Init(socket_);
  clients_->append(client_);
  QObject::connect(client_, &Client::ClientIsLeaving, this, &ClientManager::RemoveClient);

  qLog(Debug) << "Socket State is " << socket_->state();;
  qLog(Debug) << "There are now +++++++++++++++" << clients_->count() << "clients connected";
}

void ClientManager::RemoveClient()
{
  for (Client* client : *clients_)  {
    if (client->GetSocket() == socket_){
      clients_->removeAt(clients_->indexOf(client));
      client->deleteLater();
    }
  }
  socket_->close();

  qLog(Debug) << "There are now +++++++++++++++" << clients_->count() << "clients connected";
}

void ClientManager::Ready()
{
  qLog(Debug) << "Socket Ready";
}

void ClientManager::Error(QAbstractSocket::SocketError socketError)
{
    switch (socketError) {
    case QAbstractSocket::RemoteHostClosedError:
        qLog(Debug) << "Remote Host closed";
        break;
    case QAbstractSocket::HostNotFoundError:
        qLog(Debug) << "The host was not found. Please check the host name and port settings.";
        break;
    case QAbstractSocket::ConnectionRefusedError:
        qLog(Debug) << "The connection was refused by the peer. ";
        break;
    default:
        qLog(Debug)  << "The following error occurred: %1." << socket_->errorString();
    }
}

void ClientManager::StateChanged()
{
  qLog(Debug) << socket_->state();
  qLog(Debug) << "State Changed";
  if (socket_->state() == QAbstractSocket::UnconnectedState){
    RemoveClient();
  }
}
