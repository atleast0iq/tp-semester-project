#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QSoundEffect>

namespace Ui {
class GameWindow;
}

class BoardWidget;

class GameWindow : public QMainWindow
{
  Q_OBJECT

 public:
  explicit GameWindow(const QString& gameId,
                      const QString& username,
                      QWidget *parent = nullptr);
  ~GameWindow();

 private:
  Ui::GameWindow *ui;

  BoardWidget *m_leftBoard;
  BoardWidget *m_rightBoard;

  QString m_gameId;
  QString m_username;

  bool m_gameFinished = false;

  QSoundEffect m_hitSound;
  QSoundEffect m_missSound;
  QSoundEffect m_sunkSound;
  QSoundEffect m_winSound;
  QSoundEffect m_loseSound;

  void setupBoards();
  void setupSounds();
  void loadGameState();
  void fireAt(int row, int col);

  void showGameFinishedScreen(const QJsonObject& notification);
  void updateGameFinishedState(const QJsonObject& gameObj);
};

#endif