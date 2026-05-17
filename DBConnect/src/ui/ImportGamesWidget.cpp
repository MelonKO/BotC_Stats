#include "ImportGamesWidget.h"

#include <algorithm>
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>

#include "OAIImportGame_200_response.h"
#include "OAIImportGame_request.h"
#include "OAIImportGame_request_players_inner.h"
#include "OAIListRoles_200_response.h"
#include "ui_ImportGamesWidget.h"
#include "../api/BotCApiClient.h"
#include "../config/ConfigManager.h"

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

        auto* apiClient = api::BotCApiClient::instance();
        // TODO:: synchronize with CSV parsing and role updating
        connect(apiClient, &api::BotCApiClient::rolesListFinished,
                this, &ImportGamesWidget::onRoleListFinished);
        apiClient->listRoles("ru");
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

        m_lastResult = utils::games::GamesCsvParser::parse(path);

        for (const utils::games::GameRecord& record : m_lastResult.records)
        {
            for (const auto& player : record.players)
            {
                if (!availableRoles.contains(player.roleStartName))
                {
                    m_lastResult.errors << QString("Role \"%1\" didn't contains in data base. You need to add it.")
                        .arg(player.roleStartName);
                }
                if (!availableRoles.contains(player.roleEndName))
                {
                    m_lastResult.errors << QString("Role \"%1\" didn't contains in data base. You need to add it.")
                        .arg(player.roleEndName);
                }
            }
        }

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

        QVector<OpenAPI::OAIImportGame_request> requests;
        for (const utils::games::GameRecord& game : m_lastResult.records)
        {
            m_importRolesProgressDial->setValue(m_importRolesProgressDial->value() + 1);
            QVector<OpenAPI::OAIImportGame_request_players_inner> players;
            for (const utils::games::PlayerRecord& player : game.players)
            {
                OpenAPI::OAIImportGame_request_players_inner playerRequest{};
                playerRequest.setName(player.playerName);
                if (player.seatNumber.has_value())
                {
                    playerRequest.setSeatNumber(player.seatNumber.value());
                }
                playerRequest.setRoleStart(player.roleStartName);
                playerRequest.setRoleEnd(player.roleEndName);
                playerRequest.setAlignmentEnd(player.alignmentEnd);
                playerRequest.setIsAlive(player.isAlive);
                players.push_back(std::move(playerRequest));
            }

            OpenAPI::OAIImportGame_request gameRequest{};
            gameRequest.setGameDate(game.gameDate);
            gameRequest.setScenarioName(game.scenarioName);
            gameRequest.setStorytellerName(game.storytellerName);
            gameRequest.setAlignmentWin(game.alignmentWin);
            gameRequest.setLocation(game.location);
            gameRequest.setGameNumber(game.gameNumber);
            if (game.duration.isValid())
            {
                gameRequest.setDuration(game.duration.toString("hh:mm:ss"));
            }
            if (!game.notes.isEmpty())
            {
                gameRequest.setNotes(game.notes);
            }
            gameRequest.setPlayers(players);
            requests.push_back(std::move(gameRequest));
        }

        auto* apiClient = api::BotCApiClient::instance();
        connect(apiClient, &api::BotCApiClient::gameImportFinished, this, &ImportGamesWidget::onGamesImportFinished);
        m_importCount = requests.size();
        std::ranges::for_each(requests, [apiClient](const OpenAPI::OAIImportGame_request& in_request)
        {
            apiClient->importGame(in_request);
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

    void ImportGamesWidget::onGamesImportFinished(const OpenAPI::OAIImportGame_200_response& summary,
                                                  QNetworkReply::NetworkError error_type,
                                                  const QString& error_str)
    {
        assert(m_importCount > 0);
        --m_importCount;
        responses.push_back(summary);
        if (m_importCount != 0)
        {
            return;
        }

        auto* apiClient = api::BotCApiClient::instance();
        disconnect(apiClient, &api::BotCApiClient::gameImportFinished, this,
                   &ImportGamesWidget::onGamesImportFinished);
        m_importRolesProgressDial->setValue(m_importRolesProgressDial->maximum());

        uint successCount = 0;
        uint failedCount  = 0;

        QString message;

        for (const OpenAPI::OAIImportGame_200_response& response : responses)
        {
            if (!response.is_errors_Set())
            {
                successCount++;
                message += QString("Игра %1 успешно иимпортирована. Создано игроков: %2\n")
                           .arg(response.getGameId())
                           .arg(response.getPlayersCreated());
            }
            else
            {
                failedCount++;

                QString errors;
                for (const QString& error : response.getErrors())
                {
                    errors += error + "\n";
                }

                message += QString("Импорт произошёл с ошибками (%1).\nОшибки:\n%2")
                           .arg(response.getStatus())
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
            QMessageBox::information(this, "Games import", message);
            ui->statusLabel->setText(
                QString("Импорт партий прозошёл с ошибками"));
            ui->statusLabel->setStyleSheet("color: red; font-weight: bold;");
        }

        responses.clear();
    }

    void ImportGamesWidget::onRoleListFinished(const OpenAPI::OAIListRoles_200_response& summary,
                                               QNetworkReply::NetworkError error_type,
                                               const QString& error_str)
    {
        auto* apiClient = api::BotCApiClient::instance();
        if (error_type != QNetworkReply::NoError)
        {
            api::BotCApiClient::instance()->listRoles("ru");
            return;
        }

        disconnect(apiClient, &api::BotCApiClient::rolesListFinished,
                   this, &ImportGamesWidget::onRoleListFinished);

        std::ranges::transform(
            summary.getRoles(),
            std::inserter(availableRoles, availableRoles.end()),
            [](const OpenAPI::OAIListRoles_200_response_roles_inner& role) -> QString
            {
                return role.getTranslation().getName();
            }
        );
    }
} // botc::ui
