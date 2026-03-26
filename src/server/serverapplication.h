#pragma once

#include <QHash>
#include <QHostAddress>
#include <QJsonObject>
#include <QObject>
#include <QTcpServer>

#include "databasemanager.h"
#include "gamemanager.h"

class QTcpSocket;

class ServerApplication final : public QObject {
  Q_OBJECT

 public:
  explicit ServerApplication(QObject* parent = nullptr);

  bool start(const QHostAddress& address, quint16 port,
             const QString& databasePath);

 private slots:
  void onNewConnection();
  void onClientReadyRead();
  void onClientDisconnected();
  void onClientError(QAbstractSocket::SocketError socketError);
  void onServerError(QAbstractSocket::SocketError socketError);

 private:
  struct ClientContext {
    qint64 userId = 0;
    QString username;
    QString currentGameId;
  };

  QString resolveGameId(const ClientContext& context,
                        const QJsonObject& payload) const;
  void authorizeClient(ClientContext* context,
                       const DatabaseManager::UserInfo& user);
  void clearAuthorization(ClientContext* context);
  void sendJson(QTcpSocket* socket, const QJsonObject& message) const;
  void sendSuccess(QTcpSocket* socket, const QString& requestId,
                   const QJsonObject& payload = {}) const;
  void sendError(QTcpSocket* socket, const QString& requestId,
                 const QString& code, const QString& message) const;
  void handleRequest(QTcpSocket* socket, const QJsonObject& request);
  ClientContext* contextFor(QTcpSocket* socket);

  QTcpServer m_server;
  QHash<QTcpSocket*, ClientContext> m_clients;
  GameManager m_gameManager;
};
