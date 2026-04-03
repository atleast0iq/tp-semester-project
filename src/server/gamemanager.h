#pragma once

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <optional>

#include "apiprotocol.h"
#include "gameboard.h"

class GameManager final {
 public:
  struct PlayerIdentity {
    qint64 userId = 0;
    QString username;
  };

  struct ActionResult {
    bool ok = false;
    QString errorCode;
    QString errorMessage;
    QJsonObject payload;
  };

  ActionResult createGame(const PlayerIdentity& player);
  ActionResult listGames() const;
  ActionResult joinGame(const QString& gameId, const PlayerIdentity& player);
  ActionResult placeShips(const QString& gameId, qint64 userId,
                          const QVector<protocol::ShipPlacement>& placements);
  ActionResult gameState(const QString& gameId, qint64 userId) const;
  ActionResult fire(const QString& gameId, qint64 userId, int x, int y);
  QString currentGameForUser(qint64 userId) const;
  QVector<qint64> participantUserIds(const QString& gameId) const;

 private:
  enum class Status {
    WaitingForOpponent,
    WaitingForSetup,
    Active,
    Finished,
  };

  struct PlayerState {
    PlayerIdentity identity;
    GameBoard board;
    bool fleetReady = false;
  };

  struct GameState {
    QString id;
    Status status = Status::WaitingForOpponent;
    PlayerState playerOne;
    std::optional<PlayerState> playerTwo;
    qint64 turnUserId = 0;
    qint64 winnerUserId = 0;
  };

  static ActionResult makeSuccess(const QJsonObject& payload);
  static ActionResult makeError(const QString& code, const QString& message);
  static ActionResult gameNotFoundError();
  static ActionResult playerNotInGameError();
  static QString statusToString(Status status);
  static QJsonObject playerJson(const PlayerState& player, bool revealShips);
  static QJsonObject winnerJson(const PlayerState& winner);
  static QJsonObject fireResultJson(int x, int y,
                                    const GameBoard::FireResult& result);

  bool ensureUserCanStartAnotherGame(qint64 userId);
  void attachSecondPlayer(GameState& game, const PlayerIdentity& player);
  void updateStatusAfterPlacement(GameState& game);
  bool finalizeGame(GameState& game, const PlayerState& attacker,
                    const PlayerState& defender, QJsonObject& resultPayload,
                    QString& error);
  QJsonObject makeGamePayload(const GameState& game, qint64 viewerUserId) const;
  GameState* findGame(const QString& gameId);
  const GameState* findGame(const QString& gameId) const;
  PlayerState* findPlayer(GameState& game, qint64 userId);
  const PlayerState* findPlayer(const GameState& game, qint64 userId) const;
  PlayerState* findOpponent(GameState& game, qint64 userId);
  const PlayerState* findOpponent(const GameState& game, qint64 userId) const;
  QJsonObject lobbyJson(const GameState& game) const;
  QJsonObject gameJsonForPlayer(const GameState& game,
                                qint64 viewerUserId) const;

  QHash<QString, GameState> m_games;
  QHash<qint64, QString> m_userToGame;
};
