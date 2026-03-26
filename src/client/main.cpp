#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <cstdlib>

#include "commandlineclient.h"

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);

  QCoreApplication::setApplicationName("Battleship Client");
  QCoreApplication::setOrganizationName("TP Semester Project");
  QCoreApplication::setApplicationVersion("0.0.2");

  QCommandLineParser parser;
  parser.setApplicationDescription("Console client for the Battleship TCP API");
  parser.addHelpOption();
  parser.addVersionOption();

  QCommandLineOption hostOption(
      QStringList() << QStringLiteral("H") << QStringLiteral("host"),
      QStringLiteral("Server host."), QStringLiteral("host"),
      QStringLiteral("127.0.0.1"));
  QCommandLineOption portOption(
      QStringList() << QStringLiteral("p") << QStringLiteral("port"),
      QStringLiteral("Server port."), QStringLiteral("port"),
      QStringLiteral("6767"));
  QCommandLineOption timeoutOption(
      QStringList() << QStringLiteral("t") << QStringLiteral("timeout"),
      QStringLiteral("Socket timeout in milliseconds."), QStringLiteral("ms"),
      QStringLiteral("3000"));

  parser.addOption(hostOption);
  parser.addOption(portOption);
  parser.addOption(timeoutOption);
  parser.addPositionalArgument(
      QStringLiteral("command"),
      QStringLiteral("Optional single command to execute before exit."));
  parser.process(app);

  bool ok = false;
  const quint16 port = parser.value(portOption).toUShort(&ok);
  if (!ok || port == 0) {
    qCritical("Invalid port provided.");
    return EXIT_FAILURE;
  }

  const int timeoutMs = parser.value(timeoutOption).toInt(&ok);
  if (!ok || timeoutMs <= 0) {
    qCritical("Invalid timeout provided.");
    return EXIT_FAILURE;
  }

  CommandLineClient client(parser.value(hostOption), port, timeoutMs);
  const bool success = client.run(parser.positionalArguments());
  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
