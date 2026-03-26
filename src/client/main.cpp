#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <cstdlib>

#include "commandlineclient.h"

namespace {

void configureApplicationMetadata() {
  QCoreApplication::setApplicationName("Battleship Client");
  QCoreApplication::setOrganizationName("TP Semester Project");
  QCoreApplication::setApplicationVersion("0.0.2");
}

QCommandLineOption makeHostOption() {
  return QCommandLineOption(
      QStringList() << QStringLiteral("H") << QStringLiteral("host"),
      QStringLiteral("Server host."), QStringLiteral("host"),
      QStringLiteral("127.0.0.1"));
}

QCommandLineOption makePortOption() {
  return QCommandLineOption(
      QStringList() << QStringLiteral("p") << QStringLiteral("port"),
      QStringLiteral("Server port."), QStringLiteral("port"),
      QStringLiteral("6767"));
}

QCommandLineOption makeTimeoutOption() {
  return QCommandLineOption(
      QStringList() << QStringLiteral("t") << QStringLiteral("timeout"),
      QStringLiteral("Socket timeout in milliseconds."), QStringLiteral("ms"),
      QStringLiteral("3000"));
}

bool parsePortValue(const QString& value, quint16& port) {
  bool ok = false;
  port = value.toUShort(&ok);
  return ok && port != 0;
}

bool parseTimeoutValue(const QString& value, int& timeoutMs) {
  bool ok = false;
  timeoutMs = value.toInt(&ok);
  return ok && timeoutMs > 0;
}

}  // namespace

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);

  configureApplicationMetadata();

  QCommandLineParser parser;
  parser.setApplicationDescription("Console client for the Battleship TCP API");
  parser.addHelpOption();
  parser.addVersionOption();

  const QCommandLineOption hostOption = makeHostOption();
  const QCommandLineOption portOption = makePortOption();
  const QCommandLineOption timeoutOption = makeTimeoutOption();

  parser.addOption(hostOption);
  parser.addOption(portOption);
  parser.addOption(timeoutOption);
  parser.addPositionalArgument(
      QStringLiteral("command"),
      QStringLiteral("Optional single command to execute before exit."));
  parser.process(app);

  quint16 port = 0;
  if (!parsePortValue(parser.value(portOption), port)) {
    qCritical("Invalid port provided.");
    return EXIT_FAILURE;
  }

  int timeoutMs = 0;
  if (!parseTimeoutValue(parser.value(timeoutOption), timeoutMs)) {
    qCritical("Invalid timeout provided.");
    return EXIT_FAILURE;
  }

  CommandLineClient client(parser.value(hostOption), port, timeoutMs);
  if (!parser.positionalArguments().isEmpty()) {
    return client.runCommand(parser.positionalArguments()) ? EXIT_SUCCESS
                                                           : EXIT_FAILURE;
  }

  if (!client.startInteractive()) {
    return EXIT_FAILURE;
  }

  return app.exec();
}
