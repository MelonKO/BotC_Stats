#pragma once

#include <QWidget>

class QSortFilterProxyModel;

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
     * Read-only view of the player registry.
     *
     * The data comes from DBCache, the single owner of the players list, so the
     * tab follows every game import and every saved server setting without
     * issuing requests of its own.
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
        void refreshFromCache();

    private:
        QString selectedPlayerId() const;
        void selectPlayerById(const QString& playerId);
        void updateCounts();
        void applyStatus();
        void setRefreshBusy(bool isBusy);

    private:
        Ui::PlayersWidget* ui;

        PlayersTableModel* m_model = nullptr;
        QSortFilterProxyModel* m_proxy = nullptr;
    };
} // namespace botc::ui
