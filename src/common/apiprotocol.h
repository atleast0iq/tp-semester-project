#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVector>

namespace protocol {

inline constexpr int kBoardSize = 10;

struct ShipPlacement {
  int x = 0;
  int y = 0;
  int length = 0;
  bool horizontal = true;
};

struct MessageParseResult {
  bool ok = false;
  QJsonObject message;
  QString error;
};

const QVector<int>& fleetSpecification();
bool jsonValueToInt(const QJsonValue& value, int& result);
QByteArray serializeLine(const QJsonObject& message);
MessageParseResult parseMessageLine(QByteArray line);
QJsonObject makeSuccessResponse(const QString& requestId,
                                const QJsonObject& payload = {});
QJsonObject makeErrorResponse(const QString& requestId, const QString& code,
                              const QString& message);
QJsonArray shipPlacementsToJson(const QVector<ShipPlacement>& placements);
bool shipPlacementsFromJson(const QJsonValue& value,
                            QVector<ShipPlacement>& placements, QString& error);
QVector<ShipPlacement> generateRandomFleet();

}  // namespace protocol
