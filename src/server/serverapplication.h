#pragma once

#include <QObject>
#include <QTcpServer>

class QTcpSocket;

class ServerApplication final : public QObject {
  Q_OBJECT

 public:
  explicit ServerApplication(QObject* parent = nullptr);

  bool start(quint16 port);

 private slots:
  void onNewConnection();
  void onClientReadyRead();
  void onClientDisconnected();
  void onServerError(QAbstractSocket::SocketError socketError);

 private:
  QTcpServer m_server;
};
