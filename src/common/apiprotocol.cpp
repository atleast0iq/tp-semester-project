#include "apiprotocol.h"

#include <QJsonDocument>
#include <QJsonValue>
#include <QRandomGenerator>
#include <QtGlobal>
#include <cmath>
#include <limits>

#include "fleetutils.h"

namespace protocol {
namespace {

constexpr int kRandomPlacementAttempts = 2048;
constexpr auto kOrientationHorizontal = "horizontal";
constexpr auto kOrientationVertical = "vertical";

QString orientationString(bool horizontal) {
  return horizontal ? QString::fromLatin1(kOrientationHorizontal)
                    : QString::fromLatin1(kOrientationVertical);
}

QJsonObject shipPlacementJson(const ShipPlacement& placement) {
  return {
      {QStringLiteral("x"), placement.x},
      {QStringLiteral("y"), placement.y},
      {QStringLiteral("length"), placement.length},
      {QStringLiteral("orientation"), orientationString(placement.horizontal)},
  };
}

bool parseShipPlacement(const QJsonObject& object, ShipPlacement& placement,
                        QString& error) {
  const QString orientation = object.value(QStringLiteral("orientation"))
                                  .toString()
                                  .trimmed()
                                  .toLower();
  if ((orientation != QString::fromLatin1(kOrientationHorizontal) &&
       orientation != QString::fromLatin1(kOrientationVertical)) ||
      !jsonValueToInt(object.value(QStringLiteral("x")), placement.x) ||
      !jsonValueToInt(object.value(QStringLiteral("y")), placement.y) ||
      !jsonValueToInt(object.value(QStringLiteral("length")), placement.length)) {
    error = QStringLiteral(
        "Ship placement must contain integer x/y/length and a valid "
        "orientation.");
    return false;
  }

  placement.horizontal = orientation == QString::fromLatin1(kOrientationHorizontal);
  return true;
}

bool tryPlaceRandomShip(int length, fleet::OccupancyGrid& occupied,
                        ShipPlacement& placement) {
  for (int attempt = 0; attempt < kRandomPlacementAttempts; ++attempt) {
    placement = ShipPlacement{
        .x = QRandomGenerator::global()->bounded(kBoardSize),
        .y = QRandomGenerator::global()->bounded(kBoardSize),
        .length = length,
        .horizontal = QRandomGenerator::global()->bounded(2) == 0,
    };
    const QVector<QPoint> cells = fleet::shipCells(placement);
    if (fleet::validateShipPlacement(occupied, cells) !=
        fleet::PlacementError::None) {
      continue;
    }

    fleet::occupyShipCells(occupied, cells);
    return true;
  }

  return false;
}

}  // namespace

const QVector<int>& fleetSpecification() {
  static const QVector<int> kFleet{4, 3, 3, 2, 2, 2, 1, 1, 1, 1};
  return kFleet;
}

bool jsonValueToInt(const QJsonValue& value, int& result) {
  if (!value.isDouble()) {
    return false;
  }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const qint64 integer = value.toInteger();
  if (!std::in_range<int>(integer)) {
    return false;
  }
  result = static_cast<int>(integer);
  return true;
#else
  const double number = value.toDouble();
  if (!qIsFinite(number) || std::floor(number) != number ||
      number < std::numeric_limits<int>::min() ||
      number > std::numeric_limits<int>::max()) {
    return false;
  }

  result = static_cast<int>(number);
  return true;
#endif
}

QByteArray serializeLine(const QJsonObject& message) {
  QByteArray payload = QJsonDocument(message).toJson(QJsonDocument::Compact);
  payload.append('\n');
  return payload;
}

MessageParseResult parseMessageLine(QByteArray line) {
  MessageParseResult result;
  const QByteArray rawLine = line.trimmed();

  if (rawLine.isEmpty()) {
    result.error = QStringLiteral("Empty JSON line received.");
    return result;
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(rawLine, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    result.error = QStringLiteral("Invalid JSON payload: %1")
                       .arg(parseError.errorString());
    return result;
  }

  result.ok = true;
  result.message = document.object();
  return result;
}

QJsonObject makeSuccessResponse(const QString& requestId,
                                const QJsonObject& payload) {
  return {
      {QStringLiteral("requestId"), requestId},
      {QStringLiteral("status"), QStringLiteral("ok")},
      {QStringLiteral("payload"), payload},
  };
}

QJsonObject makeErrorResponse(const QString& requestId, const QString& code,
                              const QString& message) {
  return {
      {QStringLiteral("requestId"), requestId},
      {QStringLiteral("status"), QStringLiteral("error")},
      {QStringLiteral("error"),
       QJsonObject{
           {QStringLiteral("code"), code},
           {QStringLiteral("message"), message},
       }},
  };
}

QJsonObject makeEventMessage(const QString& eventName,
                             const QJsonObject& payload) {
  return {
      {QStringLiteral("type"), QStringLiteral("event")},
      {QStringLiteral("event"), eventName},
      {QStringLiteral("payload"), payload},
  };
}

bool isEventMessage(const QJsonObject& message) {
  return message.value(QStringLiteral("type")).toString() ==
         QStringLiteral("event");
}

QJsonArray shipPlacementsToJson(const QVector<ShipPlacement>& placements) {
  QJsonArray array;
  for (const ShipPlacement& placement : placements) {
    array.append(shipPlacementJson(placement));
  }

  return array;
}

bool shipPlacementsFromJson(const QJsonValue& value,
                            QVector<ShipPlacement>& placements,
                            QString& error) {
  if (!value.isArray()) {
    error = QStringLiteral("`ships` must be a JSON array.");
    return false;
  }

  placements.clear();
  const QJsonArray array = value.toArray();
  for (const QJsonValue& item : array) {
    if (!item.isObject()) {
      error = QStringLiteral("Each ship placement must be an object.");
      return false;
    }

    ShipPlacement placement;
    if (!parseShipPlacement(item.toObject(), placement, error)) {
      return false;
    }

    placements.append(placement);
  }

  return true;
}

QVector<ShipPlacement> generateRandomFleet() {
  const QVector<int>& fleet = fleetSpecification();
  while (true) {
    fleet::OccupancyGrid occupied = fleet::createOccupancyGrid();
    QVector<ShipPlacement> placements;
    placements.reserve(fleet.size());

    for (const int length : fleet) {
      ShipPlacement placement;
      if (!tryPlaceRandomShip(length, occupied, placement)) {
        placements.clear();
        break;
      }

      placements.append(placement);
    }
    if (placements.size() == fleet.size()) {
      return placements;
    }
  }
}

}  // namespace protocol
