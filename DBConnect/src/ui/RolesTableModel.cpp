#include "RolesTableModel.h"

namespace botc::ui
{
    RolesTableModel::RolesTableModel(QObject* parent) :
        QAbstractTableModel(parent)
    {
    }

    void RolesTableModel::setRoles(const QVector<RoleEntry>& roles)
    {
        beginResetModel();
        m_roles = roles;
        endResetModel();
    }

    QString RolesTableModel::roleIdAt(const int row) const
    {
        if (row < 0 || row >= m_roles.size()) return {};

        return m_roles.at(row).id;
    }

    int RolesTableModel::rowCount(const QModelIndex& parent) const
    {
        return parent.isValid() ? 0 : static_cast<int>(m_roles.size());
    }

    int RolesTableModel::columnCount(const QModelIndex& parent) const
    {
        return parent.isValid() ? 0 : ColumnCount;
    }

    QVariant RolesTableModel::data(const QModelIndex& index, const int role) const
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_roles.size()
            || index.column() < 0 || index.column() >= ColumnCount) return {};

        const RoleEntry& roleEntry = m_roles.at(index.row());
        if (role == RoleIdRole) return roleEntry.id;
        if (role == Qt::ToolTipRole)
        {
            return index.column() == Name ? roleEntry.name : roleEntry.displayDescription();
        }
        if (role != Qt::DisplayRole) return {};

        switch (index.column())
        {
        case RoleName:
            return roleEntry.displayName();
        case Name:
            return roleEntry.name;
        case Type:
            return roleEntry.displayType();
        case Side:
            return roleEntry.displaySide();
        case Description:
            return roleEntry.displayDescription();
        default:
            return {};
        }
    }

    QVariant RolesTableModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
    {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal) return {};

        switch (section)
        {
        case RoleName:
            return tr("Role");
        case Name:
            return tr("Name");
        case Type:
            return tr("Type");
        case Side:
            return tr("Side");
        case Description:
            return tr("Description");
        default:
            return {};
        }
    }

    void RolesTableModel::retranslate()
    {
        emit headerDataChanged(Qt::Horizontal, RoleName, ColumnCount - 1);
    }
} // namespace botc::ui
