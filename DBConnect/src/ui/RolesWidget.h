#pragma once

#include <QWidget>

class QSortFilterProxyModel;

namespace botc::ui
{
    QT_BEGIN_NAMESPACE

    namespace Ui
    {
        class RolesWidget;
    }

    QT_END_NAMESPACE

    class RolesTableModel;

    /**
     * Read-only view of the role registry.
     *
     * The list comes from DBCache, the single owner of role data, so it follows
     * imports and saved server settings. Editing roles is planned for a later stage.
     */
    class RolesWidget final : public QWidget
    {
        Q_OBJECT

    public:
        explicit RolesWidget(QWidget* parent = nullptr);
        ~RolesWidget() override;

    protected:
        void changeEvent(QEvent* event) override;

    private slots:
        void onSearchTextChanged(const QString& text);
        void onRefreshClicked();
        void refreshFromCache();
        void onRolesRequestStarted();

    private:
        QString selectedRoleId() const;
        void selectRoleById(const QString& roleId);
        void updateCounts();
        void applyStatus();
        void retranslate();
        void setRefreshBusy(bool isBusy);

    private:
        Ui::RolesWidget* ui;
        RolesTableModel* m_model = nullptr;
        QSortFilterProxyModel* m_proxy = nullptr;
    };
} // namespace botc::ui
