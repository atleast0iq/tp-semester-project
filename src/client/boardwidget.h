#pragma once

#include <QWidget>
#include <QVector>

class QGridLayout;
class GameSquare;


class BoardWidget : public QWidget
{
  Q_OBJECT

 public:
  explicit BoardWidget(QWidget *parent = nullptr);

  void setCellColor(int row, int col, const QColor& color);
  QColor cellColor(int row, int col) const;
  void setCell(int row, int col, QChar value);


 signals:
  void cellClicked(int row, int col);

 private:
  void buildBoard();

  QVector<QVector<GameSquare*>> m_cells;
};