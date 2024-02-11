#include "clientmanager.h"
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
  qLog(Debug) << "There are now +++++++++++++++" << clients_->count() << "clients connected";
}

void ClientManager::RemoveClient()
{
  socket_->close();
  //int pos = clients_->indexOf(client_->so);
  //clients_->removeAll(pos);
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
        RemoveClient();
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
  qLog(Debug) << "State Changed";
}
