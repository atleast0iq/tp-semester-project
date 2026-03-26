#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

class DatabaseManager final {
 public:
  struct UserInfo {
    qint64 id = 0;
    QString username;
  };

  struct UserStats {
    qint64 userId = 0;
    QString username;
    int gamesPlayed = 0;
    int wins = 0;
    int losses = 0;
    int shots = 0;
    int hits = 0;
  };

  static DatabaseManager& instance();

  bool initialize(const QString& databasePath, QString& error);
  bool registerUser(const QString& username, const QString& password,
                    UserInfo& user, QString& error);
  bool authorizeUser(const QString& username, const QString& password,
                     UserInfo& user, QString& error);
  bool fetchStatisticsByUserId(qint64 userId, UserStats& stats, QString& error);
  bool fetchStatisticsByUsername(const QString& username, UserStats& stats,
                                 QString& error);
  bool recordShot(qint64 userId, bool hit, QString& error);
  bool recordGameResult(qint64 winnerId, qint64 loserId, QString& error);

 private:
  DatabaseManager() = default;

  bool applyMigrations(QString& error);
  bool readUserVersion(int& version, QString& error) const;
  bool setUserVersion(int version, QString& error) const;
  bool queryUserByUsername(const QString& username, qint64& userId,
                           QString& passwordHash, QString& salt,
                           QString& error) const;

  QString m_connectionName = QStringLiteral("battleship_sqlite");
  QString m_databasePath;
  bool m_initialized = false;
};

QJsonObject toJson(const DatabaseManager::UserInfo& user);
QJsonObject toJson(const DatabaseManager::UserStats& stats);
