#include "PlayersTableModel.h"

namespace botc::ui
{
    PlayersTableModel::PlayersTableModel(QObject* parent) :
        QAbstractTableModel(parent)
    {
    }

    void PlayersTableModel::setPlayers(const QVector<PlayerEntry>& players)
    {
        beginResetModel();
        m_players = players;
        endResetModel();
    }

    QString PlayersTableModel::playerIdAt(const int row) const
    {
        if (row < 0 || row >= m_players.size()) return {};

        return m_players.at(row).id;
    }

    int PlayersTableModel::rowCount(const QModelIndex& parent) const
    {
        // A table has no child rows for a valid index.
        return parent.isValid() ? 0 : static_cast<int>(m_players.size());
    }

    int PlayersTableModel::columnCount(const QModelIndex& parent) const
    {
        return parent.isValid() ? 0 : ColumnCount;
    }

    QVariant PlayersTableModel::data(const QModelIndex& index, const int role) const
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_players.size()) return {};

        const PlayerEntry& player = m_players.at(index.row());
        switch (role)
        {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
            return player.name;
        case PlayerIdRole:
            return player.id;
        default:
            return {};
        }
    }

    QVariant PlayersTableModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
    {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal || section != Name) return {};

        return QString("Имя");
    }

    Qt::ItemFlags PlayersTableModel::flags(const QModelIndex& index) const
    {
        if (!index.isValid()) return Qt::NoItemFlags;

        // Editing arrives with the rename feature; the list itself is read-only.
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }
} // namespace botc::ui
