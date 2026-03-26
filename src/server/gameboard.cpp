#include "gameboard.h"

#include <algorithm>
#include <utility>

namespace {

bool sameFleetLayout(const QVector<protocol::ShipPlacement>& placements) {
  QVector<int> actual;
  for (const protocol::ShipPlacement& placement : placements) {
    actual.append(placement.length);
  }

  QVector<int> expected = protocol::fleetSpecification();
  std::sort(actual.begin(), actual.end());
  std::sort(expected.begin(), expected.end());
  return expected == actual;
}

}  // namespace

GameBoard::GameBoard() { reset(); }

bool GameBoard::placeFleet(const QVector<protocol::ShipPlacement>& placements,
                           QString& error) {
  if (placements.size() != protocol::fleetSpecification().size()) {
    error = QStringLiteral("Fleet must contain exactly 10 ships.");
    return false;
  }

  if (!sameFleetLayout(placements)) {
    error = QStringLiteral(
        "Fleet must match the classic battleship setup 4,3,3,2,2,2,1,1,1,1.");
    return false;
  }

  QVector<QVector<bool>> occupied(protocol::kBoardSize,
                                  QVector<bool>(protocol::kBoardSize, false));
  QVector<Ship> ships;
  ships.reserve(placements.size());

  for (const protocol::ShipPlacement& placement : placements) {
    Ship ship;
    ship.cells.reserve(placement.length);

    for (int offset = 0; offset < placement.length; ++offset) {
      const int x = placement.x + (placement.horizontal ? offset : 0);
      const int y = placement.y + (placement.horizontal ? 0 : offset);

      if (!inBounds(x, y)) {
        error = QStringLiteral("Ship placement goes outside the board.");
        return false;
      }

      for (int offsetY = -1; offsetY <= 1; ++offsetY) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
          const int neighborX = x + offsetX;
          const int neighborY = y + offsetY;

          if (inBounds(neighborX, neighborY) &&
              occupied[neighborY][neighborX]) {
            error =
                QStringLiteral("Ships must not overlap or touch each other.");
            return false;
          }
        }
      }

      ship.cells.append(QPoint(x, y));
    }

    ships.append(ship);
    for (const QPoint& cell : ship.cells) {
      occupied[cell.y()][cell.x()] = true;
    }
  }

  reset();
  m_ships = std::move(ships);
  for (int shipIndex = 0; shipIndex < m_ships.size(); ++shipIndex) {
    for (const QPoint& cell : m_ships[shipIndex].cells) {
      m_grid[cell.y()][cell.x()].shipIndex = shipIndex;
    }
  }

  m_fleetReady = true;
  return true;
}

bool GameBoard::fireAt(int x, int y, FireResult& result, QString& error) {
  if (!inBounds(x, y)) {
    error = QStringLiteral("Shot coordinates are outside the board.");
    return false;
  }

  Cell& cell = m_grid[y][x];
  if (cell.shot) {
    error = QStringLiteral("This cell has already been targeted.");
    return false;
  }

  cell.shot = true;
  if (cell.shipIndex < 0) {
    result.hit = false;
    result.sunk = false;
    return true;
  }

  Ship& ship = m_ships[cell.shipIndex];
  ++ship.hits;
  result.hit = true;
  result.sunk = ship.hits == ship.cells.size();
  return true;
}

bool GameBoard::fleetReady() const { return m_fleetReady; }

bool GameBoard::allShipsSunk() const {
  return m_fleetReady &&
         std::all_of(m_ships.cbegin(), m_ships.cend(), [](const Ship& ship) {
           return ship.hits == ship.cells.size();
         });
}

int GameBoard::shipsRemaining() const {
  return std::count_if(m_ships.cbegin(), m_ships.cend(), [](const Ship& ship) {
    return ship.hits != ship.cells.size();
  });
}

QJsonArray GameBoard::toJsonRows(bool revealShips) const {
  QJsonArray rows;
  for (int y = 0; y < protocol::kBoardSize; ++y) {
    QString row;
    row.reserve(protocol::kBoardSize);

    for (int x = 0; x < protocol::kBoardSize; ++x) {
      const Cell& cell = m_grid[y][x];
      if (cell.shot && cell.shipIndex >= 0) {
        row.append(QLatin1Char('X'));
      } else if (cell.shot) {
        row.append(QLatin1Char('o'));
      } else if (revealShips && cell.shipIndex >= 0) {
        row.append(QLatin1Char('S'));
      } else {
        row.append(QLatin1Char('.'));
      }
    }

    rows.append(row);
  }

  return rows;
}

void GameBoard::reset() {
  m_grid.fill(QVector<Cell>(protocol::kBoardSize, Cell{}),
              protocol::kBoardSize);
  m_ships.clear();
  m_fleetReady = false;
}

bool GameBoard::inBounds(int x, int y) const {
  return x >= 0 && x < protocol::kBoardSize && y >= 0 &&
         y < protocol::kBoardSize;
}
