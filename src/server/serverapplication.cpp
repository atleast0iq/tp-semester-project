#include "serverapplication.h"

#include <QDebug>
#include <QHostAddress>
#include <QTcpSocket>

ServerApplication::ServerApplication(QObject* parent) : QObject(parent) {
  connect(&m_server, &QTcpServer::newConnection, this,
          &ServerApplication::onNewConnection);
  connect(&m_server, &QTcpServer::acceptError, this,
          &ServerApplication::onServerError);
}

bool ServerApplication::start(quint16 port) {
  if (!m_server.listen(QHostAddress::AnyIPv4, port)) {
    qCritical("Failed to start server on port %hu: %s", port,
              qPrintable(m_server.errorString()));
    return false;
  }

  qInfo("Echo-server is listening on %s:%hu",
        qPrintable(m_server.serverAddress().toString()), m_server.serverPort());
  return true;
}

void ServerApplication::onNewConnection() {
  while (m_server.hasPendingConnections()) {
    QTcpSocket* socket = m_server.nextPendingConnection();
    if (socket == nullptr) {
      continue;
    }

    connect(socket, &QTcpSocket::readyRead, this,
            &ServerApplication::onClientReadyRead);
    connect(socket, &QTcpSocket::disconnected, this,
            &ServerApplication::onClientDisconnected);

    qInfo("Client connected: %s:%hu",
          qPrintable(socket->peerAddress().toString()), socket->peerPort());
  }
}

void ServerApplication::onClientReadyRead() {
  auto* socket = qobject_cast<QTcpSocket*>(sender());
  if (socket == nullptr) {
    return;
  }

  const QByteArray payload = socket->readAll();
  if (payload.isEmpty()) {
    return;
  }

  qInfo("Received %lld bytes from %s:%hu",
        static_cast<long long>(payload.size()),
        qPrintable(socket->peerAddress().toString()), socket->peerPort());
  socket->write(payload);
}

void ServerApplication::onClientDisconnected() {
  auto* socket = qobject_cast<QTcpSocket*>(sender());
  if (socket == nullptr) {
    return;
  }

  qInfo("Client disconnected: %s:%hu",
        qPrintable(socket->peerAddress().toString()), socket->peerPort());
  socket->deleteLater();
}

void ServerApplication::onServerError(
    QAbstractSocket::SocketError socketError) {
  Q_UNUSED(socketError);

  qCritical("Server accept error: %s", qPrintable(m_server.errorString()));
}
