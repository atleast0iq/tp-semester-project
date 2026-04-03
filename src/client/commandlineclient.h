#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

class QSocketNotifier;

class CommandLineClient final : public QObject {
  Q_OBJECT

 public:
  explicit CommandLineClient(QString host, quint16 port, int timeoutMs,
                             QObject* parent = nullptr);

  bool runCommand(const QStringList& positionalArguments);
  bool startInteractive();

 private:
  bool connect(QString& error) const;
  bool executeCommand(const QStringList& tokens, bool interactiveMode);
  void printPrompt() const;
  void printHelp() const;
  void printResponse(const QJsonObject& response) const;
  void printNotification(const QJsonObject& notification) const;

 private slots:
  void onStdinActivated();
  void onNotificationReceived(const QJsonObject& notification);
  void onProtocolErrorReceived(const QString& error);

 private:
  QString m_host;
  quint16 m_port = 0;
  int m_timeoutMs = 0;
  QSocketNotifier* m_stdinNotifier = nullptr;
};
