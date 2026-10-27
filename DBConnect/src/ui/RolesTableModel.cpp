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

    RoleEntry RolesTableModel::roleAt(const int row) const
    {
        if (row < 0 || row >= m_roles.size()) return {};

        return m_roles.at(row);
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
            switch (index.column())
            {
            case Name:
                return roleEntry.name;
            case Type:
                return roleEntry.roleType;
            case Side:
                return roleEntry.alignment;
            case Description:
                return roleEntry.description;
            case NameRu:
                return roleEntry.translationName;
            case TypeRu:
                return roleEntry.roleTypeTranslation;
            case SideRu:
                return roleEntry.alignmentTranslation;
            case DescriptionRu:
                return roleEntry.translationDescription;
            default:
                return {};
            }
        }
        if (role != Qt::DisplayRole) return {};

        switch (index.column())
        {
        case Name:
            return roleEntry.name;
        case Type:
            return roleEntry.roleType;
        case Side:
            return roleEntry.alignment;
        case Description:
            return roleEntry.description;
        case NameRu:
            return roleEntry.translationName;
        case TypeRu:
            return roleEntry.roleTypeTranslation;
        case SideRu:
            return roleEntry.alignmentTranslation;
        case DescriptionRu:
            return roleEntry.translationDescription;
        default:
            return {};
        }
    }

    QVariant RolesTableModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
    {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal) return {};

        switch (section)
        {
        case Name:
            return tr("Name");
        case Type:
            return tr("Type");
        case Side:
            return tr("Side");
        case Description:
            return tr("Description");
        case NameRu:
            return tr("Name (RU)");
        case TypeRu:
            return tr("Type (RU)");
        case SideRu:
            return tr("Side (RU)");
        case DescriptionRu:
            return tr("Description (RU)");
        default:
            return {};
        }
    }

    void RolesTableModel::retranslate()
    {
        emit headerDataChanged(Qt::Horizontal, Name, ColumnCount - 1);
    }
} // namespace botc::ui
