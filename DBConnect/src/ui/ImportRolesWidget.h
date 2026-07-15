#pragma once

#include <QWidget>

#include "QNetworkReply"
#include "../utils/roles/parser/RolesCsvParser.h"

namespace OpenAPI
{
    class OAIRolesImportResponse;
}

class QProgressDialog;

namespace botc::api
{
    class BotCApiClient;
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

    signals:
        void onRolesImported();

    private slots:
        void onBrowseClicked();
        void onImportClicked();
        void onLanguageChanged(const QString& lang);
        void onClearClicked();
        void onRolesImportFinished(const OpenAPI::OAIRolesImportResponse& summary,
                                   QNetworkReply::NetworkError error_type,
                                   const QString& error_str);

    private:
        void showPreview(const utils::ParseResult& result);
        void populateTable(const QString& language);
        void showErrors(const QStringList& errors) const;
        void clearAll();
        void setImportEnabled(bool enabled) const;
        static QString statusStyle(bool ok);

    private:
        Ui::ImportRolesWidget* ui;

        utils::ParseResult m_lastResult;
        QString m_currentLanguage;
        QProgressDialog* m_importRolesProgressDial = nullptr;
    };
} // botc::ui
