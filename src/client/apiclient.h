#pragma once

#include <QJsonObject>
#include <QString>
#include <QTcpSocket>

class ApiClient final {
 public:
  static ApiClient& instance();

  bool connectToServer(const QString& host, quint16 port, int timeoutMs,
                       QString& error);
  void disconnectFromServer();
  bool isConnected() const;
  QJsonObject sendRequest(const QString& action, const QJsonObject& payload,
                          int timeoutMs, QString& error);

 private:
  ApiClient() = default;

  QString nextRequestId();
  bool waitForResponse(const QString& requestId, int timeoutMs,
                       QJsonObject& response, QString& error);

  QTcpSocket m_socket;
  QString m_host;
  quint16 m_port = 0;
  quint64 m_requestCounter = 0;
};
