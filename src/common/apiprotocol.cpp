#include "apiprotocol.h"

#include <QJsonDocument>
#include <QJsonValue>
#include <QRandomGenerator>
#include <QtGlobal>
#include <cmath>
#include <limits>

namespace protocol {
namespace {

bool inBounds(int x, int y) {
  return x >= 0 && x < kBoardSize && y >= 0 && y < kBoardSize;
}

bool canPlaceShip(const QVector<QVector<bool>>& occupied, int x, int y,
                  int length, bool horizontal) {
  for (int index = 0; index < length; ++index) {
    const int currentX = x + (horizontal ? index : 0);
    const int currentY = y + (horizontal ? 0 : index);

    if (!inBounds(currentX, currentY)) {
      return false;
    }

    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
      for (int offsetX = -1; offsetX <= 1; ++offsetX) {
        const int neighborX = currentX + offsetX;
        const int neighborY = currentY + offsetY;
        if (inBounds(neighborX, neighborY) && occupied[neighborY][neighborX]) {
          return false;
        }
      }
    }
  }

  return true;
}

void markShip(QVector<QVector<bool>>& occupied, int x, int y, int length,
              bool horizontal) {
  for (int index = 0; index < length; ++index) {
    const int currentX = x + (horizontal ? index : 0);
    const int currentY = y + (horizontal ? 0 : index);
    occupied[currentY][currentX] = true;
  }
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

QJsonArray shipPlacementsToJson(const QVector<ShipPlacement>& placements) {
  QJsonArray array;
  for (const ShipPlacement& placement : placements) {
    array.append(QJsonObject{
        {QStringLiteral("x"), placement.x},
        {QStringLiteral("y"), placement.y},
        {QStringLiteral("length"), placement.length},
        {QStringLiteral("orientation"), placement.horizontal
                                            ? QStringLiteral("horizontal")
                                            : QStringLiteral("vertical")},
    });
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

    const QJsonObject object = item.toObject();
    const QString orientation = object.value(QStringLiteral("orientation"))
                                    .toString()
                                    .trimmed()
                                    .toLower();
    int x = 0;
    int y = 0;
    int length = 0;

    if (!jsonValueToInt(object.value(QStringLiteral("x")), x) ||
        !jsonValueToInt(object.value(QStringLiteral("y")), y) ||
        !jsonValueToInt(object.value(QStringLiteral("length")), length) ||
        (orientation != QStringLiteral("horizontal") &&
         orientation != QStringLiteral("vertical"))) {
      error = QStringLiteral(
          "Ship placement must contain integer x/y/length and a valid "
          "orientation.");
      return false;
    }

    placements.append(ShipPlacement{
        .x = x,
        .y = y,
        .length = length,
        .horizontal = orientation == QStringLiteral("horizontal"),
    });
  }

  return true;
}

QVector<ShipPlacement> generateRandomFleet() {
  const QVector<int>& fleet = fleetSpecification();
  QVector<QVector<bool>> occupied(kBoardSize, QVector<bool>(kBoardSize, false));
  QVector<ShipPlacement> placements;
  placements.reserve(fleet.size());

  for (const int length : fleet) {
    bool placed = false;
    for (int attempt = 0; attempt < 2048 && !placed; ++attempt) {
      const bool horizontal = QRandomGenerator::global()->bounded(2) == 0;
      const int x = QRandomGenerator::global()->bounded(kBoardSize);
      const int y = QRandomGenerator::global()->bounded(kBoardSize);

      if (!canPlaceShip(occupied, x, y, length, horizontal)) {
        continue;
      }

      markShip(occupied, x, y, length, horizontal);
      placements.append(ShipPlacement{
          .x = x,
          .y = y,
          .length = length,
          .horizontal = horizontal,
      });
      placed = true;
    }

    if (!placed) {
      return generateRandomFleet();
    }
  }

  return placements;
}

}  // namespace protocol
