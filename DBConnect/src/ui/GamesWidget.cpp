#include "GamesWidget.h"

#include <QColor>
#include <QEvent>
#include <QHeaderView>
#include <QTableWidgetItem>

#include "OAIGameDetailsResponse.h"
#include "OAIGamePlayerItem.h"
#include "OAIGameSummaryItem.h"
#include "OAIGamesResponse.h"
#include "ui_GamesWidget.h"
#include "../api/BotCApiClient.h"

namespace botc::ui
{
    namespace
    {
        constexpr int kGameIdRole = Qt::UserRole;
        constexpr int kGameIdColumn = 0;
        constexpr int kAliveColumn = 5;
        constexpr int kGameTableColumnCount = 9;
        constexpr int kPlayerTableColumnCount = 6;
        const QString kErrorStyle = "color: red; font-weight: bold;";
        const QString kMutedStyle = "color: gray;";
        const QString kOkStyle = "color: green;";
    }

    GamesWidget::GamesWidget(QWidget* parent) :
        QWidget(parent), ui(new Ui::GamesWidget)
    {
        ui->setupUi(this);
        ui->gamesTable->verticalHeader()->setVisible(false);
        ui->playersTable->verticalHeader()->setVisible(false);
        ui->gamesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        ui->playersTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        ui->gamesTable->setColumnCount(kGameTableColumnCount);
        ui->playersTable->setColumnCount(kPlayerTableColumnCount);
        ui->detailsLabel->setText(tr("Select a game to view its players."));
        connect(ui->refreshButton, &QPushButton::clicked, this, &GamesWidget::onRefreshClicked);
        connect(ui->searchEdit, &QLineEdit::textChanged, this, &GamesWidget::onSearchTextChanged);
        connect(ui->gamesTable, &QTableWidget::itemSelectionChanged, this, &GamesWidget::onGameSelectionChanged);
        connect(ui->previousButton, &QPushButton::clicked, this, &GamesWidget::onPreviousPageClicked);
        connect(ui->nextButton, &QPushButton::clicked, this, &GamesWidget::onNextPageClicked);
        connect(api::BotCApiClient::instance(), &api::BotCApiClient::gamesListFinished,
                this, &GamesWidget::onGamesListFinished);
        connect(api::BotCApiClient::instance(), &api::BotCApiClient::gameDetailsFinished,
                this, &GamesWidget::onGameDetailsFinished);
        populateGames({});
        ui->playersTable->setHorizontalHeaderLabels({tr("Seat"), tr("Player"), tr("Role (start)"),
                                                     tr("Role (end)"), tr("Alignment"), tr("Alive")});
        updatePagination();
        updateStatus();
        refreshGames();
    }

    GamesWidget::~GamesWidget()
    {
        delete ui;
    }

    void GamesWidget::refreshGames()
    {
        m_pageOffset = 0;
        if (m_isListRequestInFlight)
        {
            m_refreshPending = true;
            return;
        }
        loadPage();
    }

    void GamesWidget::changeEvent(QEvent* event)
    {
        if (event->type() == QEvent::LanguageChange)
            retranslate();
        QWidget::changeEvent(event);
    }

    void GamesWidget::onRefreshClicked()
    {
        refreshGames();
    }

    void GamesWidget::onSearchTextChanged(const QString& text)
    {
        updateVisibleRows(text);
        const int currentRow = ui->gamesTable->currentRow();
        if (currentRow >= 0 && !ui->gamesTable->isRowHidden(currentRow))
        {
            requestSelectedGameDetails();
            return;
        }
        for (int row = 0; row < ui->gamesTable->rowCount(); ++row)
        {
            if (ui->gamesTable->isRowHidden(row)) continue;
            ui->gamesTable->selectRow(row);
            return;
        }
        ui->gamesTable->setCurrentCell(-1, -1);
        ui->gamesTable->clearSelection();
        requestSelectedGameDetails();
    }

    void GamesWidget::onGameSelectionChanged()
    {
        requestSelectedGameDetails();
    }

    void GamesWidget::onPreviousPageClicked()
    {
        if (m_pageOffset < m_pageSize || m_isListRequestInFlight) return;
        m_pageOffset -= m_pageSize;
        loadPage();
    }

    void GamesWidget::onNextPageClicked()
    {
        if (m_pageOffset + m_pageSize >= m_totalGames || m_isListRequestInFlight) return;
        m_pageOffset += m_pageSize;
        loadPage();
    }

    void GamesWidget::onGamesListFinished(const OpenAPI::OAIGamesResponse& summary,
                                          const QNetworkReply::NetworkError errorType,
                                          const QString& errorString)
    {
        m_isListRequestInFlight = false;
        if (errorType != QNetworkReply::NoError)
        {
            m_lastError = errorType;
            m_lastErrorString = errorString;
            updateStatus();
        }
        else
        {
            const QString preferredGameId = selectedGameId();
            m_lastError = QNetworkReply::NoError;
            m_lastErrorString.clear();
            m_pageOffset = summary.getOffset();
            m_totalGames = summary.getTotal();
            m_games.clear();
            const QList<OpenAPI::OAIGameSummaryItem> games = summary.getGames();
            m_games.reserve(games.size());
            for (const OpenAPI::OAIGameSummaryItem& game : games)
            {
                m_games.append(GameSummary{
                    game.getId(), game.getGameDate(), game.getGameNumber(), game.getScenarioName(),
                    game.getLocation(), game.getStoryteller(),
                    game.getAlignmentWinRu().isEmpty() ? game.getAlignmentWin() : game.getAlignmentWinRu(),
                    game.getPlayers(), game.getSurvivors(), game.getDuration()
                });
            }
            populateGames(preferredGameId);
            updatePagination();
            updateStatus();
        }
        if (m_refreshPending)
        {
            m_refreshPending = false;
            loadPage();
        }
    }

    void GamesWidget::onGameDetailsFinished(const OpenAPI::OAIGameDetailsResponse& summary,
                                            const QNetworkReply::NetworkError errorType,
                                            const QString& errorString)
    {
        m_isDetailsRequestInFlight = false;
        const QString selectedId = selectedGameId();
        if (selectedId != m_requestedDetailsId)
        {
            requestSelectedGameDetails();
            return;
        }
        if (errorType != QNetworkReply::NoError)
        {
            ui->playersTable->setRowCount(0);
            ui->detailsLabel->setText(tr("Failed to load game details. %1").arg(errorString));
            ui->detailsLabel->setStyleSheet(kErrorStyle);
            return;
        }
        if (summary.getId() != m_requestedDetailsId)
        {
            ui->playersTable->setRowCount(0);
            ui->detailsLabel->setText(tr("The server returned details for a different game."));
            ui->detailsLabel->setStyleSheet(kErrorStyle);
            return;
        }
        populateDetails(summary);
    }

    void GamesWidget::loadPage()
    {
        m_isListRequestInFlight = true;
        ui->refreshButton->setEnabled(false);
        ui->previousButton->setEnabled(false);
        ui->nextButton->setEnabled(false);
        updateStatus();
        api::BotCApiClient::instance()->listGames(m_pageSize, m_pageOffset);
    }

    void GamesWidget::populateGames(const QString& preferredGameId)
    {
        ui->gamesTable->setSortingEnabled(false);
        ui->gamesTable->clear();
        ui->gamesTable->setColumnCount(kGameTableColumnCount);
        ui->gamesTable->setHorizontalHeaderLabels({tr("Date"), tr("Scenario"), tr("Location"), tr("No."),
                                                   tr("Storyteller"), tr("Winner"), tr("Players"),
                                                   tr("Survivors"), tr("Duration")});
        ui->gamesTable->setRowCount(m_games.size());
        int selectedRow = -1;
        for (int row = 0; row < m_games.size(); ++row)
        {
            const GameSummary& game = m_games[row];
            const QStringList cells = {
                game.gameDate.toString("dd.MM.yyyy"), game.scenarioName, game.location,
                QString::number(game.gameNumber), game.storyteller, game.winningSide,
                QString::number(game.playerCount), QString::number(game.survivorCount),
                game.duration.isEmpty() ? QStringLiteral("—") : game.duration
            };
            for (int column = 0; column < cells.size(); ++column)
            {
                auto* item = new QTableWidgetItem(cells[column]);
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                if (column == kGameIdColumn) item->setData(kGameIdRole, game.id);
                ui->gamesTable->setItem(row, column, item);
            }
            if (game.id == preferredGameId) selectedRow = row;
        }
        ui->gamesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        ui->gamesTable->resizeColumnsToContents();
        updateVisibleRows(ui->searchEdit->text());
        if (selectedRow >= 0 && ui->gamesTable->isRowHidden(selectedRow)) selectedRow = -1;
        if (selectedRow < 0)
        {
            for (int row = 0; row < ui->gamesTable->rowCount(); ++row)
            {
                if (!ui->gamesTable->isRowHidden(row))
                {
                    selectedRow = row;
                    break;
                }
            }
        }
        if (selectedRow >= 0) ui->gamesTable->selectRow(selectedRow);
        else
        {
            ui->playersTable->setRowCount(0);
            ui->detailsLabel->setText(tr("Select a game to view its players."));
            ui->detailsLabel->setStyleSheet(QString());
        }
    }

    void GamesWidget::populateDetails(const OpenAPI::OAIGameDetailsResponse& summary)
    {
        const QString date = summary.getGameDate().toString("dd.MM.yyyy");
        const QString winner = summary.getAlignmentWinRu().isEmpty()
                                   ? summary.getAlignmentWin() : summary.getAlignmentWinRu();
        const QString duration = summary.getDuration().isEmpty() ? QStringLiteral("—") : summary.getDuration();
        QString details = tr("%1, %2, game %3 — %4; winner: %5; %6 players, %7 survivors; duration: %8")
            .arg(summary.getScenarioName(), date, QString::number(summary.getGameNumber()),
                 summary.getLocation(), winner, QString::number(summary.getPlayers()),
                 QString::number(summary.getSurvivors()), duration);
        if (!summary.getStoryteller().isEmpty()) details += tr("; storyteller(s): %1").arg(summary.getStoryteller());
        if (!summary.getNotes().isEmpty()) details += tr("\nNotes: %1").arg(summary.getNotes());
        ui->detailsLabel->setText(details);
        ui->detailsLabel->setStyleSheet(kOkStyle);
        const QList<OpenAPI::OAIGamePlayerItem> players = summary.getGamePlayers();
        ui->playersTable->clear();
        ui->playersTable->setColumnCount(kPlayerTableColumnCount);
        ui->playersTable->setHorizontalHeaderLabels({tr("Seat"), tr("Player"), tr("Role (start)"),
                                                    tr("Role (end)"), tr("Alignment"), tr("Alive")});
        ui->playersTable->setRowCount(players.size());
        for (int row = 0; row < players.size(); ++row)
        {
            const OpenAPI::OAIGamePlayerItem& player = players[row];
            const QStringList cells = {
                player.is_seat_number_Set() ? QString::number(player.getSeatNumber()) : QStringLiteral("—"),
                player.getName(), player.getRoleStart(), player.getRoleEnd(), player.getAlignmentEnd(),
                player.isIsAlive() ? QStringLiteral("✔") : QStringLiteral("✘")
            };
            for (int column = 0; column < cells.size(); ++column)
            {
                auto* item = new QTableWidgetItem(cells[column]);
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                if (column == kAliveColumn) item->setForeground(player.isIsAlive() ? QColor(0, 150, 0) : QColor(200, 0, 0));
                ui->playersTable->setItem(row, column, item);
            }
        }
        ui->playersTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        ui->playersTable->resizeColumnsToContents();
    }

    void GamesWidget::requestSelectedGameDetails()
    {
        const QString gameId = selectedGameId();
        if (gameId.isEmpty())
        {
            ui->playersTable->setRowCount(0);
            ui->detailsLabel->setText(tr("Select a game to view its players."));
            ui->detailsLabel->setStyleSheet(kMutedStyle);
            return;
        }
        if (m_isDetailsRequestInFlight) return;
        m_isDetailsRequestInFlight = true;
        m_requestedDetailsId = gameId;
        ui->playersTable->setRowCount(0);
        ui->detailsLabel->setText(tr("Loading game details..."));
        ui->detailsLabel->setStyleSheet(kMutedStyle);
        api::BotCApiClient::instance()->getGame(gameId);
    }

    QString GamesWidget::selectedGameId() const
    {
        const int row = ui->gamesTable->currentRow();
        const QTableWidgetItem* item = ui->gamesTable->item(row, 0);
        return item == nullptr ? QString() : item->data(kGameIdRole).toString();
    }

    void GamesWidget::updateVisibleRows(const QString& text)
    {
        const QString filter = text.trimmed();
        for (int row = 0; row < ui->gamesTable->rowCount(); ++row)
        {
            bool matches = filter.isEmpty();
            for (int column = 0; !matches && column < ui->gamesTable->columnCount(); ++column)
            {
                const QTableWidgetItem* item = ui->gamesTable->item(row, column);
                matches = item != nullptr && item->text().contains(filter, Qt::CaseInsensitive);
            }
            ui->gamesTable->setRowHidden(row, !matches);
        }
    }

    void GamesWidget::updatePagination()
    {
        const int first = m_games.isEmpty() ? 0 : m_pageOffset + 1;
        const int last = m_pageOffset + m_games.size();
        ui->pageLabel->setText(tr("%1–%2 of %3").arg(first).arg(last).arg(m_totalGames));
        ui->previousButton->setEnabled(!m_isListRequestInFlight && m_pageOffset > 0);
        ui->nextButton->setEnabled(!m_isListRequestInFlight && m_pageOffset + m_games.size() < m_totalGames);
    }

    void GamesWidget::updateStatus()
    {
        if (m_isListRequestInFlight)
        {
            ui->statusLabel->setText(tr("Loading games..."));
            ui->statusLabel->setStyleSheet(kMutedStyle);
        }
        else if (m_lastError != QNetworkReply::NoError)
        {
            ui->statusLabel->setText(tr("Failed to load games. %1").arg(m_lastErrorString));
            ui->statusLabel->setStyleSheet(kErrorStyle);
            ui->statusLabel->setToolTip(m_lastErrorString);
        }
        else if (m_totalGames == 0)
        {
            ui->statusLabel->setText(tr("No games in the database."));
            ui->statusLabel->setStyleSheet(kMutedStyle);
        }
        else
        {
            ui->statusLabel->setText(tr("Games list loaded."));
            ui->statusLabel->setStyleSheet(kOkStyle);
        }
        ui->refreshButton->setEnabled(!m_isListRequestInFlight);
        updatePagination();
    }

    void GamesWidget::retranslate()
    {
        ui->retranslateUi(this);
        populateGames(selectedGameId());
        ui->playersTable->setHorizontalHeaderLabels({tr("Seat"), tr("Player"), tr("Role (start)"),
                                                     tr("Role (end)"), tr("Alignment"), tr("Alive")});
        updateStatus();
        requestSelectedGameDetails();
    }
}
