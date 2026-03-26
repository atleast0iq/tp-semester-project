#include "apiclient.h"

#include <QDeadlineTimer>

#include "apiprotocol.h"

ApiClient& ApiClient::instance() {
  static ApiClient instance;
  return instance;
}

bool ApiClient::connectToServer(const QString& host, quint16 port,
                                int timeoutMs, QString& error) {
  if (m_socket.state() == QAbstractSocket::ConnectedState && m_host == host &&
      m_port == port) {
    return true;
  }

  if (m_socket.state() != QAbstractSocket::UnconnectedState) {
    m_socket.abort();
  }

  m_socket.connectToHost(host, port);
  if (!m_socket.waitForConnected(timeoutMs)) {
    error = m_socket.errorString();
    return false;
  }

  m_host = host;
  m_port = port;
  return true;
}

void ApiClient::disconnectFromServer() { m_socket.disconnectFromHost(); }

bool ApiClient::isConnected() const {
  return m_socket.state() == QAbstractSocket::ConnectedState;
}

QJsonObject ApiClient::sendRequest(const QString& action,
                                   const QJsonObject& payload, int timeoutMs,
                                   QString& error) {
  if (!isConnected()) {
    error = QStringLiteral("Socket is not connected.");
    return {};
  }

  const QString requestId = nextRequestId();
  const QJsonObject request{
      {QStringLiteral("requestId"), requestId},
      {QStringLiteral("action"), action},
      {QStringLiteral("payload"), payload},
  };

  if (m_socket.write(protocol::serializeLine(request)) < 0 ||
      !m_socket.waitForBytesWritten(timeoutMs)) {
    error = m_socket.errorString();
    return {};
  }

  QJsonObject response;
  if (!waitForResponse(requestId, timeoutMs, response, error)) {
    return {};
  }

  return response;
}

QString ApiClient::nextRequestId() {
  ++m_requestCounter;
  return QStringLiteral("cli-%1").arg(m_requestCounter);
}

bool ApiClient::waitForResponse(const QString& requestId, int timeoutMs,
                                QJsonObject& response, QString& error) {
  const QDeadlineTimer deadline(timeoutMs);

  while (deadline.remainingTime() > 0) {
    while (m_socket.canReadLine()) {
      const protocol::MessageParseResult parseResult =
          protocol::parseMessageLine(m_socket.readLine());
      if (!parseResult.ok) {
        error = parseResult.error;
        return false;
      }

      const QString receivedRequestId =
          parseResult.message.value(QStringLiteral("requestId")).toString();
      if (receivedRequestId == requestId) {
        response = parseResult.message;
        return true;
      }
    }

    if (!m_socket.waitForReadyRead(deadline.remainingTime())) {
      error =
          m_socket.errorString().isEmpty()
              ? QStringLiteral("Timed out while waiting for server response.")
              : m_socket.errorString();
      return false;
    }
  }

  error = QStringLiteral("Timed out while waiting for server response.");
  return false;
}
