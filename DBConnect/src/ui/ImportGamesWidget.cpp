#include "ImportGamesWidget.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>

#include "OAIImportGame_request.h"
#include "OAIImportGame_request_players_inner.h"
#include "OAIImportGames_request.h"
#include "OAIImportGames_200_response.h"
#include "ui_ImportGamesWidget.h"
#include "../api/BotCApiClient.h"
#include "../config/ConfigManager.h"
#include "db_cache/DBCache.h"

namespace
{
    QString networkErrorMessage(QNetworkReply::NetworkError err)
    {
        switch (err)
        {
        case QNetworkReply::ServiceUnavailableError:
            return "Сервер отклонил запрос (503). Используйте пакетный импорт.";
        case QNetworkReply::TimeoutError:
            return "Превышено время ожидания ответа от сервера.";
        case QNetworkReply::ConnectionRefusedError:
        case QNetworkReply::HostNotFoundError:
            return "Нет соединения с сервером. Проверьте настройки.";
        default:
            return QString("Сетевая ошибка (код %1).").arg(static_cast<int>(err));
        }
    }
}

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

        const auto* dbCache = DBCache::instance();
        dbCache->updateRoles("ru");
        dbCache->updatePlayers();
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

        const auto* dbCache               = DBCache::instance();
        const QStringList& availableRoles = dbCache->getRolesCache();

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

        QList<OpenAPI::OAIImportGame_request> gamesList;
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
            gamesList.push_back(std::move(gameRequest));
        }

        OpenAPI::OAIImportGames_request batchRequest;
        batchRequest.setGames(gamesList);

        auto* apiClient = api::BotCApiClient::instance();
        connect(apiClient, &api::BotCApiClient::gamesImportBatchFinished,
                this, &ImportGamesWidget::onGamesImportBatchFinished,
                Qt::SingleShotConnection);
        apiClient->importGamesBatch(batchRequest);
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

        QStringList newPlayers;
        const QStringList& existingPlayers = DBCache::instance()->getPlayersCache();
        for (const auto& record : result.records)
        {
            for (const auto& player : record.players)
            {
                if (!existingPlayers.contains(player.playerName))
                {
                    newPlayers << QString("Player \"%1\" will be created after import")
                        .arg(player.playerName);
                }
            }
        }
        if (!newPlayers.isEmpty())
        {
            showInfo(newPlayers);
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

    void ImportGamesWidget::showInfo(const QStringList& messages) const
    {
        ui->infoWidget->clear();
        for (const QString& msg : messages)
        {
            ui->infoWidget->addItem(msg);
        }
        ui->infoGroup->setVisible(true);
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
        ui->infoWidget->clear();
        ui->infoGroup->setVisible(false);
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

    void ImportGamesWidget::onGamesImportBatchFinished(const OpenAPI::OAIImportGames_200_response& summary,
                                                       QNetworkReply::NetworkError error_type,
                                                       const QString& error_str)
    {
        m_importRolesProgressDial->setValue(m_importRolesProgressDial->maximum());

        if (error_type != QNetworkReply::NoError)
        {
            QMessageBox::warning(this, "Импорт партий", networkErrorMessage(error_type));
            ui->statusLabel->setText("✘ Импорт завершён с ошибкой");
            ui->statusLabel->setStyleSheet("color: red; font-weight: bold;");
            setImportEnabled(true);
            emit onGamesImported();
            return;
        }

        const auto& games = summary.getGames();
        uint successCount = 0;
        uint failedCount  = 0;
        QString message;

        for (const auto& result : games)
        {
            if (!result.getErrors().isEmpty())
            {
                failedCount++;
                message += result.getErrors().join("\n") + "\n";
            }
            else
            {
                successCount++;
                message += QString("✔ Партия %1 импортирована, создано игроков: %2\n")
                           .arg(result.getGameId())
                           .arg(result.getPlayersCreated());
            }
        }

        QMessageBox::information(this, "Импорт партий", message.trimmed());

        if (failedCount == 0)
        {
            ui->statusLabel->setText(QString("✔ Импортировано партий: %1").arg(successCount));
            ui->statusLabel->setStyleSheet("color: green; font-weight: bold;");
        }
        else if (successCount == 0)
        {
            ui->statusLabel->setText("✘ Импорт завершён с ошибками");
            ui->statusLabel->setStyleSheet("color: red; font-weight: bold;");
        }
        else
        {
            ui->statusLabel->setText(
                QString("⚠ Импортировано: %1, ошибок: %2").arg(successCount).arg(failedCount)
            );
            ui->statusLabel->setStyleSheet("color: orange; font-weight: bold;");
        }

        setImportEnabled(true);
        emit onGamesImported();
    }
} // botc::ui
