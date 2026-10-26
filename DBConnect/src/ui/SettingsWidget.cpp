#include "SettingsWidget.h"

#include <QEvent>
#include <QLocale>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSignalBlocker>

#include "OAISystemApi.h"
#include "ui_SettingsWidget.h"
#include "../api/BotCApiClient.h"
#include "../api/BotCNetworkAccessManager.h"
#include "../config/ConfigManager.h"
#include "../i18n/LocalizationManager.h"

namespace botc::ui
{
    SettingsWidget::SettingsWidget(QWidget* parent) :
        QWidget(parent), ui(new Ui::SettingsWidget)
    {
        ui->setupUi(this);

        populateLanguageCombo();
        connect(ui->languageCombo, qOverload<int>(&QComboBox::currentIndexChanged),
                this, &SettingsWidget::onLanguageChanged);

        connect(ui->saveButton, &QPushButton::clicked,
                this, &SettingsWidget::onSaveClicked);
        connect(ui->resetButton, &QPushButton::clicked,
                this, &SettingsWidget::onResetClicked);
        connect(ui->toggleKeyButton, &QPushButton::toggled,
                this, &SettingsWidget::onToggleApiKeyVisibility);
        connect(ui->TestConnectionBtn, &QPushButton::clicked,
                this, &SettingsWidget::onTestConnectionClicked);

        loadSettings();
        selectLanguage(config::ConfigManager::instance()->getUiLanguage());
    }

    SettingsWidget::~SettingsWidget()
    {
        delete ui;
    }

    void SettingsWidget::changeEvent(QEvent* event)
    {
        if (event->type() == QEvent::LanguageChange)
            retranslate();

        QWidget::changeEvent(event);
    }

    void SettingsWidget::onSaveClicked()
    {
        if (ui->apiUrlEdit->text().trimmed().isEmpty())
        {
            QMessageBox::warning(this, tr("Error"), tr("The API URL field cannot be empty."));
            ui->apiUrlEdit->setFocus();
            return;
        }

        saveSettings();

        // Confirm right on the tab
        ui->statusLabel->setText(tr("✔ Settings saved"));
        ui->statusLabel->setStyleSheet("color: green;");
    }

    void SettingsWidget::onResetClicked()
    {
        const auto btn = QMessageBox::question(
            this, tr("Reset settings"),
            tr("Reset all settings to their default values?")
        );

        if (btn == QMessageBox::Yes)
        {
            ui->apiUrlEdit->setText("https://api.example.com");
            ui->apiKeyEdit->clear();
            ui->sslVerifyCheckBox->setChecked(true);

            // The language follows the system again; selecting it stores the
            // value immediately, like any other change of the selector.
            const int systemIndex = ui->languageCombo->findData(
                QString::fromLatin1(i18n::LocalizationManager::kSystemLanguage));
            if (systemIndex >= 0)
                ui->languageCombo->setCurrentIndex(systemIndex);

            ui->statusLabel->setText(tr("↺ Settings reset"));
            ui->statusLabel->setStyleSheet("color: orange;");
        }
    }

    void SettingsWidget::onToggleApiKeyVisibility(const bool checked) const
    {
        ui->apiKeyEdit->setEchoMode(
            checked ? QLineEdit::Normal : QLineEdit::Password
        );
        ui->toggleKeyButton->setText(checked ? tr("Hide") : tr("Show"));
    }

    void SettingsWidget::onTestConnectionClicked()
    {
        testConnectionDialog = new QProgressDialog(tr("Checking the connection..."), "", 0, 0, this);
        testConnectionDialog->setWindowModality(Qt::WindowModal);
        testConnectionDialog->setCancelButton(nullptr);
        testConnectionDialog->show();

        auto* systemAPI = new OpenAPI::OAISystemApi{};
        systemAPI->addHeaders("X-API-Key", ui->apiKeyEdit->text().trimmed());
        systemAPI->setNewServerForAllOperations(QUrl(ui->apiUrlEdit->text().trimmed()));

        // The test must use the certificate policy shown on the tab, but must not
        // change the policy of the application-wide API client.
        auto* networkManager = new api::BotCNetworkAccessManager{systemAPI};
        networkManager->setPeerVerificationEnabled(ui->sslVerifyCheckBox->isChecked());
        systemAPI->setNetworkAccessManager(networkManager);

        connect(systemAPI, &OpenAPI::OAISystemApi::healthSignalError,
                this,
                [this, systemAPI](const OpenAPI::OAIHealthResponse& summary,
                                  const QNetworkReply::NetworkError errorType,
                                  const QString& errorString)
                {
                    onTestConnectionFinished(std::move(summary), std::move(errorType), std::move(errorString));
                    systemAPI->deleteLater();
                }, Qt::SingleShotConnection);

        connect(systemAPI, &OpenAPI::OAISystemApi::healthSignal,
                this, [this, systemAPI](const OpenAPI::OAIHealthResponse& summary)
                {
                    onTestConnectionFinished(summary, QNetworkReply::NoError, "");
                    systemAPI->deleteLater();
                }, Qt::SingleShotConnection);

        systemAPI->health();
    }

    void SettingsWidget::onTestConnectionFinished(const OpenAPI::OAIHealthResponse& summary,
                                                  const QNetworkReply::NetworkError errorType,
                                                  const QString& errorString)
    {
        if (testConnectionDialog) { testConnectionDialog->close(); }

        QString messageText;
        if (errorType == QNetworkReply::NoError)
        {
            messageText = tr("Connection established successfully.");
        }
        else
        {
            messageText = tr("Connection failed: \n%1").arg(summary.getStatus());
        }

        QMessageBox::information(
            this,
            tr("Connection check"),
            messageText
        );
    }

    void SettingsWidget::onLanguageChanged(const int index)
    {
        const QString preference = ui->languageCombo->itemData(index).toString();
        if (preference.isEmpty())
            return;

        // Stored immediately: the language must change now, not after "Save".
        config::ConfigManager::instance()->setUiLanguage(preference);
    }

    void SettingsWidget::loadSettings() const
    {
        const auto settings = config::ConfigManager::instance()->getServerSettings();
        ui->apiUrlEdit->setText(settings.apiUrl);
        ui->apiKeyEdit->setText(settings.apiKey);
        ui->sslVerifyCheckBox->setChecked(settings.bSslVerify);
    }

    void SettingsWidget::saveSettings() const
    {
        // One call per save: the manager stores all values and emits a single
        // serverSettingsChanged(), so receivers reload their caches once.
        config::ServerSettings settings;
        settings.apiUrl = ui->apiUrlEdit->text().trimmed();
        settings.apiKey = ui->apiKeyEdit->text().trimmed();
        settings.bSslVerify = ui->sslVerifyCheckBox->isChecked();

        config::ConfigManager::instance()->setServerSettings(settings);
    }

    void SettingsWidget::retranslate()
    {
        ui->retranslateUi(this);

        populateLanguageCombo();
        selectLanguage(config::ConfigManager::instance()->getUiLanguage());

        ui->toggleKeyButton->setText(ui->toggleKeyButton->isChecked() ? tr("Hide") : tr("Show"));
        ui->statusLabel->clear();
    }

    void SettingsWidget::populateLanguageCombo()
    {
        const QSignalBlocker blocker(ui->languageCombo);

        ui->languageCombo->clear();
        ui->languageCombo->addItem(
            tr("System (%1)").arg(QLocale::system().nativeLanguageName()),
            QString::fromLatin1(i18n::LocalizationManager::kSystemLanguage));

        for (const QString& languageCode : i18n::LocalizationManager::instance()->availableLanguages())
        {
            ui->languageCombo->addItem(
                i18n::LocalizationManager::displayName(languageCode),
                languageCode);
        }
    }

    void SettingsWidget::selectLanguage(const QString& preference)
    {
        int index = ui->languageCombo->findData(preference);
        if (index < 0)
            index = 0;

        const QSignalBlocker blocker(ui->languageCombo);
        ui->languageCombo->setCurrentIndex(index);
    }
} // namespace botc::ui
