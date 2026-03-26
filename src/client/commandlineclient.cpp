#include "commandlineclient.h"

#include <QJsonDocument>
#include <QProcess>
#include <QTextStream>
#include <utility>

#include "apiclient.h"
#include "apiprotocol.h"

CommandLineClient::CommandLineClient(QString host, quint16 port, int timeoutMs)
    : m_host(std::move(host)), m_port(port), m_timeoutMs(timeoutMs) {}

bool CommandLineClient::run(const QStringList& positionalArguments) {
  QString error;
  if (!connect(error)) {
    QTextStream(stderr) << "Connection failed: " << error << Qt::endl;
    return false;
  }

  if (!positionalArguments.isEmpty()) {
    return executeCommand(positionalArguments, false);
  }

  QTextStream out(stdout);
  QTextStream in(stdin);

  out << "Connected to " << m_host << ':' << m_port << Qt::endl;
  out << "Type `help` for available commands." << Qt::endl;

  while (true) {
    out << "battleship> " << Qt::flush;

    const QString line = in.readLine();
    if (line.isNull()) {
      out << Qt::endl;
      return true;
    }

    const QStringList tokens = QProcess::splitCommand(line);
    if (tokens.isEmpty()) {
      continue;
    }

    const QString command = tokens.first().trimmed().toLower();
    if (command == QStringLiteral("quit") ||
        command == QStringLiteral("exit")) {
      return true;
    }

    if (!executeCommand(tokens, true)) {
      return false;
    }
  }
}

bool CommandLineClient::connect(QString& error) const {
  return ApiClient::instance().connectToServer(m_host, m_port, m_timeoutMs,
                                               error);
}

bool CommandLineClient::executeCommand(const QStringList& tokens,
                                       bool interactiveMode) {
  const QString command = tokens.first().trimmed().toLower();
  if (command == QStringLiteral("help")) {
    printHelp();
    return true;
  }

  QString error;
  QJsonObject payload;
  QString action;

  if (command == QStringLiteral("register") ||
      command == QStringLiteral("login")) {
    if (tokens.size() != 3) {
      QTextStream(stderr) << "Usage: " << command << " <username> <password>"
                          << Qt::endl;
      return interactiveMode;
    }

    action = command;
    payload.insert(QStringLiteral("username"), tokens[1]);
    payload.insert(QStringLiteral("password"), tokens[2]);
  } else if (command == QStringLiteral("logout") ||
             command == QStringLiteral("list-games") ||
             command == QStringLiteral("whoami") ||
             command == QStringLiteral("ping")) {
    if (tokens.size() != 1) {
      QTextStream(stderr) << "Command `" << command
                          << "` does not take arguments." << Qt::endl;
      return interactiveMode;
    }

    if (command == QStringLiteral("list-games")) {
      action = QStringLiteral("list_games");
    } else {
      action = command;
    }
  } else if (command == QStringLiteral("stats")) {
    if (tokens.size() > 2) {
      QTextStream(stderr) << "Usage: stats [username]" << Qt::endl;
      return interactiveMode;
    }

    action = QStringLiteral("stats");
    if (tokens.size() == 2) {
      payload.insert(QStringLiteral("username"), tokens[1]);
    }
  } else if (command == QStringLiteral("create-game")) {
    if (tokens.size() != 1) {
      QTextStream(stderr) << "Usage: create-game" << Qt::endl;
      return interactiveMode;
    }

    action = QStringLiteral("create_game");
  } else if (command == QStringLiteral("join-game")) {
    if (tokens.size() != 2) {
      QTextStream(stderr) << "Usage: join-game <gameId>" << Qt::endl;
      return interactiveMode;
    }

    action = QStringLiteral("join_game");
    payload.insert(QStringLiteral("gameId"), tokens[1]);
  } else if (command == QStringLiteral("place-random")) {
    if (tokens.size() > 2) {
      QTextStream(stderr) << "Usage: place-random [gameId]" << Qt::endl;
      return interactiveMode;
    }

    action = QStringLiteral("place_ships");
    payload.insert(
        QStringLiteral("ships"),
        protocol::shipPlacementsToJson(protocol::generateRandomFleet()));
    if (tokens.size() == 2) {
      payload.insert(QStringLiteral("gameId"), tokens[1]);
    }
  } else if (command == QStringLiteral("state")) {
    if (tokens.size() > 2) {
      QTextStream(stderr) << "Usage: state [gameId]" << Qt::endl;
      return interactiveMode;
    }

    action = QStringLiteral("game_state");
    if (tokens.size() == 2) {
      payload.insert(QStringLiteral("gameId"), tokens[1]);
    }
  } else if (command == QStringLiteral("fire")) {
    if (tokens.size() < 3 || tokens.size() > 4) {
      QTextStream(stderr) << "Usage: fire <x> <y> [gameId]" << Qt::endl;
      return interactiveMode;
    }

    bool xOk = false;
    bool yOk = false;
    const int x = tokens[1].toInt(&xOk);
    const int y = tokens[2].toInt(&yOk);
    if (!xOk || !yOk) {
      QTextStream(stderr) << "Coordinates must be integers." << Qt::endl;
      return interactiveMode;
    }

    action = QStringLiteral("fire");
    payload.insert(QStringLiteral("x"), x);
    payload.insert(QStringLiteral("y"), y);
    if (tokens.size() == 4) {
      payload.insert(QStringLiteral("gameId"), tokens[3]);
    }
  } else {
    QTextStream(stderr) << "Unknown command: " << command << Qt::endl;
    printHelp();
    return interactiveMode;
  }

  const QJsonObject response =
      ApiClient::instance().sendRequest(action, payload, m_timeoutMs, error);
  if (!error.isEmpty()) {
    QTextStream(stderr) << "Request failed: " << error << Qt::endl;
    return false;
  }

  printResponse(response);
  const bool ok = response.value(QStringLiteral("status")).toString() ==
                  QStringLiteral("ok");
  return interactiveMode ? true : ok;
}

void CommandLineClient::printHelp() const {
  QTextStream out(stdout);
  out << "Commands:" << Qt::endl;
  out << "  help" << Qt::endl;
  out << "  register <username> <password>" << Qt::endl;
  out << "  login <username> <password>" << Qt::endl;
  out << "  logout" << Qt::endl;
  out << "  whoami" << Qt::endl;
  out << "  stats [username]" << Qt::endl;
  out << "  list-games" << Qt::endl;
  out << "  create-game" << Qt::endl;
  out << "  join-game <gameId>" << Qt::endl;
  out << "  place-random [gameId]" << Qt::endl;
  out << "  state [gameId]" << Qt::endl;
  out << "  fire <x> <y> [gameId]" << Qt::endl;
  out << "  ping" << Qt::endl;
  out << "  quit | exit" << Qt::endl;
}

void CommandLineClient::printResponse(const QJsonObject& response) const {
  QTextStream out(stdout);
  out << QString::fromUtf8(
             QJsonDocument(response).toJson(QJsonDocument::Indented))
      << Qt::endl;
}
