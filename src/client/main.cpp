#include <QApplication>

#include "mainwindow.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  QApplication::setApplicationName("Battleship Client");
  QApplication::setOrganizationName("TP Semester Project");

  MainWindow window;
  window.show();

  return app.exec();
}
