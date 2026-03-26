#include "serverapplication.h"

#include <QDebug>
#include <QJsonObject>
#include <QTcpSocket>

#include "apiprotocol.h"
#include "databasemanager.h"
#include "gamemanager.h"

namespace {

bool requireObjectPayload(const QJsonObject& request, QJsonObject& payload,
                          QString& error) {
  const QJsonValue rawPayload = request.value(QStringLiteral("payload"));
  if (rawPayload.isUndefined()) {
    payload = {};
    return true;
  }

  if (!rawPayload.isObject()) {
    error = QStringLiteral("`payload` must be a JSON object.");
    return false;
  }

  payload = rawPayload.toObject();
  return true;
}

bool extractRequiredString(const QJsonObject& object, const QString& key,
                           QString& value, QString& error) {
  if (!object.value(key).isString()) {
    error = QStringLiteral("`%1` must be a string.").arg(key);
    return false;
  }

  value = object.value(key).toString().trimmed();
  if (value.isEmpty()) {
    error = QStringLiteral("`%1` must not be empty.").arg(key);
    return false;
  }

  return true;
}

bool extractRequiredInt(const QJsonObject& object, const QString& key,
                        int& value, QString& error) {
  if (!protocol::jsonValueToInt(object.value(key), value)) {
    error = QStringLiteral("`%1` must be an integer.").arg(key);
    return false;
  }

  return true;
}

QJsonObject authPayload(const DatabaseManager::UserInfo& user,
                        const DatabaseManager::UserStats& stats,
                        const QString& currentGameId) {
  QJsonObject payload{
      {QStringLiteral("user"), toJson(user)},
      {QStringLiteral("statistics"), toJson(stats)},
  };

  if (!currentGameId.isEmpty()) {
    payload.insert(QStringLiteral("currentGameId"), currentGameId);
  }

  return payload;
}

}  // namespace

ServerApplication::ServerApplication(QObject* parent) : QObject(parent) {
  connect(&m_server, &QTcpServer::newConnection, this,
          &ServerApplication::onNewConnection);
  connect(&m_server, &QTcpServer::acceptError, this,
          &ServerApplication::onServerError);
}

bool ServerApplication::start(const QHostAddress& address, quint16 port,
                              const QString& databasePath) {
  QString databaseError;
  if (!DatabaseManager::instance().initialize(databasePath, databaseError)) {
    qCritical("Failed to initialize database %s: %s", qPrintable(databasePath),
              qPrintable(databaseError));
    return false;
  }

  if (!m_server.listen(address, port)) {
    qCritical("Failed to start server on port %hu: %s", port,
              qPrintable(m_server.errorString()));
    return false;
  }

  qInfo("Battleship server is listening on %s:%hu",
        qPrintable(m_server.serverAddress().toString()), m_server.serverPort());
  qInfo("SQLite database: %s", qPrintable(databasePath));
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
    connect(socket, &QTcpSocket::errorOccurred, this,
            &ServerApplication::onClientError);

    m_clients.insert(socket, ClientContext{});

    qInfo("Client connected: %s:%hu",
          qPrintable(socket->peerAddress().toString()), socket->peerPort());
  }
}

void ServerApplication::onClientReadyRead() {
  auto* socket = qobject_cast<QTcpSocket*>(sender());
  if (socket == nullptr) {
    return;
  }

  ClientContext* context = contextFor(socket);
  if (context == nullptr) {
    return;
  }

  while (socket->canReadLine()) {
    const protocol::MessageParseResult parseResult =
        protocol::parseMessageLine(socket->readLine());
    if (!parseResult.ok) {
      sendError(socket, QString(), QStringLiteral("invalid_json"),
                parseResult.error);
      continue;
    }

    handleRequest(socket, parseResult.message);
  }
}

void ServerApplication::onClientDisconnected() {
  auto* socket = qobject_cast<QTcpSocket*>(sender());
  if (socket == nullptr) {
    return;
  }

  const ClientContext context = m_clients.take(socket);
  qInfo("Client disconnected: %s:%hu",
        qPrintable(socket->peerAddress().toString()), socket->peerPort());
  if (context.userId != 0) {
    qInfo("Disconnected user: %s", qPrintable(context.username));
  }
  socket->deleteLater();
}

void ServerApplication::onClientError(
    QAbstractSocket::SocketError socketError) {
  Q_UNUSED(socketError);

  auto* socket = qobject_cast<QTcpSocket*>(sender());
  if (socket == nullptr) {
    return;
  }

  qWarning("Client socket error from %s:%hu: %s",
           qPrintable(socket->peerAddress().toString()), socket->peerPort(),
           qPrintable(socket->errorString()));
}

void ServerApplication::onServerError(
    QAbstractSocket::SocketError socketError) {
  Q_UNUSED(socketError);

  qCritical("Server accept error: %s", qPrintable(m_server.errorString()));
}

QString ServerApplication::resolveGameId(const ClientContext& context,
                                         const QJsonObject& payload) const {
  const QString explicitGameId =
      payload.value(QStringLiteral("gameId")).toString().trimmed();
  if (!explicitGameId.isEmpty()) {
    return explicitGameId;
  }

  if (context.userId != 0) {
    return m_gameManager.currentGameForUser(context.userId);
  }

  return context.currentGameId;
}

void ServerApplication::authorizeClient(ClientContext* context,
                                        const DatabaseManager::UserInfo& user) {
  if (context == nullptr) {
    return;
  }

  context->userId = user.id;
  context->username = user.username;
  context->currentGameId = m_gameManager.currentGameForUser(user.id);
}

void ServerApplication::clearAuthorization(ClientContext* context) {
  if (context == nullptr) {
    return;
  }

  context->userId = 0;
  context->username.clear();
  context->currentGameId.clear();
}

void ServerApplication::sendJson(QTcpSocket* socket,
                                 const QJsonObject& message) const {
  if (socket == nullptr) {
    return;
  }

  socket->write(protocol::serializeLine(message));
}

void ServerApplication::sendSuccess(QTcpSocket* socket,
                                    const QString& requestId,
                                    const QJsonObject& payload) const {
  sendJson(socket, protocol::makeSuccessResponse(requestId, payload));
}

void ServerApplication::sendError(QTcpSocket* socket, const QString& requestId,
                                  const QString& code,
                                  const QString& message) const {
  sendJson(socket, protocol::makeErrorResponse(requestId, code, message));
}

void ServerApplication::handleRequest(QTcpSocket* socket,
                                      const QJsonObject& request) {
  ClientContext* context = contextFor(socket);
  if (context == nullptr) {
    return;
  }

  const QString requestId =
      request.value(QStringLiteral("requestId")).toString();
  const QString action =
      request.value(QStringLiteral("action")).toString().trimmed();
  if (action.isEmpty()) {
    sendError(socket, requestId, QStringLiteral("invalid_request"),
              QStringLiteral("`action` must be provided."));
    return;
  }

  QJsonObject payload;
  QString payloadError;
  if (!requireObjectPayload(request, payload, payloadError)) {
    sendError(socket, requestId, QStringLiteral("invalid_request"),
              payloadError);
    return;
  }

  auto sendActionResult = [&](const GameManager::ActionResult& result) {
    if (result.ok) {
      sendSuccess(socket, requestId, result.payload);
      return;
    }

    sendError(socket, requestId, result.errorCode, result.errorMessage);
  };

  if (action == QStringLiteral("ping")) {
    sendSuccess(
        socket, requestId,
        QJsonObject{{QStringLiteral("message"), QStringLiteral("pong")}});
    return;
  }

  if (action == QStringLiteral("register")) {
    QString username;
    QString password;
    QString validationError;
    if (!extractRequiredString(payload, QStringLiteral("username"), username,
                               validationError) ||
        !extractRequiredString(payload, QStringLiteral("password"), password,
                               validationError)) {
      sendError(socket, requestId, QStringLiteral("validation_error"),
                validationError);
      return;
    }

    DatabaseManager::UserInfo user;
    QString databaseError;
    if (!DatabaseManager::instance().registerUser(username, password, user,
                                                  databaseError)) {
      sendError(socket, requestId, QStringLiteral("database_error"),
                databaseError);
      return;
    }

    DatabaseManager::UserStats stats;
    if (!DatabaseManager::instance().fetchStatisticsByUserId(user.id, stats,
                                                             databaseError)) {
      sendError(socket, requestId, QStringLiteral("database_error"),
                databaseError);
      return;
    }

    authorizeClient(context, user);
    sendSuccess(socket, requestId,
                authPayload(user, stats, context->currentGameId));
    return;
  }

  if (action == QStringLiteral("login")) {
    QString username;
    QString password;
    QString validationError;
    if (!extractRequiredString(payload, QStringLiteral("username"), username,
                               validationError) ||
        !extractRequiredString(payload, QStringLiteral("password"), password,
                               validationError)) {
      sendError(socket, requestId, QStringLiteral("validation_error"),
                validationError);
      return;
    }

    DatabaseManager::UserInfo user;
    QString databaseError;
    if (!DatabaseManager::instance().authorizeUser(username, password, user,
                                                   databaseError)) {
      sendError(socket, requestId, QStringLiteral("authorization_failed"),
                databaseError);
      return;
    }

    DatabaseManager::UserStats stats;
    if (!DatabaseManager::instance().fetchStatisticsByUserId(user.id, stats,
                                                             databaseError)) {
      sendError(socket, requestId, QStringLiteral("database_error"),
                databaseError);
      return;
    }

    authorizeClient(context, user);
    sendSuccess(socket, requestId,
                authPayload(user, stats, context->currentGameId));
    return;
  }

  if (action == QStringLiteral("logout")) {
    clearAuthorization(context);
    sendSuccess(
        socket, requestId,
        QJsonObject{{QStringLiteral("message"), QStringLiteral("Logged out")}});
    return;
  }

  if (action == QStringLiteral("stats")) {
    if (context->userId == 0 && payload.value(QStringLiteral("username"))
                                    .toString()
                                    .trimmed()
                                    .isEmpty()) {
      sendError(socket, requestId, QStringLiteral("unauthorized"),
                QStringLiteral("Authorize first or pass `username`."));
      return;
    }

    DatabaseManager::UserStats stats;
    QString databaseError;
    const QString username =
        payload.value(QStringLiteral("username")).toString().trimmed();

    const bool ok = !username.isEmpty()
                        ? DatabaseManager::instance().fetchStatisticsByUsername(
                              username, stats, databaseError)
                        : DatabaseManager::instance().fetchStatisticsByUserId(
                              context->userId, stats, databaseError);

    if (!ok) {
      sendError(socket, requestId, QStringLiteral("database_error"),
                databaseError);
      return;
    }

    sendSuccess(socket, requestId,
                QJsonObject{{QStringLiteral("statistics"), toJson(stats)}});
    return;
  }

  if (action == QStringLiteral("list_games")) {
    sendActionResult(m_gameManager.listGames());
    return;
  }

  if (action == QStringLiteral("whoami")) {
    if (context->userId == 0) {
      sendError(socket, requestId, QStringLiteral("unauthorized"),
                QStringLiteral("Authorize first."));
      return;
    }

    DatabaseManager::UserStats stats;
    QString databaseError;
    if (!DatabaseManager::instance().fetchStatisticsByUserId(
            context->userId, stats, databaseError)) {
      sendError(socket, requestId, QStringLiteral("database_error"),
                databaseError);
      return;
    }

    sendSuccess(socket, requestId,
                authPayload(
                    DatabaseManager::UserInfo{
                        .id = context->userId,
                        .username = context->username,
                    },
                    stats, resolveGameId(*context, payload)));
    return;
  }

  if (context->userId == 0) {
    sendError(socket, requestId, QStringLiteral("unauthorized"),
              QStringLiteral("Authorize first."));
    return;
  }

  if (action == QStringLiteral("create_game")) {
    const GameManager::ActionResult result =
        m_gameManager.createGame(GameManager::PlayerIdentity{
            .userId = context->userId, .username = context->username});
    if (result.ok) {
      context->currentGameId =
          result.payload.value(QStringLiteral("gameId")).toString();
    }
    sendActionResult(result);
    return;
  }

  if (action == QStringLiteral("join_game")) {
    QString gameId;
    QString validationError;
    if (!extractRequiredString(payload, QStringLiteral("gameId"), gameId,
                               validationError)) {
      sendError(socket, requestId, QStringLiteral("validation_error"),
                validationError);
      return;
    }

    const GameManager::ActionResult result = m_gameManager.joinGame(
        gameId, GameManager::PlayerIdentity{.userId = context->userId,
                                            .username = context->username});
    if (result.ok) {
      context->currentGameId = gameId;
    }
    sendActionResult(result);
    return;
  }

  if (action == QStringLiteral("place_random_ships")) {
    const QString gameId = resolveGameId(*context, payload);
    if (gameId.isEmpty()) {
      sendError(socket, requestId, QStringLiteral("validation_error"),
                QStringLiteral("No current game is selected."));
      return;
    }

    const GameManager::ActionResult result = m_gameManager.placeShips(
        gameId, context->userId, protocol::generateRandomFleet());
    sendActionResult(result);
    return;
  }

  if (action == QStringLiteral("place_ships")) {
    const QString gameId = resolveGameId(*context, payload);
    if (gameId.isEmpty()) {
      sendError(socket, requestId, QStringLiteral("validation_error"),
                QStringLiteral("No current game is selected."));
      return;
    }

    QVector<protocol::ShipPlacement> placements;
    QString validationError;
    if (!protocol::shipPlacementsFromJson(
            payload.value(QStringLiteral("ships")), placements,
            validationError)) {
      sendError(socket, requestId, QStringLiteral("validation_error"),
                validationError);
      return;
    }

    sendActionResult(
        m_gameManager.placeShips(gameId, context->userId, placements));
    return;
  }

  if (action == QStringLiteral("game_state")) {
    const QString gameId = resolveGameId(*context, payload);
    if (gameId.isEmpty()) {
      sendError(socket, requestId, QStringLiteral("validation_error"),
                QStringLiteral("No current game is selected."));
      return;
    }

    sendActionResult(m_gameManager.gameState(gameId, context->userId));
    return;
  }

  if (action == QStringLiteral("fire")) {
    const QString gameId = resolveGameId(*context, payload);
    if (gameId.isEmpty()) {
      sendError(socket, requestId, QStringLiteral("validation_error"),
                QStringLiteral("No current game is selected."));
      return;
    }

    int x = 0;
    int y = 0;
    QString validationError;
    if (!extractRequiredInt(payload, QStringLiteral("x"), x, validationError) ||
        !extractRequiredInt(payload, QStringLiteral("y"), y, validationError)) {
      sendError(socket, requestId, QStringLiteral("validation_error"),
                validationError);
      return;
    }

    sendActionResult(m_gameManager.fire(gameId, context->userId, x, y));
    return;
  }

  sendError(socket, requestId, QStringLiteral("unknown_action"),
            QStringLiteral("Unsupported action."));
}

ServerApplication::ClientContext* ServerApplication::contextFor(
    QTcpSocket* socket) {
  auto iterator = m_clients.find(socket);
  return iterator == m_clients.end() ? nullptr : &iterator.value();
}
