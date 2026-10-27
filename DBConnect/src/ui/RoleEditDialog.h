#pragma once

#include <QDialog>

#include "../db_cache/DBCache.h"

namespace botc::ui
{
    QT_BEGIN_NAMESPACE

    namespace Ui
    {
        class RoleEditDialog;
    }

    QT_END_NAMESPACE

    class RoleEditDialog final : public QDialog
    {
        Q_OBJECT

    public:
        explicit RoleEditDialog(const RoleEntry& role, QWidget* parent = nullptr);
        ~RoleEditDialog() override;

        RoleEntry editedRole() const;
        bool hasRussianTranslation() const;

    private slots:
        void accept() override;

    private:
        Ui::RoleEditDialog* ui;
        RoleEntry m_role;
    };
} // namespace botc::ui
