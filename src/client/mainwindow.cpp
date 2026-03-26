#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "apiclient.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QTableWidgetItem>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
  ui->setupUi(this);

  setupUIState();
  setupConnections();
}

MainWindow::~MainWindow()
{
  delete ui;
}

void MainWindow::setupUIState()
{
  ui->CurrentUser->setText("ВХОД НЕ ВЫПОЛНЕН");
}

void MainWindow::setupConnections()
{
  connect(ui->ConnectButton, &QPushButton::clicked,
          this, &MainWindow::handleConnect);

  connect(ui->LoginButton, &QPushButton::clicked,
          this, &MainWindow::handleLogin);

  connect(ui->RegisterButton, &QPushButton::clicked,
          this, &MainWindow::handleRegister);

  connect(ui->RefreshButton, &QPushButton::clicked,
          this, &MainWindow::handleRefresh);
}

void MainWindow::handleConnect()
{
  const QString host = ui->HostLine->text().trimmed();
  const quint16 port = ui->PortLine->text().toUShort();

  if (host.isEmpty()) {
    QMessageBox::warning(this, "Ошибка", "Введите хост");
    return;
  }

  if (port == 0) {
    QMessageBox::warning(this, "Ошибка", "Введите корректный порт");
    return;
  }

  QString error;
  const bool ok =
      ApiClient::instance().connectToServer(host, port, 3000, error);

  if (ok) {
    ui->statusbar->showMessage(
        QString("Подключено к %1:%2").arg(host).arg(port));
    QMessageBox::information(this, "Отлично", "Успешное подключение");
    qDebug() << "Connected to:" << host << port;
    return;
  }

  showError("Ошибка подключения", error);
}

void MainWindow::handleLogin()
{
  if (!ensureConnected()) {
    return;
  }

  QString login;
  QString password;
  if (!validAuthInput(login, password)) {
    return;
  }

  QJsonObject payload;
  payload["username"] = login;
  payload["password"] = password;

  QString error;
  const QJsonObject response =
      ApiClient::instance().sendRequest("login", payload, 3000, error);

  if (!error.isEmpty()) {
    showError("Ошибка login", error);
    return;
  }

  const QString status = response.value("status").toString();

  if (status == "ok") {
    ui->CurrentUser->setText(login);
    ui->statusbar->showMessage(QString("Вход выполнен: %1").arg(login));
    QMessageBox::information(this, "Успех", "Login successful");
    qDebug() << "Login successful:" << response;
    return;
  }

  const QJsonObject errorObj = response.value("error").toObject();
  const QString message = errorObj.value("message").toString("Login failed");
  QMessageBox::warning(this, "Ошибка", message);
  ui->statusbar->showMessage("Login failed");
  qDebug() << "Login failed:" << response;
}

void MainWindow::handleRegister()
{
  if (!ensureConnected()) {
    return;
  }

  QString login;
  QString password;
  if (!validAuthInput(login, password)) {
    return;
  }

  QJsonObject payload;
  payload["username"] = login;
  payload["password"] = password;

  QString error;
  const QJsonObject response =
      ApiClient::instance().sendRequest("register", payload, 3000, error);

  if (!error.isEmpty()) {
    showError("Ошибка register", error);
    return;
  }

  const QString status = response.value("status").toString();

  if (status == "ok") {
    ui->CurrentUser->setText(login);
    ui->statusbar->showMessage(
        QString("Пользователь зарегистрирован: %1").arg(login));
    QMessageBox::information(this, "Отлично", "Успешная регистрация");
    qDebug() << "Register successful:" << response;
    return;
  }

  const QJsonObject errorObj = response.value("error").toObject();
  const QString message =
      errorObj.value("message").toString("Ошибка регистрации");
  QMessageBox::warning(this, "Ошибка", message);
  ui->statusbar->showMessage("Ошибка регистрации");
  qDebug() << "Register failed:" << response;
}

void MainWindow::handleRefresh()
{
  if (!ensureConnected()) {
    return;
  }

  QString error;
  QJsonObject payload;

  const QJsonObject response =
      ApiClient::instance().sendRequest("list_games", payload, 3000, error);

  if (!error.isEmpty()) {
    showError("Ошибка list_games", error);
    return;
  }

  qDebug() << "list_games response:" << response;

  const QString status = response.value("status").toString();
  if (status != "ok") {
    const QJsonObject errorObj = response.value("error").toObject();
    const QString message =
        errorObj.value("message").toString("list_games failed");
    QMessageBox::warning(this, "Ошибка", message);
    return;
  }

  QJsonArray games;

  if (response.contains("games")) {
    games = response.value("games").toArray();
  } else if (response.contains("payload")) {
    const QJsonObject payloadObj = response.value("payload").toObject();
    games = payloadObj.value("games").toArray();
  }

  populateGamesTable(games);
  ui->statusbar->showMessage("Список игр обновлён");
}

bool MainWindow::ensureConnected()
{
  if (ApiClient::instance().isConnected()) {
    return true;
  }

  showError("Ошибка", "Сначала подключитесь к серверу!");
  return false;
}

bool MainWindow::validAuthInput(QString& login, QString& password)
{
  login = ui->LoginLine->text().trimmed();
  password = ui->PasswordLine->text();

  if (login.isEmpty()) {
    QMessageBox::warning(this, "Ошибка", "Введите логин");
    return false;
  }

  if (password.isEmpty()) {
    QMessageBox::warning(this, "Ошибка", "Введите пароль");
    return false;
  }

  return true;
}

void MainWindow::populateGamesTable(const QJsonArray& games)
{
  ui->GamesTable->setRowCount(0);

  for (int i = 0; i < games.size(); ++i) {
    const QJsonObject game = games[i].toObject();
    ui->GamesTable->insertRow(i);

    const QString id =
        game.contains("id") ? QString::number(game.value("id").toInt())
                            : game.value("gameId").toString();

    const QString host = game.value("host").toString();
    const QString player2 =
        game.contains("player2") ? game.value("player2").toString()
                                 : game.value("guest").toString();
    const QString gameStatus =
        game.contains("status") ? game.value("status").toString()
                                : game.value("state").toString();

    ui->GamesTable->setItem(i, 0, new QTableWidgetItem(id));
    ui->GamesTable->setItem(i, 1, new QTableWidgetItem(host));
    ui->GamesTable->setItem(i, 2, new QTableWidgetItem(player2));
    ui->GamesTable->setItem(i, 3, new QTableWidgetItem(gameStatus));
  }
}

void MainWindow::showError(const QString& title, const QString& message)
{
  QString userMessage = message;

  if (message.contains("Socket operation timed out", Qt::CaseInsensitive)) {
    userMessage =
        "Сервер не ответил вовремя.\nПроверьте, запущен ли сервер и правильно ли указан порт.";
  } else if (message.contains("Socket is not connected", Qt::CaseInsensitive)) {
    userMessage = "Нет подключения к серверу.";
  } else if (message.contains("Connection refused", Qt::CaseInsensitive)) {
    userMessage =
        "Сервер отклонил подключение.\nПроверьте хост и порт.";
  }

  ui->statusbar->showMessage(title + ": " + userMessage, 5000);
  QMessageBox::critical(this, title, userMessage);
  qDebug() << title << ":" << message;
}