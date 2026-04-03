#include "boardwidget.h"
#include "gamesqare.h"

#include <QGridLayout>
#include <QLabel>
#include <QStringList>

BoardWidget::BoardWidget(QWidget *parent)
    : QWidget(parent)
{
  buildBoard();
}

void BoardWidget::buildBoard()
{
  auto *layout = new QGridLayout(this);
  layout->setHorizontalSpacing(0);
  layout->setVerticalSpacing(0);
  layout->setContentsMargins(0, 0, 0, 0);

  const QStringList labels = {"А", "Б", "В", "Г", "Д", "Е", "Ж", "З", "И", "К"};

  layout->addWidget(new QLabel(" "), 0, 0);

  for (int col = 0; col < 10; ++col) {
    auto *label = new QLabel(labels[col], this);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label, 0, col + 1);
  }

  m_cells.resize(10);

  for (int row = 0; row < 10; ++row) {
    auto *rowLabel = new QLabel(QString::number(row), this);
    rowLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(rowLabel, row + 1, 0);

    m_cells[row].resize(10);

    for (int col = 0; col < 10; ++col) {
      auto *cell = new GameSquare(row, col, this);
      m_cells[row][col] = cell;
      layout->addWidget(cell, row + 1, col + 1);

      connect(cell, &GameSquare::clicked,
              this, &BoardWidget::cellClicked);
    }
  }
}

void BoardWidget::setCellColor(int row, int col, const QColor& color)
{
  if (row < 0 || row >= 10 || col < 0 || col >= 10) {
    return;
  }

  m_cells[row][col]->recolor(color);
}

QColor BoardWidget::cellColor(int row, int col) const
{
  if (row < 0 || row >= 10 || col < 0 || col >= 10) {
    return Qt::white;
  }

  return m_cells[row][col]->color();
}
void BoardWidget::setCell(int row, int col, QChar value)
{
  if (row < 0 || row >= 10 || col < 0 || col >= 10) {
    return;
  }

  QColor color = Qt::white;

  if (value == 'S') {
    color = Qt::gray;
  } else if (value == 'o') {
    color = Qt::blue;
  } else if (value == 'X') {
    color = Qt::red;
  }

  m_cells[row][col]->recolor(color);
}

