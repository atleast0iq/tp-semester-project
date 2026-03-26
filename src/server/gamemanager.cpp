#include "gamemanager.h"

#include <QJsonArray>
#include <QUuid>
#include <utility>

#include "databasemanager.h"

GameManager::ActionResult GameManager::makeSuccess(const QJsonObject& payload) {
  return {
      .ok = true,
      .errorCode = {},
      .errorMessage = {},
      .payload = payload,
  };
}

GameManager::ActionResult GameManager::createGame(
    const PlayerIdentity& player) {
  if (!ensureUserCanStartAnotherGame(player.userId)) {
    return makeError(QStringLiteral("conflict"),
                     QStringLiteral("Player is already assigned to a game."));
  }

  GameState game;
  game.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  game.playerOne.identity = player;

  m_userToGame.insert(player.userId, game.id);
  const auto iterator = m_games.insert(game.id, game);
  return makeSuccess(makeGamePayload(iterator.value(), player.userId));
}

GameManager::ActionResult GameManager::listGames() const {
  QJsonArray games;
  for (const GameState& game : std::as_const(m_games)) {
    games.append(lobbyJson(game));
  }

  return makeSuccess(QJsonObject{{QStringLiteral("games"), games}});
}

GameManager::ActionResult GameManager::joinGame(const QString& gameId,
                                                const PlayerIdentity& player) {
  if (!ensureUserCanStartAnotherGame(player.userId)) {
    return makeError(QStringLiteral("conflict"),
                     QStringLiteral("Player is already assigned to a game."));
  }

  GameState* game = findGame(gameId);
  if (game == nullptr) {
    return makeError(QStringLiteral("game_not_found"),
                     QStringLiteral("Game was not found."));
  }

  if (game->playerOne.identity.userId == player.userId) {
    return makeError(QStringLiteral("conflict"),
                     QStringLiteral("You cannot join your own game twice."));
  }

  if (game->playerTwo) {
    return makeError(
        QStringLiteral("game_full"),
        QStringLiteral("The selected game already has two players."));
  }

  game->playerTwo = PlayerState{
      .identity = player,
      .board = GameBoard{},
      .fleetReady = false,
  };
  game->status = Status::WaitingForSetup;
  m_userToGame.insert(player.userId, gameId);

  return makeSuccess(makeGamePayload(*game, player.userId));
}

GameManager::ActionResult GameManager::placeShips(
    const QString& gameId, qint64 userId,
    const QVector<protocol::ShipPlacement>& placements) {
  GameState* game = findGame(gameId);
  if (game == nullptr) {
    return makeError(QStringLiteral("game_not_found"),
                     QStringLiteral("Game was not found."));
  }

  PlayerState* player = findPlayer(*game, userId);
  if (player == nullptr) {
    return makeError(QStringLiteral("forbidden"),
                     QStringLiteral("Player does not belong to this game."));
  }

  if (game->status == Status::Finished) {
    return makeError(QStringLiteral("conflict"),
                     QStringLiteral("This game has already finished."));
  }

  QString validationError;
  if (!player->board.placeFleet(placements, validationError)) {
    return makeError(QStringLiteral("validation_error"), validationError);
  }

  player->fleetReady = true;
  if (game->playerTwo && game->playerOne.fleetReady &&
      game->playerTwo->fleetReady) {
    game->status = Status::Active;
    game->turnUserId = game->playerOne.identity.userId;
  } else if (game->playerTwo) {
    game->status = Status::WaitingForSetup;
  }

  return makeSuccess(makeGamePayload(*game, userId));
}

GameManager::ActionResult GameManager::gameState(const QString& gameId,
                                                 qint64 userId) const {
  const GameState* game = findGame(gameId);
  if (game == nullptr) {
    return makeError(QStringLiteral("game_not_found"),
                     QStringLiteral("Game was not found."));
  }

  if (findPlayer(*game, userId) == nullptr) {
    return makeError(QStringLiteral("forbidden"),
                     QStringLiteral("Player does not belong to this game."));
  }

  return makeSuccess(makeGamePayload(*game, userId));
}

GameManager::ActionResult GameManager::fire(const QString& gameId,
                                            qint64 userId, int x, int y) {
  GameState* game = findGame(gameId);
  if (game == nullptr) {
    return makeError(QStringLiteral("game_not_found"),
                     QStringLiteral("Game was not found."));
  }

  if (game->status != Status::Active) {
    return makeError(QStringLiteral("conflict"),
                     QStringLiteral("The game is not in active state."));
  }

  if (game->turnUserId != userId) {
    return makeError(QStringLiteral("not_your_turn"),
                     QStringLiteral("Wait for your turn."));
  }

  PlayerState* attacker = findPlayer(*game, userId);
  PlayerState* defender = findOpponent(*game, userId);
  if (attacker == nullptr || defender == nullptr) {
    return makeError(QStringLiteral("forbidden"),
                     QStringLiteral("Player does not belong to this game."));
  }

  GameBoard::FireResult fireResult;
  QString fireError;
  if (!defender->board.fireAt(x, y, fireResult, fireError)) {
    return makeError(QStringLiteral("validation_error"), fireError);
  }

  QString databaseError;
  if (!DatabaseManager::instance().recordShot(attacker->identity.userId,
                                              fireResult.hit, databaseError)) {
    return makeError(QStringLiteral("database_error"), databaseError);
  }

  QJsonObject resultPayload{
      {QStringLiteral("x"), x},
      {QStringLiteral("y"), y},
      {QStringLiteral("hit"), fireResult.hit},
      {QStringLiteral("sunk"), fireResult.sunk},
  };

  if (defender->board.allShipsSunk()) {
    game->status = Status::Finished;
    game->winnerUserId = attacker->identity.userId;

    if (!DatabaseManager::instance().recordGameResult(attacker->identity.userId,
                                                      defender->identity.userId,
                                                      databaseError)) {
      return makeError(QStringLiteral("database_error"), databaseError);
    }

    resultPayload.insert(QStringLiteral("winner"), attacker->identity.username);
  } else if (!fireResult.hit) {
    game->turnUserId = defender->identity.userId;
  }

  QJsonObject payload = makeGamePayload(*game, userId);
  payload.insert(QStringLiteral("result"), resultPayload);
  return makeSuccess(payload);
}

QString GameManager::currentGameForUser(qint64 userId) const {
  const QString gameId = m_userToGame.value(userId);
  if (gameId.isEmpty()) {
    return {};
  }

  const GameState* game = findGame(gameId);
  if (game == nullptr || game->status == Status::Finished) {
    return {};
  }

  return gameId;
}

GameManager::ActionResult GameManager::makeError(const QString& code,
                                                 const QString& message) {
  return {
      .ok = false,
      .errorCode = code,
      .errorMessage = message,
      .payload = {},
  };
}

QJsonObject GameManager::makeGamePayload(const GameState& game,
                                         qint64 viewerUserId) const {
  return {
      {QStringLiteral("gameId"), game.id},
      {QStringLiteral("game"), gameJsonForPlayer(game, viewerUserId)},
  };
}

QString GameManager::statusToString(Status status) {
  switch (status) {
    case Status::WaitingForOpponent:
      return QStringLiteral("waiting_for_opponent");
    case Status::WaitingForSetup:
      return QStringLiteral("waiting_for_setup");
    case Status::Active:
      return QStringLiteral("active");
    case Status::Finished:
      return QStringLiteral("finished");
  }

  return QStringLiteral("unknown");
}

QJsonObject GameManager::playerJson(const PlayerState& player,
                                    bool revealShips) {
  return {
      {QStringLiteral("userId"), static_cast<qint64>(player.identity.userId)},
      {QStringLiteral("username"), player.identity.username},
      {QStringLiteral("fleetReady"), player.fleetReady},
      {QStringLiteral("shipsRemaining"), player.board.shipsRemaining()},
      {QStringLiteral("board"), player.board.toJsonRows(revealShips)},
  };
}

bool GameManager::ensureUserCanStartAnotherGame(qint64 userId) {
  const QString gameId = m_userToGame.value(userId);
  if (gameId.isEmpty()) {
    return true;
  }

  const GameState* game = findGame(gameId);
  if (game == nullptr || game->status == Status::Finished) {
    m_userToGame.remove(userId);
    return true;
  }

  return false;
}

GameManager::GameState* GameManager::findGame(const QString& gameId) {
  auto iterator = m_games.find(gameId);
  return iterator == m_games.end() ? nullptr : &iterator.value();
}

const GameManager::GameState* GameManager::findGame(
    const QString& gameId) const {
  const auto iterator = m_games.constFind(gameId);
  return iterator == m_games.constEnd() ? nullptr : &iterator.value();
}

GameManager::PlayerState* GameManager::findPlayer(GameState& game,
                                                  qint64 userId) {
  if (game.playerOne.identity.userId == userId) {
    return &game.playerOne;
  }

  if (game.playerTwo && game.playerTwo->identity.userId == userId) {
    return &game.playerTwo.value();
  }

  return nullptr;
}

const GameManager::PlayerState* GameManager::findPlayer(const GameState& game,
                                                        qint64 userId) const {
  if (game.playerOne.identity.userId == userId) {
    return &game.playerOne;
  }

  if (game.playerTwo && game.playerTwo->identity.userId == userId) {
    return &game.playerTwo.value();
  }

  return nullptr;
}

GameManager::PlayerState* GameManager::findOpponent(GameState& game,
                                                    qint64 userId) {
  if (game.playerOne.identity.userId == userId) {
    return game.playerTwo ? &game.playerTwo.value() : nullptr;
  }

  if (game.playerTwo && game.playerTwo->identity.userId == userId) {
    return &game.playerOne;
  }

  return nullptr;
}

const GameManager::PlayerState* GameManager::findOpponent(const GameState& game,
                                                          qint64 userId) const {
  if (game.playerOne.identity.userId == userId) {
    return game.playerTwo ? &game.playerTwo.value() : nullptr;
  }

  if (game.playerTwo && game.playerTwo->identity.userId == userId) {
    return &game.playerOne;
  }

  return nullptr;
}

QJsonObject GameManager::lobbyJson(const GameState& game) const {
  const bool guestReady = game.playerTwo && game.playerTwo->fleetReady;
  return {
      {QStringLiteral("gameId"), game.id},
      {QStringLiteral("status"), statusToString(game.status)},
      {QStringLiteral("host"), game.playerOne.identity.username},
      {QStringLiteral("guest"),
       game.playerTwo ? game.playerTwo->identity.username : QString()},
      {QStringLiteral("readyPlayers"),
       (game.playerOne.fleetReady ? 1 : 0) + (guestReady ? 1 : 0)},
      {QStringLiteral("canJoin"), !game.playerTwo},
  };
}

QJsonObject GameManager::gameJsonForPlayer(const GameState& game,
                                           qint64 viewerUserId) const {
  const PlayerState* you = findPlayer(game, viewerUserId);
  const PlayerState* opponent = findOpponent(game, viewerUserId);

  QJsonObject object{
      {QStringLiteral("gameId"), game.id},
      {QStringLiteral("status"), statusToString(game.status)},
      {QStringLiteral("yourTurn"),
       game.status == Status::Active && game.turnUserId == viewerUserId},
      {QStringLiteral("you"),
       you == nullptr ? QJsonObject{} : playerJson(*you, true)},
  };

  if (opponent != nullptr) {
    object.insert(QStringLiteral("opponent"), playerJson(*opponent, false));
  }

  if (game.status == Status::Finished) {
    const PlayerState* winner = findPlayer(game, game.winnerUserId);
    object.insert(
        QStringLiteral("winner"),
        winner == nullptr
            ? QJsonObject{}
            : QJsonObject{
                  {QStringLiteral("userId"),
                   static_cast<qint64>(winner->identity.userId)},
                  {QStringLiteral("username"), winner->identity.username},
              });
  }

  return object;
}
