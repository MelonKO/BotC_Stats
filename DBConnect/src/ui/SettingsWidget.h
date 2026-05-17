#pragma once

#include <QWidget>

#include "QNetworkReply"

namespace OpenAPI
{
    class OAIHealth_200_response;
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

    private slots:
        void onSaveClicked();
        void onResetClicked();
        void onToggleApiKeyVisibility(bool checked) const;
        void onTestConnectionClicked();
        void onTestConnectionFinished(const OpenAPI::OAIHealth_200_response& summary, QNetworkReply::NetworkError errorType,
                                      const QString& errorString);

    private:
        void loadSettings() const;
        void saveSettings() const;
        void createApiClient();

    private:
        Ui::SettingsWidget* ui;
        QProgressDialog* testConnectionDialog = nullptr;
    };
} // botc::ui
