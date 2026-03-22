#include "mainwindow.h"

#include <QAbstractSocket>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      m_socket(new QTcpSocket(this)),
      m_hostEdit(new QLineEdit(QStringLiteral("127.0.0.1"), this)),
      m_portEdit(new QLineEdit(QStringLiteral("4242"), this)),
      m_messageEdit(new QLineEdit(this)),
      m_connectButton(new QPushButton(QStringLiteral("Connect"), this)),
      m_disconnectButton(new QPushButton(QStringLiteral("Disconnect"), this)),
      m_sendButton(new QPushButton(QStringLiteral("Send"), this)),
      m_logView(new QTextEdit(this)) {
  setWindowTitle(QStringLiteral("Battleship TCP Client"));
  resize(640, 420);

  auto* centralWidget = new QWidget(this);
  auto* rootLayout = new QVBoxLayout(centralWidget);
  auto* connectionLayout = new QFormLayout();
  auto* buttonLayout = new QHBoxLayout();
  auto* messageLayout = new QHBoxLayout();

  m_messageEdit->setPlaceholderText(QStringLiteral("Type a message to echo"));
  m_logView->setReadOnly(true);

  connectionLayout->addRow(QStringLiteral("Host:"), m_hostEdit);
  connectionLayout->addRow(QStringLiteral("Port:"), m_portEdit);

  buttonLayout->addWidget(m_connectButton);
  buttonLayout->addWidget(m_disconnectButton);
  buttonLayout->addStretch();

  messageLayout->addWidget(new QLabel(QStringLiteral("Message:"), this));
  messageLayout->addWidget(m_messageEdit);
  messageLayout->addWidget(m_sendButton);

  rootLayout->addLayout(connectionLayout);
  rootLayout->addLayout(buttonLayout);
  rootLayout->addLayout(messageLayout);
  rootLayout->addWidget(m_logView);

  setCentralWidget(centralWidget);

  connect(m_connectButton, &QPushButton::clicked, this,
          &MainWindow::connectToServer);
  connect(m_disconnectButton, &QPushButton::clicked, this,
          &MainWindow::disconnectFromServer);
  connect(m_sendButton, &QPushButton::clicked, this, &MainWindow::sendMessage);
  connect(m_messageEdit, &QLineEdit::returnPressed, this,
          &MainWindow::sendMessage);
  connect(m_socket, &QTcpSocket::connected, this, &MainWindow::onConnected);
  connect(m_socket, &QTcpSocket::disconnected, this,
          &MainWindow::onDisconnected);
  connect(m_socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
  connect(m_socket, &QTcpSocket::errorOccurred, this,
          &MainWindow::onSocketError);

  updateUiState(false);
  appendLogMessage(
      QStringLiteral("Client is ready. Use 127.0.0.1:4242 by default."));
}

void MainWindow::connectToServer() {
  bool ok = false;
  const quint16 port = m_portEdit->text().toUShort(&ok);
  if (!ok) {
    appendLogMessage(QStringLiteral("Invalid port value."));
    return;
  }

  appendLogMessage(QStringLiteral("Connecting to %1:%2...")
                       .arg(m_hostEdit->text(), QString::number(port)));

  m_socket->connectToHost(m_hostEdit->text(), port);
}

void MainWindow::disconnectFromServer() {
  if (m_socket->state() == QAbstractSocket::UnconnectedState) {
    appendLogMessage(QStringLiteral("Socket is already disconnected."));
    return;
  }

  appendLogMessage(QStringLiteral("Disconnecting from server..."));
  m_socket->disconnectFromHost();
}

void MainWindow::sendMessage() {
  if (m_socket->state() != QAbstractSocket::ConnectedState) {
    appendLogMessage(
        QStringLiteral("Connect to the server before sending messages."));
    return;
  }

  const QByteArray payload = m_messageEdit->text().toUtf8();
  if (payload.isEmpty()) {
    appendLogMessage(QStringLiteral("Message is empty."));
    return;
  }

  m_socket->write(payload);
  appendLogMessage(QStringLiteral("Sent: %1").arg(QString::fromUtf8(payload)));
  m_messageEdit->clear();
}

void MainWindow::onConnected() {
  appendLogMessage(QStringLiteral("Connected to the server."));
  updateUiState(true);
}

void MainWindow::onDisconnected() {
  appendLogMessage(QStringLiteral("Disconnected from the server."));
  updateUiState(false);
}

void MainWindow::onReadyRead() {
  const QByteArray response = m_socket->readAll();
  appendLogMessage(
      QStringLiteral("Received: %1").arg(QString::fromUtf8(response)));
}

void MainWindow::onSocketError(QAbstractSocket::SocketError socketError) {
  Q_UNUSED(socketError);

  appendLogMessage(
      QStringLiteral("Socket error: %1").arg(m_socket->errorString()));
  updateUiState(false);
}

void MainWindow::appendLogMessage(const QString& message) {
  m_logView->append(message);
}

void MainWindow::updateUiState(bool connected) {
  m_connectButton->setEnabled(!connected);
  m_disconnectButton->setEnabled(connected);
  m_sendButton->setEnabled(connected);
  m_messageEdit->setEnabled(connected);
  m_hostEdit->setEnabled(!connected);
  m_portEdit->setEnabled(!connected);
}
