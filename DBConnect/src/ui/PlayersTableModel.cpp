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
        case Qt::EditRole:
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

        return tr("Name");
    }

    void PlayersTableModel::retranslate()
    {
        emit headerDataChanged(Qt::Horizontal, Name, ColumnCount - 1);
    }

    Qt::ItemFlags PlayersTableModel::flags(const QModelIndex& index) const
    {
        if (!index.isValid()) return Qt::NoItemFlags;

        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    }

    bool PlayersTableModel::setData(const QModelIndex& index, const QVariant& value, const int role)
    {
        if (role != Qt::EditRole || !index.isValid() || index.column() != Name) return false;

        const QString playerId = playerIdAt(index.row());
        if (playerId.isEmpty()) return false;

        // The API stores the name trimmed, so that is the value to validate.
        const QString newName = value.toString().trimmed();
        if (newName.isEmpty())
        {
            emit nameChangeRejected(tr("The name cannot be empty."));
            return false;
        }
        if (newName.size() > kMaxNameLength)
        {
            emit nameChangeRejected(tr("The name cannot be longer than %1 characters.").arg(kMaxNameLength));
            return false;
        }
        if (newName == m_players.at(index.row()).name) return false;

        emit nameChangeRequested(playerId, newName);

        // The row keeps its current name until the server accepts the change.
        return false;
    }
} // namespace botc::ui
