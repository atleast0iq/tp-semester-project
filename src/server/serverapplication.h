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

  struct RequestContext {
    QTcpSocket* socket = nullptr;
    ClientContext* client = nullptr;
    QString requestId;
    QString action;
    QJsonObject payload;
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
  void sendEvent(QTcpSocket* socket, const QString& eventName,
                 const QJsonObject& payload) const;
  QJsonObject makeGameUpdatePayload(const QString& gameId, qint64 viewerUserId,
                                    const QString& reason,
                                    const QJsonObject& details = {}) const;
  void notifyPlacementUpdate(QTcpSocket* sourceSocket, const QString& gameId,
                             const GameManager::ActionResult& result);
  void notifyFireUpdate(QTcpSocket* sourceSocket, const QString& gameId,
                        const GameManager::ActionResult& result);
  void notifyGameParticipants(const QString& gameId, const QString& reason,
                              const QJsonObject& details = {},
                              QTcpSocket* excludeSocket = nullptr);
  void sendActionResult(const RequestContext& request,
                        const GameManager::ActionResult& result) const;
  bool requireAuthorizedUser(const RequestContext& request) const;
  bool resolveRequiredGameId(const RequestContext& request,
                             QString& gameId) const;
  bool handlePublicAction(const RequestContext& request);
  bool handleAuthorizedAction(const RequestContext& request);
  void handlePing(const RequestContext& request) const;
  void handleRegister(const RequestContext& request);
  void handleLogin(const RequestContext& request);
  void handleLogout(const RequestContext& request);
  void handleStats(const RequestContext& request) const;
  void handleListGames(const RequestContext& request) const;
  void handleWhoAmI(const RequestContext& request) const;
  void handleCreateGame(const RequestContext& request);
  void handleJoinGame(const RequestContext& request);
  void handlePlaceRandomShips(const RequestContext& request);
  void handlePlaceShips(const RequestContext& request);
  void handleGameState(const RequestContext& request) const;
  void handleFire(const RequestContext& request);
  void handleRequest(QTcpSocket* socket, const QJsonObject& request);
  ClientContext* contextFor(QTcpSocket* socket);

  QTcpServer m_server;
  QHash<QTcpSocket*, ClientContext> m_clients;
  GameManager m_gameManager;
};
