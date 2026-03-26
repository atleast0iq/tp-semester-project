#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QHostAddress>
#include <cstdlib>

#include "serverapplication.h"

namespace {

void configureApplicationMetadata() {
  QCoreApplication::setApplicationName("Battleship Server");
  QCoreApplication::setOrganizationName("TP Semester Project");
  QCoreApplication::setApplicationVersion("0.0.2");
}

QCommandLineOption makePortOption() {
  return QCommandLineOption(
      QStringList() << QStringLiteral("p") << QStringLiteral("port"),
      QStringLiteral("Port for the TCP battleship server."),
      QStringLiteral("port"), QStringLiteral("6767"));
}

QCommandLineOption makeAddressOption() {
  return QCommandLineOption(
      QStringList() << QStringLiteral("a") << QStringLiteral("address"),
      QStringLiteral("Listening address."), QStringLiteral("address"),
      QStringLiteral("0.0.0.0"));
}

QCommandLineOption makeDatabaseOption() {
  return QCommandLineOption(
      QStringList() << QStringLiteral("d") << QStringLiteral("database"),
      QStringLiteral("Path to the SQLite database file."),
      QStringLiteral("path"), QStringLiteral("battleship.db"));
}

bool parsePortValue(const QString& value, quint16& port) {
  bool ok = false;
  port = value.toUShort(&ok);
  return ok && port != 0;
}

bool parseAddressValue(const QString& value, QHostAddress& address) {
  address = QHostAddress(value);
  return !address.isNull();
}

}  // namespace

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);

  configureApplicationMetadata();

  QCommandLineParser parser;
  parser.setApplicationDescription(
      "TCP JSON API server for the battleship semester project");
  parser.addHelpOption();
  parser.addVersionOption();

  const QCommandLineOption portOption = makePortOption();
  const QCommandLineOption addressOption = makeAddressOption();
  const QCommandLineOption databaseOption = makeDatabaseOption();

  parser.addOption(portOption);
  parser.addOption(addressOption);
  parser.addOption(databaseOption);
  parser.process(app);

  quint16 port = 0;
  if (!parsePortValue(parser.value(portOption), port)) {
    qCritical("Invalid port provided. Use a value in range 1..65535.");
    return EXIT_FAILURE;
  }

  QHostAddress address;
  if (!parseAddressValue(parser.value(addressOption), address)) {
    qCritical("Invalid listen address provided.");
    return EXIT_FAILURE;
  }

  ServerApplication server;
  if (!server.start(address, port, parser.value(databaseOption))) {
    return EXIT_FAILURE;
  }

  return app.exec();
}
