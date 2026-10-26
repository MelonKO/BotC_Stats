#include <QApplication>
#include <QIcon>
#include "config/ConfigManager.h"
#include "i18n/LocalizationManager.h"
#include "ui/MainWindow.h"

int main(int argc, char* argv[])
{
    // Initialize Qt application and parse command-line arguments
    const QApplication app(argc, argv);
    QApplication::setWindowIcon(QIcon(":/icons/app.png"));

    // init ConfigManager and load config
    auto* configManager = botc::config::ConfigManager::instance();
    const auto serverSettings = configManager->getServerSettings();

    // Install the UI translation before the first widget is created, so that
    // setupUi() already sees the translated strings.
    auto* localizationManager = botc::i18n::LocalizationManager::instance();
    localizationManager->applyLanguage(configManager->getUiLanguage());
    QObject::connect(configManager, &botc::config::ConfigManager::uiLanguageChanged,
                     localizationManager,
                     [localizationManager](const QString& language)
                     {
                         localizationManager->applyLanguage(language);
                     });

    // init api client
    auto* apiClient = botc::api::BotCApiClient::instance();
    apiClient->init(serverSettings.apiUrl, serverSettings.apiKey, serverSettings.bSslVerify);

    // Apply a saved settings tab to the client. Slots of one signal run in the
    // order of their connections, and this connection is made before the window
    // exists, so the client is updated before anyone reloads its caches.
    QObject::connect(configManager, &botc::config::ConfigManager::serverSettingsChanged,
                     apiClient,
                     [apiClient](const botc::config::ServerSettings& settings)
                     {
                         apiClient->setBaseUrl(settings.apiUrl);
                         apiClient->setApiKey(settings.apiKey);
                         apiClient->setSslVerify(settings.bSslVerify);
                     });

    // Create main window widget
    botc::ui::MainWindow window;
    // window.setWindowTitle("Qt vcpkg CMake Demo");
    // Show the window and start the Qt event loop
    window.show();
    return QApplication::exec();
}
