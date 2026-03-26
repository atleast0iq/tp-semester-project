#include "databasemanager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

namespace {

constexpr int kMinPasswordLength = 4;

QString formatQueryError(const QSqlQuery& query, const QString& context) {
  return QStringLiteral("%1: %2").arg(context,
                                      query.lastError().text().trimmed());
}

QString generateSalt() {
  return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString hashPassword(const QString& password, const QString& salt) {
  return QString::fromLatin1(
      QCryptographicHash::hash(
          QStringLiteral("%1:%2").arg(salt, password).toUtf8(),
          QCryptographicHash::Sha256)
          .toHex());
}

void removeDatabaseConnection(const QString& connectionName) {
  if (!QSqlDatabase::contains(connectionName)) {
    return;
  }

  {
    QSqlDatabase database = QSqlDatabase::database(connectionName, false);
    if (database.isValid()) {
      database.close();
    }
  }

  QSqlDatabase::removeDatabase(connectionName);
}

bool parseMigrationVersion(const QString& fileName, int& version) {
  const qsizetype separatorIndex = fileName.indexOf(QLatin1Char('_'));
  if (separatorIndex <= 0) {
    return false;
  }

  bool ok = false;
  version = fileName.left(separatorIndex).toInt(&ok);
  return ok;
}

QFileInfoList loadMigrations(QString& error) {
  const QDir migrationDir(QDir(QCoreApplication::applicationDirPath())
                              .filePath(QStringLiteral("migrations")));
  if (!migrationDir.exists()) {
    error = QStringLiteral("Migration directory was not found: %1")
                .arg(QDir::toNativeSeparators(migrationDir.path()));
    return {};
  }

  const QFileInfoList files = migrationDir.entryInfoList(
      {QStringLiteral("*.sql")}, QDir::Files, QDir::Name);
  if (files.isEmpty()) {
    error = QStringLiteral("No SQL migrations were found.");
    return {};
  }

  int previousVersion = -1;
  for (const QFileInfo& fileInfo : files) {
    int version = 0;
    if (!parseMigrationVersion(fileInfo.fileName(), version)) {
      error = QStringLiteral(
                  "Migration file `%1` must follow `NNN_name.sql` naming.")
                  .arg(fileInfo.fileName());
      return {};
    }

    if (version == previousVersion) {
      error = QStringLiteral("Duplicate migration version `%1`.").arg(version);
      return {};
    }

    previousVersion = version;
  }

  return files;
}

QString formatMigrationError(const QFileInfo& migration,
                             const QString& reason) {
  return QStringLiteral("Migration %1 failed: %2")
      .arg(migration.fileName(), reason);
}

void populateStatsFromQuery(const QSqlQuery& query,
                            DatabaseManager::UserStats* stats) {
  stats->userId = query.value(0).toLongLong();
  stats->username = query.value(1).toString();
  stats->gamesPlayed = query.value(2).toInt();
  stats->wins = query.value(3).toInt();
  stats->losses = query.value(4).toInt();
  stats->shots = query.value(5).toInt();
  stats->hits = query.value(6).toInt();
}

bool fetchStats(const QString& connectionName, const QString& sql,
                const QString& placeholder, const QVariant& value,
                DatabaseManager::UserStats* stats, QString* error) {
  QSqlQuery query(QSqlDatabase::database(connectionName));
  query.prepare(sql);
  query.bindValue(placeholder, value);

  if (!query.exec()) {
    *error =
        formatQueryError(query, QStringLiteral("Failed to fetch statistics"));
    return false;
  }

  if (!query.next()) {
    *error = QStringLiteral("Statistics were not found.");
    return false;
  }

  populateStatsFromQuery(query, stats);
  return true;
}

}  // namespace

DatabaseManager& DatabaseManager::instance() {
  static DatabaseManager instance;
  return instance;
}

bool DatabaseManager::initialize(const QString& databasePath, QString& error) {
  if (m_initialized && m_databasePath == databasePath) {
    return true;
  }

  removeDatabaseConnection(m_connectionName);

  QSqlDatabase database =
      QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
  database.setDatabaseName(databasePath);
  if (!database.open()) {
    error = database.lastError().text().trimmed();
    return false;
  }

  m_databasePath = databasePath;
  m_initialized = true;

  QSqlQuery pragmaQuery(database);
  if (!pragmaQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
    error = formatQueryError(pragmaQuery,
                             QStringLiteral("Failed to enable foreign keys"));
    return false;
  }

  return applyMigrations(error);
}

bool DatabaseManager::registerUser(const QString& username,
                                   const QString& password, UserInfo& user,
                                   QString& error) {
  if (username.trimmed().isEmpty() || password.isEmpty()) {
    error = QStringLiteral("Username and password must be non-empty.");
    return false;
  }

  const QString trimmedUsername = username.trimmed();
  if (password.size() < kMinPasswordLength) {
    error = QStringLiteral("Password must contain at least %1 characters.")
                .arg(kMinPasswordLength);
    return false;
  }

  QSqlDatabase database = QSqlDatabase::database(m_connectionName);
  if (!database.transaction()) {
    error = database.lastError().text().trimmed();
    return false;
  }

  const QString salt = generateSalt();
  const QString passwordHash = hashPassword(password, salt);

  QSqlQuery userInsert(database);
  userInsert.prepare(QStringLiteral(
      "INSERT INTO users(username, password_hash, password_salt) "
      "VALUES(:username, :password_hash, :password_salt)"));
  userInsert.bindValue(QStringLiteral(":username"), trimmedUsername);
  userInsert.bindValue(QStringLiteral(":password_hash"), passwordHash);
  userInsert.bindValue(QStringLiteral(":password_salt"), salt);

  if (!userInsert.exec()) {
    database.rollback();
    if (userInsert.lastError().text().contains(QStringLiteral("UNIQUE"),
                                               Qt::CaseInsensitive)) {
      error = QStringLiteral("A user with this name already exists.");
      return false;
    }

    error =
        formatQueryError(userInsert, QStringLiteral("Failed to create user"));
    return false;
  }

  const qint64 userId = userInsert.lastInsertId().toLongLong();

  if (!database.commit()) {
    database.rollback();
    error = database.lastError().text().trimmed();
    return false;
  }

  user.id = userId;
  user.username = trimmedUsername;
  return true;
}

bool DatabaseManager::authorizeUser(const QString& username,
                                    const QString& password, UserInfo& user,
                                    QString& error) {
  qint64 userId = 0;
  QString storedPasswordHash;
  QString salt;
  if (!queryUserByUsername(username.trimmed(), userId, storedPasswordHash, salt,
                           error)) {
    return false;
  }

  if (hashPassword(password, salt) != storedPasswordHash) {
    error = QStringLiteral("Invalid username or password.");
    return false;
  }

  user.id = userId;
  user.username = username.trimmed();
  return true;
}

bool DatabaseManager::fetchStatisticsByUserId(qint64 userId, UserStats& stats,
                                              QString& error) {
  return fetchStats(
      m_connectionName,
      QStringLiteral(
          "SELECT user_id, username, games_played, wins, losses, shots, hits "
          "FROM user_statistics_view "
          "WHERE user_id = :user_id"),
      QStringLiteral(":user_id"), userId, &stats, &error);
}

bool DatabaseManager::fetchStatisticsByUsername(const QString& username,
                                                UserStats& stats,
                                                QString& error) {
  const QString trimmedUsername = username.trimmed();
  if (trimmedUsername.isEmpty()) {
    error = QStringLiteral("Username must be provided.");
    return false;
  }

  return fetchStats(
      m_connectionName,
      QStringLiteral(
          "SELECT user_id, username, games_played, wins, losses, shots, hits "
          "FROM user_statistics_view "
          "WHERE username = :username"),
      QStringLiteral(":username"), trimmedUsername, &stats, &error);
}

bool DatabaseManager::recordShot(qint64 userId, bool hit, QString& error) {
  QSqlQuery query(QSqlDatabase::database(m_connectionName));
  query.prepare(
      QStringLiteral("UPDATE user_stats "
                     "SET shots = shots + 1, hits = hits + :hit_increment "
                     "WHERE user_id = :user_id"));
  query.bindValue(QStringLiteral(":hit_increment"), hit ? 1 : 0);
  query.bindValue(QStringLiteral(":user_id"), userId);

  if (!query.exec()) {
    error =
        formatQueryError(query, QStringLiteral("Failed to update shot stats"));
    return false;
  }

  if (query.numRowsAffected() != 1) {
    error = QStringLiteral("Shot statistics target user was not found.");
    return false;
  }

  return true;
}

bool DatabaseManager::recordGameResult(qint64 winnerId, qint64 loserId,
                                       QString& error) {
  QSqlDatabase database = QSqlDatabase::database(m_connectionName);
  if (!database.transaction()) {
    error = database.lastError().text().trimmed();
    return false;
  }

  QSqlQuery winnerQuery(database);
  winnerQuery.prepare(
      QStringLiteral("UPDATE user_stats "
                     "SET games_played = games_played + 1, wins = wins + 1 "
                     "WHERE user_id = :user_id"));
  winnerQuery.bindValue(QStringLiteral(":user_id"), winnerId);

  if (!winnerQuery.exec() || winnerQuery.numRowsAffected() != 1) {
    database.rollback();
    error = formatQueryError(winnerQuery,
                             QStringLiteral("Failed to update winner stats"));
    return false;
  }

  QSqlQuery loserQuery(database);
  loserQuery.prepare(
      QStringLiteral("UPDATE user_stats "
                     "SET games_played = games_played + 1, losses = losses + 1 "
                     "WHERE user_id = :user_id"));
  loserQuery.bindValue(QStringLiteral(":user_id"), loserId);

  if (!loserQuery.exec() || loserQuery.numRowsAffected() != 1) {
    database.rollback();
    error = formatQueryError(loserQuery,
                             QStringLiteral("Failed to update loser stats"));
    return false;
  }

  if (!database.commit()) {
    error = database.lastError().text().trimmed();
    return false;
  }

  return true;
}

bool DatabaseManager::applyMigrations(QString& error) {
  QSqlDatabase database = QSqlDatabase::database(m_connectionName);

  int currentVersion = 0;
  if (!readUserVersion(currentVersion, error)) {
    return false;
  }

  const QFileInfoList migrations = loadMigrations(error);
  if (!error.isEmpty()) {
    return false;
  }

  for (const QFileInfo& migration : migrations) {
    int version = 0;
    parseMigrationVersion(migration.fileName(), version);
    if (version <= currentVersion) {
      continue;
    }

    QFile file(migration.filePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      error = formatMigrationError(migration,
                                   QStringLiteral("cannot open SQL file"));
      return false;
    }

    const QString sql = QString::fromUtf8(file.readAll()).trimmed();
    if (sql.isEmpty()) {
      error =
          formatMigrationError(migration, QStringLiteral("SQL file is empty"));
      return false;
    }

    if (!database.transaction()) {
      error = formatMigrationError(migration,
                                   database.lastError().text().trimmed());
      return false;
    }

    QSqlQuery query(database);
    if (!query.exec(sql)) {
      database.rollback();
      error =
          formatMigrationError(migration, query.lastError().text().trimmed());
      return false;
    }

    if (!setUserVersion(version, error)) {
      database.rollback();
      error = formatMigrationError(migration, error);
      return false;
    }

    if (!database.commit()) {
      error = formatMigrationError(migration,
                                   database.lastError().text().trimmed());
      return false;
    }

    currentVersion = version;
  }

  return true;
}

bool DatabaseManager::readUserVersion(int& version, QString& error) const {
  QSqlQuery query(QSqlDatabase::database(m_connectionName));
  if (!query.exec(QStringLiteral("PRAGMA user_version"))) {
    error =
        formatQueryError(query, QStringLiteral("Failed to read user_version"));
    return false;
  }

  if (!query.next()) {
    error = QStringLiteral("Failed to fetch SQLite user_version.");
    return false;
  }

  version = query.value(0).toInt();
  return true;
}

bool DatabaseManager::setUserVersion(int version, QString& error) const {
  QSqlQuery query(QSqlDatabase::database(m_connectionName));
  if (!query.exec(QStringLiteral("PRAGMA user_version = %1").arg(version))) {
    error = formatQueryError(query,
                             QStringLiteral("Failed to update user_version"));
    return false;
  }

  return true;
}

bool DatabaseManager::queryUserByUsername(const QString& username,
                                          qint64& userId, QString& passwordHash,
                                          QString& salt, QString& error) const {
  if (username.isEmpty()) {
    error = QStringLiteral("Username must be provided.");
    return false;
  }

  QSqlQuery query(QSqlDatabase::database(m_connectionName));
  query.prepare(
      QStringLiteral("SELECT id, password_hash, password_salt FROM users "
                     "WHERE username = :username"));
  query.bindValue(QStringLiteral(":username"), username);

  if (!query.exec()) {
    error = formatQueryError(query, QStringLiteral("Failed to query user"));
    return false;
  }

  if (!query.next()) {
    error = QStringLiteral("User was not found.");
    return false;
  }

  userId = query.value(0).toLongLong();
  passwordHash = query.value(1).toString();
  salt = query.value(2).toString();
  return true;
}

QJsonObject toJson(const DatabaseManager::UserInfo& user) {
  return {
      {QStringLiteral("id"), static_cast<qint64>(user.id)},
      {QStringLiteral("username"), user.username},
  };
}

QJsonObject toJson(const DatabaseManager::UserStats& stats) {
  return {
      {QStringLiteral("userId"), static_cast<qint64>(stats.userId)},
      {QStringLiteral("username"), stats.username},
      {QStringLiteral("gamesPlayed"), stats.gamesPlayed},
      {QStringLiteral("wins"), stats.wins},
      {QStringLiteral("losses"), stats.losses},
      {QStringLiteral("shots"), stats.shots},
      {QStringLiteral("hits"), stats.hits},
      {QStringLiteral("misses"), stats.shots - stats.hits},
      {QStringLiteral("accuracy"),
       stats.shots == 0 ? 0.0 : static_cast<double>(stats.hits) / stats.shots},
  };
}
