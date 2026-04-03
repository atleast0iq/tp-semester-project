#include "gamesqare.h"

#include <QDebug>
#include <QMouseEvent>
#include <QPainter>

GameSquare::GameSquare(int row, int col, QWidget *parent)
    : QWidget(parent),
      m_row(row),
      m_col(col),
      m_currentColor(Qt::white)
{
  setFixedSize(40, 40);
}

void GameSquare::recolor(const QColor& newColor)
{
  m_currentColor = newColor;
  update();
}

QColor GameSquare::color() const
{
  return m_currentColor;
}

void GameSquare::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  painter.setBrush(m_currentColor);
  painter.setPen(Qt::black);

  QRect squareRect = rect().adjusted(0, 0, -1, -1);
  painter.drawRect(squareRect);
}

void GameSquare::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton) {
    qDebug() << "Square clicked:" << m_row << m_col;
    emit clicked(m_row, m_col);
  }

  QWidget::mousePressEvent(event);
}