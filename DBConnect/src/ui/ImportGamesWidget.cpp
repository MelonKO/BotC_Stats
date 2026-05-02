#include "ImportGamesWidget.h"

#include <QFileDialog>

#include "ui_ImportGamesWidget.h"

namespace botc::ui
{
    ImportGamesWidget::ImportGamesWidget(QWidget* parent) :
        QWidget(parent), ui(new Ui::ImportGamesWidget)
    {
        ui->setupUi(this);

        connect(ui->browseButton, &QPushButton::clicked,
                this, &ImportGamesWidget::onBrowseClicked);
        connect(ui->importButton, &QPushButton::clicked,
                this, &ImportGamesWidget::onImportClicked);
        connect(ui->clearButton, &QPushButton::clicked,
                this, &ImportGamesWidget::onClearClicked);

        // При клике на партию — показываем её игроков
        connect(ui->partiesTable, &QTableWidget::currentCellChanged,
                this, [this](int row, int, int, int) { onGameSelected(row); });

        setImportEnabled(false);
    }

    ImportGamesWidget::~ImportGamesWidget()
    {
        delete ui;
    }

    void ImportGamesWidget::onBrowseClicked()
    {
        const QString path = QFileDialog::getOpenFileName(
            this, "Выберите CSV файл",
            QDir::homePath(),
            "CSV файлы (*.csv);;Все файлы (*)"
        );
        if (path.isEmpty()) return;

        ui->filePathEdit->setText(path);
        clearAll();

        utils::games::GamesCsvParser parser;
        m_lastResult = parser.parse(path);
        showPreview(m_lastResult);
    }

    void ImportGamesWidget::onImportClicked()
    {
        if (m_lastResult.records.isEmpty()) return;

        emit partiesImported(m_lastResult.records);

        ui->statusLabel->setText(
            QString("✔ Импортировано партий: %1").arg(m_lastResult.records.size())
        );
        ui->statusLabel->setStyleSheet("color: green; font-weight: bold;");
        setImportEnabled(false);
    }

    void ImportGamesWidget::onClearClicked()
    {
        ui->filePathEdit->clear();
        clearAll();
    }

    void ImportGamesWidget::onGameSelected(int row)
    {
        if (row < 0 || row >= m_lastResult.records.size()) return;
        populatePlayersTable(row);
    }

    void ImportGamesWidget::showPreview(const utils::games::GamesParseResult& result)
    {
        if (!result.errors.isEmpty())
            showErrors(result.errors);

        if (result.records.isEmpty())
        {
            ui->statusLabel->setText("✘ Записи не загружены.");
            ui->statusLabel->setStyleSheet("color: red; font-weight: bold;");
            setImportEnabled(false);
            return;
        }

        populatePartiesTable();

        // Выбираем первую партию автоматически
        ui->partiesTable->selectRow(0);
        populatePlayersTable(0);

        const bool hasErrors = !result.errors.isEmpty();
        ui->statusLabel->setText(
            hasErrors
                ? QString("⚠ Загружено партий: %1, обнаружены ошибки валидации")
                .arg(result.records.size())
                : QString("✔ Загружено партий: %1, ошибок нет")
                .arg(result.records.size())
        );
        ui->statusLabel->setStyleSheet(statusStyle(!hasErrors));
        setImportEnabled(true);
    }

    void ImportGamesWidget::populatePartiesTable()
    {
        const QStringList headers = {
            "Дата", "Сценарий", "Место", "№", "Ведущий", "Победа", "Длит.(мин)", "Игроков"
        };

        auto& records = m_lastResult.records;

        ui->partiesTable->clear();
        ui->partiesTable->setColumnCount(headers.size());
        ui->partiesTable->setRowCount(records.size());
        ui->partiesTable->setHorizontalHeaderLabels(headers);

        for (int row = 0; row < records.size(); ++row)
        {
            const utils::games::GameRecord& p = records[row];

            QStringList cells = {
                p.gameDate.toString("dd.MM.yyyy"),
                p.scenarioName,
                p.location,
                QString::number(p.gameNumber),
                p.storytellerName,
                p.alignmentWin,
                p.duration > 0 ? QString::number(p.duration) : "—",
                QString::number(p.players.size())
            };

            for (int col = 0; col < cells.size(); ++col)
            {
                auto* item = new QTableWidgetItem(cells[col]);
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                ui->partiesTable->setItem(row, col, item);
            }
        }

        ui->partiesTable->horizontalHeader()
          ->setSectionResizeMode(1, QHeaderView::Stretch);
        ui->partiesTable->resizeColumnsToContents();
    }

    void ImportGamesWidget::populatePlayersTable(int partyIndex)
    {
        const QStringList headers = {
            "Место", "Игрок", "Роль (старт)", "Роль (финал)", "Фракция", "Жив"
        };

        const utils::games::GameRecord& party = m_lastResult.records[partyIndex];

        ui->playersTable->clear();
        ui->playersTable->setColumnCount(headers.size());
        ui->playersTable->setRowCount(party.players.size());
        ui->playersTable->setHorizontalHeaderLabels(headers);

        ui->playersGroupBox->setTitle(
            QString("Игроки — %1, %2 (%3)")
            .arg(party.scenarioName,
                 party.gameDate.toString("dd.MM.yyyy"),
                 party.storytellerName)
        );

        for (int row = 0; row < party.players.size(); ++row)
        {
            const utils::games::PlayerRecord& pl = party.players[row];

            QStringList cells = {
                QString::number(pl.seatNumber),
                pl.playerName,
                pl.roleStartName,
                pl.roleEndName,
                pl.alignmentEnd,
                pl.isAlive ? "✔" : "✘"
            };

            for (int col = 0; col < cells.size(); ++col)
            {
                auto* item = new QTableWidgetItem(cells[col]);
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);

                // Подсветка живых/мёртвых
                if (col == 5)
                    item->setForeground(pl.isAlive ? QColor(0, 150, 0) : QColor(200, 0, 0));

                ui->playersTable->setItem(row, col, item);
            }
        }

        ui->playersTable->horizontalHeader()
          ->setSectionResizeMode(1, QHeaderView::Stretch);
        ui->playersTable->resizeColumnsToContents();
    }

    void ImportGamesWidget::showErrors(const QStringList& errors)
    {
        ui->errorsWidget->clear();
        for (const QString& err : errors)
            ui->errorsWidget->addItem(err);
        ui->errorsGroup->setVisible(true);
    }

    void ImportGamesWidget::clearAll()
    {
        m_lastResult = {};
        ui->partiesTable->clear();
        ui->partiesTable->setRowCount(0);
        ui->partiesTable->setColumnCount(0);
        ui->playersTable->clear();
        ui->playersTable->setRowCount(0);
        ui->playersTable->setColumnCount(0);
        ui->errorsWidget->clear();
        ui->errorsGroup->setVisible(false);
        ui->statusLabel->clear();
        setImportEnabled(false);
    }

    void ImportGamesWidget::setImportEnabled(bool enabled)
    {
        ui->importButton->setEnabled(enabled);
    }

    QString ImportGamesWidget::statusStyle(bool ok)
    {
        return ok
                   ? "color: green; font-weight: bold;"
                   : "color: orange; font-weight: bold;";
    }
} // botc::ui
