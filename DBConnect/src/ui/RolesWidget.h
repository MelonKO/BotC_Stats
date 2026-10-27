#pragma once

#include <QWidget>
#include <QNetworkReply>

class QSortFilterProxyModel;

namespace OpenAPI
{
    class OAIRoleItem;
}

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
     * imports and saved server settings. Role edits are sent to the server and
     * applied to the cache only after the server accepts them.
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
        void onEditClicked();
        void onRoleUpdateFinished(const OpenAPI::OAIRoleItem& role,
                                  QNetworkReply::NetworkError errorType,
                                  int httpStatus, const QString& errorStr);
        void onSelectionChanged();
    private:
        QString selectedRoleId() const;
        void selectRoleById(const QString& roleId);
        void updateCounts();
        void applyStatus();
        void retranslate();
        void setRefreshBusy(bool isBusy);
        void setRoleUpdateBusy(bool isBusy);
        void showMessage(const QString& message, const QString& style);

    private:
        Ui::RolesWidget* ui;
        RolesTableModel* m_model = nullptr;
        QSortFilterProxyModel* m_proxy = nullptr;
        bool m_isRoleUpdateInFlight = false;
        QString m_actionMessage;
        QString m_actionStyle;
    };
} // namespace botc::ui
