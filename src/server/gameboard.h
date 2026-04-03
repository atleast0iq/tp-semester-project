#pragma once

#include <QJsonArray>
#include <QPoint>
#include <QString>
#include <QVector>

#include "apiprotocol.h"

class GameBoard final {
 public:
  struct FireResult {
    bool hit = false;
    bool sunk = false;
  };

  GameBoard();

  bool placeFleet(const QVector<protocol::ShipPlacement>& placements,
                  QString& error);
  bool fireAt(int x, int y, FireResult& result, QString& error);

  bool fleetReady() const;
  bool allShipsSunk() const;
  int shipsRemaining() const;
  QJsonArray toJsonRows(bool revealShips) const;

 private:
  struct Cell {
    int shipIndex = -1;
    bool shot = false;
  };

  struct Ship {
    QVector<QPoint> cells;
    int hits = 0;
  };

  void reset();
  bool inBounds(int x, int y) const;

  QVector<QVector<Cell>> m_grid;
  QVector<Ship> m_ships;
  bool m_fleetReady = false;
};
