#include "PlayersWidget.h"

#include <algorithm>

#include <QHeaderView>
#include <QItemSelectionModel>
#include <QMessageBox>
#include <QSortFilterProxyModel>

#include "PlayersTableModel.h"
#include "OAIPlayerItem.h"
#include "OAIPlayerUpdateRequest.h"
#include "ui_PlayersWidget.h"
#include "../api/BotCApiClient.h"
#include "../db_cache/DBCache.h"

namespace
{
    // Editing is suspended only while a rename is in flight, so a row cannot be
    // edited twice before the server answers.
    constexpr auto kPlayerEditTriggers =
        QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed;

    const QString kErrorStyle = "color: red; font-weight: bold;";
    const QString kOkStyle = "color: green;";
    const QString kMutedStyle = "color: gray;";

    /**
     * User-facing reason for a failed request.
     *
     * The raw error string from the client combines a technical network message
     * with the response body, so it is shown as a tooltip instead of the status
     * line.
     */
    QString describeNetworkError(const QNetworkReply::NetworkError errorType)
    {
        switch (errorType)
        {
        case QNetworkReply::TimeoutError:
            return "Превышено время ожидания ответа от сервера.";
        case QNetworkReply::ConnectionRefusedError:
        case QNetworkReply::HostNotFoundError:
            return "Нет соединения с сервером. Проверьте настройки.";
        case QNetworkReply::AuthenticationRequiredError:
        case QNetworkReply::ContentAccessDenied:
            return "Сервер отклонил API-ключ.";
        default:
            return QString("Сетевая ошибка (код %1).").arg(static_cast<int>(errorType));
        }
    }
}

namespace botc::ui
{
    PlayersWidget::PlayersWidget(QWidget* parent) :
        QWidget(parent), ui(new Ui::PlayersWidget)
    {
        ui->setupUi(this);

        m_model = new PlayersTableModel(this);
        m_proxy = new QSortFilterProxyModel(this);
        m_proxy->setSourceModel(m_model);
        m_proxy->setFilterKeyColumn(PlayersTableModel::Name);
        m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
        m_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);

        ui->playersTable->setModel(m_proxy);
        ui->playersTable->setSortingEnabled(true);
        ui->playersTable->sortByColumn(PlayersTableModel::Name, Qt::AscendingOrder);
        ui->playersTable->verticalHeader()->setVisible(false);
        ui->playersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        setRenameBusy(false);

        connect(ui->searchEdit, &QLineEdit::textChanged,
                this, &PlayersWidget::onSearchTextChanged);
        connect(ui->refreshButton, &QPushButton::clicked,
                this, &PlayersWidget::onRefreshClicked);
        connect(DBCache::instance(), &DBCache::onPlayersCacheUpdated,
                this, &PlayersWidget::refreshFromCache);
        connect(m_model, &PlayersTableModel::nameChangeRequested,
                this, &PlayersWidget::onNameChangeRequested);
        connect(m_model, &PlayersTableModel::nameChangeRejected,
                this, [this](const QString& reason) { showMessage(QString("✘ %1").arg(reason), kErrorStyle); });
        connect(api::BotCApiClient::instance(), &api::BotCApiClient::playerUpdateFinished,
                this, &PlayersWidget::onPlayerUpdateFinished);

        // Show what the cache already holds, then ask for fresh data. A request
        // started by another tab is coalesced inside DBCache, so startup never
        // doubles the traffic.
        refreshFromCache();
        DBCache::instance()->updatePlayers();
        setRefreshBusy(DBCache::instance()->isPlayersRequestInFlight());
    }

    PlayersWidget::~PlayersWidget()
    {
        delete ui;
    }

    void PlayersWidget::onSearchTextChanged(const QString& text)
    {
        // A new search replaces the previous action feedback with the load state.
        m_actionMessage.clear();
        // A fixed string, not a regular expression: player names may contain
        // '.', '(' and similar characters that would change the filter meaning.
        m_proxy->setFilterFixedString(text);
        updateCounts();
        applyStatus();
    }

    void PlayersWidget::onRefreshClicked()
    {
        m_actionMessage.clear();
        // Disable right away: DBCache may coalesce this request into one that is
        // already in flight, and only that reply can report completion.
        setRefreshBusy(true);
        DBCache::instance()->updatePlayers();
    }

    void PlayersWidget::onNameChangeRequested(const QString& playerId, const QString& newName)
    {
        if (m_isRenameInFlight) return;

        const QString currentName = currentPlayerName(playerId);
        const auto answer = QMessageBox::question(
            this,
            "Переименование игрока",
            QString("Переименовать игрока «%1» в «%2»?\n\n"
                    "История партий останется у этого игрока, но CSV-файлы со старым "
                    "именем при следующем импорте создадут нового игрока.")
            .arg(currentName, newName),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes)
        {
            showMessage("Переименование отменено.", kMutedStyle);
            return;
        }

        m_isRenameInFlight = true;
        m_pendingPlayerId = playerId;
        setRenameBusy(true);

        OpenAPI::OAIPlayerUpdateRequest request;
        request.setName(newName);
        api::BotCApiClient::instance()->updatePlayer(playerId, request);
    }

    void PlayersWidget::onPlayerUpdateFinished(const OpenAPI::OAIPlayerItem& player,
                                               const QNetworkReply::NetworkError errorType,
                                               const int httpStatus, const QString& errorStr)
    {
        // A reply nobody in this view waits for: ignore it.
        if (!m_isRenameInFlight) return;

        const QString playerId = m_pendingPlayerId;
        m_isRenameInFlight = false;
        m_pendingPlayerId.clear();
        setRenameBusy(false);

        switch (httpStatus)
        {
        case 200:
            // Patch the cache first: the refresh below then shows the new name
            // instead of briefly restoring the old one.
            DBCache::instance()->applyPlayerRename(playerId, player.getName());
            showMessage(QString("✔ Игрок переименован в «%1».").arg(player.getName()), kOkStyle);
            DBCache::instance()->updatePlayers();
            return;
        case 409:
            showMessage("✘ Это имя уже занято другим игроком. Имя не изменено.", kErrorStyle);
            return;
        case 404:
            showMessage("✘ Игрок не найден — возможно, список устарел. Список обновлён.", kErrorStyle);
            DBCache::instance()->updatePlayers();
            return;
        case 422:
            showMessage("✘ Недопустимое имя: от 1 до 200 символов. Имя не изменено.", kErrorStyle);
            return;
        default:
            break;
        }

        showMessage(QString("✘ Не удалось переименовать игрока. %1").arg(describeNetworkError(errorType)),
                    kErrorStyle);
        ui->statusLabel->setToolTip(errorStr);
    }

    void PlayersWidget::refreshFromCache()
    {
        const QString selectedId = selectedPlayerId();

        m_model->setPlayers(DBCache::instance()->getPlayerEntries());

        // The model reset dropped the selection; put it back on the same player.
        selectPlayerById(selectedId);
        updateCounts();
        applyStatus();
        setRefreshBusy(DBCache::instance()->isPlayersRequestInFlight());
    }

    QString PlayersWidget::selectedPlayerId() const
    {
        const QModelIndexList selectedRows = ui->playersTable->selectionModel()->selectedRows();
        if (selectedRows.isEmpty()) return {};

        return m_model->playerIdAt(m_proxy->mapToSource(selectedRows.first()).row());
    }

    QString PlayersWidget::currentPlayerName(const QString& playerId) const
    {
        const QVector<PlayerEntry>& entries = DBCache::instance()->getPlayerEntries();
        const auto entry = std::ranges::find_if(entries, [&playerId](const PlayerEntry& player)
        {
            return player.id == playerId;
        });

        return entry == entries.end() ? QString() : entry->name;
    }

    void PlayersWidget::selectPlayerById(const QString& playerId)
    {
        if (playerId.isEmpty()) return;

        for (int row = 0; row < m_model->rowCount(); ++row)
        {
            if (m_model->playerIdAt(row) != playerId) continue;

            // The row may be hidden by the current search text.
            const QModelIndex proxyIndex = m_proxy->mapFromSource(m_model->index(row, PlayersTableModel::Name));
            if (proxyIndex.isValid()) ui->playersTable->selectRow(proxyIndex.row());
            return;
        }
    }

    void PlayersWidget::updateCounts()
    {
        ui->countLabel->setText(QString("Игроков: %1, показано: %2")
            .arg(m_model->rowCount())
            .arg(m_proxy->rowCount()));
    }

    void PlayersWidget::applyStatus()
    {
        const QNetworkReply::NetworkError errorType = DBCache::instance()->getPlayersErrorType();
        if (errorType != QNetworkReply::NoError)
        {
            ui->statusLabel->setText(
                QString("✘ Не удалось загрузить игроков. %1").arg(describeNetworkError(errorType)));
            ui->statusLabel->setStyleSheet(kErrorStyle);
            ui->statusLabel->setToolTip(DBCache::instance()->getPlayersError());
            return;
        }

        ui->statusLabel->setToolTip(QString());
        if (!m_actionMessage.isEmpty())
        {
            ui->statusLabel->setText(m_actionMessage);
            ui->statusLabel->setStyleSheet(m_actionStyle);
            return;
        }

        if (m_model->rowCount() == 0)
        {
            ui->statusLabel->setText("Игроков в базе нет.");
            ui->statusLabel->setStyleSheet(kMutedStyle);
            return;
        }

        ui->statusLabel->setText("✔ Список игроков загружен.");
        ui->statusLabel->setStyleSheet(kOkStyle);
    }

    void PlayersWidget::showMessage(const QString& message, const QString& style)
    {
        m_actionMessage = message;
        m_actionStyle = style;
        applyStatus();
    }

    void PlayersWidget::setRefreshBusy(const bool isBusy)
    {
        ui->refreshButton->setEnabled(!isBusy);
    }

    void PlayersWidget::setRenameBusy(const bool isBusy)
    {
        ui->playersTable->setEditTriggers(isBusy ? QAbstractItemView::NoEditTriggers : kPlayerEditTriggers);
    }
} // namespace botc::ui
