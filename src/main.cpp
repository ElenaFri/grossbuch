#include "core/BackupService.h"
#include "core/CategoryRepository.h"
#include "core/Database.h"
#include "core/ExpenseRepository.h"
#include "core/RecurringRepository.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QDate>
#include <QLocale>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("grossbuch"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setApplicationDisplayName(QStringLiteral("grossbuch"));

    // Application francophone : dates et montants toujours au format français,
    // indépendamment de la locale du système. Voir Phase 7.
    QLocale::setDefault(QLocale(QLocale::French, QLocale::France));

    grossbuch::Database database(grossbuch::Database::defaultDatabasePath());
    if (!database.open()) {
        QMessageBox::critical(nullptr, QStringLiteral("grossbuch"),
                              QObject::tr("Impossible d'ouvrir la base de données."));
        return 1;
    }

    // Filet de sécurité : une sauvegarde cohérente par jour, avec rotation des
    // plus anciennes. Voir docs/adr/0013.
    {
        grossbuch::BackupService backups(database, grossbuch::BackupService::defaultBackupDirectory());
        backups.dailyBackup();
        backups.rotate();
    }

    grossbuch::CategoryRepository categories(database);
    grossbuch::ExpenseRepository expenses(database);

    // Reporte les paiements récurrents dus jusqu'au mois en cours avant d'afficher
    // l'interface, de sorte que leurs occurrences soient visibles partout.
    grossbuch::RecurringRepository recurrings(database);
    recurrings.materializeDueOccurrences(QDate::currentDate());

    grossbuch::MainWindow window(categories, expenses, recurrings);
    window.show();

    return QApplication::exec();
}
