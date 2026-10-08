#include "ui/RestoreDialog.h"

#include "core/BackupService.h"
#include "core/DataController.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace grossbuch {

RestoreDialog::RestoreDialog(DataController &controller, QWidget *parent)
    : QDialog(parent), m_controller(controller)
{
    setWindowTitle(tr("Restaurer une sauvegarde"));
    resize(480, 360);

    auto *help = new QLabel(
        tr("Une sauvegarde est créée chaque jour et avant chaque import. Choisissez "
           "une sauvegarde puis restaurez-la pour revenir à cet état ; l'application "
           "redémarrera ensuite."),
        this);
    help->setWordWrap(true);

    m_backups = new QListWidget(this);
    m_backups->setObjectName(QStringLiteral("backupList"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_restore = buttons->addButton(tr("Restaurer"), QDialogButtonBox::AcceptRole);
    m_restore->setObjectName(QStringLiteral("restoreButton"));
    m_restore->setEnabled(false);

    connect(m_backups, &QListWidget::itemSelectionChanged, this,
            &RestoreDialog::updateRestoreButton);
    connect(m_restore, &QPushButton::clicked, this, &RestoreDialog::onRestore);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(help);
    layout->addWidget(m_backups);
    layout->addWidget(buttons);

    reloadBackups();
}

void RestoreDialog::reloadBackups()
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

void RestoreDialog::updateRestoreButton()
{
    m_restore->setEnabled(m_backups->currentItem() != nullptr);
}

void RestoreDialog::onRestore()
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
        QMessageBox::warning(this, tr("Restauration impossible"), result.message);
        reloadBackups();
        return;
    }

    QMessageBox::information(this, tr("Restauration"),
                            tr("%1\nL'application va redémarrer.").arg(result.message));
    accept();
}

} // namespace grossbuch
