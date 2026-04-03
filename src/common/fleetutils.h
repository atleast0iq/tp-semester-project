#pragma once

#include <QPoint>
#include <QVector>

#include "apiprotocol.h"

namespace protocol::fleet {

enum class PlacementError {
  None,
  OutOfBounds,
  OverlapOrTouch,
};

using OccupancyGrid = QVector<QVector<bool>>;

bool inBounds(int x, int y);
OccupancyGrid createOccupancyGrid();
QVector<QPoint> shipCells(const ShipPlacement& placement);
PlacementError validateShipPlacement(const OccupancyGrid& occupied,
                                     const QVector<QPoint>& cells);
void occupyShipCells(OccupancyGrid& occupied, const QVector<QPoint>& cells);
bool matchesFleetSpecification(const QVector<ShipPlacement>& placements);

}  // namespace protocol::fleet
