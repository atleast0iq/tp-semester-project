#pragma once

#include <QMainWindow>
#include <QTcpSocket>

class QLineEdit;
class QPushButton;
class QTextEdit;

class MainWindow final : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);

 private slots:
  void connectToServer();
  void disconnectFromServer();
  void sendMessage();
  void onConnected();
  void onDisconnected();
  void onReadyRead();
  void onSocketError(QAbstractSocket::SocketError socketError);

 private:
  void appendLogMessage(const QString& message);
  void updateUiState(bool connected);

  QTcpSocket* m_socket;
  QLineEdit* m_hostEdit;
  QLineEdit* m_portEdit;
  QLineEdit* m_messageEdit;
  QPushButton* m_connectButton;
  QPushButton* m_disconnectButton;
  QPushButton* m_sendButton;
  QTextEdit* m_logView;
};
