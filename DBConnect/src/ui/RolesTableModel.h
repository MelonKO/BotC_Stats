#pragma once

#include <QAbstractTableModel>
#include <QString>
#include <QVariant>
#include <QVector>

#include "../db_cache/DBCache.h"

namespace botc::ui
{
    /**
     * Read-only table model over role records held by DBCache.
     */
    class RolesTableModel final : public QAbstractTableModel
    {
        Q_OBJECT

    public:
        enum Column
        {
            RoleName = 0,
            Name,
            Type,
            Side,
            Description,
            ColumnCount
        };

        enum Role
        {
            RoleIdRole = Qt::UserRole + 1
        };

        explicit RolesTableModel(QObject* parent = nullptr);

        /** Replace all rows with the supplied role records. */
        void setRoles(const QVector<RoleEntry>& roles);

        /** Database id of the row, or an empty string when row is out of range. */
        QString roleIdAt(int row) const;

        int rowCount(const QModelIndex& parent = {}) const override;
        int columnCount(const QModelIndex& parent = {}) const override;
        QVariant data(const QModelIndex& index, int role) const override;
        QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    public slots:
        /** Announces translated column headers after a UI language change. */
        void retranslate();

    private:
        QVector<RoleEntry> m_roles;
    };
} // namespace botc::ui
