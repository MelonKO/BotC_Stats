#pragma once

#include <QWidget>

#include "../utils/roles/parser/RolesCsvParser.h"

class QProgressDialog;

namespace botc::api
{
    class BotCApiClient;

    namespace models::roles
    {
        struct RolesImportResponse;
    }
}

namespace botc::utils
{
    struct ParseResult;
}

namespace botc::ui
{
    QT_BEGIN_NAMESPACE

    namespace Ui
    {
        class ImportRolesWidget;
    }

    QT_END_NAMESPACE

    class ImportRolesWidget : public QWidget
    {
        Q_OBJECT

    public:
        explicit ImportRolesWidget(QWidget* parent = nullptr);
        ~ImportRolesWidget() override;

    private slots:
        void onBrowseClicked();
        void onImportClicked();
        void onLanguageChanged(const QString& lang);
        void onClearClicked();
        void onRolesImportFinished(bool in_bSuccess, const api::models::roles::RolesImportResponse& in_response);

    private:
        void showPreview(const utils::ParseResult& result);
        void populateTable(const QString& language);
        void showErrors(const QStringList& errors) const;
        void clearAll();
        void setImportEnabled(bool enabled) const;
        static QString statusStyle(bool ok);
        void initApiClient();

    private:
        Ui::ImportRolesWidget* ui;

        utils::ParseResult m_lastResult;
        QString m_currentLanguage;
        QProgressDialog* m_importRolesProgressDial = nullptr;
        api::BotCApiClient* m_apiClient            = nullptr;
    };
} // botc::ui
