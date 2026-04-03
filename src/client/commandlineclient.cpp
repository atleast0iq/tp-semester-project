#include "commandlineclient.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QProcess>
#include <QSocketNotifier>
#include <QTextStream>
#include <cstdio>
#include <utility>

#include "apiclient.h"

namespace {

struct ParsedCommand {
  bool ok = false;
  QString action;
  QJsonObject payload;
  QString errorMessage;
};

ParsedCommand makeParsedCommand(QString action, QJsonObject payload = {}) {
  return {
      .ok = true,
      .action = std::move(action),
      .payload = std::move(payload),
      .errorMessage = {},
  };
}

ParsedCommand makeCommandError(const QString& errorMessage) {
  return {
      .ok = false,
      .action = {},
      .payload = {},
      .errorMessage = errorMessage,
  };
}

ParsedCommand parseAuthCommand(const QString& command,
                               const QStringList& tokens) {
  if (tokens.size() != 3) {
    return makeCommandError(QStringLiteral("Usage: %1 <username> <password>")
                                .arg(command));
  }

  return makeParsedCommand(
      command,
      QJsonObject{
          {QStringLiteral("username"), tokens[1]},
          {QStringLiteral("password"), tokens[2]},
      });
}

ParsedCommand parseNoArgumentCommand(const QString& command,
                                     const QStringList& tokens) {
  if (tokens.size() != 1) {
    return makeCommandError(
        QStringLiteral("Command `%1` does not take arguments.").arg(command));
  }

  return makeParsedCommand(command == QStringLiteral("list-games")
                               ? QStringLiteral("list_games")
                               : command);
}

ParsedCommand parseStatsCommand(const QStringList& tokens) {
  if (tokens.size() > 2) {
    return makeCommandError(QStringLiteral("Usage: stats [username]"));
  }

  QJsonObject payload;
  if (tokens.size() == 2) {
    payload.insert(QStringLiteral("username"), tokens[1]);
  }

  return makeParsedCommand(QStringLiteral("stats"), std::move(payload));
}

ParsedCommand parseCreateGameCommand(const QStringList& tokens) {
  if (tokens.size() != 1) {
    return makeCommandError(QStringLiteral("Usage: create-game"));
  }

  return makeParsedCommand(QStringLiteral("create_game"));
}

ParsedCommand parseJoinGameCommand(const QStringList& tokens) {
  if (tokens.size() != 2) {
    return makeCommandError(QStringLiteral("Usage: join-game <gameId>"));
  }

  return makeParsedCommand(
      QStringLiteral("join_game"),
      QJsonObject{{QStringLiteral("gameId"), tokens[1]}});
}

ParsedCommand parsePlaceRandomCommand(const QStringList& tokens) {
  if (tokens.size() > 2) {
    return makeCommandError(QStringLiteral("Usage: place-random [gameId]"));
  }

  QJsonObject payload;
  if (tokens.size() == 2) {
    payload.insert(QStringLiteral("gameId"), tokens[1]);
  }

  return makeParsedCommand(QStringLiteral("place_random_ships"),
                           std::move(payload));
}

ParsedCommand parseStateCommand(const QStringList& tokens) {
  if (tokens.size() > 2) {
    return makeCommandError(QStringLiteral("Usage: state [gameId]"));
  }

  QJsonObject payload;
  if (tokens.size() == 2) {
    payload.insert(QStringLiteral("gameId"), tokens[1]);
  }

  return makeParsedCommand(QStringLiteral("game_state"), std::move(payload));
}

ParsedCommand parseFireCommand(const QStringList& tokens) {
  if (tokens.size() < 3 || tokens.size() > 4) {
    return makeCommandError(QStringLiteral("Usage: fire <x> <y> [gameId]"));
  }

  bool xOk = false;
  bool yOk = false;
  const int x = tokens[1].toInt(&xOk);
  const int y = tokens[2].toInt(&yOk);
  if (!xOk || !yOk) {
    return makeCommandError(QStringLiteral("Coordinates must be integers."));
  }

  QJsonObject payload{
      {QStringLiteral("x"), x},
      {QStringLiteral("y"), y},
  };
  if (tokens.size() == 4) {
    payload.insert(QStringLiteral("gameId"), tokens[3]);
  }

  return makeParsedCommand(QStringLiteral("fire"), std::move(payload));
}

ParsedCommand parseCommand(const QStringList& tokens) {
  const QString command = tokens.first().trimmed().toLower();

  if (command == QStringLiteral("register") ||
      command == QStringLiteral("login")) {
    return parseAuthCommand(command, tokens);
  }

  if (command == QStringLiteral("logout") ||
      command == QStringLiteral("list-games") ||
      command == QStringLiteral("whoami") ||
      command == QStringLiteral("ping")) {
    return parseNoArgumentCommand(command, tokens);
  }

  if (command == QStringLiteral("stats")) {
    return parseStatsCommand(tokens);
  }

  if (command == QStringLiteral("create-game")) {
    return parseCreateGameCommand(tokens);
  }

  if (command == QStringLiteral("join-game")) {
    return parseJoinGameCommand(tokens);
  }

  if (command == QStringLiteral("place-random")) {
    return parsePlaceRandomCommand(tokens);
  }

  if (command == QStringLiteral("state")) {
    return parseStateCommand(tokens);
  }

  if (command == QStringLiteral("fire")) {
    return parseFireCommand(tokens);
  }

  return makeCommandError(QStringLiteral("Unknown command: %1").arg(command));
}

}  // namespace

CommandLineClient::CommandLineClient(QString host, quint16 port, int timeoutMs,
                                     QObject* parent)
    : QObject(parent),
      m_host(std::move(host)),
      m_port(port),
      m_timeoutMs(timeoutMs) {}

bool CommandLineClient::runCommand(const QStringList& positionalArguments) {
  QString error;
  if (!connect(error)) {
    QTextStream(stderr) << "Connection failed: " << error << Qt::endl;
    return false;
  }

  return executeCommand(positionalArguments, false);
}

bool CommandLineClient::startInteractive() {
  QString error;
  if (!connect(error)) {
    QTextStream(stderr) << "Connection failed: " << error << Qt::endl;
    return false;
  }

  QTextStream out(stdout);

  out << "Connected to " << m_host << ':' << m_port << Qt::endl;
  out << "Type `help` for available commands." << Qt::endl;
  QObject::connect(&ApiClient::instance(), &ApiClient::notificationReceived,
                   this, &CommandLineClient::onNotificationReceived);
  QObject::connect(&ApiClient::instance(), &ApiClient::protocolErrorReceived,
                   this, &CommandLineClient::onProtocolErrorReceived);
  m_stdinNotifier =
      new QSocketNotifier(fileno(stdin), QSocketNotifier::Read, this);
  QObject::connect(m_stdinNotifier, &QSocketNotifier::activated, this,
                   &CommandLineClient::onStdinActivated);
  printPrompt();
  return true;
}

bool CommandLineClient::connect(QString& error) const {
  return ApiClient::instance().connectToServer(m_host, m_port, m_timeoutMs,
                                               error);
}

void CommandLineClient::onStdinActivated() {
  if (m_stdinNotifier != nullptr) {
    m_stdinNotifier->setEnabled(false);
  }

  QTextStream in(stdin);
  const QString line = in.readLine();
  if (m_stdinNotifier != nullptr) {
    m_stdinNotifier->setEnabled(true);
  }

  QTextStream out(stdout);
  if (line.isNull()) {
    out << Qt::endl;
    QCoreApplication::quit();
    return;
  }

  const QStringList tokens = QProcess::splitCommand(line);
  if (tokens.isEmpty()) {
    printPrompt();
    return;
  }

  const QString command = tokens.first().trimmed().toLower();
  if (command == QStringLiteral("quit") || command == QStringLiteral("exit")) {
    QCoreApplication::quit();
    return;
  }

  if (!executeCommand(tokens, true)) {
    QCoreApplication::exit(1);
    return;
  }

  printPrompt();
}

void CommandLineClient::onNotificationReceived(const QJsonObject& notification) {
  printNotification(notification);
  printPrompt();
}

void CommandLineClient::onProtocolErrorReceived(const QString& error) {
  QTextStream(stderr) << "Protocol error: " << error << Qt::endl;
  printPrompt();
}

void CommandLineClient::printPrompt() const {
  QTextStream(stdout) << "battleship> " << Qt::flush;
}

bool CommandLineClient::executeCommand(const QStringList& tokens,
                                       bool interactiveMode) {
  const QString command = tokens.first().trimmed().toLower();
  if (command == QStringLiteral("help")) {
    printHelp();
    return true;
  }

  const ParsedCommand parsedCommand = parseCommand(tokens);
  if (!parsedCommand.ok) {
    QTextStream(stderr) << parsedCommand.errorMessage << Qt::endl;
    if (parsedCommand.errorMessage.startsWith(QStringLiteral("Unknown command:"))) {
      printHelp();
    }
    return interactiveMode;
  }

  QString error;
  const QJsonObject response =
      ApiClient::instance().sendRequest(parsedCommand.action,
                                        parsedCommand.payload, m_timeoutMs,
                                        error);
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

void CommandLineClient::printNotification(const QJsonObject& notification) const {
  QTextStream out(stdout);
  const QJsonObject payload = notification.value(QStringLiteral("payload")).toObject();
  const QString message = payload.value(QStringLiteral("message")).toString();

  out << Qt::endl;
  out << "[notification]";
  if (!message.isEmpty()) {
    out << ' ' << message;
  }
  out << Qt::endl;
  out << QString::fromUtf8(
             QJsonDocument(notification).toJson(QJsonDocument::Indented))
      << Qt::endl;
}
