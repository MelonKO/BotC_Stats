#include "PlayersWidget.h"

#include <QHeaderView>
#include <QItemSelectionModel>
#include <QSortFilterProxyModel>

#include "PlayersTableModel.h"
#include "ui_PlayersWidget.h"
#include "../db_cache/DBCache.h"

namespace
{
    /**
     * User-facing reason for a failed players list request.
     *
     * The raw error string from the client combines a technical network message
     * with the response body, so it is shown as a tooltip instead of the status
     * line.
     */
    QString describeLoadError(const QNetworkReply::NetworkError errorType)
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

        connect(ui->searchEdit, &QLineEdit::textChanged,
                this, &PlayersWidget::onSearchTextChanged);
        connect(ui->refreshButton, &QPushButton::clicked,
                this, &PlayersWidget::onRefreshClicked);
        connect(DBCache::instance(), &DBCache::onPlayersCacheUpdated,
                this, &PlayersWidget::refreshFromCache);

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
        // A fixed string, not a regular expression: player names may contain
        // '.', '(' and similar characters that would change the filter meaning.
        m_proxy->setFilterFixedString(text);
        updateCounts();
    }

    void PlayersWidget::onRefreshClicked()
    {
        // Disable right away: DBCache may coalesce this request into one that is
        // already in flight, and only that reply can report completion.
        setRefreshBusy(true);
        DBCache::instance()->updatePlayers();
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
                QString("✘ Не удалось загрузить игроков. %1").arg(describeLoadError(errorType)));
            ui->statusLabel->setStyleSheet("color: red; font-weight: bold;");
            ui->statusLabel->setToolTip(DBCache::instance()->getPlayersError());
            return;
        }

        ui->statusLabel->setToolTip(QString());
        if (m_model->rowCount() == 0)
        {
            ui->statusLabel->setText("Игроков в базе нет.");
            ui->statusLabel->setStyleSheet("color: gray;");
            return;
        }

        ui->statusLabel->setText("✔ Список игроков загружен.");
        ui->statusLabel->setStyleSheet("color: green;");
    }

    void PlayersWidget::setRefreshBusy(const bool isBusy)
    {
        ui->refreshButton->setEnabled(!isBusy);
    }
} // namespace botc::ui
