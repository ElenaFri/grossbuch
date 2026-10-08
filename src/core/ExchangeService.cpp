#include "core/ExchangeService.h"

#include "core/BackupService.h"
#include "core/Category.h"
#include "core/CategoryRepository.h"
#include "core/Database.h"
#include "core/SyncMeta.h"

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

#include <climits>
#include <cmath>

namespace grossbuch {

namespace {

// Lit un montant JSON (stocké en nombre) en centimes entiers exacts.
qint64 jsonAmount(const QJsonValue &value)
{
    return static_cast<qint64>(std::llround(value.toDouble(0)));
}

// Ordre absolu d'un mois (année*12 + mois-1) ; un repère vide (0/0) vaut INT_MIN
// pour qu'un vrai repère l'emporte toujours lors d'un max.
int markerOrdinal(int year, int month)
{
    if (month < 1 || month > 12)
        return INT_MIN;
    return year * 12 + (month - 1);
}

std::pair<int, int> markerFromOrdinal(int ordinal)
{
    if (ordinal == INT_MIN)
        return {0, 0};
    return {ordinal / 12, ordinal % 12 + 1};
}

QVariant labelBind(const QJsonValue &labelValue)
{
    if (labelValue.isNull())
        return QVariant(QMetaType(QMetaType::QString));
    const QString label = labelValue.toString();
    if (label.isEmpty())
        return QVariant(QMetaType(QMetaType::QString));
    return QVariant(label);
}

// Vrai si le libellé entrant diffère du libellé local, « vide » et « null » étant
// traités comme équivalents.
bool labelChanged(bool localLabelNull, const QString &localLabel, const QJsonValue &labelValue)
{
    const bool incomingLabelNull = labelValue.isNull() || labelValue.toString().isEmpty();
    if (localLabelNull != incomingLabelNull)
        return true;
    return !incomingLabelNull && localLabel != labelValue.toString();
}

// Vrai si le rattachement à un paiement récurrent diffère (null compris).
bool recurringLinkChanged(const QVariant &local, const QVariant &incoming)
{
    const bool incomingNull = incoming.isNull();
    if (local.isNull() != incomingNull)
        return true;
    return !incomingNull && local.toInt() != incoming.toInt();
}

bool mergeRecurring(QSqlDatabase &db, const QJsonObject &object,
                    const QHash<QString, int> &categoryByKey, MergeReport &report)
{
    const QString uuid = object.value(QStringLiteral("uuid")).toString();
    const QString categoryKey = object.value(QStringLiteral("category")).toString();
    if (uuid.isEmpty() || !categoryByKey.contains(categoryKey)) {
        ++report.skipped;
        return true;
    }
    const int categoryId = categoryByKey.value(categoryKey);

    const qint64 amount = jsonAmount(object.value(QStringLiteral("amount")));
    const QJsonValue labelValue = object.value(QStringLiteral("label"));
    const int dayOfMonth = object.value(QStringLiteral("dayOfMonth")).toInt();
    const int startYear = object.value(QStringLiteral("startYear")).toInt();
    const int startMonth = object.value(QStringLiteral("startMonth")).toInt();
    const bool active = object.value(QStringLiteral("active")).toBool();
    const int lastYear = object.value(QStringLiteral("lastYear")).toInt();
    const int lastMonth = object.value(QStringLiteral("lastMonth")).toInt();
    const QString createdAt = object.value(QStringLiteral("createdAt")).toString();
    const QString updatedAt = object.value(QStringLiteral("updatedAt")).toString();
    const bool deleted = object.value(QStringLiteral("deleted")).toBool();

    QSqlQuery select(db);
    select.prepare(QStringLiteral(
        "SELECT amount, label, category_id, day_of_month, start_year, start_month, active, "
        "last_year, last_month, updated_at FROM recurring_expenses WHERE uuid = ?"));
    select.addBindValue(uuid);
    if (!select.exec())
        return false;

    if (!select.next()) {
        QSqlQuery insert(db);
        insert.prepare(QStringLiteral(
            "INSERT INTO recurring_expenses(amount, label, category_id, day_of_month, start_year, "
            "start_month, active, last_year, last_month, uuid, created_at, updated_at, deleted) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
        insert.addBindValue(amount);
        insert.addBindValue(labelBind(labelValue));
        insert.addBindValue(categoryId);
        insert.addBindValue(dayOfMonth);
        insert.addBindValue(startYear);
        insert.addBindValue(startMonth);
        insert.addBindValue(active ? 1 : 0);
        insert.addBindValue(lastYear);
        insert.addBindValue(lastMonth);
        insert.addBindValue(uuid);
        insert.addBindValue(createdAt);
        insert.addBindValue(updatedAt);
        insert.addBindValue(deleted ? 1 : 0);
        if (!insert.exec())
            return false;
        ++report.recurringAdded;
        return true;
    }

    const qint64 localAmount = select.value(0).toLongLong();
    const bool localLabelNull = select.value(1).isNull();
    const QString localLabel = select.value(1).toString();
    const int localCategory = select.value(2).toInt();
    const int localDay = select.value(3).toInt();
    const int localStartYear = select.value(4).toInt();
    const int localStartMonth = select.value(5).toInt();
    const bool localActive = select.value(6).toInt() != 0;
    const int localLastYear = select.value(7).toInt();
    const int localLastMonth = select.value(8).toInt();
    const QString localUpdatedAt = select.value(9).toString();

    if (updatedAt <= localUpdatedAt) {
        ++report.recurringUnchanged;
        return true;
    }

    QStringList changes;
    if (localAmount != amount)
        changes << QStringLiteral("montant %1→%2").arg(localAmount).arg(amount);
    if (localCategory != categoryId)
        changes << QStringLiteral("catégorie");
    if (localDay != dayOfMonth)
        changes << QStringLiteral("jour du mois");
    if (localStartYear != startYear || localStartMonth != startMonth)
        changes << QStringLiteral("mois de début");
    if (localActive != active)
        changes << QStringLiteral("état actif");
    if (labelChanged(localLabelNull, localLabel, labelValue))
        changes << QStringLiteral("libellé");

    // Le repère de matérialisation est porté au plus avancé des deux (jamais en
    // arrière), indépendamment de la règle d'horodatage. Voir docs/adr/0012.
    const int mergedMarker = std::max(markerOrdinal(localLastYear, localLastMonth),
                                      markerOrdinal(lastYear, lastMonth));
    const auto [mergedYear, mergedMonth] = markerFromOrdinal(mergedMarker);

    QSqlQuery update(db);
    update.prepare(QStringLiteral(
        "UPDATE recurring_expenses SET amount = ?, label = ?, category_id = ?, day_of_month = ?, "
        "start_year = ?, start_month = ?, active = ?, last_year = ?, last_month = ?, "
        "updated_at = ?, deleted = ? WHERE uuid = ?"));
    update.addBindValue(amount);
    update.addBindValue(labelBind(labelValue));
    update.addBindValue(categoryId);
    update.addBindValue(dayOfMonth);
    update.addBindValue(startYear);
    update.addBindValue(startMonth);
    update.addBindValue(active ? 1 : 0);
    update.addBindValue(mergedYear);
    update.addBindValue(mergedMonth);
    update.addBindValue(updatedAt);
    update.addBindValue(deleted ? 1 : 0);
    update.addBindValue(uuid);
    if (!update.exec())
        return false;

    ++report.recurringUpdated;
    if (!changes.isEmpty()) {
        report.conflicts.append(MergeConflict{QStringLiteral("recurring_expenses"), uuid,
                                              localUpdatedAt, updatedAt, changes.join(QStringLiteral(", "))});
    }
    return true;
}

bool mergeExpense(QSqlDatabase &db, const QJsonObject &object,
                  const QHash<QString, int> &categoryByKey,
                  const QHash<QString, int> &recurringByUuid, MergeReport &report)
{
    const QString uuid = object.value(QStringLiteral("uuid")).toString();
    const QString categoryKey = object.value(QStringLiteral("category")).toString();
    if (uuid.isEmpty() || !categoryByKey.contains(categoryKey)) {
        ++report.skipped;
        return true;
    }
    const int categoryId = categoryByKey.value(categoryKey);

    const qint64 amount = jsonAmount(object.value(QStringLiteral("amount")));
    const QString date = object.value(QStringLiteral("date")).toString();
    const QJsonValue labelValue = object.value(QStringLiteral("label"));
    const QString createdAt = object.value(QStringLiteral("createdAt")).toString();
    const QString updatedAt = object.value(QStringLiteral("updatedAt")).toString();
    const bool deleted = object.value(QStringLiteral("deleted")).toBool();

    // Lien vers un modèle récurrent, résolu par uuid vers l'identifiant local.
    QVariant recurringId(QMetaType(QMetaType::Int));
    const QJsonValue recurringValue = object.value(QStringLiteral("recurring"));
    if (!recurringValue.isNull() && recurringByUuid.contains(recurringValue.toString()))
        recurringId = recurringByUuid.value(recurringValue.toString());

    QSqlQuery select(db);
    select.prepare(QStringLiteral(
        "SELECT amount, date, label, category_id, recurring_id, updated_at, deleted "
        "FROM expenses WHERE uuid = ?"));
    select.addBindValue(uuid);
    if (!select.exec())
        return false;

    if (!select.next()) {
        QSqlQuery insert(db);
        insert.prepare(QStringLiteral(
            "INSERT INTO expenses(amount, date, label, category_id, recurring_id, uuid, "
            "created_at, updated_at, deleted) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)"));
        insert.addBindValue(amount);
        insert.addBindValue(date);
        insert.addBindValue(labelBind(labelValue));
        insert.addBindValue(categoryId);
        insert.addBindValue(recurringId);
        insert.addBindValue(uuid);
        insert.addBindValue(createdAt);
        insert.addBindValue(updatedAt);
        insert.addBindValue(deleted ? 1 : 0);
        if (!insert.exec())
            return false;
        ++report.expensesAdded;
        return true;
    }

    const qint64 localAmount = select.value(0).toLongLong();
    const QString localDate = select.value(1).toString();
    const bool localLabelNull = select.value(2).isNull();
    const QString localLabel = select.value(2).toString();
    const int localCategory = select.value(3).toInt();
    const QVariant localRecurring = select.value(4);
    const QString localUpdatedAt = select.value(5).toString();
    const bool localDeleted = select.value(6).toInt() != 0;

    if (updatedAt <= localUpdatedAt) {
        ++report.expensesUnchanged;
        return true;
    }

    QStringList changes;
    if (localAmount != amount)
        changes << QStringLiteral("montant %1→%2").arg(localAmount).arg(amount);
    if (localDate != date)
        changes << QStringLiteral("date");
    if (localCategory != categoryId)
        changes << QStringLiteral("catégorie");
    if (labelChanged(localLabelNull, localLabel, labelValue))
        changes << QStringLiteral("libellé");
    if (recurringLinkChanged(localRecurring, recurringId))
        changes << QStringLiteral("rattachement récurrent");

    QSqlQuery update(db);
    update.prepare(QStringLiteral(
        "UPDATE expenses SET amount = ?, date = ?, label = ?, category_id = ?, recurring_id = ?, "
        "updated_at = ?, deleted = ? WHERE uuid = ?"));
    update.addBindValue(amount);
    update.addBindValue(date);
    update.addBindValue(labelBind(labelValue));
    update.addBindValue(categoryId);
    update.addBindValue(recurringId);
    update.addBindValue(updatedAt);
    update.addBindValue(deleted ? 1 : 0);
    update.addBindValue(uuid);
    if (!update.exec())
        return false;

    if (deleted && !localDeleted)
        ++report.expensesDeleted;
    else
        ++report.expensesUpdated;

    if (!changes.isEmpty()) {
        report.conflicts.append(MergeConflict{QStringLiteral("expenses"), uuid, localUpdatedAt,
                                              updatedAt, changes.join(QStringLiteral(", "))});
    }
    return true;
}

// Fusionne tous les modèles récurrents du tableau ; s'arrête au premier échec.
bool mergeRecurringArray(QSqlDatabase &db, const QJsonArray &array,
                         const QHash<QString, int> &categoryByKey, MergeReport &report)
{
    for (const QJsonValue &value : array) {
        if (!mergeRecurring(db, value.toObject(), categoryByKey, report))
            return false;
    }
    return true;
}

// Fusionne toutes les dépenses du tableau ; s'arrête au premier échec.
bool mergeExpenseArray(QSqlDatabase &db, const QJsonArray &array,
                       const QHash<QString, int> &categoryByKey,
                       const QHash<QString, int> &recurringByUuid, MergeReport &report)
{
    for (const QJsonValue &value : array) {
        if (!mergeExpense(db, value.toObject(), categoryByKey, recurringByUuid, report))
            return false;
    }
    return true;
}

// Valide l'en-tête du document d'échange (format et version). Renseigne *error.
bool validateExchangeRoot(const QJsonObject &root, QString *error)
{
    if (root.value(QStringLiteral("format")).toString() != QString::fromLatin1(ExchangeService::formatName())) {
        if (error)
            *error = QStringLiteral("Format de fichier non reconnu.");
        return false;
    }
    const int version = root.value(QStringLiteral("formatVersion")).toInt(-1);
    if (version < 1 || version > ExchangeService::formatVersion) {
        if (error)
            *error = QStringLiteral("Version de format non prise en charge : %1").arg(version);
        return false;
    }
    return true;
}

// Sauvegarde systématique avant modification en place (voir docs/adr/0013).
// Un répertoire vide désactive la sauvegarde. Renseigne *error en cas d'échec.
bool performPreImportBackup(Database &database, const QString &directory, QString *error)
{
    if (directory.isEmpty())
        return true;
    BackupService backup(database, directory);
    QString backupError;
    if (!backup.createBackup(&backupError).has_value()) {
        if (error)
            *error = QStringLiteral("Sauvegarde préalable impossible, import annulé : %1")
                         .arg(backupError);
        return false;
    }
    backup.rotate();
    return true;
}

} // namespace

const char *ExchangeService::formatName()
{
    return "grossbuch-exchange";
}

ExchangeService::ExchangeService(Database &database) : m_database(database)
{
}

void ExchangeService::setBackupDirectory(const QString &directory)
{
    m_backupDirectory = directory;
}

QJsonDocument ExchangeService::exportDocument() const
{
    QSqlDatabase db = m_database.connection();

    QJsonArray expenses;
    {
        QSqlQuery query(db);
        query.exec(QStringLiteral(
            "SELECT e.uuid, e.amount, e.date, e.label, c.key, r.uuid, e.created_at, e.updated_at, "
            "e.deleted FROM expenses e JOIN categories c ON c.id = e.category_id "
            "LEFT JOIN recurring_expenses r ON r.id = e.recurring_id ORDER BY e.id"));
        while (query.next()) {
            QJsonObject object;
            object.insert(QStringLiteral("uuid"), query.value(0).toString());
            object.insert(QStringLiteral("amount"), static_cast<double>(query.value(1).toLongLong()));
            object.insert(QStringLiteral("date"), query.value(2).toString());
            object.insert(QStringLiteral("label"),
                          query.value(3).isNull() ? QJsonValue() : QJsonValue(query.value(3).toString()));
            object.insert(QStringLiteral("category"), query.value(4).toString());
            object.insert(QStringLiteral("recurring"),
                          query.value(5).isNull() ? QJsonValue() : QJsonValue(query.value(5).toString()));
            object.insert(QStringLiteral("createdAt"), query.value(6).toString());
            object.insert(QStringLiteral("updatedAt"), query.value(7).toString());
            object.insert(QStringLiteral("deleted"), query.value(8).toInt() != 0);
            expenses.append(object);
        }
    }

    QJsonArray recurring;
    {
        QSqlQuery query(db);
        query.exec(QStringLiteral(
            "SELECT r.uuid, r.amount, r.label, c.key, r.day_of_month, r.start_year, r.start_month, "
            "r.active, r.last_year, r.last_month, r.created_at, r.updated_at, r.deleted "
            "FROM recurring_expenses r JOIN categories c ON c.id = r.category_id ORDER BY r.id"));
        while (query.next()) {
            QJsonObject object;
            object.insert(QStringLiteral("uuid"), query.value(0).toString());
            object.insert(QStringLiteral("amount"), static_cast<double>(query.value(1).toLongLong()));
            object.insert(QStringLiteral("label"),
                          query.value(2).isNull() ? QJsonValue() : QJsonValue(query.value(2).toString()));
            object.insert(QStringLiteral("category"), query.value(3).toString());
            object.insert(QStringLiteral("dayOfMonth"), query.value(4).toInt());
            object.insert(QStringLiteral("startYear"), query.value(5).toInt());
            object.insert(QStringLiteral("startMonth"), query.value(6).toInt());
            object.insert(QStringLiteral("active"), query.value(7).toInt() != 0);
            object.insert(QStringLiteral("lastYear"), query.value(8).toInt());
            object.insert(QStringLiteral("lastMonth"), query.value(9).toInt());
            object.insert(QStringLiteral("createdAt"), query.value(10).toString());
            object.insert(QStringLiteral("updatedAt"), query.value(11).toString());
            object.insert(QStringLiteral("deleted"), query.value(12).toInt() != 0);
            recurring.append(object);
        }
    }

    QJsonObject root;
    root.insert(QStringLiteral("format"), QString::fromLatin1(formatName()));
    root.insert(QStringLiteral("formatVersion"), formatVersion);
    root.insert(QStringLiteral("exportedAt"), nowTimestampUtc());
    root.insert(QStringLiteral("expenses"), expenses);
    root.insert(QStringLiteral("recurring"), recurring);
    return QJsonDocument(root);
}

bool ExchangeService::exportToFile(const QString &path, QString *error) const
{
    const QJsonDocument document = exportDocument();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = QStringLiteral("Impossible d'écrire le fichier : %1").arg(file.errorString());
        return false;
    }
    const QByteArray data = document.toJson(QJsonDocument::Indented);
    const qint64 written = file.write(data);
    file.close();
    if (written != data.size()) {
        if (error)
            *error = QStringLiteral("Écriture incomplète du fichier d'échange.");
        return false;
    }
    return true;
}

bool ExchangeService::importDocument(const QJsonDocument &document, MergeReport &report,
                                     QString *error)
{
    if (!document.isObject()) {
        if (error)
            *error = QStringLiteral("Fichier d'échange invalide (racine non-objet).");
        return false;
    }
    const QJsonObject root = document.object();
    if (!validateExchangeRoot(root, error))
        return false;

    if (!performPreImportBackup(m_database, m_backupDirectory, error))
        return false;

    QHash<QString, int> categoryByKey;
    {
        CategoryRepository categories(m_database);
        for (const Category &category : categories.all())
            categoryByKey.insert(category.key, category.id);
    }

    QSqlDatabase db = m_database.connection();
    if (!db.transaction()) {
        if (error)
            *error = QStringLiteral("Impossible de démarrer la transaction d'import.");
        return false;
    }

    // Les modèles d'abord : le rattachement d'une dépense se résout ensuite.
    if (!mergeRecurringArray(db, root.value(QStringLiteral("recurring")).toArray(), categoryByKey, report)) {
        db.rollback();
        if (error)
            *error = QStringLiteral("Échec de la fusion des paiements récurrents.");
        return false;
    }

    QHash<QString, int> recurringByUuid;
    {
        QSqlQuery query(db);
        if (query.exec(QStringLiteral("SELECT uuid, id FROM recurring_expenses"))) {
            while (query.next())
                recurringByUuid.insert(query.value(0).toString(), query.value(1).toInt());
        }
    }

    if (!mergeExpenseArray(db, root.value(QStringLiteral("expenses")).toArray(), categoryByKey,
                           recurringByUuid, report)) {
        db.rollback();
        if (error)
            *error = QStringLiteral("Échec de la fusion des dépenses.");
        return false;
    }

    if (!db.commit()) {
        if (error)
            *error = QStringLiteral("Impossible de valider la transaction d'import.");
        return false;
    }
    return true;
}

bool ExchangeService::importFromFile(const QString &path, MergeReport &report, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Impossible de lire le fichier : %1").arg(file.errorString());
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error)
            *error = QStringLiteral("JSON invalide : %1").arg(parseError.errorString());
        return false;
    }
    return importDocument(document, report, error);
}

} // namespace grossbuch
