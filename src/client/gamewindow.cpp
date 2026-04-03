#include "gamewindow.h"
#include "ui_gamewindow.h"
#include "apiclient.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QDebug>
#include <QVBoxLayout>
#include <QLabel>
#include <QUrl>

#include "boardwidget.h"

GameWindow::GameWindow(const QString& gameId,
                       const QString& username,
                       QWidget *parent)
    : QMainWindow(parent)
      , ui(new Ui::GameWindow)
      , m_leftBoard(nullptr)
      , m_rightBoard(nullptr)
      , m_gameId(gameId)
      , m_username(username)
{
  ui->setupUi(this);

  setupSounds();

  ui->FirstPlayerLabel->setText(username);

  setupBoards();
  loadGameState();

  connect(&ApiClient::instance(),
          &ApiClient::notificationReceived,
          this,
          [this](const QJsonObject& notification) {
            const QString event = notification.value("event").toString();
            if (event != "game_updated") {
              return;
            }

            const QJsonObject payloadObj = notification.value("payload").toObject();
            const QString eventGameId = payloadObj.value("gameId").toString();

            if (!eventGameId.isEmpty() && eventGameId != m_gameId) {
              return;
            }

            qDebug() << "game_updated received:" << notification;

            const QString reason = payloadObj.value("reason").toString();
            if (reason == "game_finished") {
              showGameFinishedScreen(notification);
            }

            loadGameState();
          });

  QLabel* gameIdLabel = new QLabel(QString("ID: %1").arg(m_gameId), this);
  statusBar()->addPermanentWidget(gameIdLabel);
}

GameWindow::~GameWindow()
{
  delete ui;
}

void GameWindow::setupBoards()
{
  m_leftBoard = new BoardWidget(this);
  m_rightBoard = new BoardWidget(this);

  auto *leftLayout = new QVBoxLayout(ui->LeftBoardContainer);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->addWidget(m_leftBoard);

  auto *rightLayout = new QVBoxLayout(ui->RightBoardContainer);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->addWidget(m_rightBoard);

  connect(m_rightBoard, &BoardWidget::cellClicked,
          this, [this](int row, int col) {
            fireAt(row, col);
          });
}

void GameWindow::setupSounds()
{
  m_hitSound.setSource(QUrl::fromLocalFile("sounds/hit.wav"));
  m_missSound.setSource(QUrl::fromLocalFile("sounds/miss.wav"));
  m_sunkSound.setSource(QUrl::fromLocalFile("sounds/sunk.wav"));
  m_winSound.setSource(QUrl::fromLocalFile("sounds/win.wav"));
  m_loseSound.setSource(QUrl::fromLocalFile("sounds/lose.wav"));

  m_hitSound.setVolume(0.7f);
  m_missSound.setVolume(0.7f);
  m_sunkSound.setVolume(0.8f);
  m_winSound.setVolume(0.9f);
  m_loseSound.setVolume(0.9f);
}

void GameWindow::loadGameState()
{
  if (m_gameId.isEmpty()) {
    qDebug() << "loadGameState: gameId is empty";
    return;
  }

  QJsonObject payload;
  payload["gameId"] = m_gameId;

  QString error;
  const QJsonObject response =
      ApiClient::instance().sendRequest("game_state", payload, 3000, error);

  if (!error.isEmpty()) {
    QMessageBox::warning(this, "game_state error", error);
    qDebug() << "game_state error:" << error;
    return;
  }

  qDebug() << "game_state response:" << response;

  const QJsonObject payloadObj = response.value("payload").toObject();
  const QJsonObject gameObj = payloadObj.value("game").toObject();

  if (gameObj.contains("opponent")) {
    const QJsonObject opponentObj = gameObj.value("opponent").toObject();
    const QString opponentName = opponentObj.value("username").toString();

    if (!opponentName.isEmpty()) {
      ui->SecondPlayerLabel->setText(opponentName);
    } else {
      ui->SecondPlayerLabel->setText("Opponent");
    }
  } else {
    ui->SecondPlayerLabel->setText("Opponent");
  }

  updateGameFinishedState(gameObj);

  const bool yourTurn = gameObj.value("yourTurn").toBool();
  if (m_gameFinished) {
    statusBar()->showMessage("Игра завершена");
  } else {
    statusBar()->showMessage(yourTurn ? "Ваш ход" : "Ход соперника");
  }


  for (int row = 0; row < 10; ++row) {
    for (int col = 0; col < 10; ++col) {
      m_leftBoard->setCell(row, col, '.');
    }
  }

  const QJsonObject youObj = gameObj.value("you").toObject();
  const QJsonArray boardArray = youObj.value("board").toArray();

  for (int row = 0; row < boardArray.size(); ++row) {
    const QString rowString = boardArray[row].toString();
    for (int col = 0; col < rowString.size(); ++col) {
      m_leftBoard->setCell(row, col, rowString[col]);
    }
  }


  if (gameObj.contains("opponent")) {
    const QJsonObject opponentObj = gameObj.value("opponent").toObject();

    if (opponentObj.contains("board")) {
      for (int row = 0; row < 10; ++row) {
        for (int col = 0; col < 10; ++col) {
          m_rightBoard->setCell(row, col, '.');
        }
      }

      const QJsonArray opponentBoard = opponentObj.value("board").toArray();

      for (int row = 0; row < opponentBoard.size(); ++row) {
        const QString rowString = opponentBoard[row].toString();
        for (int col = 0; col < rowString.size(); ++col) {
          m_rightBoard->setCell(row, col, rowString[col]);
        }
      }
    }
  }

  qDebug() << "Game state applied";
}

void GameWindow::fireAt(int row, int col)
{
  if (m_gameFinished) {
    QMessageBox::information(this, "Игра завершена", "Партия уже окончена");
    return;
  }

  if (m_gameId.isEmpty()) {
    QMessageBox::warning(this, "Ошибка", "gameId пустой");
    return;
  }

  if (m_rightBoard->cellColor(row, col) == Qt::red ||
      m_rightBoard->cellColor(row, col) == Qt::blue) {
    return;
  }

  QJsonObject payload;
  payload["gameId"] = m_gameId;

  QString error;
  const QJsonObject state =
      ApiClient::instance().sendRequest("game_state", payload, 3000, error);

  if (!error.isEmpty()) {
    qDebug() << "game_state before fire error:" << error;
    return;
  }

  const bool yourTurn =
      state.value("payload").toObject()
          .value("game").toObject()
          .value("yourTurn").toBool();

  if (!yourTurn) {
    QMessageBox::information(this, "Не ваш ход", "Подождите соперника");
    return;
  }

  payload["x"] = col;
  payload["y"] = row;

  error.clear();
  const QJsonObject response =
      ApiClient::instance().sendRequest("fire", payload, 3000, error);

  if (!error.isEmpty()) {
    QMessageBox::warning(this, "Ошибка fire", error);
    qDebug() << "fire error:" << error;
    return;
  }

  qDebug() << "fire response:" << response;

  const QString status = response.value("status").toString();
  if (status != "ok") {
    const QJsonObject errorObj = response.value("error").toObject();
    const QString message =
        errorObj.value("message").toString("fire failed");
    QMessageBox::warning(this, "Ошибка", message);
    return;
  }

  const QJsonObject payloadObj = response.value("payload").toObject();
  const QJsonObject resultObj = payloadObj.value("result").toObject();

  const int x = resultObj.value("x").toInt();
  const int y = resultObj.value("y").toInt();
  const bool hit = resultObj.value("hit").toBool();
  const bool sunk = resultObj.value("sunk").toBool();

  if (hit) {
    m_rightBoard->setCell(y, x, 'X');
    m_hitSound.play();

    if (sunk) {
      m_sunkSound.play();
    }
  } else {
    m_rightBoard->setCell(y, x, 'o');
    m_missSound.play();
  }

  loadGameState();
}

void GameWindow::updateGameFinishedState(const QJsonObject& gameObj)
{
  const QString status = gameObj.value("status").toString();
  m_gameFinished = (status == "finished");

  m_rightBoard->setEnabled(!m_gameFinished);
}

void GameWindow::showGameFinishedScreen(const QJsonObject& notification)
{
  if (m_gameFinished) {
    return;
  }

  const QJsonObject payloadObj = notification.value("payload").toObject();
  const QString messageFromServer = payloadObj.value("message").toString();

  QString title = "Игра окончена";
  QString text;

  const QString lower = messageFromServer.toLower();

  if (lower.contains("you win") || lower.contains("you won") || lower.contains("victory")) {
    text = "Победа!";
    m_winSound.play();
  } else if (lower.contains("you lose") || lower.contains("you lost") || lower.contains("defeat")) {
    text = "Поражение";
    m_loseSound.play();
  } else {
    text = "Партия завершена";
  }

  if (!messageFromServer.isEmpty()) {
    text += "\n\n";
    text += messageFromServer;
  }

  m_gameFinished = true;
  m_rightBoard->setEnabled(false);

  QMessageBox::information(this, title, text);
}