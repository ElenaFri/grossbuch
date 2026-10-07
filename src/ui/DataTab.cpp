#include "ui/DataTab.h"

#include "core/BackupService.h"
#include "core/DataController.h"

#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace grossbuch {

DataTab::DataTab(DataController &controller, QWidget *parent)
    : QWidget(parent), m_controller(controller)
{
    auto *layout = new QVBoxLayout(this);

    // --- Échange de données -------------------------------------------------
    auto *exchangeBox = new QGroupBox(tr("Échange de données"), this);
    auto *exchangeLayout = new QVBoxLayout(exchangeBox);
    auto *exchangeHelp = new QLabel(
        tr("Exportez vos données vers un fichier à partager, ou importez un fichier reçu. "
           "L'import fusionne les données sans jamais créer de doublon ; une sauvegarde est "
           "créée automatiquement au préalable."),
        exchangeBox);
    exchangeHelp->setWordWrap(true);
    exchangeLayout->addWidget(exchangeHelp);

    auto *exchangeButtons = new QHBoxLayout;
    m_export = new QPushButton(tr("Exporter vers un fichier…"), exchangeBox);
    m_import = new QPushButton(tr("Importer depuis un fichier…"), exchangeBox);
    exchangeButtons->addWidget(m_export);
    exchangeButtons->addWidget(m_import);
    exchangeButtons->addStretch();
    exchangeLayout->addLayout(exchangeButtons);
    layout->addWidget(exchangeBox);

    // --- Sauvegardes --------------------------------------------------------
    auto *backupBox = new QGroupBox(tr("Sauvegardes"), this);
    auto *backupLayout = new QVBoxLayout(backupBox);
    auto *backupHelp = new QLabel(
        tr("Une sauvegarde est créée chaque jour et avant chaque import. Pour revenir à un "
           "état antérieur, choisissez une sauvegarde puis restaurez-la."),
        backupBox);
    backupHelp->setWordWrap(true);
    backupLayout->addWidget(backupHelp);

    m_backups = new QListWidget(backupBox);
    backupLayout->addWidget(m_backups);

    auto *restoreButtons = new QHBoxLayout;
    m_restore = new QPushButton(tr("Restaurer la sauvegarde sélectionnée…"), backupBox);
    restoreButtons->addWidget(m_restore);
    restoreButtons->addStretch();
    backupLayout->addLayout(restoreButtons);
    layout->addWidget(backupBox);

    // --- Résultat de la dernière action ------------------------------------
    m_result = new QLabel(this);
    m_result->setWordWrap(true);
    m_result->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_result);

    layout->addStretch();

    connect(m_export, &QPushButton::clicked, this, &DataTab::onExport);
    connect(m_import, &QPushButton::clicked, this, &DataTab::onImport);
    connect(m_restore, &QPushButton::clicked, this, &DataTab::onRestore);
    connect(m_backups, &QListWidget::itemSelectionChanged, this, &DataTab::updateRestoreButton);

    reloadBackups();
}

void DataTab::refresh()
{
    reloadBackups();
}

void DataTab::reloadBackups()
{
    m_backups->clear();
    const QVector<BackupService::BackupInfo> backups = m_controller.backups();
    for (const BackupService::BackupInfo &backup : backups) {
        const QString when = backup.created.toString(QStringLiteral("dd/MM/yyyy HH:mm"));
        const QString size = QLocale().formattedDataSize(backup.sizeBytes);
        auto *item = new QListWidgetItem(tr("%1  (%2)").arg(when, size), m_backups);
        item->setData(Qt::UserRole, backup.path);
    }
    updateRestoreButton();
}

void DataTab::updateRestoreButton()
{
    m_restore->setEnabled(m_backups->currentItem() != nullptr);
}

void DataTab::showResult(const QString &message, bool error)
{
    m_result->setStyleSheet(error ? QStringLiteral("color: #b00020;")
                                  : QStringLiteral("color: #1b5e20;"));
    m_result->setText(message);
}

void DataTab::onExport()
{
    const QString documents =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString defaultName =
        QStringLiteral("grossbuch_backup_%1.json")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString suggested = QDir(documents).filePath(defaultName);

    const QString path = QFileDialog::getSaveFileName(
        this, tr("Exporter les données"), suggested, tr("Fichier d'échange (*.json)"));
    if (path.isEmpty())
        return;

    const DataController::Result result = m_controller.exportToFile(path);
    showResult(result.message, !result.ok);
}

void DataTab::onImport()
{
    const QMessageBox::StandardButton confirm = QMessageBox::question(
        this, tr("Importer des données"),
        tr("L'import fusionne le fichier choisi avec vos données actuelles.\n"
           "Une sauvegarde complète est créée automatiquement au préalable.\n\n"
           "Continuer ?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (confirm != QMessageBox::Yes)
        return;

    const QString documents =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Importer des données"), documents, tr("Fichier d'échange (*.json)"));
    if (path.isEmpty())
        return;

    const DataController::Result result = m_controller.importFromFile(path);
    showResult(result.message, !result.ok);
    reloadBackups(); // un import réussi a créé une sauvegarde préalable
}

void DataTab::onRestore()
{
    QListWidgetItem *item = m_backups->currentItem();
    if (item == nullptr)
        return;

    const QMessageBox::StandardButton confirm = QMessageBox::warning(
        this, tr("Restaurer une sauvegarde"),
        tr("Cette opération remplace toutes vos données actuelles par la sauvegarde "
           "sélectionnée, puis redémarre l'application.\n\nConfirmer la restauration ?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (confirm != QMessageBox::Yes)
        return;

    const QString backupPath = item->data(Qt::UserRole).toString();
    const DataController::Result result = m_controller.restoreFromBackup(backupPath);
    if (!result.ok) {
        showResult(result.message, true);
        reloadBackups();
        return;
    }

    QMessageBox::information(
        this, tr("Restauration"),
        tr("%1\nL'application va redémarrer.").arg(result.message));
    emit restoreCompleted();
}

} // namespace grossbuch
