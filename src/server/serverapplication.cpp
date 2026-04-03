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

QJsonObject mergeObjects(QJsonObject base, const QJsonObject& extra) {
  for (auto iterator = extra.begin(); iterator != extra.end(); ++iterator) {
    base.insert(iterator.key(), iterator.value());
  }

  return base;
}

QString shotCoordinatesText(const QJsonObject& result) {
  if (!result.contains(QStringLiteral("x")) || !result.contains(QStringLiteral("y"))) {
    return {};
  }

  return QStringLiteral(" (%1, %2)")
      .arg(result.value(QStringLiteral("x")).toInt())
      .arg(result.value(QStringLiteral("y")).toInt());
}

QString buildGameUpdateMessage(const QString& reason,
                               const QJsonObject& payload) {
  const QJsonObject game = payload.value(QStringLiteral("game")).toObject();
  const QJsonObject result = payload.value(QStringLiteral("result")).toObject();
  const bool yourTurn = game.value(QStringLiteral("yourTurn")).toBool();

  if (reason == QStringLiteral("opponent_joined")) {
    return QStringLiteral(
        "Opponent joined the game. Place ships to start the match.");
  }

  if (reason == QStringLiteral("opponent_ready")) {
    return QStringLiteral(
        "Opponent finished ship placement. Complete your setup to start.");
  }

  if (reason == QStringLiteral("game_started")) {
    return yourTurn
               ? QStringLiteral(
                     "Both players are ready. The game started and it is your turn.")
               : QStringLiteral(
                     "Both players are ready. The game started; wait for the opponent move.");
  }

  if (reason == QStringLiteral("opponent_fired")) {
    const QString coordinates = shotCoordinatesText(result);
    if (result.value(QStringLiteral("hit")).toBool()) {
      return QStringLiteral("Opponent fired%1 and hit your ship.")
          .arg(coordinates);
    }

    return yourTurn
               ? QStringLiteral("Opponent fired%1 and missed. It is your turn.")
                     .arg(coordinates)
               : QStringLiteral("Opponent fired%1. Wait for your turn.")
                     .arg(coordinates);
  }

  if (reason == QStringLiteral("game_finished")) {
    const QJsonObject you = game.value(QStringLiteral("you")).toObject();
    const QJsonObject winner = game.value(QStringLiteral("winner")).toObject();
    const qint64 yourUserId =
        you.value(QStringLiteral("userId")).toVariant().toLongLong();
    const qint64 winnerUserId =
        winner.value(QStringLiteral("userId")).toVariant().toLongLong();

    if (winnerUserId != 0 && winnerUserId == yourUserId) {
      return QStringLiteral("The game is over. You won.");
    }

    const QString winnerName = winner.value(QStringLiteral("username")).toString();
    return winnerName.isEmpty()
               ? QStringLiteral("The game is over.")
               : QStringLiteral("The game is over. Winner: %1.").arg(winnerName);
  }

  return QStringLiteral("Game state changed.");
}

QString gameStatusFromPayload(const QJsonObject& payload) {
  return payload.value(QStringLiteral("game"))
      .toObject()
      .value(QStringLiteral("status"))
      .toString();
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

void ServerApplication::sendEvent(QTcpSocket* socket, const QString& eventName,
                                  const QJsonObject& payload) const {
  sendJson(socket, protocol::makeEventMessage(eventName, payload));
}

QJsonObject ServerApplication::makeGameUpdatePayload(
    const QString& gameId, qint64 viewerUserId, const QString& reason,
    const QJsonObject& details) const {
  const GameManager::ActionResult stateResult =
      m_gameManager.gameState(gameId, viewerUserId);
  if (!stateResult.ok) {
    return {};
  }

  QJsonObject payload = mergeObjects(stateResult.payload, details);
  payload.insert(QStringLiteral("reason"), reason);
  payload.insert(QStringLiteral("message"),
                 buildGameUpdateMessage(reason, payload));
  return payload;
}

void ServerApplication::notifyGameParticipants(const QString& gameId,
                                               const QString& reason,
                                               const QJsonObject& details,
                                               QTcpSocket* excludeSocket) {
  const QVector<qint64> participants = m_gameManager.participantUserIds(gameId);
  if (participants.isEmpty()) {
    return;
  }

  for (auto iterator = m_clients.cbegin(); iterator != m_clients.cend();
       ++iterator) {
    if (iterator.key() == excludeSocket || iterator.value().userId == 0 ||
        !participants.contains(iterator.value().userId)) {
      continue;
    }

    const QJsonObject payload =
        makeGameUpdatePayload(gameId, iterator.value().userId, reason, details);
    if (!payload.isEmpty()) {
      sendEvent(iterator.key(), QStringLiteral("game_updated"), payload);
    }
  }
}

void ServerApplication::notifyPlacementUpdate(
    QTcpSocket* sourceSocket, const QString& gameId,
    const GameManager::ActionResult& result) {
  if (!result.ok) {
    return;
  }

  notifyGameParticipants(
      gameId,
      gameStatusFromPayload(result.payload) == QStringLiteral("active")
          ? QStringLiteral("game_started")
          : QStringLiteral("opponent_ready"),
      {}, sourceSocket);
}

void ServerApplication::notifyFireUpdate(
    QTcpSocket* sourceSocket, const QString& gameId,
    const GameManager::ActionResult& result) {
  if (!result.ok) {
    return;
  }

  notifyGameParticipants(
      gameId,
      gameStatusFromPayload(result.payload) == QStringLiteral("finished")
          ? QStringLiteral("game_finished")
          : QStringLiteral("opponent_fired"),
      QJsonObject{{QStringLiteral("result"),
                   result.payload.value(QStringLiteral("result")).toObject()}},
      sourceSocket);
}

void ServerApplication::sendActionResult(
    const RequestContext& request,
    const GameManager::ActionResult& result) const {
  if (result.ok) {
    sendSuccess(request.socket, request.requestId, result.payload);
    return;
  }

  sendError(request.socket, request.requestId, result.errorCode,
            result.errorMessage);
}

bool ServerApplication::requireAuthorizedUser(
    const RequestContext& request) const {
  if (request.client != nullptr && request.client->userId != 0) {
    return true;
  }

  sendError(request.socket, request.requestId, QStringLiteral("unauthorized"),
            QStringLiteral("Authorize first."));
  return false;
}

bool ServerApplication::resolveRequiredGameId(const RequestContext& request,
                                              QString& gameId) const {
  if (request.client == nullptr) {
    return false;
  }

  gameId = resolveGameId(*request.client, request.payload);
  if (!gameId.isEmpty()) {
    return true;
  }

  sendError(request.socket, request.requestId, QStringLiteral("validation_error"),
            QStringLiteral("No current game is selected."));
  return false;
}

bool ServerApplication::handlePublicAction(const RequestContext& request) {
  if (request.action == QStringLiteral("ping")) {
    handlePing(request);
    return true;
  }

  if (request.action == QStringLiteral("register")) {
    handleRegister(request);
    return true;
  }

  if (request.action == QStringLiteral("login")) {
    handleLogin(request);
    return true;
  }

  if (request.action == QStringLiteral("logout")) {
    handleLogout(request);
    return true;
  }

  if (request.action == QStringLiteral("stats")) {
    handleStats(request);
    return true;
  }

  if (request.action == QStringLiteral("list_games")) {
    handleListGames(request);
    return true;
  }

  if (request.action == QStringLiteral("whoami")) {
    handleWhoAmI(request);
    return true;
  }

  return false;
}

bool ServerApplication::handleAuthorizedAction(const RequestContext& request) {
  if (request.action == QStringLiteral("create_game")) {
    handleCreateGame(request);
    return true;
  }

  if (request.action == QStringLiteral("join_game")) {
    handleJoinGame(request);
    return true;
  }

  if (request.action == QStringLiteral("place_random_ships")) {
    handlePlaceRandomShips(request);
    return true;
  }

  if (request.action == QStringLiteral("place_ships")) {
    handlePlaceShips(request);
    return true;
  }

  if (request.action == QStringLiteral("game_state")) {
    handleGameState(request);
    return true;
  }

  if (request.action == QStringLiteral("fire")) {
    handleFire(request);
    return true;
  }

  return false;
}

void ServerApplication::handlePing(const RequestContext& request) const {
  sendSuccess(
      request.socket, request.requestId,
      QJsonObject{{QStringLiteral("message"), QStringLiteral("pong")}});
}

void ServerApplication::handleRegister(const RequestContext& request) {
  QString username;
  QString password;
  QString validationError;
  if (!extractRequiredString(request.payload, QStringLiteral("username"),
                             username, validationError) ||
      !extractRequiredString(request.payload, QStringLiteral("password"),
                             password, validationError)) {
    sendError(request.socket, request.requestId, QStringLiteral("validation_error"),
              validationError);
    return;
  }

  DatabaseManager::UserInfo user;
  QString databaseError;
  if (!DatabaseManager::instance().registerUser(username, password, user,
                                                databaseError)) {
    sendError(request.socket, request.requestId, QStringLiteral("database_error"),
              databaseError);
    return;
  }

  DatabaseManager::UserStats stats;
  if (!DatabaseManager::instance().fetchStatisticsByUserId(user.id, stats,
                                                           databaseError)) {
    sendError(request.socket, request.requestId, QStringLiteral("database_error"),
              databaseError);
    return;
  }

  authorizeClient(request.client, user);
  sendSuccess(request.socket, request.requestId,
              authPayload(user, stats, request.client->currentGameId));
}

void ServerApplication::handleLogin(const RequestContext& request) {
  QString username;
  QString password;
  QString validationError;
  if (!extractRequiredString(request.payload, QStringLiteral("username"),
                             username, validationError) ||
      !extractRequiredString(request.payload, QStringLiteral("password"),
                             password, validationError)) {
    sendError(request.socket, request.requestId, QStringLiteral("validation_error"),
              validationError);
    return;
  }

  DatabaseManager::UserInfo user;
  QString databaseError;
  if (!DatabaseManager::instance().authorizeUser(username, password, user,
                                                 databaseError)) {
    sendError(request.socket, request.requestId,
              QStringLiteral("authorization_failed"), databaseError);
    return;
  }

  DatabaseManager::UserStats stats;
  if (!DatabaseManager::instance().fetchStatisticsByUserId(user.id, stats,
                                                           databaseError)) {
    sendError(request.socket, request.requestId, QStringLiteral("database_error"),
              databaseError);
    return;
  }

  authorizeClient(request.client, user);
  sendSuccess(request.socket, request.requestId,
              authPayload(user, stats, request.client->currentGameId));
}

void ServerApplication::handleLogout(const RequestContext& request) {
  clearAuthorization(request.client);
  sendSuccess(
      request.socket, request.requestId,
      QJsonObject{{QStringLiteral("message"), QStringLiteral("Logged out")}});
}

void ServerApplication::handleStats(const RequestContext& request) const {
  const QString username =
      request.payload.value(QStringLiteral("username")).toString().trimmed();
  if ((request.client == nullptr || request.client->userId == 0) &&
      username.isEmpty()) {
    sendError(request.socket, request.requestId, QStringLiteral("unauthorized"),
              QStringLiteral("Authorize first or pass `username`."));
    return;
  }

  DatabaseManager::UserStats stats;
  QString databaseError;
  const bool ok =
      username.isEmpty()
          ? DatabaseManager::instance().fetchStatisticsByUserId(
                request.client->userId, stats, databaseError)
          : DatabaseManager::instance().fetchStatisticsByUsername(
                username, stats, databaseError);
  if (!ok) {
    sendError(request.socket, request.requestId, QStringLiteral("database_error"),
              databaseError);
    return;
  }

  sendSuccess(request.socket, request.requestId,
              QJsonObject{{QStringLiteral("statistics"), toJson(stats)}});
}

void ServerApplication::handleListGames(const RequestContext& request) const {
  sendActionResult(request, m_gameManager.listGames());
}

void ServerApplication::handleWhoAmI(const RequestContext& request) const {
  if (request.client == nullptr || request.client->userId == 0) {
    sendError(request.socket, request.requestId, QStringLiteral("unauthorized"),
              QStringLiteral("Authorize first."));
    return;
  }

  DatabaseManager::UserStats stats;
  QString databaseError;
  if (!DatabaseManager::instance().fetchStatisticsByUserId(
          request.client->userId, stats, databaseError)) {
    sendError(request.socket, request.requestId, QStringLiteral("database_error"),
              databaseError);
    return;
  }

  sendSuccess(
      request.socket, request.requestId,
      authPayload(DatabaseManager::UserInfo{.id = request.client->userId,
                                            .username = request.client->username},
                  stats, resolveGameId(*request.client, request.payload)));
}

void ServerApplication::handleCreateGame(const RequestContext& request) {
  const GameManager::ActionResult result =
      m_gameManager.createGame(GameManager::PlayerIdentity{
          .userId = request.client->userId, .username = request.client->username});
  if (result.ok) {
    request.client->currentGameId =
        result.payload.value(QStringLiteral("gameId")).toString();
  }

  sendActionResult(request, result);
}

void ServerApplication::handleJoinGame(const RequestContext& request) {
  QString gameId;
  QString validationError;
  if (!extractRequiredString(request.payload, QStringLiteral("gameId"), gameId,
                             validationError)) {
    sendError(request.socket, request.requestId, QStringLiteral("validation_error"),
              validationError);
    return;
  }

  const GameManager::ActionResult result = m_gameManager.joinGame(
      gameId, GameManager::PlayerIdentity{.userId = request.client->userId,
                                          .username = request.client->username});
  if (result.ok) {
    request.client->currentGameId = gameId;
  }

  sendActionResult(request, result);
  if (result.ok) {
    notifyGameParticipants(gameId, QStringLiteral("opponent_joined"), {},
                           request.socket);
  }
}

void ServerApplication::handlePlaceRandomShips(const RequestContext& request) {
  QString gameId;
  if (!resolveRequiredGameId(request, gameId)) {
    return;
  }

  const GameManager::ActionResult result = m_gameManager.placeShips(
      gameId, request.client->userId, protocol::generateRandomFleet());
  sendActionResult(request, result);
  notifyPlacementUpdate(request.socket, gameId, result);
}

void ServerApplication::handlePlaceShips(const RequestContext& request) {
  QString gameId;
  if (!resolveRequiredGameId(request, gameId)) {
    return;
  }

  QVector<protocol::ShipPlacement> placements;
  QString validationError;
  if (!protocol::shipPlacementsFromJson(request.payload.value(QStringLiteral("ships")),
                                        placements, validationError)) {
    sendError(request.socket, request.requestId, QStringLiteral("validation_error"),
              validationError);
    return;
  }

  const GameManager::ActionResult result =
      m_gameManager.placeShips(gameId, request.client->userId, placements);
  sendActionResult(request, result);
  notifyPlacementUpdate(request.socket, gameId, result);
}

void ServerApplication::handleGameState(const RequestContext& request) const {
  QString gameId;
  if (!resolveRequiredGameId(request, gameId)) {
    return;
  }

  sendActionResult(request,
                   m_gameManager.gameState(gameId, request.client->userId));
}

void ServerApplication::handleFire(const RequestContext& request) {
  QString gameId;
  if (!resolveRequiredGameId(request, gameId)) {
    return;
  }

  int x = 0;
  int y = 0;
  QString validationError;
  if (!extractRequiredInt(request.payload, QStringLiteral("x"), x,
                          validationError) ||
      !extractRequiredInt(request.payload, QStringLiteral("y"), y,
                          validationError)) {
    sendError(request.socket, request.requestId, QStringLiteral("validation_error"),
              validationError);
    return;
  }

  const GameManager::ActionResult result =
      m_gameManager.fire(gameId, request.client->userId, x, y);
  sendActionResult(request, result);
  notifyFireUpdate(request.socket, gameId, result);
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

  const RequestContext requestContext{
      .socket = socket,
      .client = context,
      .requestId = requestId,
      .action = action,
      .payload = payload,
  };

  if (handlePublicAction(requestContext)) {
    return;
  }

  if (!requireAuthorizedUser(requestContext)) {
    return;
  }

  if (handleAuthorizedAction(requestContext)) {
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
