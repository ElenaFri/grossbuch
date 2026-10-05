#include "core/CategoryRepository.h"
#include "core/Database.h"
#include "core/ExpenseRepository.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("grossbuch"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setApplicationDisplayName(QStringLiteral("grossbuch"));

    grossbuch::Database database(grossbuch::Database::defaultDatabasePath());
    if (!database.open()) {
        QMessageBox::critical(nullptr, QStringLiteral("grossbuch"),
                              QObject::tr("Impossible d'ouvrir la base de données."));
        return 1;
    }

    grossbuch::CategoryRepository categories(database);
    grossbuch::ExpenseRepository expenses(database);

    grossbuch::MainWindow window(categories, expenses);
    window.show();

    return QApplication::exec();
}
