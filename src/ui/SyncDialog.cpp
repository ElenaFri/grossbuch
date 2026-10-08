#include "ui/SyncDialog.h"

#include "ui/SyncController.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace grossbuch {

SyncDialog::SyncDialog(SyncController &controller, QWidget *parent)
    : QDialog(parent), m_controller(controller)
{
    setWindowTitle(tr("Synchronisation"));
    resize(520, 200);

    auto *help = new QLabel(
        tr("Choisissez un dossier synchronisé par un outil tiers (Syncthing, "
           "Nextcloud, Dropbox…). L'application y dépose son propre instantané et "
           "fusionne ceux des autres appareils. La base locale n'est jamais "
           "synchronisée directement."),
        this);
    help->setWordWrap(true);

    m_folderEdit = new QLineEdit(this);
    m_folderEdit->setObjectName(QStringLiteral("folderEdit"));
    m_folderEdit->setText(m_controller.sharedFolder());

    auto *browseButton = new QPushButton(tr("Parcourir…"), this);
    browseButton->setObjectName(QStringLiteral("browseButton"));

    auto *folderRow = new QHBoxLayout;
    folderRow->addWidget(m_folderEdit);
    folderRow->addWidget(browseButton);

    m_autoSyncCheck = new QCheckBox(tr("Synchroniser automatiquement à l'ouverture et à la "
                                       "fermeture"),
                                    this);
    m_autoSyncCheck->setObjectName(QStringLiteral("autoSyncCheck"));
    m_autoSyncCheck->setChecked(m_controller.autoSync());

    auto *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    connect(browseButton, &QPushButton::clicked, this, &SyncDialog::onBrowse);
    connect(buttons, &QDialogButtonBox::accepted, this, &SyncDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(help);
    layout->addWidget(new QLabel(tr("Dossier partagé :"), this));
    layout->addLayout(folderRow);
    layout->addWidget(m_autoSyncCheck);
    layout->addStretch();
    layout->addWidget(buttons);
}

void SyncDialog::onAccept()
{
    m_controller.setSharedFolder(m_folderEdit->text().trimmed());
    m_controller.setAutoSync(m_autoSyncCheck->isChecked());
    accept();
}

// Slot modal : QFileDialog bloquant. Non couvrable sans automatisation fragile de
// fenêtres modales : exclu de la mesure de couverture. Voir docs/adr/0019.
// LCOV_EXCL_START
void SyncDialog::onBrowse()
{
    const QString folder = QFileDialog::getExistingDirectory(
        this, tr("Choisir le dossier partagé"), m_folderEdit->text());
    if (!folder.isEmpty())
        m_folderEdit->setText(folder);
}
// LCOV_EXCL_STOP

} // namespace grossbuch
