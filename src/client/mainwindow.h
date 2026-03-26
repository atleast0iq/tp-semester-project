#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow();

 private:
  Ui::MainWindow* ui;

  void setupUIState();
  void setupConnections();
  void handleConnect();
  void handleLogin();
  void handleRegister();
  void handleRefresh();



  bool ensureConnected();
  bool validAuthInput(QString& login, QString& password);
  void populateGamesTable(const QJsonArray& games);
  void showError(const QString& title, const QString& message);
};

#endif  // MAINWINDOW_H
