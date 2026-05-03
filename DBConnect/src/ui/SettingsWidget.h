#pragma once

#include <QWidget>

#include "../api/models/GameImportRequest.h"

class QProgressDialog;

namespace botc::api
{
    namespace models
    {
        struct HealthResponse;
    }

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
        void onTestConnectionFinished(bool in_bSuccess, const api::models::HealthResponse& in_response);

    private:
        void loadSettings() const;
        void saveSettings() const;
        void creatApiClient();

    private:
        Ui::SettingsWidget* ui;
        api::BotCApiClient* m_apiClient       = nullptr;
        QProgressDialog* testConnectionDialog = nullptr;
    };
} // botc::ui
