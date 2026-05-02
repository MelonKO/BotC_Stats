#include "SettingsWidget.h"
#include "ui_SettingsWidget.h"
#include <QSettings>
#include <QMessageBox>

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

        loadSettings();
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
        auto btn = QMessageBox::question(
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

    void SettingsWidget::onToggleApiKeyVisibility(const bool checked)
    {
        ui->apiKeyEdit->setEchoMode(
            checked ? QLineEdit::Normal : QLineEdit::Password
        );
        ui->toggleKeyButton->setText(checked ? "Скрыть" : "Показать");
    }

    void SettingsWidget::loadSettings()
    {
        auto ConfigManager = botc::config::ConfigManager::instance();
        ui->apiUrlEdit->setText(ConfigManager->getApiUrl());
        ui->apiKeyEdit->setText(ConfigManager->getApiKey());
        ui->sslVerifyCheckBox->setChecked(ConfigManager->getSslVerify());
    }

    void SettingsWidget::saveSettings()
    {
        auto ConfigManager = botc::config::ConfigManager::instance();
        ConfigManager->setApiUrl(ui->apiUrlEdit->text().trimmed());
        ConfigManager->setApiKey(ui->apiKeyEdit->text().trimmed());
        ConfigManager->setSslVerify(ui->sslVerifyCheckBox->isChecked());
    }
} // botc::ui
