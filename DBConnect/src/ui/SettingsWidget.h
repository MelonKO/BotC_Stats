#pragma once

#include <QWidget>

#include "QNetworkReply"

namespace OpenAPI
{
    class OAIHealthResponse;
    class OAISystemApi;
}

class QProgressDialog;

namespace botc::api
{
    class BotCApiClient;
}

namespace botc::ui
{
    QT_BEGIN_NAMESPACE

    namespace Ui
    {
        class SettingsWidget;
    }

    QT_END_NAMESPACE

    class SettingsWidget : public QWidget
    {
        Q_OBJECT

    public:
        explicit SettingsWidget(QWidget* parent = nullptr);
        ~SettingsWidget() override;

    protected:
        void changeEvent(QEvent* event) override;

    private slots:
        void onSaveClicked();
        void onResetClicked();
        void onToggleApiKeyVisibility(bool checked) const;
        void onTestConnectionClicked();
        void onTestConnectionFinished(const OpenAPI::OAIHealthResponse& summary, QNetworkReply::NetworkError errorType,
                                      const QString& errorString);
        void onLanguageChanged(int index);

    private:
        void loadSettings() const;
        void saveSettings() const;
        void createApiClient();
        void retranslate();
        void populateLanguageCombo();
        void selectLanguage(const QString& preference);

    private:
        Ui::SettingsWidget* ui;
        QProgressDialog* testConnectionDialog = nullptr;
    };
} // namespace botc::ui
