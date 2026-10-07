#pragma once

#include <QDateTime>
#include <QString>
#include <QUuid>

namespace grossbuch {

// Fabriques partagées pour l'identité synchronisable des lignes (voir
// docs/adr/0011). Horodatage ISO 8601 en temps universel et UUID sans accolades.

inline QString nowTimestampUtc()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

inline QString newUuid()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

} // namespace grossbuch
