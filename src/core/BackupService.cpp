#include "core/BackupService.h"

#include "core/Database.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

namespace grossbuch {

namespace {

constexpr char kPrefix[] = "grossbuch-";

// Échappe un chemin pour l'insérer dans une instruction SQL (VACUUM INTO
// n'accepte pas de paramètre lié) : les apostrophes sont doublées.
QString sqlQuote(const QString &path)
{
    QString escaped = path;
    escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QStringLiteral("'%1'").arg(escaped);
}

} // namespace

QString BackupService::defaultBackupDirectory()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dir).filePath(QStringLiteral("backups"));
}

BackupService::BackupService(Database &database, QString backupDirectory)
    : m_database(database), m_directory(std::move(backupDirectory))
{
}

std::optional<QString> BackupService::createBackup(QString *error)
{
    if (!QDir().mkpath(m_directory)) {
        if (error)
            *error = QStringLiteral("Impossible de créer le répertoire de sauvegarde : %1")
                         .arg(m_directory);
        return std::nullopt;
    }

    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    const QString name = QString::fromLatin1(kPrefix) + stamp + QStringLiteral(".db");
    const QString path = QDir(m_directory).filePath(name);

    QSqlQuery query(m_database.connection());
    if (!query.exec(QStringLiteral("VACUUM INTO %1").arg(sqlQuote(path)))) {
        if (error)
            *error = QStringLiteral("Échec de la sauvegarde : %1")
                         .arg(query.lastError().text());
        return std::nullopt;
    }
    return path;
}

std::optional<QString> BackupService::dailyBackup(QString *error)
{
    if (hasBackupForDate(QDate::currentDate()))
        return std::nullopt;
    return createBackup(error);
}

QVector<BackupService::BackupInfo> BackupService::listBackups() const
{
    QVector<BackupInfo> backups;
    QDir dir(m_directory);
    if (!dir.exists())
        return backups;

    const QStringList filters{QString::fromLatin1(kPrefix) + QStringLiteral("*.db")};
    // QDir::Time trie du plus récent au plus ancien.
    const QFileInfoList entries = dir.entryInfoList(filters, QDir::Files, QDir::Time);
    backups.reserve(entries.size());
    for (const QFileInfo &info : entries) {
        backups.append(BackupInfo{info.absoluteFilePath(), info.fileName(),
                                  info.lastModified(), info.size()});
    }
    return backups;
}

bool BackupService::hasBackupForDate(const QDate &date) const
{
    const QString expected =
        QString::fromLatin1(kPrefix) + date.toString(QStringLiteral("yyyyMMdd"));
    for (const BackupInfo &backup : listBackups()) {
        if (backup.name.startsWith(expected))
            return true;
    }
    return false;
}

int BackupService::rotate(int keep)
{
    if (keep < 0)
        keep = 0;
    const QVector<BackupInfo> backups = listBackups();
    int removed = 0;
    for (int i = keep; i < backups.size(); ++i) {
        if (QFile::remove(backups.at(i).path))
            ++removed;
    }
    return removed;
}

bool BackupService::restore(const QString &livePath, const QString &backupPath, QString *error)
{
    if (!QFile::exists(backupPath)) {
        if (error)
            *error = QStringLiteral("Sauvegarde introuvable : %1").arg(backupPath);
        return false;
    }

    // Vérifie l'intégrité de la sauvegarde sur une connexion temporaire isolée.
    const QString connectionName =
        QStringLiteral("backup-check-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    bool integrityOk = false;
    {
        QSqlDatabase check = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        check.setDatabaseName(backupPath);
        if (check.open()) {
            QSqlQuery query(check);
            if (query.exec(QStringLiteral("PRAGMA integrity_check")) && query.next())
                integrityOk = query.value(0).toString() == QStringLiteral("ok");
            check.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);

    if (!integrityOk) {
        if (error)
            *error = QStringLiteral("La sauvegarde est corrompue ou illisible : %1").arg(backupPath);
        return false;
    }

    // Supprime la base vivante et ses fichiers annexes de journal, puis copie la
    // sauvegarde à sa place.
    QFile::remove(livePath);
    QFile::remove(livePath + QStringLiteral("-wal"));
    QFile::remove(livePath + QStringLiteral("-shm"));
    if (!QFile::copy(backupPath, livePath)) {
        if (error)
            *error = QStringLiteral("Impossible de restaurer la sauvegarde vers %1").arg(livePath);
        return false;
    }
    return true;
}

} // namespace grossbuch
