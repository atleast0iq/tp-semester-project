#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTcpSocket>

class ApiClient final : public QObject {
  Q_OBJECT

 public:
  static ApiClient& instance();

  bool connectToServer(const QString& host, quint16 port, int timeoutMs,
                       QString& error);
  void disconnectFromServer();
  bool isConnected() const;
  QJsonObject sendRequest(const QString& action, const QJsonObject& payload,
                          int timeoutMs, QString& error);

 signals:
  void notificationReceived(const QJsonObject& notification);
  void protocolErrorReceived(const QString& error);

 private:
  ApiClient();

  QString nextRequestId();
  bool takePendingResponse(const QString& requestId, QJsonObject& response);
  bool takePendingProtocolError(QString& error);
  QString waitFailureMessage() const;
  bool waitForResponse(const QString& requestId, int timeoutMs,
                       QJsonObject& response, QString& error);
  void handleParsedMessage(const QJsonObject& message);

 private slots:
  void onReadyRead();

 private:
  QTcpSocket m_socket;
  QHash<QString, QJsonObject> m_pendingResponses;
  QString m_host;
  QString m_lastProtocolError;
  quint16 m_port = 0;
  quint64 m_requestCounter = 0;
};
