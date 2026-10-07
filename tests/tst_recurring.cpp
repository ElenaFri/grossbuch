#include "core/CategoryRepository.h"
#include "core/Database.h"
#include "core/ExpenseRepository.h"
#include "core/RecurringExpense.h"
#include "core/RecurringRepository.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace grossbuch;

// Tests unitaires des modèles de paiements récurrents (hors matérialisation, qui
// est traitée séparément). Base SQLite en mémoire recréée à chaque test.
class RecurringTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void addStoresFieldsAndAssignsId();
    void addSetsRepereToMonthBeforeStart();
    void addInJanuarySetsRepereToPreviousDecember();
    void addStoresEmptyLabelAsNull();
    void updateChangesFutureFieldsOnly();
    void updateUnknownIdFails();
    void deactivateStopsGenerationKeepsHistory();
    void reactivateResumesAtCurrentMonth();
    void stateChangeOnUnknownIdFails();
    void allIsSortedByLabel();
    void reopeningKeepsSchemaAndData();

    void materializeCreatesOneOccurrencePerMonth();
    void materializeOccurrenceCarriesModelFields();
    void materializeIsIdempotent();
    void materializeGeneratesNoFutureMonth();
    void materializeIgnoresFutureStartModel();
    void materializeSkipsInactiveModel();
    void materializeNeverRegeneratesDeletedOrPastMonths();

    void addAssignsSyncIdentity();
    void materializedOccurrenceCarriesSyncIdentity();
    void updateRefreshesTimestamp();

private:
    int categoryId(const QString &name) const;
    RecurringExpense makeSample(const QString &label, int startYear, int startMonth) const;

    std::unique_ptr<Database> m_db;
    std::unique_ptr<CategoryRepository> m_categories;
    std::unique_ptr<ExpenseRepository> m_expenses;
    std::unique_ptr<RecurringRepository> m_recurring;
};

void RecurringTest::init()
{
    m_db = std::make_unique<Database>(QStringLiteral(":memory:"), QStringLiteral("rectest"));
    QVERIFY(m_db->open());
    m_categories = std::make_unique<CategoryRepository>(*m_db);
    m_expenses = std::make_unique<ExpenseRepository>(*m_db);
    m_recurring = std::make_unique<RecurringRepository>(*m_db);
}

void RecurringTest::cleanup()
{
    m_recurring.reset();
    m_expenses.reset();
    m_categories.reset();
    m_db.reset();
}

int RecurringTest::categoryId(const QString &name) const
{
    for (const Category &category : m_categories->all()) {
        if (category.name == name)
            return category.id;
    }
    return 0;
}

RecurringExpense RecurringTest::makeSample(const QString &label, int startYear, int startMonth) const
{
    RecurringExpense recurring;
    recurring.amountCents = 85000; // 850,00 €
    recurring.label = label;
    recurring.categoryId = categoryId(QStringLiteral("Résidence principale"));
    recurring.dayOfMonth = 5;
    recurring.startYear = startYear;
    recurring.startMonth = startMonth;
    return recurring;
}

void RecurringTest::addStoresFieldsAndAssignsId()
{
    const RecurringExpense sample = makeSample(QStringLiteral("Loyer"), 2024, 3);
    const std::optional<int> id = m_recurring->add(sample);
    QVERIFY(id.has_value());

    const std::optional<RecurringExpense> stored = m_recurring->byId(*id);
    QVERIFY(stored.has_value());
    QCOMPARE(stored->amountCents, qint64(85000));
    QCOMPARE(stored->label, QStringLiteral("Loyer"));
    QCOMPARE(stored->categoryId, sample.categoryId);
    QCOMPARE(stored->dayOfMonth, 5);
    QCOMPARE(stored->startYear, 2024);
    QCOMPARE(stored->startMonth, 3);
    QVERIFY(stored->active);

    // Un identifiant inexistant ne renvoie rien.
    QVERIFY(!m_recurring->byId(999999).has_value());
}

void RecurringTest::addSetsRepereToMonthBeforeStart()
{
    // Le repère doit pointer le mois précédant le début, pour que la future
    // matérialisation rattrape l'historique à partir du mois de début.
    const std::optional<int> id = m_recurring->add(makeSample(QStringLiteral("Loyer"), 2024, 3));
    QVERIFY(id.has_value());
    const std::optional<RecurringExpense> stored = m_recurring->byId(*id);
    QVERIFY(stored.has_value());
    QCOMPARE(stored->lastYear, 2024);
    QCOMPARE(stored->lastMonth, 2);
}

void RecurringTest::addInJanuarySetsRepereToPreviousDecember()
{
    // Cas limite : un début en janvier donne un repère en décembre de l'an passé.
    const std::optional<int> id = m_recurring->add(makeSample(QStringLiteral("Assurance"), 2024, 1));
    QVERIFY(id.has_value());
    const std::optional<RecurringExpense> stored = m_recurring->byId(*id);
    QVERIFY(stored.has_value());
    QCOMPARE(stored->lastYear, 2023);
    QCOMPARE(stored->lastMonth, 12);
}

void RecurringTest::addStoresEmptyLabelAsNull()
{
    RecurringExpense recurring = makeSample(QString(), 2025, 1);
    const std::optional<int> id = m_recurring->add(recurring);
    QVERIFY(id.has_value());
    const std::optional<RecurringExpense> stored = m_recurring->byId(*id);
    QVERIFY(stored.has_value());
    QVERIFY(stored->label.isEmpty());
}

void RecurringTest::updateChangesFutureFieldsOnly()
{
    const std::optional<int> id = m_recurring->add(makeSample(QStringLiteral("Loyer"), 2024, 3));
    QVERIFY(id.has_value());

    RecurringExpense edited = *m_recurring->byId(*id);
    edited.amountCents = 90000;
    edited.label = QStringLiteral("Loyer révisé");
    edited.categoryId = categoryId(QStringLiteral("Résidence secondaire"));
    edited.dayOfMonth = 10;
    // On tente aussi de modifier des champs qui ne doivent PAS bouger.
    edited.startYear = 1999;
    edited.startMonth = 12;
    edited.active = false;
    edited.lastYear = 1999;
    edited.lastMonth = 11;
    QVERIFY(m_recurring->update(edited));

    const std::optional<RecurringExpense> stored = m_recurring->byId(*id);
    QVERIFY(stored.has_value());
    // Champs modifiables (pour l'avenir).
    QCOMPARE(stored->amountCents, qint64(90000));
    QCOMPARE(stored->label, QStringLiteral("Loyer révisé"));
    QCOMPARE(stored->categoryId, edited.categoryId);
    QCOMPARE(stored->dayOfMonth, 10);
    // Champs protégés : le début, l'état actif et le repère sont inchangés.
    QCOMPARE(stored->startYear, 2024);
    QCOMPARE(stored->startMonth, 3);
    QVERIFY(stored->active);
    QCOMPARE(stored->lastYear, 2024);
    QCOMPARE(stored->lastMonth, 2);
}

void RecurringTest::updateUnknownIdFails()
{
    RecurringExpense ghost = makeSample(QStringLiteral("Fantôme"), 2024, 1);
    ghost.id = 999999;
    QVERIFY(!m_recurring->update(ghost));
}

void RecurringTest::deactivateStopsGenerationKeepsHistory()
{
    const std::optional<int> id = m_recurring->add(makeSample(QStringLiteral("Loyer"), 2024, 3));
    QVERIFY(id.has_value());

    QVERIFY(m_recurring->deactivate(*id));
    const std::optional<RecurringExpense> stored = m_recurring->byId(*id);
    QVERIFY(stored.has_value());
    QVERIFY(!stored->active);
    // L'historique (repère) est conservé tel quel.
    QCOMPARE(stored->lastYear, 2024);
    QCOMPARE(stored->lastMonth, 2);
}

void RecurringTest::reactivateResumesAtCurrentMonth()
{
    const std::optional<int> id = m_recurring->add(makeSample(QStringLiteral("Loyer"), 2024, 3));
    QVERIFY(id.has_value());
    QVERIFY(m_recurring->deactivate(*id));

    // Réactivation « au » juin 2025 : le repère doit passer à mai 2025, de sorte
    // que la matérialisation reprenne en juin sans rattraper la pause.
    QVERIFY(m_recurring->reactivate(*id, QDate(2025, 6, 20)));
    const std::optional<RecurringExpense> stored = m_recurring->byId(*id);
    QVERIFY(stored.has_value());
    QVERIFY(stored->active);
    QCOMPARE(stored->lastYear, 2025);
    QCOMPARE(stored->lastMonth, 5);
}

void RecurringTest::stateChangeOnUnknownIdFails()
{
    QVERIFY(!m_recurring->deactivate(999999));
    QVERIFY(!m_recurring->reactivate(999999, QDate(2025, 6, 20)));
}

void RecurringTest::allIsSortedByLabel()
{
    QVERIFY(m_recurring->all().isEmpty());

    m_recurring->add(makeSample(QStringLiteral("Loyer"), 2024, 1));
    m_recurring->add(makeSample(QStringLiteral("Assurance"), 2024, 1));
    m_recurring->add(makeSample(QStringLiteral("Netflix"), 2024, 1));

    const QVector<RecurringExpense> all = m_recurring->all();
    QCOMPARE(all.size(), 3);
    QCOMPARE(all.at(0).label, QStringLiteral("Assurance"));
    QCOMPARE(all.at(1).label, QStringLiteral("Loyer"));
    QCOMPARE(all.at(2).label, QStringLiteral("Netflix"));
}

void RecurringTest::reopeningKeepsSchemaAndData()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("grossbuch.db"));

    int savedId = 0;
    {
        Database db(path, QStringLiteral("recreopen"));
        QVERIFY(db.open());
        CategoryRepository categories(db);
        RecurringRepository recurring(db);
        RecurringExpense sample;
        sample.amountCents = 1200;
        sample.label = QStringLiteral("Abonnement");
        sample.dayOfMonth = 1;
        sample.startYear = 2025;
        sample.startMonth = 1;
        sample.categoryId = categories.all().first().id;
        const std::optional<int> id = recurring.add(sample);
        QVERIFY(id.has_value());
        savedId = *id;
    }

    // Réouverture du même fichier : le schéma est en place et la donnée persiste.
    {
        Database db(path, QStringLiteral("recreopen"));
        QVERIFY(db.open());
        RecurringRepository recurring(db);
        QCOMPARE(recurring.all().size(), 1);
        const std::optional<RecurringExpense> stored = recurring.byId(savedId);
        QVERIFY(stored.has_value());
        QCOMPARE(stored->amountCents, qint64(1200));
        QCOMPARE(stored->label, QStringLiteral("Abonnement"));
    }
}

void RecurringTest::materializeCreatesOneOccurrencePerMonth()
{
    const std::optional<int> id = m_recurring->add(makeSample(QStringLiteral("Loyer"), 2025, 1));
    QVERIFY(id.has_value());

    // De janvier à mars inclus : trois occurrences, une par mois.
    const int created = m_recurring->materializeDueOccurrences(QDate(2025, 3, 20));
    QCOMPARE(created, 3);
    QCOMPARE(m_expenses->forMonth(2025, 1).size(), 1);
    QCOMPARE(m_expenses->forMonth(2025, 2).size(), 1);
    QCOMPARE(m_expenses->forMonth(2025, 3).size(), 1);

    // Le repère a avancé jusqu'au mois de asOf.
    const std::optional<RecurringExpense> stored = m_recurring->byId(*id);
    QVERIFY(stored.has_value());
    QCOMPARE(stored->lastYear, 2025);
    QCOMPARE(stored->lastMonth, 3);
}

void RecurringTest::materializeOccurrenceCarriesModelFields()
{
    RecurringExpense model = makeSample(QStringLiteral("Loyer"), 2025, 2);
    model.amountCents = 73500;
    model.dayOfMonth = 5;
    const std::optional<int> id = m_recurring->add(model);
    QVERIFY(id.has_value());

    QCOMPARE(m_recurring->materializeDueOccurrences(QDate(2025, 2, 28)), 1);

    const QVector<Expense> february = m_expenses->forMonth(2025, 2);
    QCOMPARE(february.size(), 1);
    const Expense &occurrence = february.first();
    QCOMPARE(occurrence.amountCents, qint64(73500));
    QCOMPARE(occurrence.label, QStringLiteral("Loyer"));
    QCOMPARE(occurrence.categoryId, model.categoryId);
    // Le jour du mois du modèle est respecté.
    QCOMPARE(occurrence.date, QDate(2025, 2, 5));
}

void RecurringTest::materializeIsIdempotent()
{
    m_recurring->add(makeSample(QStringLiteral("Loyer"), 2025, 1));

    QCOMPARE(m_recurring->materializeDueOccurrences(QDate(2025, 3, 20)), 3);
    // Rejouée pour le même mois : rien de nouveau, et pas de doublon.
    QCOMPARE(m_recurring->materializeDueOccurrences(QDate(2025, 3, 20)), 0);
    QCOMPARE(m_expenses->forMonth(2025, 1).size(), 1);
    QCOMPARE(m_expenses->forMonth(2025, 2).size(), 1);
    QCOMPARE(m_expenses->forMonth(2025, 3).size(), 1);
}

void RecurringTest::materializeGeneratesNoFutureMonth()
{
    m_recurring->add(makeSample(QStringLiteral("Loyer"), 2025, 1));

    // asOf = février : seuls janvier et février sont générés, jamais mars.
    QCOMPARE(m_recurring->materializeDueOccurrences(QDate(2025, 2, 10)), 2);
    QVERIFY(m_expenses->forMonth(2025, 3).isEmpty());
}

void RecurringTest::materializeIgnoresFutureStartModel()
{
    const std::optional<int> id =
        m_recurring->add(makeSample(QStringLiteral("Futur"), 2026, 1));
    QVERIFY(id.has_value());

    // Le modèle commence après asOf : rien n'est généré et le repère ne bouge pas.
    QCOMPARE(m_recurring->materializeDueOccurrences(QDate(2025, 6, 15)), 0);
    const std::optional<RecurringExpense> stored = m_recurring->byId(*id);
    QVERIFY(stored.has_value());
    QCOMPARE(stored->lastYear, 2025);
    QCOMPARE(stored->lastMonth, 12);
}

void RecurringTest::materializeSkipsInactiveModel()
{
    const std::optional<int> id = m_recurring->add(makeSample(QStringLiteral("Loyer"), 2025, 1));
    QVERIFY(id.has_value());
    QVERIFY(m_recurring->deactivate(*id));

    QCOMPARE(m_recurring->materializeDueOccurrences(QDate(2025, 3, 20)), 0);
    QVERIFY(m_expenses->forMonth(2025, 1).isEmpty());
}

void RecurringTest::materializeNeverRegeneratesDeletedOrPastMonths()
{
    m_recurring->add(makeSample(QStringLiteral("Loyer"), 2025, 1));
    QCOMPARE(m_recurring->materializeDueOccurrences(QDate(2025, 3, 20)), 3);

    // L'utilisateur supprime à la main l'occurrence de février.
    const QVector<Expense> february = m_expenses->forMonth(2025, 2);
    QCOMPARE(february.size(), 1);
    QVERIFY(m_expenses->remove(february.first().id));

    // Rejouer pour le même mois ne recrée pas février (le repère ne recule pas).
    QCOMPARE(m_recurring->materializeDueOccurrences(QDate(2025, 3, 20)), 0);
    QVERIFY(m_expenses->forMonth(2025, 2).isEmpty());

    // Avancer d'un mois ne génère que le nouveau mois, jamais le passé effacé.
    QCOMPARE(m_recurring->materializeDueOccurrences(QDate(2025, 4, 10)), 1);
    QCOMPARE(m_expenses->forMonth(2025, 4).size(), 1);
    QVERIFY(m_expenses->forMonth(2025, 2).isEmpty());
}

void RecurringTest::addAssignsSyncIdentity()
{
    const std::optional<int> id = m_recurring->add(makeSample(QStringLiteral("Loyer"), 2025, 1));
    QVERIFY(id.has_value());
    const std::optional<RecurringExpense> stored = m_recurring->byId(*id);
    QVERIFY(stored.has_value());
    QVERIFY(!stored->uuid.isEmpty());
    QVERIFY(!stored->createdAt.isEmpty());
    QCOMPARE(stored->createdAt, stored->updatedAt); // égaux à la création
    QVERIFY(!stored->deleted);
}

void RecurringTest::materializedOccurrenceCarriesSyncIdentity()
{
    m_recurring->add(makeSample(QStringLiteral("Loyer"), 2025, 1));
    QCOMPARE(m_recurring->materializeDueOccurrences(QDate(2025, 1, 20)), 1);

    // Une occurrence matérialisée est une dépense synchronisable à part entière.
    const QVector<Expense> january = m_expenses->forMonth(2025, 1);
    QCOMPARE(january.size(), 1);
    QVERIFY(!january.first().uuid.isEmpty());
    QVERIFY(!january.first().createdAt.isEmpty());
    QVERIFY(!january.first().updatedAt.isEmpty());
}

void RecurringTest::updateRefreshesTimestamp()
{
    const std::optional<int> id = m_recurring->add(makeSample(QStringLiteral("Loyer"), 2025, 1));
    QVERIFY(id.has_value());

    // Horodatage ancien planté : modifier le modèle doit le rafraîchir pour que le
    // changement soit synchronisable (« la plus récente l'emporte »).
    {
        QSqlQuery seed(m_db->connection());
        seed.prepare(QStringLiteral(
            "UPDATE recurring_expenses SET updated_at = ? WHERE id = ?"));
        seed.addBindValue(QStringLiteral("2000-01-01T00:00:00"));
        seed.addBindValue(*id);
        QVERIFY(seed.exec());
    }

    RecurringExpense edited = *m_recurring->byId(*id);
    QCOMPARE(edited.updatedAt, QStringLiteral("2000-01-01T00:00:00"));
    edited.amountCents = 90000;
    QVERIFY(m_recurring->update(edited));

    const std::optional<RecurringExpense> stored = m_recurring->byId(*id);
    QVERIFY(stored.has_value());
    QVERIFY(stored->updatedAt > QStringLiteral("2000-01-01T00:00:00"));
    QCOMPARE(stored->createdAt, edited.createdAt); // la création ne bouge pas
    QCOMPARE(stored->uuid, edited.uuid);           // l'identité reste stable
}

QTEST_GUILESS_MAIN(RecurringTest)

#include "tst_recurring.moc"
