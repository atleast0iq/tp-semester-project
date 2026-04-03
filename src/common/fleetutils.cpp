#include "fleetutils.h"

#include <algorithm>

namespace protocol::fleet {

bool inBounds(int x, int y) {
  return x >= 0 && x < kBoardSize && y >= 0 && y < kBoardSize;
}

OccupancyGrid createOccupancyGrid() {
  return OccupancyGrid(kBoardSize, QVector<bool>(kBoardSize, false));
}

QVector<QPoint> shipCells(const ShipPlacement& placement) {
  QVector<QPoint> cells;
  cells.reserve(placement.length);

  for (int offset = 0; offset < placement.length; ++offset) {
    cells.append(QPoint(placement.x + (placement.horizontal ? offset : 0),
                        placement.y + (placement.horizontal ? 0 : offset)));
  }

  return cells;
}

PlacementError validateShipPlacement(const OccupancyGrid& occupied,
                                     const QVector<QPoint>& cells) {
  for (const QPoint& cell : cells) {
    if (!inBounds(cell.x(), cell.y())) {
      return PlacementError::OutOfBounds;
    }

    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
      for (int offsetX = -1; offsetX <= 1; ++offsetX) {
        const int neighborX = cell.x() + offsetX;
        const int neighborY = cell.y() + offsetY;
        if (inBounds(neighborX, neighborY) && occupied[neighborY][neighborX]) {
          return PlacementError::OverlapOrTouch;
        }
      }
    }
  }

  return PlacementError::None;
}

void occupyShipCells(OccupancyGrid& occupied, const QVector<QPoint>& cells) {
  for (const QPoint& cell : cells) {
    occupied[cell.y()][cell.x()] = true;
  }
}

bool matchesFleetSpecification(const QVector<ShipPlacement>& placements) {
  QVector<int> actual;
  actual.reserve(placements.size());
  for (const ShipPlacement& placement : placements) {
    actual.append(placement.length);
  }

  QVector<int> expected = fleetSpecification();
  std::ranges::sort(actual);
  std::ranges::sort(expected);
  return expected == actual;
}

}  // namespace protocol::fleet
