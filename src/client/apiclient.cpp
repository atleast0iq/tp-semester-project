#include "apiclient.h"

#include <QDeadlineTimer>

#include "apiprotocol.h"

namespace {

QString timeoutErrorMessage() {
  return QStringLiteral("Timed out while waiting for server response.");
}

}  // namespace

ApiClient::ApiClient() {
  connect(&m_socket, &QTcpSocket::readyRead, this, &ApiClient::onReadyRead);
}

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

  m_pendingResponses.clear();
  m_lastProtocolError.clear();
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

bool ApiClient::takePendingResponse(const QString& requestId,
                                    QJsonObject& response) {
  const auto iterator = m_pendingResponses.find(requestId);
  if (iterator == m_pendingResponses.end()) {
    return false;
  }

  response = iterator.value();
  m_pendingResponses.erase(iterator);
  return true;
}

bool ApiClient::takePendingProtocolError(QString& error) {
  if (m_lastProtocolError.isEmpty()) {
    return false;
  }

  error = m_lastProtocolError;
  m_lastProtocolError.clear();
  return true;
}

QString ApiClient::waitFailureMessage() const {
  return m_socket.errorString().isEmpty() ? timeoutErrorMessage()
                                          : m_socket.errorString();
}

bool ApiClient::waitForResponse(const QString& requestId, int timeoutMs,
                                QJsonObject& response, QString& error) {
  if (takePendingResponse(requestId, response)) {
    return true;
  }

  const QDeadlineTimer deadline(timeoutMs);

  while (deadline.remainingTime() > 0) {
    if (takePendingProtocolError(error)) {
      return false;
    }

    if (m_socket.canReadLine()) {
      onReadyRead();
      if (takePendingResponse(requestId, response)) {
        return true;
      }
      continue;
    }

    if (!m_socket.waitForReadyRead(deadline.remainingTime())) {
      if (takePendingResponse(requestId, response)) {
        return true;
      }

      if (takePendingProtocolError(error)) {
        return false;
      }

      error = waitFailureMessage();
      return false;
    }

    onReadyRead();
    if (takePendingResponse(requestId, response)) {
      return true;
    }
  }

  error = timeoutErrorMessage();
  return false;
}

void ApiClient::onReadyRead() {
  while (m_socket.canReadLine()) {
    const protocol::MessageParseResult parseResult =
        protocol::parseMessageLine(m_socket.readLine());
    if (!parseResult.ok) {
      m_lastProtocolError = parseResult.error;
      emit protocolErrorReceived(m_lastProtocolError);
      continue;
    }

    handleParsedMessage(parseResult.message);
  }
}

void ApiClient::handleParsedMessage(const QJsonObject& message) {
  if (protocol::isEventMessage(message)) {
    emit notificationReceived(message);
    return;
  }

  const QString requestId = message.value(QStringLiteral("requestId")).toString();
  if (requestId.isEmpty()) {
    m_lastProtocolError =
        QStringLiteral("Received a response without `requestId`.");
    emit protocolErrorReceived(m_lastProtocolError);
    return;
  }

  m_pendingResponses.insert(requestId, message);
}
