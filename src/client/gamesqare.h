#pragma once

#include <QColor>
#include <QWidget>

class GameSquare : public QWidget
{
  Q_OBJECT

 public:
  explicit GameSquare(int row, int col, QWidget *parent = nullptr);

  void recolor(const QColor& newColor);
  QColor color() const;

 signals:
  void clicked(int row, int col);

 protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

 private:
  int m_row;
  int m_col;
  QColor m_currentColor;
};