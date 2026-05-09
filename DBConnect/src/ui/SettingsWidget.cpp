#include "SettingsWidget.h"

#include <QMessageBox>
#include <QProgressDialog>

#include "ui_SettingsWidget.h"
#include "../api/BotCApiClient.h"
#include "../api/models/HealthResponse.h"
#include "../config/ConfigManager.h"

namespace botc::ui
{
    SettingsWidget::SettingsWidget(QWidget* parent) :
        QWidget(parent), ui(new Ui::SettingsWidget)
    {
        ui->setupUi(this);

        connect(ui->saveButton, &QPushButton::clicked,
                this, &SettingsWidget::onSaveClicked);
        connect(ui->resetButton, &QPushButton::clicked,
                this, &SettingsWidget::onResetClicked);
        connect(ui->toggleKeyButton, &QPushButton::toggled,
                this, &SettingsWidget::onToggleApiKeyVisibility);
        connect(ui->TestConnectionBtn, &QPushButton::clicked,
                this, &SettingsWidget::onTestConnectionClicked);

        loadSettings();
        creatApiClient();
    }

    SettingsWidget::~SettingsWidget()
    {
        delete ui;
    }

    void SettingsWidget::onSaveClicked()
    {
        if (ui->apiUrlEdit->text().trimmed().isEmpty())
        {
            QMessageBox::warning(this, "Ошибка", "Поле API URL не может быть пустым.");
            ui->apiUrlEdit->setFocus();
            return;
        }

        saveSettings();

        // Показываем подтверждение прямо на вкладке
        ui->statusLabel->setText("✔ Настройки сохранены");
        ui->statusLabel->setStyleSheet("color: green;");

        // emit settingsSaved(); // уведомляем другие части приложения
    }

    void SettingsWidget::onResetClicked()
    {
        const auto btn = QMessageBox::question(
            this, "Сброс настроек",
            "Сбросить все настройки к значениям по умолчанию?"
        );

        if (btn == QMessageBox::Yes)
        {
            ui->apiUrlEdit->setText("https://api.example.com");
            ui->apiKeyEdit->clear();
            ui->sslVerifyCheckBox->setChecked(true);

            ui->statusLabel->setText("↺ Настройки сброшены");
            ui->statusLabel->setStyleSheet("color: orange;");
        }
    }

    void SettingsWidget::onToggleApiKeyVisibility(const bool checked) const
    {
        ui->apiKeyEdit->setEchoMode(
            checked ? QLineEdit::Normal : QLineEdit::Password
        );
        ui->toggleKeyButton->setText(checked ? "Скрыть" : "Показать");
    }

    void SettingsWidget::onTestConnectionClicked()
    {
        testConnectionDialog = new QProgressDialog("Проверка подключения...", "", 0, 0, this);
        testConnectionDialog->setWindowModality(Qt::WindowModal);
        testConnectionDialog->setCancelButton(nullptr);
        testConnectionDialog->show();

        m_apiClient->setBaseUrl(ui->apiUrlEdit->text().trimmed());
        m_apiClient->setApiKey(ui->apiKeyEdit->text().trimmed());
        m_apiClient->setSslVerify(ui->sslVerifyCheckBox->isChecked());

        connect(m_apiClient, &api::BotCApiClient::healthCheckFinished,
                this, &SettingsWidget::onTestConnectionFinished);
        m_apiClient->healthCheck();
    }

    void SettingsWidget::onTestConnectionFinished(const bool in_bSuccess,
                                                  const api::models::HealthResponse& in_response)
    {
        disconnect(m_apiClient, &api::BotCApiClient::healthCheckFinished,
                   this, &SettingsWidget::onTestConnectionFinished);
        if (testConnectionDialog) { testConnectionDialog->close(); }

        QString messageText;
        if (in_bSuccess)
        {
            messageText = "Подключение успешно установлено.";
        }
        else
        {
            messageText = "Ошибка подключения: \n" + in_response.status;
        }

        QMessageBox::information(
            this,
            "Проверка подключения",
            messageText
        );
    }

    void SettingsWidget::loadSettings() const
    {
        const auto ConfigManager = config::ConfigManager::instance();
        ui->apiUrlEdit->setText(ConfigManager->getApiUrl());
        ui->apiKeyEdit->setText(ConfigManager->getApiKey());
        ui->sslVerifyCheckBox->setChecked(ConfigManager->getSslVerify());
    }

    void SettingsWidget::saveSettings() const
    {
        const auto ConfigManager = config::ConfigManager::instance();
        ConfigManager->setApiUrl(ui->apiUrlEdit->text().trimmed());
        ConfigManager->setApiKey(ui->apiKeyEdit->text().trimmed());
        ConfigManager->setSslVerify(ui->sslVerifyCheckBox->isChecked());
    }

    void SettingsWidget::creatApiClient()
    {
        const auto ConfigManager = config::ConfigManager::instance();
        m_apiClient              = new api::BotCApiClient(ConfigManager->getApiUrl(), ConfigManager->getApiKey(),
                                             ConfigManager->getSslVerify());
    }
} // botc::ui
