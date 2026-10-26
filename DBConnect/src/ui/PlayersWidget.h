#pragma once

#include <QWidget>

#include "QNetworkReply"

class QSortFilterProxyModel;

namespace OpenAPI
{
    class OAIPlayerItem;
}

namespace botc::ui
{
    QT_BEGIN_NAMESPACE

    namespace Ui
    {
        class PlayersWidget;
    }

    QT_END_NAMESPACE

    class PlayersTableModel;

    /**
     * View of the player registry with inline renaming.
     *
     * The data comes from DBCache, the single owner of the players list, so the
     * tab follows every game import and every saved server setting without
     * issuing list requests of its own. A rename goes through the API and is
     * only shown once the server has accepted it.
     */
    class PlayersWidget final : public QWidget
    {
        Q_OBJECT

    public:
        explicit PlayersWidget(QWidget* parent = nullptr);
        ~PlayersWidget() override;

    private slots:
        void onSearchTextChanged(const QString& text);
        void onRefreshClicked();
        void onNameChangeRequested(const QString& playerId, const QString& newName);
        void onPlayerUpdateFinished(const OpenAPI::OAIPlayerItem& player,
                                    QNetworkReply::NetworkError errorType,
                                    int httpStatus, const QString& errorStr);
        void refreshFromCache();

    private:
        QString selectedPlayerId() const;
        QString currentPlayerName(const QString& playerId) const;
        void selectPlayerById(const QString& playerId);
        void updateCounts();
        void applyStatus();
        void showMessage(const QString& message, const QString& style);
        void setRefreshBusy(bool isBusy);
        void setRenameBusy(bool isBusy);

    private:
        Ui::PlayersWidget* ui;

        PlayersTableModel* m_model = nullptr;
        QSortFilterProxyModel* m_proxy = nullptr;

        // One rename at a time: only the reply of the pending request is applied.
        bool m_isRenameInFlight = false;
        QString m_pendingPlayerId{};

        // Result of the last user action. A cache refresh cannot report it, so it
        // stays visible until the next search or refresh.
        QString m_actionMessage{};
        QString m_actionStyle{};
    };
} // namespace botc::ui
