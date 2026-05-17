#include <QApplication>

#include "config/ConfigManager.h"
#include "ui/MainWindow.h"

int main(int argc, char* argv[])
{
    // Initialize Qt application and parse command-line arguments
    const QApplication app(argc, argv);

    // init ConfigManager and load config
    const auto* configManager = botc::config::ConfigManager::instance();

    // init api client
    auto* apiClient = botc::api::BotCApiClient::instance();
    apiClient->init(configManager->getApiUrl(), configManager->getApiKey(), configManager->getSslVerify());

    // Create main window widget
    botc::ui::MainWindow window;
    // window.setWindowTitle("Qt vcpkg CMake Demo");
    // Show the window and start the Qt event loop
    window.show();
    return QApplication::exec();
}
