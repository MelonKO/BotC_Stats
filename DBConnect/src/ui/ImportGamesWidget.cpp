#include "ImportGamesWidget.h"

#include <QColor>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>

#include "OAIGameImportRequest.h"
#include "OAIPlayerImportRequest.h"
#include "OAIGamesImportRequest.h"
#include "OAIGamesImportStatusResponse.h"
#include "ui_ImportGamesWidget.h"
#include "../api/BotCApiClient.h"
#include "NetworkErrorMessages.h"
#include "db_cache/DBCache.h"

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

        // Selecting a game shows its players
        connect(ui->partiesTable, &QTableWidget::currentCellChanged,
                this, [this](const int row, int, int, int) { onGameSelected(row); });

        setImportEnabled(false);

        auto* dbCache = DBCache::instance();
        dbCache->updateRoles("ru");
        dbCache->updatePlayers();
    }

    ImportGamesWidget::~ImportGamesWidget()
    {
        delete ui;
    }

    void ImportGamesWidget::changeEvent(QEvent* event)
    {
        if (event->type() == QEvent::LanguageChange)
            retranslate();

        QWidget::changeEvent(event);
    }

    void ImportGamesWidget::onBrowseClicked()
    {
        const QString path = QFileDialog::getOpenFileName(
            this,
            tr("Select a CSV file"),
            QDir::homePath(),
            tr("CSV files (*.csv);;All files (*)")
        );
        if (path.isEmpty()) return;

        ui->filePathEdit->setText(path);
        loadPreview(path);
    }

    void ImportGamesWidget::loadPreview(const QString& path)
    {
        clearAll();

        m_lastResult = utils::games::GamesCsvParser::parse(path);

        const QStringList& availableRoles = DBCache::instance()->getRolesCache();

        for (const utils::games::GameRecord& record : m_lastResult.records)
        {
            for (const auto& player : record.players)
            {
                if (!availableRoles.contains(player.roleStartName))
                {
                    m_lastResult.errors << tr("Role \"%1\" is missing in the database. Add it before importing.")
                        .arg(player.roleStartName);
                }
                if (!availableRoles.contains(player.roleEndName))
                {
                    m_lastResult.errors << tr("Role \"%1\" is missing in the database. Add it before importing.")
                        .arg(player.roleEndName);
                }
            }
        }

        showPreview(m_lastResult);
    }

    void ImportGamesWidget::retranslate()
    {
        ui->retranslateUi(this);

        const QString path = ui->filePathEdit->text();
        if (path.isEmpty())
        {
            ui->statusLabel->clear();
            return;
        }

        // Re-parse so that the parser errors and the preview follow the new
        // language as well.
        loadPreview(path);
    }

    void ImportGamesWidget::onImportClicked()
    {
        if (m_lastResult.records.isEmpty()) return;
        setImportEnabled(false);

        m_importGamesProgressDial = new QProgressDialog(tr("Importing games"),
                                                        tr("Cancel"),
                                                        0,
                                                        static_cast<int>(m_lastResult.records.size()) + 1, this);
        m_importGamesProgressDial->setWindowModality(Qt::WindowModal);
        m_importGamesProgressDial->show();

        QList<OpenAPI::OAIGameImportRequest> gamesList;
        for (const utils::games::GameRecord& game : m_lastResult.records)
        {
            m_importGamesProgressDial->setValue(m_importGamesProgressDial->value() + 1);
            QVector<OpenAPI::OAIPlayerImportRequest> players;
            for (const utils::games::PlayerRecord& player : game.players)
            {
                OpenAPI::OAIPlayerImportRequest playerRequest{};
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

            OpenAPI::OAIGameImportRequest gameRequest{};
            gameRequest.setGameDate(game.gameDate);
            gameRequest.setScenarioName(game.scenarioName);
            gameRequest.setStorytellerNames(game.storytellerNames);
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

        OpenAPI::OAIGamesImportRequest batchRequest;
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
            ui->statusLabel->setText(tr("✘ No records loaded."));
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
                    newPlayers << tr("Player \"%1\" will be created after import")
                        .arg(player.playerName);
                }
            }
        }
        if (!newPlayers.isEmpty())
        {
            showInfo(newPlayers);
        }

        populateGamesTable();

        // Select the first game automatically
        ui->partiesTable->selectRow(0);
        populatePlayersTable(0);

        const bool hasErrors = !result.errors.isEmpty();
        ui->statusLabel->setText(
            hasErrors
                ? tr("⚠ Loaded %1 games, validation errors found").arg(result.records.size())
                : tr("✔ Loaded %1 games, no errors").arg(result.records.size())
        );
        ui->statusLabel->setStyleSheet(statusStyle(!hasErrors));
        setImportEnabled(true);
    }

    void ImportGamesWidget::populateGamesTable()
    {
        const QStringList headers = {
            tr("Date"), tr("Scenario"), tr("Location"), tr("No."),
            tr("Storyteller"), tr("Winner"), tr("Duration (min)"), tr("Players")
        };

        auto& records = m_lastResult.records;

        ui->partiesTable->clear();
        ui->partiesTable->setColumnCount(headers.size());
        ui->partiesTable->setRowCount(records.size());
        ui->partiesTable->setHorizontalHeaderLabels(headers);

        for (int row = 0; row < records.size(); ++row)
        {
            const utils::games::GameRecord& record = records[row];

            QStringList cells = {
                record.gameDate.toString("dd.MM.yyyy"),
                record.scenarioName,
                record.location,
                QString::number(record.gameNumber),
                record.storytellerNames.join(", "),
                record.alignmentWin,
                record.duration.isValid() ? record.duration.toString("hh:mm:ss") : "—",
                QString::number(record.players.size())
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
            tr("Seat"), tr("Player"), tr("Role (start)"), tr("Role (end)"),
            tr("Alignment"), tr("Alive")
        };

        const utils::games::GameRecord& game_record = m_lastResult.records[partyIndex];

        ui->playersTable->clear();
        ui->playersTable->setColumnCount(headers.size());
        ui->playersTable->setRowCount(game_record.players.size());
        ui->playersTable->setHorizontalHeaderLabels(headers);

        ui->playersGroupBox->setTitle(
            tr("Players — %1, %2 (%3)")
            .arg(game_record.scenarioName,
                 game_record.gameDate.toString("dd.MM.yyyy"),
                 game_record.storytellerNames.join(", "))
        );

        for (int row = 0; row < game_record.players.size(); ++row)
        {
            const utils::games::PlayerRecord& player = game_record.players[row];

            QStringList cells = {
                player.seatNumber.has_value() ? QString::number(player.seatNumber.value()) : "-",
                player.playerName,
                player.roleStartName,
                player.roleEndName,
                player.alignmentEnd,
                player.isAlive ? "✔" : "✘"
            };

            for (int col = 0; col < cells.size(); ++col)
            {
                auto* item = new QTableWidgetItem(cells[col]);
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);

                // Highlight alive/dead players
                if (col == 5)
                    item->setForeground(player.isAlive ? QColor(0, 150, 0) : QColor(200, 0, 0));

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

    void ImportGamesWidget::onGamesImportBatchFinished(const OpenAPI::OAIGamesImportStatusResponse& summary,
                                                       QNetworkReply::NetworkError error_type,
                                                       const QString& error_str)
    {
        m_importGamesProgressDial->setValue(m_importGamesProgressDial->maximum());

        if (error_type != QNetworkReply::NoError)
        {
            QMessageBox::warning(this, tr("Games import"), describeNetworkError(error_type));
            ui->statusLabel->setText(tr("✘ Import finished with an error"));
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
                message += tr("✔ Game %1 imported, players created: %2\n")
                           .arg(result.getGameId())
                           .arg(result.getPlayersCreated());
            }
        }

        QMessageBox::information(this, tr("Games import"), message.trimmed());

        if (failedCount == 0)
        {
            ui->statusLabel->setText(tr("✔ Games imported: %1").arg(successCount));
            ui->statusLabel->setStyleSheet("color: green; font-weight: bold;");
        }
        else if (successCount == 0)
        {
            ui->statusLabel->setText(tr("✘ Import finished with errors"));
            ui->statusLabel->setStyleSheet("color: red; font-weight: bold;");
        }
        else
        {
            ui->statusLabel->setText(
                tr("⚠ Imported: %1, errors: %2").arg(successCount).arg(failedCount)
            );
            ui->statusLabel->setStyleSheet("color: orange; font-weight: bold;");
        }

        setImportEnabled(true);
        emit onGamesImported();
    }
} // namespace botc::ui
