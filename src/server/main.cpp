#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <cstdlib>

#include "serverapplication.h"

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);

  QCoreApplication::setApplicationName("Battleship Server");
  QCoreApplication::setOrganizationName("TP Semester Project");
  QCoreApplication::setApplicationVersion("0.1.0");

  QCommandLineParser parser;
  parser.setApplicationDescription(
      "Simple TCP echo-server for the battleship semester project");
  parser.addHelpOption();
  parser.addVersionOption();

  QCommandLineOption portOption(
      QStringList() << QStringLiteral("p") << QStringLiteral("port"),
      QStringLiteral("Port for the TCP echo-server."), QStringLiteral("port"),
      QStringLiteral("4242"));
  parser.addOption(portOption);
  parser.process(app);

  bool ok = false;
  const quint16 port = parser.value(portOption).toUShort(&ok);
  if (!ok || port == 0) {
    qCritical("Invalid port provided. Use a value in range 1..65535.");
    return EXIT_FAILURE;
  }

  ServerApplication server;
  if (!server.start(port)) {
    return EXIT_FAILURE;
  }

  return app.exec();
}
