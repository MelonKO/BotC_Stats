#pragma once

#include <QAbstractTableModel>
#include <QString>
#include <QVariant>
#include <QVector>

#include "../db_cache/DBCache.h"

namespace botc::ui
{
    /**
     * Table model over the cached player records.
     *
     * The database id is exposed through PlayerIdRole instead of a visible
     * column, so the view stays a single "name" column while a rename can still
     * address a row by its stable id.
     *
     * The model never changes a name by itself: it only reports a valid edit
     * through nameChangeRequested, and the owner applies the change once the API
     * has accepted it.
     */
    class PlayersTableModel final : public QAbstractTableModel
    {
        Q_OBJECT

    public:
        /** Longest accepted name, matching PlayerUpdateRequest in the API. */
        static constexpr int kMaxNameLength = 200;

        enum Column
        {
            Name = 0,
            ColumnCount = 1
        };

        enum Role
        {
            PlayerIdRole = Qt::UserRole + 1
        };

        explicit PlayersTableModel(QObject* parent = nullptr);

        /**
         * Replace every row with the given records.
         *
         * The model is reset, so the view loses its selection; callers that
         * need to keep it should restore the selection by player id.
         */
        void setPlayers(const QVector<PlayerEntry>& players);

        /** Database id of the row, or an empty string when row is out of range. */
        QString playerIdAt(int row) const;

        int rowCount(const QModelIndex& parent = {}) const override;
        int columnCount(const QModelIndex& parent = {}) const override;
        QVariant data(const QModelIndex& index, int role) const override;
        QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
        Qt::ItemFlags flags(const QModelIndex& index) const override;
        bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    signals:
        /** A valid, changed name was entered; the owner has to store it. */
        void nameChangeRequested(const QString& playerId, const QString& newName);

        /** The entered value cannot be stored; the reason is user-facing. */
        void nameChangeRejected(const QString& reason);

    private:
        QVector<PlayerEntry> m_players;
    };
} // namespace botc::ui
