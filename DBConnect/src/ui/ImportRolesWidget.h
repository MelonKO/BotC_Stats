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

    protected:
        void changeEvent(QEvent* event) override;

    private slots:
        void onBrowseClicked();
        void onImportClicked();
        void onLanguageChanged(int index);
        void onClearClicked();
        void onRolesImportFinished(const OpenAPI::OAIRolesImportResponse& summary,
                                   QNetworkReply::NetworkError error_type,
                                   const QString& error_str);

    private:
        void showPreview(const utils::ParseResult& result);
        void populateLanguageCombo(const QStringList& languages);
        void populateTable(const QString& languageCode);
        void updatePreviewStatus();
        void retranslate();
        void showErrors(const QStringList& errors) const;
        void clearAll();
        void setImportEnabled(bool enabled) const;
        static QString statusStyle(bool ok);

    private:
        Ui::ImportRolesWidget* ui;

        utils::ParseResult m_lastResult;
        // Empty string means the original (source) values, otherwise a language
        // code such as "ru". The code, never the visible label, identifies the
        // preview language, so translating the label cannot break the preview.
        QString m_currentLanguage;
        QProgressDialog* m_importRolesProgressDial = nullptr;
    };
} // namespace botc::ui
