#include "ImportGamesWidget.h"

#include <QFileDialog>
#include <QProgressDialog>

#include "ui_ImportGamesWidget.h"
#include "../config/ConfigManager.h"
#include "../api/BotCApiClient.h"

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
                this, [this](const int row, int, int, int) { onGameSelected(row); });

        setImportEnabled(false);

        initApiClient();
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
        setImportEnabled(false);

        m_importRolesProgressDial = new QProgressDialog("Импортирование ролей",
                                                        "Отмена",
                                                        0,
                                                        static_cast<int>(m_lastResult.records.size()) + 1, this);
        m_importRolesProgressDial->setWindowModality(Qt::WindowModal);
        m_importRolesProgressDial->show();

        QVector<api::models::games::GameImportRequest> requests;
        for (const utils::games::GameRecord& game : m_lastResult.records)
        {
            QVector<api::models::games::PlayerImportRequest> players;
            for (const utils::games::PlayerRecord& player : game.players)
            {
                players.push_back(api::models::games::PlayerImportRequest{
                    .name         = player.playerName,
                    .seatNumber   = player.seatNumber,
                    .roleStart    = player.roleStartName,
                    .roleEnd      = player.roleEndName,
                    .alignmentEnd = player.alignmentEnd,
                    .bIsAlive     = player.isAlive
                });
            }

            requests.push_back(api::models::games::GameImportRequest{
                    .gameDate = game.gameDate,
                    .scenarioName = game.scenarioName,
                    .storytellerName = game.storytellerName,
                    .alignmentWin = game.alignmentWin,
                    .location = game.location,
                    .gameNumber = static_cast<uint8_t>(game.gameNumber),
                    .duration = game.duration.isValid() ? game.duration.toString("hh:mm:ss") : std::optional<QString>{},
                    .notes = game.notes.isEmpty() ? std::optional<QString>{} : game.notes,
                    .players = players
                }
            );
        }

        connect(m_apiClient, &api::BotCApiClient::gameImportFinished, this, &ImportGamesWidget::onGamesImportFinished);
        m_importCount = requests.size();
        std::ranges::for_each(requests, [this](const api::models::games::GameImportRequest& in_request)
        {
            m_apiClient->importGame(in_request);
        });
    }

    void ImportGamesWidget::onClearClicked()
    {
        ui->filePathEdit->clear();
        clearAll();
    }

    void ImportGamesWidget::onGameSelected(const int row)
    {
        if (row < 0 || row >= m_lastResult.records.size()) return;
        populatePlayersTable(row);
    }

    void ImportGamesWidget::initApiClient()
    {
        const auto ConfigManager = config::ConfigManager::instance();
        m_apiClient              = new api::BotCApiClient(ConfigManager->getApiUrl(), ConfigManager->getApiKey(),
                                             ConfigManager->getSslVerify(), this);
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

        populateGamesTable();

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

    void ImportGamesWidget::populateGamesTable()
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
                p.duration.isValid() ? p.duration.toString("hh:mm:ss") : "—",
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

    void ImportGamesWidget::populatePlayersTable(const int partyIndex)
    {
        const QStringList headers = {
            "Место", "Игрок", "Роль (старт)", "Роль (финал)", "Фракция", "Жив"
        };

        const utils::games::GameRecord& game_record = m_lastResult.records[partyIndex];

        ui->playersTable->clear();
        ui->playersTable->setColumnCount(headers.size());
        ui->playersTable->setRowCount(game_record.players.size());
        ui->playersTable->setHorizontalHeaderLabels(headers);

        ui->playersGroupBox->setTitle(
            QString("Игроки — %1, %2 (%3)")
            .arg(game_record.scenarioName,
                 game_record.gameDate.toString("dd.MM.yyyy"),
                 game_record.storytellerName)
        );

        for (int row = 0; row < game_record.players.size(); ++row)
        {
            const utils::games::PlayerRecord& pl = game_record.players[row];

            QStringList cells = {
                pl.seatNumber.has_value() ? QString::number(pl.seatNumber.value()) : "-",
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

    void ImportGamesWidget::showErrors(const QStringList& errors) const
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

    void ImportGamesWidget::setImportEnabled(const bool enabled) const
    {
        ui->importButton->setEnabled(enabled);
    }

    QString ImportGamesWidget::statusStyle(const bool ok)
    {
        return ok
                   ? "color: green; font-weight: bold;"
                   : "color: orange; font-weight: bold;";
    }

    void ImportGamesWidget::onGamesImportFinished(bool in_bSuccess,
                                                  const api::models::games::GameImportResponse& in_response)
    {
        using namespace api::models::games;
        assert(m_importCount > 0);
        --m_importCount;
        responses.push_back(in_response);
        if (m_importCount == 0)
        {
            disconnect(m_apiClient, &api::BotCApiClient::gameImportFinished, this,
                       &ImportGamesWidget::onGamesImportFinished);
        }

        /*if (std::ranges::all_of(responses, std::mem_fn(&GameImportResponse::isSuccess)))
        {
        }*/

        uint successCount = 0;
        uint failedCount  = 0;

        QString message;

        std::ranges::stable_sort(responses, [](const GameImportResponse& lhs, const GameImportResponse& rhs)
        {
            return lhs.isSuccess() > rhs.isSuccess();
        });
        for (const GameImportResponse& response : responses)
        {
            if (response.isSuccess())
            {
                successCount++;
                message += QString("Игра %1 успешно иимпортирована. Создано игроков: %2\n")
                           .arg(response.gameId)
                           .arg(response.playersCreated);
            }
            else
            {
                failedCount++;

                QString errors;
                for (const QString& error : response.errors)
                {
                    errors += error + "\n";
                }

                message += QString("Импорт произошёл с ошибками.\nОшибки:\n%1")
                    .arg(errors);
            }
        }

        if (failedCount == 0)
        {
            ui->statusLabel->setText(
                QString("✔ Импортировано партий: %1").arg(m_lastResult.records.size())
            );
        }
        else
        {
            ui->statusLabel->setText(
                QString("Импорт партий прозошёл с ошибками"));
            ui->statusLabel->setStyleSheet("color: red; font-weight: bold;");
        }
    }
} // botc::ui
