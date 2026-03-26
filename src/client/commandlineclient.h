#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

class CommandLineClient final {
 public:
  CommandLineClient(QString host, quint16 port, int timeoutMs);

  bool run(const QStringList& positionalArguments);

 private:
  bool connect(QString& error) const;
  bool executeCommand(const QStringList& tokens, bool interactiveMode);
  void printHelp() const;
  void printResponse(const QJsonObject& response) const;

  QString m_host;
  quint16 m_port = 0;
  int m_timeoutMs = 0;
};
