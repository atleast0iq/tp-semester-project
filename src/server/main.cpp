#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QHostAddress>
#include <cstdlib>

#include "serverapplication.h"

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);

  QCoreApplication::setApplicationName("Battleship Server");
  QCoreApplication::setOrganizationName("TP Semester Project");
  QCoreApplication::setApplicationVersion("0.0.2");

  QCommandLineParser parser;
  parser.setApplicationDescription(
      "TCP JSON API server for the battleship semester project");
  parser.addHelpOption();
  parser.addVersionOption();

  QCommandLineOption portOption(
      QStringList() << QStringLiteral("p") << QStringLiteral("port"),
      QStringLiteral("Port for the TCP battleship server."),
      QStringLiteral("port"), QStringLiteral("4242"));
  QCommandLineOption addressOption(
      QStringList() << QStringLiteral("a") << QStringLiteral("address"),
      QStringLiteral("Listening address."), QStringLiteral("address"),
      QStringLiteral("0.0.0.0"));
  QCommandLineOption databaseOption(
      QStringList() << QStringLiteral("d") << QStringLiteral("database"),
      QStringLiteral("Path to the SQLite database file."),
      QStringLiteral("path"), QStringLiteral("battleship.db"));
  parser.addOption(portOption);
  parser.addOption(addressOption);
  parser.addOption(databaseOption);
  parser.process(app);

  bool ok = false;
  const quint16 port = parser.value(portOption).toUShort(&ok);
  if (!ok || port == 0) {
    qCritical("Invalid port provided. Use a value in range 1..65535.");
    return EXIT_FAILURE;
  }

  const QHostAddress address(parser.value(addressOption));
  if (address.isNull()) {
    qCritical("Invalid listen address provided.");
    return EXIT_FAILURE;
  }

  ServerApplication server;
  if (!server.start(address, port, parser.value(databaseOption))) {
    return EXIT_FAILURE;
  }

  return app.exec();
}
