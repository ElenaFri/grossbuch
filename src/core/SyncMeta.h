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

// UUID déterministe d'une occurrence de paiement récurrent, dérivé de l'uuid du
// modèle et du couple année-mois (UUID v5). Deux machines partageant le même
// modèle produisent le même uuid pour la même échéance, ce qui évite les doublons
// à la fusion. Voir docs/adr/0012.
inline QString occurrenceUuid(const QString &modelUuid, int year, int month)
{
    static const QUuid ns(QStringLiteral("{b5f8a0c2-7e3d-4a1b-9c6e-0a1b2c3d4e5f}"));
    const QString base = QStringLiteral("%1:%2-%3")
                             .arg(modelUuid)
                             .arg(year, 4, 10, QLatin1Char('0'))
                             .arg(month, 2, 10, QLatin1Char('0'));
    return QUuid::createUuidV5(ns, base.toUtf8()).toString(QUuid::WithoutBraces);
}

} // namespace grossbuch
